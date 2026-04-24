#include "element.h"
#include "fault-injection.h"
#include "ordered-element.h"
#include "vector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp>

#include <string>
#include <utility>

namespace ct {

template class Vector<int>;
template class Vector<::ct_test::Element>;
template class Vector<std::string>;
template class Vector<::ct_test::OrderedElement>;

} // namespace ct

namespace ct_test {

using ct::Vector;

template <typename C>
class StrongExceptionSafetyGuard {
public:
  explicit StrongExceptionSafetyGuard(const C& c) noexcept
      : ref(c)
      , expected((FaultInjectionDisable{}, c)) {}

  StrongExceptionSafetyGuard(const StrongExceptionSafetyGuard&) = delete;

  ~StrongExceptionSafetyGuard() {
    if (std::uncaught_exceptions() > 0) {
      FaultInjectionDisable dg{};
      CHECK_THAT(expected, Catch::Matchers::RangeEquals(ref));
    }
  }

private:
  const C& ref;
  C expected;
};

template <>
class StrongExceptionSafetyGuard<Element> {
public:
  explicit StrongExceptionSafetyGuard(const Element& c) noexcept
      : ref(c)
      , expected((FaultInjectionDisable{}, c)) {}

  StrongExceptionSafetyGuard(const StrongExceptionSafetyGuard&) = delete;

  ~StrongExceptionSafetyGuard() {
    if (std::uncaught_exceptions() > 0) {
      FaultInjectionDisable dg;
      CHECK(expected == ref);
    }
  }

private:
  const Element& ref;
  Element expected;
};

class BaseTest {
protected:
  BaseTest() {
    OrderedElement::insertion_order().clear();
  }

  void expect_empty_storage(const Vector<Element>& a) {
    instances_guard.expect_no_instances();
    CHECK(a.empty());
    CHECK(a.size() == 0);
    CHECK(a.capacity() == 0);
    CHECK(a.data() == nullptr);
  }

  NoNewInstancesGuard instances_guard;
};

class CorrectnessTest : public BaseTest {};

class ExceptionSafetyTest : public BaseTest {};

class PerformanceTest : public BaseTest {};

TEST_CASE_METHOD(CorrectnessTest, "default ctor") {
  Vector<Element> a;
  expect_empty_storage(a);
}

TEST_CASE_METHOD(ExceptionSafetyTest, "non-throwing default ctor") {
  assert_nothrow([] {
    [[maybe_unused]] Vector<Element> a;
  });
}

TEST_CASE_METHOD(CorrectnessTest, "`push_back`") {
  static constexpr int N = 5000;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  CHECK(a.size() == N);
  CHECK(a.capacity() >= N);

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing lvalue `push_back`") {
  static constexpr int N = 10;

  faulty_run([] {
    Vector<Element> a;
    for (int i = 0; i < N; ++i) {
      CAPTURE(i);
      Element x = 2 * i + 1;
      StrongExceptionSafetyGuard sg_1(a);
      StrongExceptionSafetyGuard sg_2(x);
      a.push_back(x);
    }
  });
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing xvalue `push_back`") {
  static constexpr int N = 10;
  faulty_run([] {
    Vector<Element> a;
    for (int i = 0; i < N; ++i) {
      CAPTURE(i);
      Element x = 2 * i + 1;
      a.push_back(std::move(x));
    }
  });
}

TEST_CASE_METHOD(CorrectnessTest, "`push_back` from self") {
  static constexpr int N = 500;

  Vector<Element> a;
  a.push_back(42);
  for (int i = 1; i < N; ++i) {
    a.push_back(a[0]);
  }

  CHECK(a.size() == N);
  CHECK(a.capacity() >= N);

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 42);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "`push_back` xvalue from self with no reallocation") {
  static constexpr int N = 500;

  Vector<Element> a;
  a.reserve(N);
  a.push_back(42);
  for (int i = 1; i < N; ++i) {
    a.push_back(std::move(a[i - 1]));
  }

  CHECK(N == a.size());
  CHECK(N <= a.capacity());
  for (int i = 0; i < N - 1; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == -1);
  }
  REQUIRE(a[N - 1] == 42);
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing `push_back` lvalue from self") {
  static constexpr int N = 10;

  faulty_run([] {
    Vector<Element> a;
    a.push_back(42);
    for (int i = 1; i < N; ++i) {
      CAPTURE(i);
      StrongExceptionSafetyGuard sg(a);
      a.push_back(a[0]);
    }
  });
}

