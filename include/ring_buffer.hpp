#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

// Single-Producer Single-Consumer ring buffer for hard real‑time, low‑latency systems
// Requires power‑of‑two Capacity, provides lock‑free, wait‑free operations

template <typename T, size_t Capacity>
class RingBuffer
{
  static_assert(Capacity > 1, "Capacity must be at least 2");
  static_assert(
    (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two for mask optimisation");
  static_assert(std::atomic<size_t>::is_always_lock_free, "std::atomic<size_t> must be lock-free");

public:
  RingBuffer()
  {
    head_.store(0, std::memory_order_relaxed);
    tail_.store(0, std::memory_order_relaxed);
  }

  // Push an item; returns false if buffer is full
  bool push(const T& item)
  {
    size_t head = head_.load(std::memory_order_relaxed);
    size_t next = (head + 1) & mask_;
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }
    buffer_[head] = item;
    head_.store(next, std::memory_order_release);
    return true;
  }

  // Pop an item; returns false if buffer is empty
  bool pop(T& item)
  {
    size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }
    item = buffer_[tail];
    tail_.store((tail + 1) & mask_, std::memory_order_release);
    return true;
  }

  // Number of items currently in buffer
  size_t size() const noexcept
  {
    size_t head = head_.load(std::memory_order_acquire);
    size_t tail = tail_.load(std::memory_order_acquire);
    return (head - tail) & mask_;
  }

  // True if buffer is empty
  bool empty() const noexcept
  {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
  }

  // True if buffer is full
  bool full() const noexcept
  {
    size_t head = head_.load(std::memory_order_acquire);
    return (((head + 1) & mask_) == tail_.load(std::memory_order_acquire));
  }

  // Maximum number of storable items (one slot is reserved)
  static constexpr size_t capacity() noexcept { return Capacity - 1; }

private:
  static constexpr size_t mask_ = Capacity - 1;

  std::array<T, Capacity> buffer_;
  alignas(64) std::atomic<size_t> head_;
  char pad1_[64 - sizeof(std::atomic<size_t>)];
  alignas(64) std::atomic<size_t> tail_;
  char pad2_[64 - sizeof(std::atomic<size_t>)];
};
