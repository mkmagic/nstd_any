#include "nstd/memory/mempool/MemPool.hpp"
#include <future>
#include <gtest/gtest.h>

TEST(MemPoolTest, ConstructWithValidArgs) {
  nstd::memory::MemPool<int> pool(1024, 4);
  EXPECT_EQ(pool.capacity(), 4u);
  EXPECT_EQ(pool.block_size(), 1024u);
}

TEST(MemPoolTest, AllocateAndDeallocate) {
  nstd::memory::MemPool<char> pool(64, 2);

  auto buf1 = pool.allocate();
  auto buf2 = pool.allocate();

  // Should throw when out of blocks
  EXPECT_THROW(pool.allocate(), std::runtime_error);

  // Deallocating one should allow another allocation
  buf1.reset(); // or let it go out scope to release
  auto buf3 = pool.allocate();
  EXPECT_NE(buf3.get(), nullptr);
}

TEST(MemPoolTest, AvailableCount) {
  nstd::memory::MemPool<int> pool(64, 2);
  EXPECT_EQ(pool.available(), 2u);

  auto b1 = pool.allocate();
  EXPECT_EQ(pool.available(), 1u);

  auto b2 = pool.allocate();
  EXPECT_EQ(pool.available(), 0u);

  // Should now be zero
  EXPECT_THROW(pool.allocate(), std::runtime_error);
}

TEST(MemPoolTest, ConstructorInvalidArgs) {
  EXPECT_THROW((nstd::memory::MemPool<int>(0, 1)), std::invalid_argument);
  EXPECT_THROW((nstd::memory::MemPool<int>(1, 0)), std::invalid_argument);
}

/// NOTE: ⚠️ If using std::thread, include <future> and link with -pthread.
TEST(MemPoolTest, ThreadSafeAllocation) {
  nstd::memory::MemPool<char> pool(64, 10);
  std::vector<std::future<void>> futures;

  for (int i = 0; i < 100; ++i) {
    futures.push_back(std::async(std::launch::async, [&pool]() {
      auto buf = pool.allocate();
      // Do something small
    }));
  }

  for (auto &f : futures)
    f.wait();
}

TEST(MemPoolTest, Alignment) {
  constexpr size_t Alignment = 128; // Choose a distinct alignment
  nstd::memory::MemPool<char, Alignment> pool(100, 3); // 100 bytes, 3 blocks

  auto b1 = pool.allocate();
  auto b2 = pool.allocate();
  auto b3 = pool.allocate();

  auto check_alignment = [&](void *ptr) {
    auto addr = reinterpret_cast<std::uintptr_t>(ptr);
    return (addr % Alignment) == 0;
  };

  EXPECT_TRUE(check_alignment(b1.get())) << "b1 not aligned to " << Alignment;
  EXPECT_TRUE(check_alignment(b2.get())) << "b2 not aligned to " << Alignment;
  EXPECT_TRUE(check_alignment(b3.get())) << "b3 not aligned to " << Alignment;
}

TEST(MemPoolTest, ReuseFreedBlock) {
  nstd::memory::MemPool<char> pool(64, 2);

  auto b1 = pool.allocate();
  void *ptr1 = b1.get();

  b1.reset(); // Return to pool manually

  auto b2 = pool.allocate();
  void *ptr2 = b2.get();

  // MemPool uses LIFO strategy, so immediate reallocation should return the
  // same block
  EXPECT_EQ(ptr1, ptr2)
      << "Pool should reuse the most recently freed block (LIFO)";
}

TEST(MemPoolTest, ScopedReturn) {
  nstd::memory::MemPool<int> pool(64, 1); // Only 1 block

  {
    auto b1 = pool.allocate();
    EXPECT_EQ(pool.available(), 0u);
  } // b1 goes out of scope, returns to pool

  EXPECT_EQ(pool.available(), 1u);
  auto b2 = pool.allocate(); // Should succeed
  EXPECT_NE(b2.get(), nullptr);
}

TEST(MemPoolTest, AlignmentStride) {
  // Test ensuring that stride calculation preserves alignment for all blocks
  constexpr size_t Align = 64;
  nstd::memory::MemPool<char, Align> pool(10, 5);
  // Block size 10 < Align 64.
  // Stride should be rounded up to 64 to ensure next block starts at aligned
  // address.

  std::vector<nstd::memory::unique_buffer<char>> bufs;
  for (int i = 0; i < 5; ++i) {
    bufs.push_back(pool.allocate());
    auto addr = reinterpret_cast<std::uintptr_t>(bufs.back().get());
    EXPECT_EQ(addr % Align, 0) << "Block " << i << " not aligned";
  }
}

TEST(MemPoolTest, LargeAlignment) {
  // Alignment larger than the data size
  constexpr size_t Align = 4096;                   // Page size
  nstd::memory::MemPool<double, Align> pool(1, 2); // 1 double (8 bytes)

  auto b1 = pool.allocate();
  auto b2 = pool.allocate();

  auto check = [&](void *p) {
    return (reinterpret_cast<std::uintptr_t>(p) % Align) == 0;
  };
  EXPECT_TRUE(check(b1.get())) << "b1 not aligned to 4096";
  EXPECT_TRUE(check(b2.get())) << "b2 not aligned to 4096";
}
