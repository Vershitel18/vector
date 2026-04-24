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
  Vector() noexcept;

  // O(N) strong
  Vector(const Vector& other);

  // O(1) nothrow
  Vector(Vector&& other);

  // O(N) strong
  Vector& operator=(const Vector& other);

  // O(N) nothrow
  Vector& operator=(Vector&& other);

  // O(N) nothrow
  ~Vector() noexcept;

  // O(1) nothrow
  Reference operator[](size_t index);

  // O(1) nothrow
  ConstReference operator[](size_t index) const;

  // O(1) nothrow
  Pointer data() noexcept;

  // O(1) nothrow
  ConstPointer data() const noexcept;

  // O(1) nothrow
  size_t size() const noexcept;

  // O(1) nothrow
  Reference front();

  // O(1) nothrow
  ConstReference front() const;

  // O(1) nothrow
  Reference back();

  // O(1) nothrow
  ConstReference back() const;

  // O(1)* strong
  void push_back(const T& value);

  // O(1)* ???
  void push_back(T&& value);

  // O(1) nothrow
  void pop_back();

  // O(1) nothrow
  bool empty() const noexcept;

  // O(1) nothrow
  size_t capacity() const noexcept;

  // O(N) strong
  void reserve(size_t new_capacity);

  // O(N) strong
  void shrink_to_fit();

  // O(N) nothrow
  void clear() noexcept;

  // O(1) nothrow
  void swap(Vector& other) noexcept;

  // O(1) nothrow
  Iterator begin() noexcept;

  // O(1) nothrow
  Iterator end() noexcept;

  // O(1) nothrow
  ConstIterator begin() const noexcept;

  // O(1) nothrow
  ConstIterator end() const noexcept;

  // O(N) ???
  Iterator insert(ConstIterator pos, const T& value);

  // O(N) ???
  Iterator insert(ConstIterator pos, T&& value);

  // O(N) ???
  Iterator erase(ConstIterator pos);

  // O(N) ???
  Iterator erase(ConstIterator first, ConstIterator last);
};

} // namespace ct