TEST_CASE_METHOD(CorrectnessTest, "`push_back` with reallocation") {
  static constexpr int N = 500;

  Vector<Element> a;
  a.reserve(N);
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  Element x = N;
  Element::reset_counters();
  a.push_back(x);
  REQUIRE(Element::get_copy_counter() == N + 1);
}

TEST_CASE_METHOD(CorrectnessTest, "`push_back` with reallocation and noexcept move") {
  static constexpr int N = 500;

  Vector<ElementWithNonThrowingMove> a;
  a.reserve(N);
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  ElementWithNonThrowingMove x = N;
  Element::reset_counters();
  a.push_back(std::move(x));
  REQUIRE(Element::get_copy_counter() == 0);
  REQUIRE(Element::get_move_counter() <= 501);
}

TEST_CASE_METHOD(CorrectnessTest, "`push_back` xvalue from self with reallocation") {
  static constexpr int N = 500;

  Vector<Element> a;
  a.push_back(42);
  for (int i = 1; i < N; ++i) {
    a.push_back(std::move(a[i - 1]));
  }

  CHECK(a.size() == N);
  CHECK(a.capacity() >= N);
  for (int i = 0; i < N - 1; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == -1);
  }
  REQUIRE(a[N - 1] == 42);
}

TEST_CASE_METHOD(CorrectnessTest, "`push_back` xvalue from self with reallocation and noexcept move") {
  static constexpr int N = 500;

  Vector<ElementWithNonThrowingMove> a;
  a.push_back(42);
  for (int i = 1; i < N; ++i) {
    a.push_back(std::move(a[i - 1]));
  }

  CHECK(a.size() == N);
  CHECK(a.capacity() >= N);
  for (int i = 0; i < N - 1; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == -1);
  }
  REQUIRE(a[N - 1] == 42);
}

TEST_CASE_METHOD(CorrectnessTest, "`push_back` lvalue from self with reallocation and noexcept move") {
  static constexpr int N = 500;

  Vector<ElementWithNonThrowingMove> a;
  a.reserve(N);
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  a.push_back(a[0]);
  REQUIRE(a.back() == a[0]);
}

TEST_CASE_METHOD(CorrectnessTest, "subscripting") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }

  const Vector<Element>& ca = a;

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(ca[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "`data`") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  {
    Element* ptr = a.data();
    for (int i = 0; i < N; ++i) {
      CAPTURE(i);
      REQUIRE(ptr[i] == 2 * i + 1);
    }
  }

  {
    const Element* cptr = std::as_const(a).data();
    for (int i = 0; i < N; ++i) {
      CAPTURE(i);
      REQUIRE(cptr[i] == 2 * i + 1);
    }
  }
}

TEST_CASE_METHOD(CorrectnessTest, "`front` and `back`") {
  static constexpr int N = 500;
  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  CHECK(a.front() == 1);
  CHECK(std::as_const(a).front() == 1);

  CHECK(&a[0] == &a.front());
  CHECK(&a[0] == &std::as_const(a).front());

  CHECK(a.back() == 2 * N - 1);
  CHECK(std::as_const(a).back() == 2 * N - 1);

  CHECK(&a[N - 1] == &a.back());
  CHECK(&a[N - 1] == &std::as_const(a).back());
}

