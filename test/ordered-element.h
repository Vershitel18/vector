#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

namespace ct_test {

class OrderedElement {
public:
  OrderedElement(size_t val)
      : val(val) {
    insertion_order().push_back(val);
  }

  OrderedElement(const OrderedElement& other)
      : val(other.val) {
    other.val = 0;
  }

  ~OrderedElement() {
    delete_instance();
  }

  static std::vector<size_t>& insertion_order() {
    static std::vector<size_t> insertion_order;
    return insertion_order;
  }

private:
  mutable size_t val;

  void delete_instance() const noexcept {
    if (val == 0) {
      return;
    }

    FaultInjectionDisable dg;
    size_t back = insertion_order().back();
    INFO("Elements must be destroyed in reverse order of insertion");
    CHECK(val == back);
    insertion_order().pop_back();
  }
};

} // namespace ct_test
