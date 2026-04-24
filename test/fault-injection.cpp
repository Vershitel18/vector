#include "fault-injection.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {

void* injected_allocate(std::size_t count, std::size_t alignment) {
  if (ct_test::should_inject_fault()) {
    throw std::bad_alloc();
  }

  alignment = std::max(alignment, sizeof(void*));
  if (count % alignment != 0) {
    count += (alignment - count % alignment);
  }

#if defined(_MSC_VER) || defined(__MINGW32__)
  void* ptr = _aligned_malloc(count, alignment);
#else
  void* ptr = std::aligned_alloc(alignment, count);
#endif
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }

  return ptr;
}

void injected_deallocate(void* ptr) {
#if defined(_MSC_VER) || defined(__MINGW32__)
  _aligned_free(ptr);
#else
  std::free(ptr);
#endif
}

template <typename T>
struct FaultInjectionAllocator {
  using value_type = T;

  FaultInjectionAllocator() = default;

  template <typename U>
  FaultInjectionAllocator([[maybe_unused]] const FaultInjectionAllocator<U>& other) {}

  template <typename U>
  FaultInjectionAllocator& operator=([[maybe_unused]] const FaultInjectionAllocator<U>& other) {
    return *this;
  }

  T* allocate(std::size_t count) {
    return static_cast<T*>(injected_allocate(count * sizeof(T), alignof(T)));
  }

  void deallocate(void* ptr, [[maybe_unused]] std::size_t sz) {
    injected_deallocate(ptr);
  }
};

struct FaultInjectionContext {
  std::vector<std::size_t, FaultInjectionAllocator<std::size_t>> skip_ranges;
  std::size_t error_index = 0;
  std::size_t skip_index = 0;
  bool fault_registered = false;
};

thread_local bool g_disabled = false;
thread_local FaultInjectionContext* g_context = nullptr;

struct ContextGuard {
  explicit ContextGuard(FaultInjectionContext& ctx) noexcept {
    g_context = &ctx;
  }

  ~ContextGuard() {
    g_context = nullptr;
  }
};

void dump_state() {
#if 0
  ct_test::FaultInjectionDisable dg;
  std::cout << "skip_ranges: {";
  if (!g_context->skip_ranges.empty()) {
    std::cout << g_context->skip_ranges[0];
    for (std::size_t i = 1; i != g_context->skip_ranges.size(); ++i) {
      std::cout << ", " << g_context->skip_ranges[i];
    }
  }
  std::cout << "}\nerror_index: " << g_context->error_index << "\nskip_index: " << g_context->skip_index << '\n'
            << std::flush;
#endif
}

} // namespace

void* operator new(std::size_t count) {
  return injected_allocate(count, 1);
}

void* operator new(std::size_t count, std::align_val_t al) {
  return injected_allocate(count, static_cast<std::size_t>(al));
}

void* operator new[](std::size_t count) {
  return injected_allocate(count, 1);
}

void* operator new[](std::size_t count, std::align_val_t al) {
  return injected_allocate(count, static_cast<std::size_t>(al));
}

void operator delete(void* ptr) noexcept {
  injected_deallocate(ptr);
}

void operator delete(void* ptr, [[maybe_unused]] std::align_val_t al) noexcept {
  injected_deallocate(ptr);
}

void operator delete(void* ptr, [[maybe_unused]] std::size_t sz) noexcept {
  injected_deallocate(ptr);
}

void operator delete(void* ptr, [[maybe_unused]] std::size_t sz, [[maybe_unused]] std::align_val_t al) noexcept {
  injected_deallocate(ptr);
}

void operator delete[](void* ptr) noexcept {
  injected_deallocate(ptr);
}

void operator delete[](void* ptr, [[maybe_unused]] std::align_val_t al) noexcept {
  injected_deallocate(ptr);
}

void operator delete[](void* ptr, [[maybe_unused]] std::size_t sz) noexcept {
  injected_deallocate(ptr);
}

void operator delete[](void* ptr, [[maybe_unused]] std::size_t sz, [[maybe_unused]] std::align_val_t al) noexcept {
  injected_deallocate(ptr);
}

namespace ct_test {

bool should_inject_fault() {
  if (g_context == nullptr) {
    return false;
  }

  if (g_disabled) {
    return false;
  }

  assert(g_context->error_index <= g_context->skip_ranges.size());
  if (g_context->error_index == g_context->skip_ranges.size()) {
    FaultInjectionDisable dg;
    ++g_context->error_index;
    g_context->skip_ranges.push_back(0);
    g_context->fault_registered = true;
    return true;
  }

  assert(g_context->skip_index <= g_context->skip_ranges[g_context->error_index]);

  if (g_context->skip_index == g_context->skip_ranges[g_context->error_index]) {
    ++g_context->error_index;
    g_context->skip_index = 0;
    g_context->fault_registered = true;
    return true;
  }

  ++g_context->skip_index;
  return false;
}

void fault_injection_point() {
  if (should_inject_fault()) {
    FaultInjectionDisable dg;
    throw InjectedFault("injected fault");
  }
}

void faulty_run(const std::function<void()>& f) {
  assert(g_context == nullptr);
  FaultInjectionContext ctx;
  ContextGuard cg(ctx);
  for (;;) {
    try {
      f();
    } catch (...) {
      FaultInjectionDisable dg;
      dump_state();
      if (!ctx.fault_registered) {
        FAIL_CHECK("Exception thrown while none were expected");
        throw;
      }
      ctx.skip_ranges.resize(ctx.error_index);
      ++ctx.skip_ranges.back();
      ctx.error_index = 0;
      ctx.skip_index = 0;
      ctx.fault_registered = false;
      continue;
    }
    if (ctx.fault_registered) {
      FaultInjectionDisable dg;
      FAIL(
          "A fault was injected during testing, but the test suite didn't detect the error. "
          "If you see this message, check if exceptions are properly rethrown in your solution"
      );
    }
    break;
  }
}

void assert_nothrow(const std::function<void()>& f) {
  assert(g_context == nullptr);
  FaultInjectionContext ctx;
  ContextGuard cg(ctx);

  try {
    f();
  } catch (...) {
    FaultInjectionDisable dg;
    FAIL_CHECK("Exception thrown while none were expected");
    throw;
  }
}

FaultInjectionDisable::FaultInjectionDisable()
    : was_disabled(g_disabled) {
  g_disabled = true;
}

void FaultInjectionDisable::reset() const {
  g_disabled = was_disabled;
}

FaultInjectionDisable::~FaultInjectionDisable() {
  reset();
}

} // namespace ct_test