TEST_CASE_METHOD(CorrectnessTest, "`reserve`") {
  static constexpr int N = 500, M = 100, K = 5000;

  Vector<Element> a;
  a.reserve(N);
  CHECK(a.size() == 0);
  CHECK(a.capacity() == N);

  for (int i = 0; i < M; ++i) {
    a.push_back(2 * i + 1);
  }
  CHECK(a.size() == M);
  CHECK(a.capacity() == N);

  for (int i = 0; i < M; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }

  a.reserve(K);
  CHECK(a.size() == M);
  CHECK(a.capacity() == K);

  for (int i = 0; i < M; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "superfluous `reserve`") {
  static constexpr int N = 5000, M = 100, K = 500;

  Vector<Element> a;
  a.reserve(N);
  REQUIRE(a.size() == 0);
  REQUIRE(a.capacity() == N);

  for (int i = 0; i < M; ++i) {
    a.push_back(2 * i + 1);
  }
  REQUIRE(a.size() == M);
  REQUIRE(a.capacity() == N);

  for (int i = 0; i < M; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }

  Element* old_data = a.data();

  a.reserve(K);
  CHECK(a.size() == M);
  CHECK(a.capacity() == N);
  CHECK(a.data() == old_data);

  for (int i = 0; i < M; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "empty `reserve`") {
  Vector<Element> a;
  a.reserve(0);
  expect_empty_storage(a);
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing `reserve`") {
  static constexpr int N = 10;

  faulty_run([] {
    FaultInjectionDisable dg;
    Vector<Element> a;
    a.reserve(N);

    for (int i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();

    StrongExceptionSafetyGuard sg(a);
    a.reserve(N + 1);
  });
}

TEST_CASE_METHOD(CorrectnessTest, "`reserve` with noexcept move") {
  static constexpr int N = 500, M = 100, K = 5000;

  Vector<ElementWithNonThrowingMove> a;
  a.reserve(N);
  CHECK(a.size() == 0);
  CHECK(a.capacity() == N);

  for (int i = 0; i < M; ++i) {
    a.push_back(2 * i + 1);
  }
  CHECK(a.size() == M);
  CHECK(a.capacity() == N);

  for (int i = 0; i < M; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }

  Element::reset_counters();
  a.reserve(K);

  REQUIRE(Element::get_copy_counter() == 0);
  REQUIRE(Element::get_move_counter() <= 100);

  CHECK(a.size() == M);
  CHECK(a.capacity() == K);

  for (int i = 0; i < M; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "`shrink_to_fit`") {
  static constexpr int N = 500, M = 100;

  Vector<Element> a;
  a.reserve(N);
  CHECK(a.size() == 0);
  CHECK(a.capacity() == N);

  for (int i = 0; i < M; ++i) {
    a.push_back(2 * i + 1);
  }
  CHECK(a.size() == M);
  CHECK(a.capacity() == N);

  for (int i = 0; i < M; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }

  a.shrink_to_fit();
  CHECK(a.size() == M);
  CHECK(a.capacity() == M);

  for (int i = 0; i < M; ++i) {
    CAPTURE(i);
    REQUIRE(2 * i + 1 == a[i]);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "superfluous `shrink_to_fit`") {
  static constexpr int N = 500;

  Vector<Element> a;
  a.reserve(N);
  CHECK(a.size() == 0);
  CHECK(a.capacity() == N);

  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }
  CHECK(a.size() == N);

  size_t old_capacity = a.capacity();
  Element* old_data = a.data();

  a.shrink_to_fit();
  CHECK(a.size() == N);
  CHECK(a.capacity() == old_capacity);
  CHECK(a.data() == old_data);
}

TEST_CASE_METHOD(CorrectnessTest, "empty `shrink_to_fit`") {
  Vector<Element> a;
  a.shrink_to_fit();
  expect_empty_storage(a);
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing `shrink_to_fit`") {
  static constexpr int N = 10;

  faulty_run([] {
    FaultInjectionDisable dg;
    Vector<Element> a;
    a.reserve(N * 2);

    for (int i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();

    StrongExceptionSafetyGuard sg(a);
    a.shrink_to_fit();
  });
}

TEST_CASE_METHOD(CorrectnessTest, "`shrink_to_fit` with noexcept move") {
  static constexpr int N = 500, M = 100;

  Vector<ElementWithNonThrowingMove> a;
  a.reserve(N);
  REQUIRE(a.size() == 0);
  REQUIRE(a.capacity() == N);

  for (int i = 0; i < M; ++i) {
    a.push_back(2 * i + 1);
  }
  REQUIRE(a.size() == M);
  REQUIRE(a.capacity() == N);

  for (int i = 0; i < M; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }

  Element::reset_counters();
  a.shrink_to_fit();
  REQUIRE(Element::get_copy_counter() == 0);
  REQUIRE(Element::get_move_counter() <= 100);

  CHECK(a.size() == M);
  CHECK(a.capacity() == M);

  for (int i = 0; i < M; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "`clear`") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }
  REQUIRE(a.size() == N);

  size_t old_capacity = a.capacity();
  Element* old_data = a.data();

  a.clear();
  instances_guard.expect_no_instances();
  CHECK(a.empty());
  CHECK(a.size() == 0);
  CHECK(a.capacity() == old_capacity);
  CHECK(a.data() == old_data);
}

TEST_CASE_METHOD(ExceptionSafetyTest, "non-throwing `clear`") {
  assert_nothrow([] {
    FaultInjectionDisable dg;
    Vector<Element> a;
    for (int i = 0; i < 10; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();
    a.clear();
  });
}

TEST_CASE_METHOD(CorrectnessTest, "copy ctor") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  Vector<Element> b = a;
  CHECK(a.size() == b.size());
  CHECK(a.size() == b.capacity());
  CHECK_FALSE(a.data() == b.data());

  for (int i = 0; i < N; ++i) {
    REQUIRE(b[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "move ctor") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  Element* a_data = a.data();

  Element::reset_counters();
  Vector<Element> b = std::move(a);
  REQUIRE(Element::get_copy_counter() == 0);

  CHECK(b.size() == N);
  CHECK(b.capacity() >= N);
  CHECK(b.data() == a_data);
  CHECK_FALSE(a.data() == b.data());

  CHECK(a.data() == nullptr);
  CHECK(a.size() == 0);
  CHECK(a.capacity() == 0);

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(2 * i + 1 == b[i]);
  }
}

TEST_CASE_METHOD(PerformanceTest, "move ctor performance") {
  static constexpr int N = 8'000;

  Vector<Vector<int>> a;
  for (int i = 0; i < N; ++i) {
    Vector<int> b;
    for (int j = 0; j < N; ++j) {
      b.push_back(2 * i + 3 * j);
    }
    a.push_back(std::move(b));
  }

  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      REQUIRE(2 * i + 3 * j == a[i][j]);
    }
  }
}

TEST_CASE_METHOD(CorrectnessTest, "copy assignment operator") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  Vector<Element> b;
  b = a;
  CHECK(a.size() == b.size());
  CHECK(a.size() == b.capacity());
  CHECK(a.data() != b.data());

  Vector<Element> c;
  c.push_back(42);
  c = a;
  CHECK(a.size() == c.size());
  CHECK(a.size() == c.capacity());
  CHECK_FALSE(a.data() == c.data());

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
    REQUIRE(b[i] == 2 * i + 1);
    REQUIRE(c[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "move assignment operator to empty") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  Element* a_data = a.data();

  Element::reset_counters();
  Vector<Element> b;
  b = std::move(a);
  REQUIRE(Element::get_copy_counter() == 0);

  CHECK(b.size() == N);
  CHECK(b.capacity() >= N);
  CHECK(b.data() == a_data);
  CHECK_FALSE(a.data() == b.data());

  CHECK(a.data() == nullptr);
  CHECK(a.size() == 0);
  CHECK(a.capacity() == 0);

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(b[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "move assignment operator to non-empty") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  Element* a_data = a.data();

  Vector<Element> b;
  b.push_back(42);

  Element::reset_counters();
  b = std::move(a);
  REQUIRE(Element::get_copy_counter() == 0);

  CHECK(b.size() == N);
  CHECK(b.capacity() >= N);
  CHECK(b.data() == a_data);
  CHECK_FALSE(a.data() == b.data());

  CHECK(a.data() == nullptr);
  CHECK(a.size() == 0);
  CHECK(a.capacity() == 0);

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(b[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "self copy assignment") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  Element* old_data = a.data();

  Element::reset_counters();
  a = a;
  REQUIRE(Element::get_copy_counter() == 0);
  REQUIRE(Element::get_move_counter() == 0);

  CHECK(a.size() == N);
  CHECK(a.capacity() == old_capacity);
  CHECK(a.data() == old_data);

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "self move assignment") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  Element* old_data = a.data();

  Element::reset_counters();
  a = std::move(a);
  REQUIRE(Element::get_copy_counter() == 0);
  REQUIRE(Element::get_move_counter() == 0);

  CHECK(a.size() == N);
  CHECK(a.capacity() == old_capacity);
  CHECK(a.data() == old_data);

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(PerformanceTest, "move assignment performance") {
  static constexpr int N = 8'000;

  Vector<Vector<int>> a;
  for (int i = 0; i < N; ++i) {
    Vector<int> b;
    for (int j = 0; j < N; ++j) {
      b.push_back(2 * i + 3 * j);
    }
    a.push_back({});
    a.back() = std::move(b);
  }

  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      REQUIRE(a[i][j] == 2 * i + 3 * j);
    }
  }
}

TEST_CASE_METHOD(CorrectnessTest, "empty storage") {
  Vector<Element> a;
  expect_empty_storage(a);

  Vector<Element> b = a;
  expect_empty_storage(b);

  a = b;
  expect_empty_storage(a);
}

TEST_CASE_METHOD(CorrectnessTest, "`pop_back`") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  Element* old_data = a.data();

  for (int i = N; i > 0; --i) {
    CAPTURE(i);
    REQUIRE(a.back() == 2 * i - 1);
    REQUIRE(a.size() == static_cast<size_t>(i));
    a.pop_back();
  }
  instances_guard.expect_no_instances();
  CHECK(a.empty());
  CHECK(a.size() == 0);
  CHECK(a.capacity() == old_capacity);
  CHECK(a.data() == old_data);
}

TEST_CASE_METHOD(CorrectnessTest, "destroy order") {
  Vector<OrderedElement> a;

  a.push_back(1);
  a.push_back(2);
  a.push_back(3);
}

TEST_CASE_METHOD(CorrectnessTest, "`insert` to begin") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    Element x = 2 * i + 1;
    auto it = a.insert(std::as_const(a).begin(), x);
    REQUIRE(it == a.begin());
    REQUIRE(a.size() == i + 1uz);
  }

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(a.back() == 2 * i + 1);
    a.pop_back();
  }
  REQUIRE(a.empty());
}

