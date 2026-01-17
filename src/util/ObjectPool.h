#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

template <typename T> class ObjectPool {
public:
  ObjectPool(size_t size) {
    for (size_t i = 0; i < size; ++i) {
      pool_.push_back(std::make_unique<T>());
    }
  }

  // Simple get/return for single thread usage or mutex protected
  // For now, let's keep it simple. If we need high perf, we avoid mutex.
  // Given the constraints, let's make it thread-safe for safety.

  std::unique_ptr<T> acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pool_.empty()) {
      return std::make_unique<T>(); // Expand if empty
    }
    auto ptr = std::move(pool_.back());
    pool_.pop_back();
    return ptr;
  }

  void release(std::unique_ptr<T> ptr) {
    if (!ptr)
      return;
    std::lock_guard<std::mutex> lock(mutex_);
    // Reset state if T has reset method
    // ptr->reset();
    pool_.push_back(std::move(ptr));
  }

private:
  std::vector<std::unique_ptr<T>> pool_;
  std::mutex mutex_;
};
