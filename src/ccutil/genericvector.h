///////////////////////////////////////////////////////////////////////
// File:        genericvector.h
// Description: Generic vector class
// Author:      Daria Antonova
//
// (C) Copyright 2007, Google Inc.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////
//
#ifndef TESSERACT_CCUTIL_GENERICVECTOR_H_
#define TESSERACT_CCUTIL_GENERICVECTOR_H_

#include <tesseract/helpers.h>
#include "serialis.h"

#include <algorithm>
#include <cassert>
#include <climits>  // for LONG_MAX
#include <cstdint>  // for uint32_t
#include <cstdio>
#include <cstdlib>
#include <functional>  // for std::function
#include <vector>

namespace tesseract {

// Use PointerVector<T> below in preference to GenericVector<T*>, as that
// provides automatic deletion of pointers, [De]Serialize that works, and
// sort that works.
template <typename T>
class GenericVector : public std::vector<T> {
  using base = std::vector<T>;
 public:
  using std::vector<T>::vector;

  using base::begin;
  using base::end;
  using base::data;
  using base::capacity;
  using base::reserve;
  using base::resize;
  using base::back;
  using base::clear;
  using base::push_back;

  GenericVector<T>& operator+=(const GenericVector& other);

  // Double the size of the internal array.
  void double_the_size();

  // Resizes to size and sets all values to t.
  void init_to_size(int size, const T& t);
  // Resizes to size without any initialization.
  void resize_no_init(int size) {
    resize(size);
  }

  // Workaround to avoid g++ -Wsign-compare warnings.
  size_t unsigned_size() const {
    return size();
  }
  int size_reserved() const {
    return capacity();
  }

  int size() const {
      return base::size();
  }

  // Return the object from an index.
  T& get(int index);
  const T& get(int index) const;
  // Returns the last object and removes it.
  T pop_back();

  // Return the index of the T object.
  // This method NEEDS a compare_callback to be passed to
  // set_compare_callback.
  int get_index(const T& object) const;

  // Return true if T is in the array
  bool contains(const T& object) const;

  // Return true if the index is valid
  T contains_index(int index) const;

  // Push an element in the end of the array
  void operator+=(const T& t);

  // Push an element in the end of the array if the same
  // element is not already contained in the array.
  int push_back_new(const T& object);

  // Push an element in the front of the array
  // Note: This function is O(n)
  int push_front(const T& object);

  // Set the value at the given index
  void set(const T& t, int index);

  // Insert t at the given index, push other elements to the right.
  void insert(const T& t, int index);

  // Removes an element at the given index and
  // shifts the remaining elements to the left.
  void remove(int index);

  // Truncates the array to the given size by removing the end.
  // If the current size is less, the array is not expanded.
  void truncate(int size) {
      resize(size);
  }

  // Add a callback to be called to delete the elements when the array took
  // their ownership.
  void set_clear_callback(std::function<void(T)> cb) {
    clear_cb_ = cb;
  }

  // Add a callback to be called to compare the elements when needed (contains,
  // get_id, ...)
  void set_compare_callback(std::function<bool(const T&, const T&)> cb) {
    compare_cb_ = cb;
  }

  // Delete objects pointed to by data()[i]
  void delete_data_pointers();

  // This method clears the current object, then, does a shallow copy of
  // its argument, and finally invalidates its argument.
  // Callbacks are moved to the current object;
  void move(GenericVector<T>* from);

