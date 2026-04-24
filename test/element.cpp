#include "element.h"

#include "fault-injection.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>

namespace ct_test {

ElementBase::ElementBase(int data)
    : data(data) {
  fault_injection_point();
  add_instance();
}

ElementBase::ElementBase(const ElementBase& other)
    : data(other.data) {
  fault_injection_point();
  add_instance();
  ++copy_counter;
}

ElementBase::ElementBase(ElementBase&& other) noexcept
    : data(std::exchange(other.data, -1)) {
  add_instance();
  ++move_counter;
}

ElementBase::~ElementBase() {
  delete_instance();
}

ElementBase& ElementBase::operator=(const ElementBase& c) {
  assert_exists();
  fault_injection_point();

  ++copy_counter;
  data = c.data;
  return *this;
}

ElementBase& ElementBase::operator=(ElementBase&& c) noexcept {
  assert_exists();
  ++move_counter;
  data = std::exchange(c.data, -1);
  return *this;
}

ElementBase::operator int() const {
  assert_exists();
  fault_injection_point();

  return data;
}

void ElementBase::reset_counters() {
  copy_counter = 0;
  move_counter = 0;
}

size_t ElementBase::get_copy_counter() {
  return copy_counter;
}

size_t ElementBase::get_move_counter() {
  return move_counter;
}

void ElementBase::add_instance() noexcept {
  FaultInjectionDisable dg;
  auto p = instances.insert(this);
  if (!p.second) {
    FAIL_CHECK(
        "A new object is created at the address " << static_cast<void*>(this)
                                                  << " while the previous object at this address was not destroyed"
    );
  }
}

void ElementBase::delete_instance() noexcept {
  FaultInjectionDisable dg;
  size_t erased = instances.erase(this);
  if (erased != 1) {
    FAIL_CHECK("Attempt of destroying non-existing object at address " << static_cast<void*>(this));
  }
}

void ElementBase::assert_exists() const noexcept {
  FaultInjectionDisable dg;
  bool exists = instances.find(this) != instances.end();
  if (!exists) {
    FAIL_CHECK("Accessing a non-existing object at address " << static_cast<const void*>(this));
  }
}

std::set<const ElementBase*> ElementBase::instances;

NoNewInstancesGuard::NoNewInstancesGuard()
    : old_instances(ElementBase::instances) {}

NoNewInstancesGuard::~NoNewInstancesGuard() {
  CHECK(old_instances == ElementBase::instances);
}

void NoNewInstancesGuard::expect_no_instances() {
  CHECK(old_instances == ElementBase::instances);
}

Element::Element(Element&& other)
    : ElementBase(std::move(other)) {
  fault_injection_point();
}

Element& Element::operator=(Element&& c) {
  fault_injection_point();
  ElementBase::operator=(std::move(c));
  return *this;
}

void swap(Element& lhs, Element& rhs) {
  fault_injection_point();
  std::swap(lhs.data, rhs.data);
}

void swap(ElementWithNonThrowingMove& lhs, ElementWithNonThrowingMove& rhs) noexcept {
  std::swap(lhs.data, rhs.data);
}

} // namespace ct_test
