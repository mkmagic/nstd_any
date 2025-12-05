#include <nstd/any.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// Helper for tracking construction/destruction
struct Tracker {
  static int constructed;
  static int destructed;
  int val;

  Tracker(int v) : val(v) { constructed++; }
  Tracker(const Tracker &t) : val(t.val) { constructed++; }
  Tracker(Tracker &&t) noexcept : val(t.val) {
    constructed++;
    t.val = 0;
  }
  ~Tracker() { destructed++; }

  static void Reset() {
    constructed = 0;
    destructed = 0;
  }
};

int Tracker::constructed = 0;
int Tracker::destructed = 0;

// Helper for move-only type
struct MoveOnly {
  int val;
  MoveOnly(int v) : val(v) {}
  MoveOnly(const MoveOnly &) = delete;
  MoveOnly &operator=(const MoveOnly &) = delete;
  MoveOnly(MoveOnly &&) = default;
  MoveOnly &operator=(MoveOnly &&) = default;
};

// Helper for throwing copy constructor
struct ThrowingCopy {
  ThrowingCopy() = default;
  ThrowingCopy(const ThrowingCopy &) {
    throw std::runtime_error("Copy failed");
  }
  ThrowingCopy(ThrowingCopy &&) = default;
};

TEST(NStdAnyTest, DefaultConstruction) {
  nstd::any a;
  EXPECT_FALSE(a.has_value());
  EXPECT_EQ(a.type(), typeid(void));
}

TEST(NStdAnyTest, PrimitivesAndSVO) {
  nstd::any a = 42;
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(a.type(), typeid(int));
  EXPECT_EQ(nstd::any_cast<int>(a), 42);

  a = 3.14;
  EXPECT_EQ(a.type(), typeid(double));
  EXPECT_DOUBLE_EQ(nstd::any_cast<double>(a), 3.14);
}

TEST(NStdAnyTest, StandardContainers) {
  std::vector<int> v = {1, 2, 3};
  nstd::any a = v;
  EXPECT_EQ(a.type(), typeid(std::vector<int>));

  auto &ref = nstd::any_cast<std::vector<int> &>(a);
  ASSERT_EQ(ref.size(), 3);
  EXPECT_EQ(ref[0], 1);

  std::string s = "hello";
  nstd::any b = s;
  EXPECT_EQ(nstd::any_cast<std::string>(b), "hello");
}

TEST(NStdAnyTest, MoveOnlyTypes) {
  auto ptr = std::make_unique<int>(100);
  nstd::any a = std::move(ptr);
  EXPECT_TRUE(ptr == nullptr);
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(a.type(), typeid(std::unique_ptr<int>));

  // Move construct any
  nstd::any b = std::move(a);
  EXPECT_FALSE(a.has_value()); // Post-move state is empty
  EXPECT_TRUE(b.has_value());
  EXPECT_EQ(*nstd::any_cast<std::unique_ptr<int> &>(b), 100);

  // To move out of 'any', we must cast the 'any' object to an rvalue.
  // This invokes any_cast<T>(any&&), which moves the contained object.
  std::unique_ptr<int> val = nstd::any_cast<std::unique_ptr<int>>(std::move(b));

  // The 'any' object still contains a value (the moved-from unique_ptr, which
  // is null). It is NOT empty/reset.
  EXPECT_TRUE(b.has_value());

  // The extracted value should be correct.
  EXPECT_EQ(*val, 100);

  // The contained value in 'b' should now be null (moved-from).
  EXPECT_EQ(nstd::any_cast<std::unique_ptr<int> &>(b), nullptr);
}

TEST(NStdAnyTest, CopyLogicErrorForMoveOnly) {
  nstd::any a = std::make_unique<int>(200);
  EXPECT_THROW({ nstd::any b = a; }, std::logic_error);
}

TEST(NStdAnyTest, CopyConstructionForCopyable) {
  nstd::any a = std::string("test");
  nstd::any b = a;
  EXPECT_TRUE(a.has_value());
  EXPECT_TRUE(b.has_value());
  EXPECT_EQ(nstd::any_cast<std::string>(a), "test");
  EXPECT_EQ(nstd::any_cast<std::string>(b), "test");
}