  // Read/Write the array to a file. This does _NOT_ read/write the callbacks.
  // The callback given must be permanent since they will be called more than
  // once. The given callback will be deleted at the end.
  // If the callbacks are nullptr, then the data is simply read/written using
  // fread (and swapping)/fwrite.
  // Returns false on error or if the callback returns false.
  // DEPRECATED. Use [De]Serialize[Classes] instead.
  bool write(FILE* f, std::function<bool(FILE*, const T&)> cb) const;
  bool read(TFile* f, std::function<bool(TFile*, T*)> cb);
  // Writes a vector of simple types to the given file. Assumes that bitwise
  // read/write of T will work. Returns false in case of error.
  // TODO(rays) Change all callers to use TFile and remove deprecated methods.
  bool Serialize(FILE* fp) const;
  bool Serialize(TFile* fp) const;
  // Reads a vector of simple types from the given file. Assumes that bitwise
  // read/write will work with ReverseN according to sizeof(T).
  // Returns false in case of error.
  // If swap is true, assumes a big/little-endian swap is needed.
  // TFile is assumed to know about swapping.
  bool DeSerialize(bool swap, FILE* fp);
  bool DeSerialize(TFile* fp);
  // Skips the deserialization of the vector.
  static bool SkipDeSerialize(TFile* fp);
  // Writes a vector of classes to the given file. Assumes the existence of
  // bool T::Serialize(FILE* fp) const that returns false in case of error.
  // Returns false in case of error.
  bool SerializeClasses(FILE* fp) const;
  bool SerializeClasses(TFile* fp) const;
  // Reads a vector of classes from the given file. Assumes the existence of
  // bool T::Deserialize(bool swap, FILE* fp) that returns false in case of
  // error. Also needs T::T() and T::T(constT&), as init_to_size is used in
  // this function. Returns false in case of error.
  // If swap is true, assumes a big/little-endian swap is needed.
  bool DeSerializeClasses(bool swap, FILE* fp);
  bool DeSerializeClasses(TFile* fp);
  // Calls SkipDeSerialize on the elements of the vector.
  static bool SkipDeSerializeClasses(TFile* fp);

  // Allocates a new array of double the current_size, copies over the
  // information from data to the new location, deletes data and returns
  // the pointed to the new larger array.
  // This function uses memcpy to copy the data, instead of invoking
  // operator=() for each element like double_the_size() does.
  static T* double_the_size_memcpy(int current_size, T* data) {
    T* data_new = new T[current_size * 2];
    memcpy(data_new, data, sizeof(T) * current_size);
    delete[] data;
    return data_new;
  }

  // Reverses the elements of the vector.
  void reverse() {
    for (int i = 0; i < size() / 2; ++i) {
      Swap(&data()[i], &data()[size() - 1 - i]);
    }
  }

  // Sorts the members of this vector using the less than comparator (cmp_lt),
  // which compares the values. Useful for GenericVectors to primitive types.
  // Will not work so great for pointers (unless you just want to sort some
  // pointers). You need to provide a specialization to sort_cmp to use
  // your type.
  void sort();

  // Sort the array into the order defined by the qsort function comparator.
  // The comparator function is as defined by qsort, ie. it receives pointers
  // to two Ts and returns negative if the first element is to appear earlier
  // in the result and positive if it is to appear later, with 0 for equal.
  void sort(int (*comparator)(const void*, const void*)) {
    qsort(data(), size(), sizeof(*data()), comparator);
  }

  // Searches the array (assuming sorted in ascending order, using sort()) for
  // an element equal to target and returns true if it is present.
  // Use binary_search to get the index of target, or its nearest candidate.
  bool bool_binary_search(const T& target) const {
    int index = binary_search(target);
    if (index >= size()) {
      return false;
    }
    return data()[index] == target;
  }
  // Searches the array (assuming sorted in ascending order, using sort()) for
  // an element equal to target and returns the index of the best candidate.
  // The return value is conceptually the largest index i such that
  // data()[i] <= target or 0 if target < the whole vector.
  // NOTE that this function uses operator> so really the return value is
  // the largest index i such that data()[i] > target is false.
  int binary_search(const T& target) const {
    int bottom = 0;
    int top = size();
    while (top - bottom > 1) {
      int middle = (bottom + top) / 2;
      if (data()[middle] > target) {
        top = middle;
      } else {
        bottom = middle;
      }
    }
    return bottom;
  }

