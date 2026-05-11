#pragma once

#include <cstddef>
#include <type_traits>

namespace ct {

template <typename T>
class Vector {
  static_assert(
      (std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T> &&
       std::is_nothrow_swappable_v<T>) ||
      (std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>)
  );

public:
  using ValueType = T;

  using Reference = T&;
  using ConstReference = const T&;

  using Pointer = T*;
  using ConstPointer = const T*;

  using Iterator = Pointer;
  using ConstIterator = ConstPointer;

public:
  // O(1) nothrow
  Vector() noexcept
      : data_(nullptr)
      , size_(0)
      , capacity_(0) {}

  // O(N) strong
  Vector(const Vector& other)
      : data_(nullptr)
      , size_(other.size())
      , capacity_(other.size()) {
    if (other.size() == 0) {
      return;
    }

    data_ = alocation_buffer(other.size());
    std::size_t index = 0;
    try {
      for (; index < size_; ++index) {
        new (data_ + index) T(other[index]);
      }
    } catch (...) {
      clear_buffer(data_, index);
      data_ = nullptr;
      throw;
    }
  }

  // O(1) nothrow
  Vector(Vector&& other) noexcept
      : Vector() {
    swap(other);
  }

  // O(N) strong
  Vector& operator=(const Vector& other) {
    if (this != &other) {
      Vector tmp(other);
      swap(tmp);
    }
    return *this;
  }

  // O(N) nothrow
  Vector& operator=(Vector&& other) noexcept {
    Vector(std::move(other)).swap(*this);
    return *this;
  }

  // O(N) nothrow
  ~Vector() noexcept {
    clear_buffer(data_, size());
  }

  // O(1) nothrow
  Reference operator[](size_t index) {
    return *(data_ + index);
  }

  // O(1) nothrow
  ConstReference operator[](size_t index) const {
    return *(data_ + index);
  }

  // O(1) nothrow
  Pointer data() noexcept {
    return begin();
  }

  // O(1) nothrow
  ConstPointer data() const noexcept {
    return begin();
  }

  // O(1) nothrow
  size_t size() const noexcept {
    return size_;
  }

  // O(1) nothrow
  Reference front() {
    return *begin();
  }

  // O(1) nothrow
  ConstReference front() const {
    return *begin();
  }

  // O(1) nothrow
  Reference back() {
    return *(end() - 1);
  }

  // O(1) nothrow
  ConstReference back() const {
    return *(end() - 1);
  }

  static T* alocation_buffer(std::size_t capacity) {
    return static_cast<T*>(operator new(sizeof(T) * capacity, std::align_val_t(alignof(T))));
  }

  template <typename U>
  void push_back_method(U&& value) {
    if (size() + 1 <= capacity()) {
      new (data_ + size()) T(std::forward<U>(value));
      size_ += 1;
      return;
    }
    T* newArr = alocation_buffer(new_capacity(capacity()));
    std::size_t index = 0;
    bool constructed = false;
    try {
      new (newArr + size()) T(std::forward<U>(value));
      constructed = true;
      for (; index < size_; ++index) {
        new (newArr + index) T(std::move_if_noexcept(data_[index]));
      }
    } catch (...) {
      for (; index > 0; --index) {
        (newArr + index - 1)->~T();
      }
      if (constructed) {
        (newArr + size())->~T();
      }
      operator delete(newArr, std::align_val_t(alignof(T)));
      throw;
    }
    clear_buffer(data_, size());
    data_ = newArr;
    size_ += 1;
    capacity_ = new_capacity(capacity_);
  }

  // O(1)* strong
  void push_back(const T& value) {
    push_back_method(value);
  }

  // O(1)* basic garanty т.к от push_back(const T&) отличается тем,
  // что последний элемент мы делаем move,
  // и случиться так, что это будет элемент из вектора,
  // то он станет moved from
  void push_back(T&& value) {
    push_back_method(std::move(value));
  }

