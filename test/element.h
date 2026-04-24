#pragma once

#include <cstddef>
#include <set>

namespace ct_test {

class ElementBase;

class NoNewInstancesGuard {
public:
  NoNewInstancesGuard();

  NoNewInstancesGuard(const NoNewInstancesGuard&) = delete;
  NoNewInstancesGuard& operator=(const NoNewInstancesGuard&) = delete;

  ~NoNewInstancesGuard();

  void expect_no_instances();

private:
  std::set<const ElementBase*> old_instances;
};

class ElementBase {
public:
  ElementBase(int data);
  ElementBase(const ElementBase& other);
  ElementBase(ElementBase&& other) noexcept;
  ~ElementBase();

  ElementBase& operator=(const ElementBase& c);
  ElementBase& operator=(ElementBase&& c) noexcept;
  operator int() const;

  static void reset_counters();
  static size_t get_copy_counter();
  static size_t get_move_counter();

protected:
  void add_instance() noexcept;
  void delete_instance() noexcept;
  void assert_exists() const noexcept;

protected:
  int data;

  static std::set<const ElementBase*> instances;
  inline static size_t copy_counter = 0;
  inline static size_t move_counter = 0;

  friend class NoNewInstancesGuard;
};

class Element : public ElementBase {
public:
  using ElementBase::ElementBase;

  Element(const Element& other) = default;
  Element(Element&& other);

  Element& operator=(const Element& c) = default;
  Element& operator=(Element&& c);

  friend void swap(Element&, Element&);
};

class ElementWithNonThrowingMove : public ElementBase {
public:
  using ElementBase::ElementBase;

  friend void swap(ElementWithNonThrowingMove&, ElementWithNonThrowingMove&) noexcept;
};

} // namespace ct_test