  // Compact the vector by deleting elements using operator!= on basic types.
  // The vector must be sorted.
  void compact_sorted() {
    if (size() == 0) {
      return;
    }

    // First element is in no matter what, hence the i = 1.
    int last_write = 0;
    for (int i = 1; i < size(); ++i) {
      // Finds next unique item and writes it.
      if (data()[last_write] != data()[i]) {
        data()[++last_write] = data()[i];
      }
    }
    // last_write is the index of a valid data cell, so add 1.
    resize(last_write + 1);
  }

  // Returns the index of what would be the target_index_th item in the array
  // if the members were sorted, without actually sorting. Members are
  // shuffled around, but it takes O(n) time.
  // NOTE: uses operator< and operator== on the members.
  int choose_nth_item(int target_index) {
    // Make sure target_index is legal.
    if (target_index < 0) {
      target_index = 0;  // ensure legal
    } else if (target_index >= size()) {
      target_index = size() - 1;
    }
    unsigned int seed = 1;
    return choose_nth_item(target_index, 0, size(), &seed);
  }

  // Swaps the elements with the given indices.
  void swap(int index1, int index2) {
    if (index1 != index2) {
      T tmp = data()[index1];
      data()[index1] = data()[index2];
      data()[index2] = tmp;
    }
  }
  // Returns true if all elements of *this are within the given range.
  // Only uses operator<
  bool WithinBounds(const T& rangemin, const T& rangemax) const {
    for (int i = 0; i < size(); ++i) {
      if (data()[i] < rangemin || rangemax < data()[i]) {
        return false;
      }
    }
    return true;
  }

 protected:
  // Internal recursive version of choose_nth_item.
  int choose_nth_item(int target_index, int start, int end, unsigned int* seed);

  // Init the object, allocating size memory.
  void init(int size);

  // We are assuming that the object generally placed in the
  // vector are small enough that for efficiency it makes sense
  // to start with a larger initial size.
  static const int kDefaultVectorSize = 4;
  std::function<void(T)> clear_cb_;
  std::function<bool(const T&, const T&)> compare_cb_;
};

#if defined(_MSC_VER) || defined(__APPLE__)
// MSVC stl does not have ::data() in vector<bool>,
// so we add custom specialization.
// On Apple there are also errors when using std::vector<bool>,
// so we replace it with vector<int> as a workaround.
template <>
class GenericVector<bool> : public std::vector<int> {};
#endif

template <typename T>
bool cmp_eq(T const& t1, T const& t2) {
  return t1 == t2;
}

// Used by sort()
// return < 0 if t1 < t2
// return 0 if t1 == t2
// return > 0 if t1 > t2
template <typename T>
int sort_cmp(const void* t1, const void* t2) {
  const T* a = static_cast<const T*>(t1);
  const T* b = static_cast<const T*>(t2);
  if (*a < *b) {
    return -1;
  }
  if (*b < *a) {
    return 1;
  }
  return 0;
}

// Used by PointerVector::sort()
// return < 0 if t1 < t2
// return 0 if t1 == t2
// return > 0 if t1 > t2
template <typename T>
int sort_ptr_cmp(const void* t1, const void* t2) {
  const T* a = *static_cast<T* const*>(t1);
  const T* b = *static_cast<T* const*>(t2);
  if (*a < *b) {
    return -1;
  }
  if (*b < *a) {
    return 1;
  }
  return 0;
}

// Subclass for a vector of pointers. Use in preference to GenericVector<T*>
// as it provides automatic deletion and correct serialization, with the
// corollary that all copy operations are deep copies of the pointed-to objects.
template <typename T>
class PointerVector : public GenericVector<T*> {
 public:
  PointerVector() : GenericVector<T*>() {}
  explicit PointerVector(int size) : GenericVector<T*>(size) {}
  ~PointerVector() {
    // Clear must be called here, even though it is called again by the base,
    // as the base will call the wrong clear.
    clear();
  }
  // Copy must be deep, as the pointers will be automatically deleted on
  // destruction.
  PointerVector(const PointerVector& other) : GenericVector<T*>(other) {
    this->init(other.size());
    this->operator+=(other);
  }
  PointerVector<T>& operator+=(const PointerVector& other) {
    this->reserve(this->size() + other.size());
    for (int i = 0; i < other.size(); ++i) {
      this->push_back(new T(*other.data()[i]));
    }
    return *this;
  }

