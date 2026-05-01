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
      : data_(static_cast<T*>(operator new(sizeof(T) * other.size(), std::align_val_t(alignof(T)))))
      , size_(other.size())
      , capacity_(other.size()) {
    if (other.size() == 0) {
      data_ = nullptr;
      return;
    }

    for (std::size_t i = 0; i < size(); ++i) {
      try {
        new (data_ + i) T(other[i]);
      } catch (...) {
        for (std::size_t last = i; last > 0; last--) {
          (data_ + last - 1)->~T();
        }
        throw;
      }
    }
  }

  // O(1) nothrow
  Vector(Vector&& other)
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
  Vector& operator=(Vector&& other) {
    if (this != &other) {
      Vector{}.swap(*this);
      swap(other);
    }
    return *this;
  }

  // O(N) nothrow
  ~Vector() noexcept {
    for (std::size_t i = size(); i > 0; --i) {
      (data_ + i - 1)->~T();
    }
    operator delete(data_, std::align_val_t(alignof(T)));
  }

  // O(1) nothrow
  Reference operator[](size_t index) {
    return *(data_ + index); // надо подумать что делает этот оператор если index некорректен
  }

  // O(1) nothrow
  ConstReference operator[](size_t index) const {
    return *(data_ + index);
  }

  // O(1) nothrow
  Pointer data() noexcept {
    return data_;
  }

  // O(1) nothrow
  ConstPointer data() const noexcept {
    return data_;
  }

  // O(1) nothrow
  size_t size() const noexcept {
    return size_;
  }

  // O(1) nothrow
  Reference front() {
    return data_[0];
  }

  // O(1) nothrow
  ConstReference front() const {
    return data_[0];
  }

  // O(1) nothrow
  Reference back() {
    return data_[size_ - 1];
  }

  // O(1) nothrow
  ConstReference back() const {
    return data_[size_ - 1];
  }

  // O(1)* strong
  void push_back(const T& value) {
    if (size() + 1 <= capacity()) {
      new (data_ + size()) T(value);
      size_ += 1;
      return;
    }
    T* newArr = static_cast<T*>(operator new(sizeof(T) * ((capacity_ * 2) + 1), std::align_val_t(alignof(T))));
    try {
      new (newArr + size()) T(value);
      std::size_t index = 0;
      try {
        for (; index < size_; ++index) {
          new (newArr + index) T(std::move_if_noexcept(data_[index]));
        }
      } catch (...) {
        for (; index > 0; --index) {
          (newArr + index - 1)->~T();
        }
        (newArr + size())->~T();
        throw;
      }
    } catch (...) {
      operator delete(newArr, std::align_val_t(alignof(T)));
      throw;
    }
    for (std::size_t i = size(); i > 0; --i) {
      (data_ + i - 1)->~T();
    }
    operator delete(data_, std::align_val_t(alignof(T)));
    data_ = newArr;
    size_ += 1;
    capacity_ = capacity_ * 2 + 1;
  }

  // O(1)* strong т.к от push_back(const T&) отличается только тем,
  // что последний элемент мы делаем move,
  // и если он бросит исключение мы его поймаем в try,
  // и в итоге оригинальный вектор останется в валидном состоянии
  void push_back(T&& value) {
    if (size() + 1 <= capacity_) {
      new (data_ + size()) T(std::move(value));
      size_ += 1;
      return;
    }
    T* newArr = static_cast<T*>(operator new(sizeof(T) * ((capacity_ * 2) + 1), std::align_val_t(alignof(T))));
    try {
      new (newArr + size()) T(std::move(value));
      std::size_t index = 0;
      try {
        for (; index < size_; ++index) {
          new (newArr + index) T(std::move_if_noexcept(data_[index]));
        }
      } catch (...) {
        for (; index > 0; --index) {
          (newArr + index - 1)->~T();
        }
        (newArr + size())->~T();
        throw;
      }
    } catch (...) {
      operator delete(newArr, std::align_val_t(alignof(T)));
      throw;
    }
    for (std::size_t i = size(); i > 0; --i) {
      (data_ + i - 1)->~T();
    }
    operator delete(data_, std::align_val_t(alignof(T)));
    data_ = newArr;
    size_ += 1;
    capacity_ = capacity_ * 2 + 1;
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
    T* newArr = static_cast<T*>(operator new(sizeof(T) * (new_capacity), std::align_val_t(alignof(T))));
    std::size_t index = 0;
    try {
      for (; index < size_; ++index) {
        new (newArr + index) T(
            std::move_if_noexcept(data_[index])
        ); // если для T move сонструктор не бросает исключений выгоднее будет мувать а не копировать
      }
    } catch (...) {
      for (; index > 0; --index) {
        (newArr + index - 1)->~T();
      }
      operator delete(newArr, std::align_val_t(alignof(T)));
      throw;
    }
    for (std::size_t i = size(); i > 0; --i) {
      (data_ + i - 1)->~T();
    }
    operator delete(data_, std::align_val_t(alignof(T)));
    data_ = newArr;
    capacity_ = new_capacity;
  }

  // O(N) strong
  void shrink_to_fit() {
    if (capacity_ > size_) {
      T* newArr = static_cast<T*>(operator new(sizeof(T) * (size_), std::align_val_t(alignof(T))));
      std::size_t index = 0;
      try {
        for (; index < size_; ++index) {
          new (newArr + index) T(
              std::move_if_noexcept(data_[index])
          ); // если для T move сонструктор не бросает исключений выгоднее будет мувать а не копировать
        }
      } catch (...) {
        for (; index > 0; --index) {
          (newArr + index - 1)->~T();
        }
        operator delete(newArr, std::align_val_t(alignof(T)));
        throw;
      }
      for (std::size_t i = size(); i > 0; --i) {
        (data_ + i - 1)->~T();
      }
      operator delete(data_, std::align_val_t(alignof(T)));
      data_ = newArr;
      capacity_ = size_;
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

  // O(N) basic garanty так как если мы не знаем swap noexcept для T
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

  // O(N) basic garanty, if swap for T noexcept
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
  Iterator erase(ConstIterator pos) {
    std::size_t offset = pos - begin();
    Iterator mutable_iterator = const_cast<Iterator>(pos);
    for (auto it = mutable_iterator; it != end() - 1; ++it) {
      std::swap(*it, *(it + 1));
    }
    pop_back();
    return begin() + offset;
  }

  // O(N) basic garanty, потому что swap для T может быть не noexcept
  Iterator erase(ConstIterator first, ConstIterator last) {
    std::size_t elements = end() - last;
    std::size_t offset = first - begin();
    std::size_t length = last - first;
    Iterator mutable_first = const_cast<Iterator>(first);
    Iterator mutable_last = const_cast<Iterator>(last);
    for (std::size_t index = 0; index < elements; ++index) {
      std::swap(*(mutable_first + index), *(mutable_last + index));
    }
    for (std::size_t index = 0; index < length; ++index) {
      pop_back();
    }
    return begin() + offset;
  }

private:
  T* data_;
  size_t size_;
  size_t capacity_;
};

} // namespace ct