TEST(NStdAnyTest, CopyAssignmentForCopyable) {
  nstd::any a = std::string("test");
  nstd::any b;
  b = a;
  EXPECT_EQ(nstd::any_cast<std::string>(b), "test");
}

TEST(NStdAnyTest, MoveAssignment) {
  nstd::any a = std::string("move me");
  nstd::any b;
  b = std::move(a);
  EXPECT_FALSE(a.has_value());
  EXPECT_EQ(nstd::any_cast<std::string>(b), "move me");
}

TEST(NStdAnyTest, Emplace) {
  nstd::any a;
  a.emplace<std::string>("emplaced");
  EXPECT_EQ(nstd::any_cast<std::string>(a), "emplaced");

  a.emplace<std::vector<int>>({1, 2, 3});
  EXPECT_EQ(a.type(), typeid(std::vector<int>));
  EXPECT_EQ(nstd::any_cast<std::vector<int> &>(a).size(), 3);
}

TEST(NStdAnyTest, Reset) {
  nstd::any a = 10;
  a.reset();
  EXPECT_FALSE(a.has_value());
  EXPECT_EQ(a.type(), typeid(void));
}

TEST(NStdAnyTest, Swap) {
  nstd::any a = 1;
  nstd::any b = 2;
  a.swap(b);
  EXPECT_EQ(nstd::any_cast<int>(a), 2);
  EXPECT_EQ(nstd::any_cast<int>(b), 1);

  nstd::any c;
  a.swap(c);
  EXPECT_FALSE(a.has_value());
  EXPECT_EQ(nstd::any_cast<int>(c), 2);
}

TEST(NStdAnyTest, AnyCast) {
  nstd::any a = 5;

  // Pointer
  EXPECT_NE(nstd::any_cast<int>(&a), nullptr);
  EXPECT_EQ(nstd::any_cast<double>(&a), nullptr);

  // Const Pointer
  const nstd::any ca = 5;
  EXPECT_NE(nstd::any_cast<int>(&ca), nullptr);

  // Reference
  EXPECT_EQ(nstd::any_cast<int &>(a), 5);
  EXPECT_THROW(nstd::any_cast<double &>(a), nstd::bad_any_cast);

  // Value
  EXPECT_EQ(nstd::any_cast<int>(a), 5);
}

TEST(NStdAnyTest, ComplexTypesLifecycle) {
  Tracker::Reset();
  {
    nstd::any a = Tracker(10);
    // Tracker(10) -> temp (constructed=1)
    // any(Tracker&&) -> move construct into storage (constructed=2)
    // temp destroyed (destructed=1)
    EXPECT_EQ(Tracker::constructed, 2);
    EXPECT_EQ(Tracker::destructed, 1);
  }
  // a destroyed (destructed=2)
  EXPECT_EQ(Tracker::constructed, 2);
  EXPECT_EQ(Tracker::destructed, 2);
}

TEST(NStdAnyTest, ExceptionSafety) {
  nstd::any a = 10;
  try {
    // Throw during copy
    ThrowingCopy tc;
    nstd::any b = tc; // Copy into b
    // Now try to assign b to a, which will trigger copy of ThrowingCopy
    a = b;
  } catch (...) {
  }

  // a should remain unchanged or at least valid (strong guarantee not strictly
  // required by std::any but good to have) std::any::operator= provides strong
  // guarantee if it throws. Our implementation uses copy-and-swap:
  // any(rhs).swap(*this). If any(rhs) throws, *this is untouched.
  EXPECT_TRUE(a.has_value());
  EXPECT_EQ(nstd::any_cast<int>(a), 10);
}

TEST(NStdAnyTest, InPlaceConstruction) {
  nstd::any a(std::in_place_type<std::string>, "in place");
  EXPECT_EQ(nstd::any_cast<std::string>(a), "in place");

  nstd::any b(std::in_place_type<std::vector<int>>, {1, 2, 3});
  EXPECT_EQ(nstd::any_cast<std::vector<int> &>(b).size(), 3);
}