  PointerVector<T>& operator=(const PointerVector& other) {
    if (&other != this) {
      this->truncate(0);
      this->operator+=(other);
    }
    return *this;
  }

  // Removes an element at the given index and
  // shifts the remaining elements to the left.
  void remove(int index) {
    delete GenericVector<T*>::data()[index];
    GenericVector<T*>::remove(index);
  }

  // Truncates the array to the given size by removing the end.
  // If the current size is less, the array is not expanded.
  void truncate(int size) {
    for (int i = size; i < GenericVector<T*>::size(); ++i) {
      delete GenericVector<T*>::data()[i];
    }
    GenericVector<T*>::truncate(size);
  }

  // Compact the vector by deleting elements for which delete_cb returns
  // true. delete_cb is a permanent callback and will be deleted.
  void compact(std::function<bool(const T*)> delete_cb) {
    int new_size = 0;
    int old_index = 0;
    // Until the callback returns true, the elements stay the same.
    while (old_index < GenericVector<T*>::size() &&
           !delete_cb(GenericVector<T*>::data()[old_index++])) {
      ++new_size;
    }
    // Now just copy anything else that gets false from delete_cb.
    for (; old_index < GenericVector<T*>::size(); ++old_index) {
      if (!delete_cb(GenericVector<T*>::data()[old_index])) {
        GenericVector<T*>::data()[new_size++] =
            GenericVector<T*>::data()[old_index];
      } else {
        delete GenericVector<T*>::data()[old_index];
      }
    }
    GenericVector<T*>::resize(new_size);
  }

  // Clear the array, calling the clear callback function if any.
  // All the owned callbacks are also deleted.
  // If you don't want the callbacks to be deleted, before calling clear, set
  // the callback to nullptr.
  void clear() {
    GenericVector<T*>::delete_data_pointers();
    GenericVector<T*>::clear();
  }

