#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

template <typename T> class ObjectPool : public std::enable_shared_from_this<ObjectPool<T>> {
public:
  ObjectPool(size_t size) {
    pool_.reserve(size * 2);
    for (size_t i = 0; i < size; ++i) {
      pool_.push_back(std::make_unique<T>());
    }
  }

  /**
   * Acquire an object from the pool. 
   * The returned unique_ptr will automatically return the object to the pool when destroyed.
   */
  auto acquire() {
    std::unique_ptr<T> ptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!pool_.empty()) {
        ptr = std::move(pool_.back());
        pool_.pop_back();
      }
    }

    if (!ptr) {
        ptr = std::make_unique<T>();
    }

    // Return a unique_ptr with a custom deleter that puts it back in the pool
    auto weak_this = std::weak_ptr<ObjectPool<T>>(this->shared_from_this());
    return std::unique_ptr<T, std::function<void(T*)>>(ptr.release(), [weak_this](T* p) {
        if (auto pool = weak_this.lock()) {
            pool->release(std::unique_ptr<T>(p));
        } else {
            delete p;
        }
    });
  }

  void release(std::unique_ptr<T> ptr) {
    if (!ptr) return;
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.push_back(std::move(ptr));
  }

private:
  std::vector<std::unique_ptr<T>> pool_;
  std::mutex mutex_;
};
