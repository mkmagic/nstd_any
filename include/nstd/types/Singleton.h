#pragma once

#include <memory>
#include <utility>

namespace nstd::types {
/**
 * @brief Singleton CTRP class.
 * Derived classes of this singleton will inherit the `getInstance` method
 * @tparam Derived The derived type
 */
template <typename Derived> class singleton {
public:
  /**
   * Getting the singleton instance, it is also created here for the first time.
   * @tparam Args variadic arguments for the constructors.
   * @param arguments The arguments for the constructor of the derived type.
   * @return derived object reference
   */
  template <typename... Args>
  static std::shared_ptr<Derived> getInstance(Args... arguments) {
    if (instance == nullptr) {
      instance = std::make_shared<Derived>(std::forward<Args>(arguments)...);
    }
    return instance;
  }

  singleton &operator=(const singleton &) = delete;
  singleton(const singleton &) = delete;
  singleton() = default;
  virtual ~singleton() = default;

private:
  inline static std::shared_ptr<Derived> instance = nullptr;
};

} // namespace nstd::types