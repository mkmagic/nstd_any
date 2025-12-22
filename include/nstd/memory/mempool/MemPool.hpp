#pragma once

#include "../smart_buffers/unique_buffer.hpp"
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace nstd::memory {
/**
 * @brief A thread-safe, aligned memory pool that efficiently manages fixed-size
 * blocks.
 *
 * This pool allocates a single contiguous chunk of memory and divides it into
 * blocks. It ensures that each block is aligned to `Alignment` bytes, which is
 * critical for SIMD performance (e.g., AVX2/AVX512).
 *
 * @tparam T The type of elements in the pool.
 * @tparam Alignment The alignment requirement in bytes (default 64 for SIMD).
 */
template <typename T, size_t Alignment = 64> class MemPool {
public:
  /**
   * @brief Constructs the memory pool and pre-allocates all blocks.
   *
   * @param block_size The number of elements (T) in each block.
   * @param block_count The total number of blocks to allocate.
   * @param loc The metadata location tag (e.g., Host).
   *
   * @throws std::invalid_argument if size or count is 0.
   * @throws std::bad_alloc if memory allocation fails.
   */
  MemPool(std::size_t block_size, std::size_t block_count,
          MemoryLocation loc = MemoryLocation::Host)
      : block_size_(block_size), block_count_(block_count), location_(loc) {
    if (block_size_ == 0 || block_count_ == 0) {
      throw std::invalid_argument(
          "allocation_size and allocation_count must be > 0");
    }

    // Calculate stride to ensure every block start address is aligned.
    // block_size_ * sizeof(T) might not be a multiple of Alignment.
    // We pad each block so that (stride_ * sizeof(T)) % Alignment == 0.
    size_t byte_size = block_size_ * sizeof(T);
    size_t aligned_byte_size =
        (byte_size + Alignment - 1) / Alignment * Alignment;
    stride_ = aligned_byte_size / sizeof(T);

    size_t total_bytes = stride_ * sizeof(T) * block_count_;

    // Use C++17 aligned_alloc for base alignment.
    void *ptr = std::aligned_alloc(Alignment, total_bytes);
    if (!ptr) {
      throw std::bad_alloc();
    }
    data_ = static_cast<T *>(ptr);

    // Default construct elements if needed.
    // We use a try-catch block to ensure exception safety:
    // If a constructor throws, we must destroy previous blocks and free memory
    // because the MemPool destructor won't be called.
    if constexpr (!std::is_trivially_default_constructible_v<T>) {
      size_t constructed_count = 0;
      try {
        for (size_t i = 0; i < block_count_; ++i) {
          T *block_start = data_ + i * stride_;
          std::uninitialized_default_construct(block_start,
                                               block_start + block_size_);
          constructed_count++;
        }
      } catch (...) {
        // Rollback: destroy already constructed blocks
        for (size_t i = 0; i < constructed_count; ++i) {
          T *block_start = data_ + i * stride_;
          std::destroy(block_start, block_start + block_size_);
        }
        std::free(data_);
        throw;
      }
    }

    // Initialize free list (LIFO for cache locality)
    free_blocks_.reserve(block_count_);
    for (std::size_t i = 0; i < block_count_; ++i) {
      free_blocks_.push_back(data_ + i * stride_);
    }
  }

  /**
   * @brief Destructor. Destroys all elements and frees memory.
   *
   * @warning All `unique_buffer`s allocated from this pool MUST be destroyed
   * before the pool itself. The pool does not track outstanding buffers.
   */
  ~MemPool() {
    if (data_) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        for (size_t i = 0; i < block_count_; ++i) {
          T *block_start = data_ + i * stride_;
          std::destroy(block_start, block_start + block_size_);
        }
      }
      std::free(data_);
    }
  }

  MemPool(const MemPool &) = delete;
  MemPool &operator=(const MemPool &) = delete;

  /**
   * @brief Allocates a unique_buffer from the pool.
   *
   * @return A `unique_buffer<T>` with a custom deleter that returns memory to
   * this pool.
   *
   * @throws std::runtime_error if the pool is empty.
   *
   * @warning The returned buffer holds a reference to this pool. Ensure the
   * pool outlives the buffer!
   */
  unique_buffer<T> allocate() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (free_blocks_.empty()) {
      throw std::runtime_error("MemPool: out of buffers");
    }
    // LIFO (Stack) order: reuse mostly recently freed block for hot cache.
    T *ptr = free_blocks_.back();
    free_blocks_.pop_back();

    return unique_buffer<T>(
        ptr, block_size_, [this](T *p) { this->release_block(p); }, location_);
  }

  /// @return number of elements in each block
  std::size_t block_size() const noexcept { return block_size_; }

  /// @return total number of blocks in the pool
  std::size_t capacity() const noexcept { return block_count_; }

  /// @return number of currently available blocks
  std::size_t available() const noexcept {
    std::lock_guard<std::mutex> lock(mtx_);
    return free_blocks_.size();
  }

private:
  /**
   * @brief Returns a block to the free list. Called by unique_buffer deleter.
   */
  void release_block(T *p) {
    std::lock_guard<std::mutex> lock(mtx_);
    free_blocks_.push_back(p);
  }

  std::size_t block_size_;
  std::size_t stride_; ///< Stride in elements (includes padding for alignment)
  std::size_t block_count_;
  MemoryLocation location_;
  T *data_ = nullptr; ///< Pointer to the single contiguous memory chunk
  std::vector<T *> free_blocks_; ///< Stack of free block pointers
  mutable std::mutex mtx_;
};
} // namespace nstd::memory