TEST_CASE_METHOD(CorrectnessTest, "`insert` to end") {
  static constexpr int N = 500;

  Vector<Element> a;

  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }
  REQUIRE(a.size() == N);

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    Element x = 4 * i + 1;
    auto it = a.insert(a.end(), x);
    REQUIRE(it == a.end() - 1);
    REQUIRE(a.size() == N + i + 1uz);
  }

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }
  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(a[N + i] == 4 * i + 1);
  }
}

TEST_CASE_METHOD(PerformanceTest, "`insert` performance") {
  static constexpr int N = 8'000;

  Vector<Vector<int>> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(Vector<int>());
    for (int j = 0; j < N; ++j) {
      a.back().push_back(2 * (i + 1) + 3 * j);
    }
  }

  Vector<int> temp;
  for (int i = 0; i < N; ++i) {
    temp.push_back(3 * i);
  }
  auto it = a.insert(a.begin(), temp);
  CHECK(it == a.begin());

  for (int i = 0; i <= N; ++i) {
    for (int j = 0; j < N; ++j) {
      REQUIRE(a[i][j] == 2 * i + 3 * j);
    }
  }
}

TEST_CASE_METHOD(CorrectnessTest, "`insert` xvalue with reallocation") {
  static constexpr int N = 500, K = 7;

  Vector<Element> a;
  a.reserve(N);
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  Element x = N;
  Element::reset_counters();
  a.insert(a.begin() + K, std::move(x));

  REQUIRE(Element::get_copy_counter() <= 500);
}