  // Writes a vector of (pointers to) classes to the given file. Assumes the
  // existence of bool T::Serialize(FILE*) const that returns false in case of
  // error. There is no Serialize for simple types, as you would have a
  // normal GenericVector of those.
  // Returns false in case of error.
  bool Serialize(FILE* fp) const {
    int32_t used = GenericVector<T*>::size();
    if (fwrite(&used, sizeof(used), 1, fp) != 1) {
      return false;
    }
    for (int i = 0; i < used; ++i) {
      int8_t non_null = GenericVector<T*>::data()[i] != nullptr;
      if (fwrite(&non_null, sizeof(non_null), 1, fp) != 1) {
        return false;
      }
      if (non_null && !GenericVector<T*>::data()[i]->Serialize(fp)) {
        return false;
      }
    }
    return true;
  }
  bool Serialize(TFile* fp) const {
    int32_t used = GenericVector<T*>::size();
    if (fp->FWrite(&used, sizeof(used), 1) != 1) {
      return false;
    }
    for (int i = 0; i < used; ++i) {
      int8_t non_null = GenericVector<T*>::data()[i] != nullptr;
      if (fp->FWrite(&non_null, sizeof(non_null), 1) != 1) {
        return false;
      }
      if (non_null && !GenericVector<T*>::data()[i]->Serialize(fp)) {
        return false;
      }
    }
    return true;
  }
  // Reads a vector of (pointers to) classes to the given file. Assumes the
  // existence of bool T::DeSerialize(bool, Tfile*) const that returns false in
  // case of error. There is no Serialize for simple types, as you would have a
  // normal GenericVector of those.
  // If swap is true, assumes a big/little-endian swap is needed.
  // Also needs T::T(), as new T is used in this function.
  // Returns false in case of error.
  bool DeSerialize(bool swap, FILE* fp) {
    uint32_t reserved;
    if (fread(&reserved, sizeof(reserved), 1, fp) != 1) {
      return false;
    }
    if (swap) {
      Reverse32(&reserved);
    }
    // Arbitrarily limit the number of elements to protect against bad data.
    assert(reserved <= UINT16_MAX);
    if (reserved > UINT16_MAX) {
      return false;
    }
    GenericVector<T*>::reserve(reserved);
    truncate(0);
    for (uint32_t i = 0; i < reserved; ++i) {
      int8_t non_null;
      if (fread(&non_null, sizeof(non_null), 1, fp) != 1) {
        return false;
      }
      T* item = nullptr;
      if (non_null != 0) {
        item = new T;
        if (!item->DeSerialize(swap, fp)) {
          delete item;
          return false;
        }
        this->push_back(item);
      } else {
        // Null elements should keep their place in the vector.
        this->push_back(nullptr);
      }
    }
    return true;
  }
  bool DeSerialize(TFile* fp) {
    int32_t reserved;
    if (!DeSerializeSize(fp, &reserved)) {
      return false;
    }
    GenericVector<T*>::reserve(reserved);
    truncate(0);
    for (int i = 0; i < reserved; ++i) {
      if (!DeSerializeElement(fp)) {
        return false;
      }
    }
    return true;
  }
  // Enables deserialization of a selection of elements. Note that in order to
  // retain the integrity of the stream, the caller must call some combination
  // of DeSerializeElement and DeSerializeSkip of the exact number returned in
  // *size, assuming a true return.
  static bool DeSerializeSize(TFile* fp, int32_t* size) {
    return fp->FReadEndian(size, sizeof(*size), 1) == 1;
  }
  // Reads and appends to the vector the next element of the serialization.
  bool DeSerializeElement(TFile* fp) {
    int8_t non_null;
    if (fp->FRead(&non_null, sizeof(non_null), 1) != 1) {
      return false;
    }
    T* item = nullptr;
    if (non_null != 0) {
      item = new T;
      if (!item->DeSerialize(fp)) {
        delete item;
        return false;
      }
      this->push_back(item);
    } else {
      // Null elements should keep their place in the vector.
      this->push_back(nullptr);
    }
    return true;
  }
  // Skips the next element of the serialization.
  static bool DeSerializeSkip(TFile* fp) {
    int8_t non_null;
    if (fp->FRead(&non_null, sizeof(non_null), 1) != 1) {
      return false;
    }
    if (non_null != 0) {
      if (!T::SkipDeSerialize(fp)) {
        return false;
      }
    }
    return true;
  }

