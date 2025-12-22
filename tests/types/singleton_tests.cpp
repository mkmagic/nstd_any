#include <gtest/gtest.h>
#include <nstd/types/Singleton.h>
#include <string>
#include <vector>

using namespace nstd::types;

// Test helper classes
class SimpleSingleton : public singleton<SimpleSingleton> {
public:
  int value = 0;
};

class ComplexSingleton : public singleton<ComplexSingleton> {
public:
  std::string name;
  int id;

  ComplexSingleton(std::string n, int i) : name(std::move(n)), id(i) {}
};

class AnotherSingleton : public singleton<AnotherSingleton> {
public:
  int value = 0;
};

TEST(SingletonTest, Uniqueness) {
  auto instance1 = SimpleSingleton::getInstance();
  auto instance2 = SimpleSingleton::getInstance();

  // Check if both pointers point to the same object
  EXPECT_EQ(instance1, instance2);

  // Check if state persists
  instance1->value = 42;
  EXPECT_EQ(instance2->value, 42);
}

TEST(SingletonTest, ArgumentForwarding) {
  auto instance = ComplexSingleton::getInstance("test", 123);

  EXPECT_EQ(instance->name, "test");
  EXPECT_EQ(instance->id, 123);

  // Subsequent calls should ignore arguments and return the existing instance
  auto instance2 = ComplexSingleton::getInstance("ignored", 999);
  EXPECT_EQ(instance2->name, "test");
  EXPECT_EQ(instance2->id, 123);
  EXPECT_EQ(instance, instance2);
}

TEST(SingletonTest, DiscreteTypes) {
  auto s1 = SimpleSingleton::getInstance();
  auto s2 = AnotherSingleton::getInstance();

  s1->value = 10;
  s2->value = 20;

  EXPECT_EQ(s1->value, 10);
  EXPECT_EQ(s2->value, 20);

  EXPECT_NE(static_cast<void *>(s1.get()), static_cast<void *>(s2.get()));
}