TEST_CASE_METHOD(CorrectnessTest, "`insert` xvalue with reallocation and noexcept move") {
  static constexpr int N = 500, K = 0;

  Vector<ElementWithNonThrowingMove> a;
  a.reserve(N);
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  ElementWithNonThrowingMove x = N;
  Element::reset_counters();
  a.insert(a.begin() + K, std::move(x));

  REQUIRE(Element::get_copy_counter() == 0);
}

TEST_CASE_METHOD(CorrectnessTest, "`erase`") {
  static constexpr int N = 500;

  for (int i = 0; i < N; ++i) {
    Vector<Element> a;
    for (int j = 0; j < N; ++j) {
      a.push_back(2 * j + 1);
    }

    size_t old_capacity = a.capacity();
    Element* old_data = a.data();

    auto it = a.erase(std::as_const(a).begin() + i);
    REQUIRE(it == a.begin() + i);
    REQUIRE(a.size() == N - 1);
    REQUIRE(a.capacity() == old_capacity);
    REQUIRE(a.data() == old_data);

    for (int j = 0; j < i; ++j) {
      CAPTURE(j);
      REQUIRE(a[j] == 2 * j + 1);
    }
    for (int j = i; j < N - 1; ++j) {
      CAPTURE(j);
      REQUIRE(a[j] == 2 * (j + 1) + 1);
    }
  }
}

