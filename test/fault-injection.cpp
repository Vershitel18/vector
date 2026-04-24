#include "fault-injection.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <vector>

namespace {

void* injected_allocate(size_t count, size_t alignment) {
  if (ct_test::should_inject_fault()) {
    throw std::bad_alloc();
  }

  alignment = std::max(alignment, sizeof(void*));
  if (count % alignment != 0) {
    count += (alignment - (count % alignment));
  }

  void* ptr = std::aligned_alloc(alignment, count);
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }

  return ptr;
}

void injected_deallocate(void* ptr) {
  std::free(ptr);
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

  T* allocate(size_t count) {
    return static_cast<T*>(injected_allocate(count * sizeof(T), alignof(T)));
  }

  void deallocate(void* ptr, [[maybe_unused]] size_t sz) {
    injected_deallocate(ptr);
  }
};

struct FaultInjectionContext {
  std::vector<size_t, FaultInjectionAllocator<size_t>> skip_ranges;
  size_t error_index = 0;
  size_t skip_index = 0;
  bool fault_registered = false;
};

thread_local bool disabled = false;
thread_local FaultInjectionContext* context = nullptr;

void dump_state() {
#if 0
  FaultInjectionDisable dg;
  std::cout << "skip_ranges: {";
  if (!context->skip_ranges.empty()) {
    std::cout << context->skip_ranges[0];
    for (size_t i = 1; i != context->skip_ranges.size(); ++i) {
      std::cout << ", " << context->skip_ranges[i];
    }
  }
  std::cout << "}\nerror_index: " << context->error_index << "\nskip_index: " << context->skip_index << '\n'
            << std::flush;
#endif
}
} // namespace

void* operator new(size_t count) {
  return injected_allocate(count, 1);
}

void* operator new(size_t count, std::align_val_t al) {
  return injected_allocate(count, static_cast<size_t>(al));
}

void* operator new[](size_t count) {
  return injected_allocate(count, 1);
}

void* operator new[](size_t count, std::align_val_t al) {
  return injected_allocate(count, static_cast<size_t>(al));
}

void operator delete(void* ptr) noexcept {
  injected_deallocate(ptr);
}

void operator delete(void* ptr, [[maybe_unused]] std::align_val_t al) noexcept {
  injected_deallocate(ptr);
}

void operator delete(void* ptr, [[maybe_unused]] size_t sz) noexcept {
  injected_deallocate(ptr);
}

void operator delete(void* ptr, [[maybe_unused]] size_t sz, [[maybe_unused]] std::align_val_t al) noexcept {
  injected_deallocate(ptr);
}

void operator delete[](void* ptr) noexcept {
  injected_deallocate(ptr);
}

void operator delete[](void* ptr, [[maybe_unused]] std::align_val_t al) noexcept {
  injected_deallocate(ptr);
}

void operator delete[](void* ptr, [[maybe_unused]] size_t sz) noexcept {
  injected_deallocate(ptr);
}

void operator delete[](void* ptr, [[maybe_unused]] size_t sz, [[maybe_unused]] std::align_val_t al) noexcept {
  injected_deallocate(ptr);
}

namespace ct_test {

bool should_inject_fault() {
  if (context == nullptr) {
    return false;
  }

  if (disabled) {
    return false;
  }

  assert(context->error_index <= context->skip_ranges.size());
  if (context->error_index == context->skip_ranges.size()) {
    FaultInjectionDisable dg;
    ++context->error_index;
    context->skip_ranges.push_back(0);
    context->fault_registered = true;
    return true;
  }

  assert(context->skip_index <= context->skip_ranges[context->error_index]);

  if (context->skip_index == context->skip_ranges[context->error_index]) {
    ++context->error_index;
    context->skip_index = 0;
    context->fault_registered = true;
    return true;
  }

  ++context->skip_index;
  return false;
}

void fault_injection_point() {
  if (should_inject_fault()) {
    FaultInjectionDisable dg;
    throw InjectedFault("injected fault");
  }
}

void faulty_run(const std::function<void()>& f) {
  assert(!context);
  FaultInjectionContext ctx;
  context = &ctx;
  for (;;) {
    try {
      f();
    } catch (...) {
      FaultInjectionDisable dg;
      dump_state();
      ctx.skip_ranges.resize(ctx.error_index);
      ++ctx.skip_ranges.back();
      ctx.error_index = 0;
      ctx.skip_index = 0;
      assert(ctx.fault_registered);
      ctx.fault_registered = false;
      continue;
    }
    assert(!ctx.fault_registered);
    break;
  }
  context = nullptr;
}

FaultInjectionDisable::FaultInjectionDisable()
    : was_disabled(disabled) {
  disabled = true;
}

void FaultInjectionDisable::reset() const {
  disabled = was_disabled;
}

FaultInjectionDisable::~FaultInjectionDisable() {
  reset();
}

} // namespace ct_test