  // Sorts the items pointed to by the members of this vector using
  // t::operator<().
  void sort() {
    this->GenericVector<T*>::sort(&sort_ptr_cmp<T>);
  }
};

// A useful vector that uses operator== to do comparisons.
template <typename T>
class GenericVectorEqEq : public GenericVector<T> {
 public:
  GenericVectorEqEq() {
    using namespace std::placeholders;  // for _1
    GenericVector<T>::set_compare_callback(
        std::bind(cmp_eq<T>, _1, _2));
  }
  explicit GenericVectorEqEq(int size) : GenericVector<T>(size) {
    using namespace std::placeholders;  // for _1
    GenericVector<T>::set_compare_callback(
        std::bind(cmp_eq<T>, _1, _2));
  }
};

template <typename T>
void GenericVector<T>::init(int size) {
    clear();
    resize(size);
}

template <typename T>
void GenericVector<T>::double_the_size() {
  if (capacity() == 0) {
    reserve(kDefaultVectorSize);
  } else {
    reserve(2 * capacity());
  }
}

// Resizes to size and sets all values to t.
template <typename T>
void GenericVector<T>::init_to_size(int size, const T& t) {
  resize(size, t);
}

// Return the object from an index.
template <typename T>
T& GenericVector<T>::get(int index) {
  assert(index >= 0 && index < size());
  return data()[index];
}

// Return the object from an index.
template <typename T>
const T& GenericVector<T>::get(int index) const {
  assert(index >= 0 && index < size());
  return data()[index];
}

// Returns the last object and removes it.
template <typename T>
T GenericVector<T>::pop_back() {
  auto b = back();
  base::pop_back();
  return b;
}

// Return the object from an index.
template <typename T>
void GenericVector<T>::set(const T& t, int index) {
  assert(index >= 0 && index < size());
  data()[index] = t;
}

// Shifts the rest of the elements to the right to make
// space for the new elements and inserts the given element
// at the specified index.
template <typename T>
void GenericVector<T>::insert(const T& t, int index) {
  base::insert(begin() + index, t);
}

// Removes an element at the given index and
// shifts the remaining elements to the left.
template <typename T>
void GenericVector<T>::remove(int index) {
  assert(index >= 0 && index < size());
  for (int i = index; i < size() - 1; ++i) {
    data()[i] = data()[i + 1];
  }
  resize(size() - 1);
}

// Return true if the index is valindex
template <typename T>
T GenericVector<T>::contains_index(int index) const {
  return index >= 0 && index < size();
}

// Return the index of the T object.
template <typename T>
int GenericVector<T>::get_index(const T& object) const {
  for (int i = 0; i < size(); ++i) {
    assert(compare_cb_ != nullptr);
    if (compare_cb_(object, data()[i])) {
      return i;
    }
  }
  return -1;
}

// Return true if T is in the array
template <typename T>
bool GenericVector<T>::contains(const T& object) const {
  return get_index(object) != -1;
}

template <typename T>
int GenericVector<T>::push_back_new(const T& object) {
  int index = get_index(object);
  if (index >= 0) {
    return index;
  }
  push_back(object);
  return size();
}

// Add an element in the array (front)
template <typename T>
int GenericVector<T>::push_front(const T& object) {
  insert(begin(), object);
  return 0;
}

template <typename T>
void GenericVector<T>::operator+=(const T& t) {
  push_back(t);
}

template <typename T>
GenericVector<T>& GenericVector<T>::operator+=(const GenericVector& other) {
  this->reserve(size() + other.size());
  for (int i = 0; i < other.size(); ++i) {
    this->operator+=(other.data()[i]);
  }
  return *this;
}

template <typename T>
void GenericVector<T>::delete_data_pointers() {
  for (int i = 0; i < size(); ++i) {
    delete data()[i];
  }
}

template <typename T>
bool GenericVector<T>::write(FILE* f,
                             std::function<bool(FILE*, const T&)> cb) const {
  int32_t cp = capacity();
  if (fwrite(&cp, sizeof(cp), 1, f) != 1) {
    return false;
  }
  int32_t sz = size();
  if (fwrite(&sz, sizeof(sz), 1, f) != 1) {
    return false;
  }
  if (cb != nullptr) {
    for (int i = 0; i < size(); ++i) {
      if (!cb(f, data()[i])) {
        return false;
      }
    }
  } else {
    if (fwrite(data(), sizeof(T), size(), f) != unsigned_size()) {
      return false;
    }
  }
  return true;
}

template <typename T>
bool GenericVector<T>::read(TFile* f,
                            std::function<bool(TFile*, T*)> cb) {
  int32_t reserved, size;
  if (f->FReadEndian(&reserved, sizeof(reserved), 1) != 1) {
    return false;
  }
  reserve(reserved);
  if (f->FReadEndian(&size, sizeof(size), 1) != 1) {
    return false;
  }
  resize(size);
  if (cb != nullptr) {
    for (int i = 0; i < size; ++i) {
      if (!cb(f, data() + i)) {
        return false;
      }
    }
  } else {
    if (f->FReadEndian(data(), sizeof(T), size) != size) {
      return false;
    }
  }
  return true;
}

// Writes a vector of simple types to the given file. Assumes that bitwise
// read/write of T will work. Returns false in case of error.
template <typename T>
bool GenericVector<T>::Serialize(FILE* fp) const {
  int32_t sz = size();
  if (fwrite(&sz, sizeof(sz), 1, fp) != 1) {
    return false;
  }
  if (fwrite(data(), sizeof(T), sz, fp) != unsigned_size()) {
    return false;
  }
  return true;
}
template <typename T>
bool GenericVector<T>::Serialize(TFile* fp) const {
  int32_t sz = size();
  if (fp->FWrite(&sz, sizeof(sz), 1) != 1) {
    return false;
  }
  if (fp->FWrite(data(), sizeof(T), sz) != sz) {
    return false;
  }
  return true;
}

// Reads a vector of simple types from the given file. Assumes that bitwise
// read/write will work with ReverseN according to sizeof(T).
// Returns false in case of error.
// If swap is true, assumes a big/little-endian swap is needed.
template <typename T>
bool GenericVector<T>::DeSerialize(bool swap, FILE* fp) {
  uint32_t reserved;
  if (fread(&reserved, sizeof(reserved), 1, fp) != 1) {
    return false;
  }
  if (swap) {
    Reverse32(&reserved);
  }
  // Arbitrarily limit the number of elements to protect against bad data.
  assert(reserved <= UINT16_MAX);
  if (reserved > UINT16_MAX) {
    return false;
  }
  resize(reserved);
  if (fread(data(), sizeof(T), size(), fp) != unsigned_size()) {
    return false;
  }
  if (swap) {
    for (int i = 0; i < size(); ++i) {
      ReverseN(&data()[i], sizeof(data()[i]));
    }
  }
  return true;
}
template <typename T>
bool GenericVector<T>::DeSerialize(TFile* fp) {
  uint32_t reserved;
  if (fp->FReadEndian(&reserved, sizeof(reserved), 1) != 1) {
    return false;
  }
  // Arbitrarily limit the number of elements to protect against bad data.
  const uint32_t limit = 50000000;
  assert(reserved <= limit);
  if (reserved > limit) {
    return false;
  }
  resize(reserved);
  return fp->FReadEndian(data(), sizeof(T), size()) == size();
}
template <typename T>
bool GenericVector<T>::SkipDeSerialize(TFile* fp) {
  uint32_t reserved;
  if (fp->FReadEndian(&reserved, sizeof(reserved), 1) != 1) {
    return false;
  }
  return (uint32_t)fp->FRead(nullptr, sizeof(T), reserved) == reserved;
}

// Writes a vector of classes to the given file. Assumes the existence of
// bool T::Serialize(FILE* fp) const that returns false in case of error.
// Returns false in case of error.
template <typename T>
bool GenericVector<T>::SerializeClasses(FILE* fp) const {
  int32_t sz = size();
  if (fwrite(&sz, sizeof(sz), 1, fp) != 1) {
    return false;
  }
  for (int i = 0; i < sz; ++i) {
    if (!data()[i].Serialize(fp)) {
      return false;
    }
  }
  return true;
}
template <typename T>
bool GenericVector<T>::SerializeClasses(TFile* fp) const {
  int32_t sz = size();
  if (fp->FWrite(&sz, sizeof(sz), 1) != 1) {
    return false;
  }
  for (int i = 0; i < sz; ++i) {
    if (!data()[i].Serialize(fp)) {
      return false;
    }
  }
  return true;
}

// Reads a vector of classes from the given file. Assumes the existence of
// bool T::Deserialize(bool swap, FILE* fp) that returns false in case of
// error. Also needs T::T() and T::T(constT&), as init_to_size is used in
// this function. Returns false in case of error.
// If swap is true, assumes a big/little-endian swap is needed.
template <typename T>
bool GenericVector<T>::DeSerializeClasses(bool swap, FILE* fp) {
  int32_t reserved;
  if (fread(&reserved, sizeof(reserved), 1, fp) != 1) {
    return false;
  }
  if (swap) {
    Reverse32(&reserved);
  }
  T empty;
  init_to_size(reserved, empty);
  for (int i = 0; i < reserved; ++i) {
    if (!data()[i].DeSerialize(swap, fp)) {
      return false;
    }
  }
  return true;
}
template <typename T>
bool GenericVector<T>::DeSerializeClasses(TFile* fp) {
  int32_t reserved;
  if (fp->FReadEndian(&reserved, sizeof(reserved), 1) != 1) {
    return false;
  }
  T empty;
  init_to_size(reserved, empty);
  for (int i = 0; i < reserved; ++i) {
    if (!data()[i].DeSerialize(fp)) {
      return false;
    }
  }
  return true;
}
template <typename T>
bool GenericVector<T>::SkipDeSerializeClasses(TFile* fp) {
  int32_t reserved;
  if (fp->FReadEndian(&reserved, sizeof(reserved), 1) != 1) {
    return false;
  }
  for (int i = 0; i < reserved; ++i) {
    if (!T::SkipDeSerialize(fp)) {
      return false;
    }
  }
  return true;
}

// This method clear the current object, then, does a shallow copy of
// its argument, and finally invalidates its argument.
template <typename T>
void GenericVector<T>::move(GenericVector<T>* from) {
  *this = std::move(*from);
}

template <typename T>
void GenericVector<T>::sort() {
  sort(&sort_cmp<T>);
}

// Internal recursive version of choose_nth_item.
// The algorithm used comes from "Algorithms" by Sedgewick:
// http://books.google.com/books/about/Algorithms.html?id=idUdqdDXqnAC
// The principle is to choose a random pivot, and move everything less than
// the pivot to its left, and everything greater than the pivot to the end
// of the array, then recurse on the part that contains the desired index, or
// just return the answer if it is in the equal section in the middle.
// The random pivot guarantees average linear time for the same reason that
// n times vector::push_back takes linear time on average.
// target_index, start and and end are all indices into the full array.
// Seed is a seed for rand_r for thread safety purposes. Its value is
// unimportant as the random numbers do not affect the result except
// between equal answers.
template <typename T>
int GenericVector<T>::choose_nth_item(int target_index, int start, int end,
                                      unsigned int* seed) {
  // Number of elements to process.
  int num_elements = end - start;
  // Trivial cases.
  if (num_elements <= 1) {
    return start;
  }
  if (num_elements == 2) {
    if (data()[start] < data()[start + 1]) {
      return target_index > start ? start + 1 : start;
    }
    return target_index > start ? start : start + 1;
  }
// Place the pivot at start.
#ifndef rand_r  // _MSC_VER, ANDROID
  srand(*seed);
#  define rand_r(seed) rand()
#endif  // _MSC_VER
  int pivot = rand_r(seed) % num_elements + start;
  swap(pivot, start);
  // The invariant condition here is that items [start, next_lesser) are less
  // than the pivot (which is at index next_lesser) and items
  // [prev_greater, end) are greater than the pivot, with items
  // [next_lesser, prev_greater) being equal to the pivot.
  int next_lesser = start;
  int prev_greater = end;
  for (int next_sample = start + 1; next_sample < prev_greater;) {
    if (data()[next_sample] < data()[next_lesser]) {
      swap(next_lesser++, next_sample++);
    } else if (data()[next_sample] == data()[next_lesser]) {
      ++next_sample;
    } else {
      swap(--prev_greater, next_sample);
    }
  }
  // Now the invariant is set up, we recurse on just the section that contains
  // the desired index.
  if (target_index < next_lesser) {
    return choose_nth_item(target_index, start, next_lesser, seed);
  }
  if (target_index < prev_greater) {
    return next_lesser;  // In equal bracket.
  }
  return choose_nth_item(target_index, prev_greater, end, seed);
}

}  // namespace tesseract

#endif  // TESSERACT_CCUTIL_GENERICVECTOR_H_