TEST_CASE_METHOD(CorrectnessTest, "`erase` from begin") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N * 2; ++i) {
    a.push_back(2 * i + 1);
  }

  for (int i = 0; i < N; ++i) {
    auto it = a.erase(a.begin());
    REQUIRE(it == a.begin());
  }

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * (i + N) + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "`erase` from end") {
  static constexpr int N = 500;

  Vector<Element> a;
  for (int i = 0; i < N * 2; ++i) {
    a.push_back(2 * i + 1);
  }

  for (int i = 0; i < N; ++i) {
    auto it = a.erase(a.end() - 1);
    CAPTURE(i);
    REQUIRE(it == a.end());
  }

  for (int i = 0; i < N; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "`erase` range from begin") {
  static constexpr int N = 500, K = 100;

  Vector<Element> a;
  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  Element* old_data = a.data();

  auto it = a.erase(std::as_const(a).begin(), std::as_const(a).begin() + K);
  CHECK(it == a.begin());
  CHECK(a.size() == N - K);
  CHECK(a.capacity() == old_capacity);
  CHECK(a.data() == old_data);

  for (int i = 0; i < N - K; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * (i + K) + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "`erase` range from middle") {
  static constexpr int N = 500, K = 100;

  Vector<Element> a;

  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  Element* old_data = a.data();

  auto it = a.erase(a.begin() + K, a.end() - K);
  CHECK(it == a.begin() + K);
  CHECK(a.size() == K * 2);
  CHECK(a.capacity() == old_capacity);
  CHECK(a.data() == old_data);

  for (int i = 0; i < K; ++i) {
    CAPTURE(i);
    REQUIRE(a[i] == 2 * i + 1);
  }
  for (int i = 0; i < K; ++i) {
    CAPTURE(i + K);
    REQUIRE(a[i + K] == 2 * (i + N - K) + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "`erase` range from end") {
  static constexpr int N = 500, K = 100;

  Vector<Element> a;

  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  Element* old_data = a.data();

  auto it = a.erase(a.end() - K, a.end());
  CHECK(it == a.end());
  CHECK(a.size() == N - K);
  CHECK(a.capacity() == old_capacity);
  CHECK(a.data() == old_data);

  for (int i = 0; i < N - K; ++i) {
    REQUIRE(a[i] == 2 * i + 1);
  }
}

TEST_CASE_METHOD(CorrectnessTest, "`erase` range all") {
  static constexpr int N = 500;

  Vector<Element> a;

  for (int i = 0; i < N; ++i) {
    a.push_back(2 * i + 1);
  }

  size_t old_capacity = a.capacity();
  Element* old_data = a.data();

  auto it = a.erase(a.begin(), a.end());
  CHECK(a.end() == it);

  instances_guard.expect_no_instances();
  CHECK(a.empty());
  CHECK(a.size() == 0);
  CHECK(a.capacity() == old_capacity);
  CHECK(a.data() == old_data);
}

TEST_CASE_METHOD(PerformanceTest, "`erase` performance") {
  static constexpr int N = 8'000, M = 50'000, K = 100;

  Vector<int> a;
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < M; ++j) {
      a.push_back(j);
    }
    auto it = a.erase(a.begin() + K, a.end() - K);
    REQUIRE(it == a.begin() + K);
    REQUIRE(a.size() == K * 2);
    a.clear();
  }
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing `push_back` with reallocation") {
  static constexpr int N = 10;

  faulty_run([] {
    FaultInjectionDisable dg;
    Vector<Element> a;
    a.reserve(N);
    REQUIRE(N == a.capacity());

    for (int i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    Element x = 42;

    dg.reset();

    StrongExceptionSafetyGuard sg(a);
    a.push_back(x);
  });
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing `push_back` from self") {
  static constexpr int N = 10;

  faulty_run([] {
    FaultInjectionDisable dg;
    Vector<Element> a;
    a.reserve(N);
    REQUIRE(N == a.capacity());

    for (int i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();

    a.push_back(std::move(a[0]));
  });
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing `push_back` xvalue from self with reallocation and noexcept move") {
  static constexpr int N = 10;

  faulty_run([] {
    FaultInjectionDisable dg;
    Vector<ElementWithNonThrowingMove> a;
    a.reserve(N);
    REQUIRE(N == a.capacity());

    for (int i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();

    StrongExceptionSafetyGuard sg(a);
    a.push_back(std::move(a[0]));
  });
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing `push_back` lvalue from self with reallocation and noexcept move") {
  static constexpr int N = 500;
  faulty_run([] {
    FaultInjectionDisable dg;
    Vector<ElementWithNonThrowingMove> a;
    a.reserve(N);
    for (int i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();

    StrongExceptionSafetyGuard sg(a);
    a.push_back(a[0]);
  });
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing `insert` with reallocation") {
  static constexpr int N = 500;

  faulty_run([] {
    FaultInjectionDisable dg;
    Vector<Element> a;
    a.reserve(N);
    REQUIRE(N == a.capacity());

    for (int i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    Element x = 42;
    dg.reset();

    [[maybe_unused]] auto it = a.insert(std::as_const(a).begin(), std::move(x));
  });
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing `insert` with reallocation and noexcept move") {
  static constexpr int N = 500;

  faulty_run([] {
    FaultInjectionDisable dg;
    Vector<ElementWithNonThrowingMove> a;
    a.reserve(N);
    REQUIRE(N == a.capacity());

    for (int i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }

    dg.reset();

    StrongExceptionSafetyGuard sg(a);
    ElementWithNonThrowingMove x = 42;
    [[maybe_unused]] auto it = a.insert(std::as_const(a).begin(), std::move(x));
  });
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing copy ctor") {
  static constexpr int N = 10;

  faulty_run([] {
    FaultInjectionDisable dg;
    Vector<Element> a;
    a.reserve(N);
    REQUIRE(N == a.capacity());

    for (int i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();

    StrongExceptionSafetyGuard sg(a);
    [[maybe_unused]] Vector<Element> b{a};
  });
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing move ctor") {
  static constexpr int N = 10;

  faulty_run([] {
    FaultInjectionDisable dg;
    Vector<Element> a;
    a.reserve(N);
    REQUIRE(N == a.capacity());

    for (int i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }
    dg.reset();

    StrongExceptionSafetyGuard sg(a);
    [[maybe_unused]] Vector<Element> b{std::move(a)};
  });
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing copy assignment") {
  static constexpr int N = 10;

  faulty_run([] {
    FaultInjectionDisable dg;
    Vector<Element> a;
    a.reserve(N);

    for (int i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }

    Vector<Element> b;
    b.push_back(0);
    dg.reset();

    StrongExceptionSafetyGuard sg(a);
    b = std::as_const(a);
  });
}

TEST_CASE_METHOD(ExceptionSafetyTest, "throwing move assignment") {
  static constexpr int N = 10;

  faulty_run([] {
    FaultInjectionDisable dg;
    Vector<Element> a;
    a.reserve(N);

    for (int i = 0; i < N; ++i) {
      a.push_back(2 * i + 1);
    }

    Vector<Element> b;
    b.push_back(0);
    dg.reset();

    StrongExceptionSafetyGuard sg(a);
    b = std::move(a);
  });
}

TEST_CASE_METHOD(CorrectnessTest, "member aliases") {
  STATIC_CHECK(std::is_same<Element, Vector<Element>::ValueType>::value);
  STATIC_CHECK(std::is_same<Element&, Vector<Element>::Reference>::value);
  STATIC_CHECK(std::is_same<const Element&, Vector<Element>::ConstReference>::value);
  STATIC_CHECK(std::is_same<Element*, Vector<Element>::Pointer>::value);
  STATIC_CHECK(std::is_same<const Element*, Vector<Element>::ConstPointer>::value);
  STATIC_CHECK(std::is_same<Element*, Vector<Element>::Iterator>::value);
  STATIC_CHECK(std::is_same<const Element*, Vector<Element>::ConstIterator>::value);
}

TEST_CASE_METHOD(CorrectnessTest, "overaligned type") {
  struct alignas(alignof(std::max_align_t) * 8) Overaligned {
    int x;
  };

  Vector<Overaligned> v;
  v.push_back(Overaligned{42});
  v.push_back(Overaligned{1337});
  v.push_back(Overaligned{228});

  REQUIRE(v.size() == 3);
  CHECK(v[0].x == 42);
  CHECK(v[1].x == 1337);
  CHECK(v[2].x == 228);
}

} // namespace ct_test