  // O(1) nothrow
  void pop_back() {
    (data_ + size_ - 1)->~T();
    --size_;
  }

  // O(1) nothrow
  bool empty() const noexcept {
    return size_ == 0;
  }

  // O(1) nothrow
  size_t capacity() const noexcept {
    return capacity_;
  }

  // O(N) strong
  void reserve(size_t new_capacity) {
    if (capacity() >= new_capacity) {
      return;
    }
    Vector tmp(*this, new_capacity);
    swap(tmp);
  }

  // O(N) strong
  void shrink_to_fit() {
    if (capacity_ > size_) {
      Vector tmp(*this, size());
      swap(tmp);
    }
  }

  // O(N) nothrow
  void clear() noexcept {
    for (std::size_t i = size(); i > 0; --i) {
      (data_ + i - 1)->~T();
    }
    size_ = 0;
  }

  // O(1) nothrow
  void swap(Vector& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
    swap(size_, other.size_);
    swap(capacity_, other.capacity_);
  }

  // O(1) nothrow
  Iterator begin() noexcept {
    return Iterator(data_);
  }

  // O(1) nothrow
  Iterator end() noexcept {
    return Iterator(data_ + size_);
  }

  // O(1) nothrow
  ConstIterator begin() const noexcept {
    return ConstIterator(data_);
  }

  // O(1) nothrow
  ConstIterator end() const noexcept {
    return ConstIterator(data_ + size_);
  }

  // O(N) basic garanty, так как мы не знаем noexcept ли swap для T
  // если для T swap noexcept, то strong
  Iterator insert(ConstIterator pos, const T& value) {
    std::size_t offset = pos - begin();
    push_back(value);
    pos = begin() + offset;
    for (auto it = end() - 1; it != pos; --it) {
      std::swap(*it, *(it - 1));
    }
    return begin() + offset;
  }

  // O(N) basic garanty, if swap for T no noexcept
  // если же swap noexcept для T, то это strong garanty
  Iterator insert(ConstIterator pos, T&& value) {
    std::size_t offset = pos - begin();
    push_back(std::move(value));
    pos = begin() + offset;
    for (auto it = end() - 1; it != pos; --it) {
      std::swap(*it, *(it - 1));
    }
    return begin() + offset;
  }

  // O(N) basic garanty, потому что swap для T может быть не noexcept
  // если же swap noexcept для T, то это strong garanty
  Iterator erase(ConstIterator pos) {
    return erase(pos, pos + 1);
  }

  // O(N) basic garanty, потому что swap для T может быть не noexcept
  Iterator erase(ConstIterator first, ConstIterator last) {
    std::size_t elements = end() - last;
    std::size_t offset = first - begin();
    std::size_t length = last - first;
    Iterator mutable_first = begin() + offset;
    Iterator mutable_last = begin() + offset + length;
    for (std::size_t index = 0; index < elements; ++index) {
      std::swap(*(mutable_first + index), *(mutable_last + index));
    }
    for (std::size_t index = 0; index < length; ++index) {
      pop_back();
    }
    return begin() + offset;
  }

private:
  static std::size_t new_capacity(std::size_t capacity) noexcept {
    return capacity * 2 + 1;
  }

  static void clear_buffer(T* data, std::size_t size) noexcept {
    for (std::size_t i = size; i > 0; --i) {
      (data + i - 1)->~T();
    }
    operator delete(data, std::align_val_t(alignof(T)));
  }

  Vector(Vector& other, std::size_t capacity) {
    T* newArr = alocation_buffer(capacity);
    std::size_t index = 0;
    try {
      for (; index < other.size(); ++index) {
        new (newArr + index) T(std::move_if_noexcept(other.data()[index]));
      }
    } catch (...) {
      clear_buffer(newArr, index);
      throw;
    }
    data_ = newArr;
    if (capacity == 0) {
      data_ = nullptr;
    }
    capacity_ = capacity;
    size_ = other.size();
  }

  T* data_;
  size_t size_;
  size_t capacity_;
};

} // namespace ct
