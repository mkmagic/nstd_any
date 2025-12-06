#ifndef NSTD_ANY_HPP
#define NSTD_ANY_HPP

#include <initializer_list>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace nstd {

class any;

namespace detail {
// Trait to check if a type is in_place_type_t
template <typename T> struct is_in_place_type : std::false_type {};

template <typename T>
struct is_in_place_type<std::in_place_type_t<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_in_place_type_v = is_in_place_type<T>::value;
} // namespace detail

/**
 * @brief Exception thrown by failed any_cast operations.
 */
class bad_any_cast : public std::bad_cast {
public:
  const char *what() const noexcept override { return "nstd::bad_any_cast"; }
};

/**
 * @brief A type-safe container for single values of any type.
 *
 * nstd::any is a generalized version of std::any that supports:
 * - CopyConstructible types (like std::any)
 * - MoveConstructible-only types (unlike std::any)
 *
 * It implements Small Value Optimization (SVO) to avoid heap allocation
 * for small, nothrow-move-constructible types.
 *
 * @note Copying an nstd::any holding a move-only type will throw
 * std::logic_error.
 */
class any {
public:
  /// @brief Constructs an empty any.
  constexpr any() noexcept : manager(nullptr) {}

  /**
   * @brief Copy constructor.
   * @param other The any object to copy.
   * @throws std::logic_error if other contains a move-only type.
   */
  any(const any &other) {
    if (other.has_value()) {
      Arg arg;
      arg.any_ptr = this;
      other.manager(Op::Clone, &other, &arg);
    } else {
      manager = nullptr;
    }
  }

  /**
   * @brief Move constructor.
   * @param other The any object to move from.
   * @post other is empty.
   */
  any(any &&other) noexcept {
    if (other.has_value()) {
      Arg arg;
      arg.any_ptr = this;
      other.manager(Op::Xfer, &other, &arg);
    } else {
      manager = nullptr;
    }
  }

  /**
   * @brief Constructs an any holding a copy/move of value.
   * @tparam T The type of the value.
   * @param value The value to store.
   */
  template <typename T, typename VT = std::decay_t<T>,
            typename = std::enable_if_t<!std::is_same_v<VT, any> &&
                                        !detail::is_in_place_type_v<VT>>>
  any(T &&value) {
    emplace<VT>(std::forward<T>(value));
  }

  /**
   * @brief Constructs an any holding a T constructed in-place.
   * @tparam T The type to construct.
   * @param args Arguments to forward to T's constructor.
   */
  template <typename T, typename... Args, typename VT = std::decay_t<T>>
  explicit any(std::in_place_type_t<T>, Args &&...args) {
    emplace<VT>(std::forward<Args>(args)...);
  }

  /**
   * @brief Constructs an any holding a T constructed in-place with
   * initializer_list.
   * @tparam T The type to construct.
   * @param il Initializer list to forward.
   * @param args Additional arguments to forward.
   */
  template <typename T, typename U, typename... Args,
            typename VT = std::decay_t<T>>
  explicit any(std::in_place_type_t<T>, std::initializer_list<U> il,
               Args &&...args) {
    emplace<VT>(il, std::forward<Args>(args)...);
  }

  /// @brief Destructor. Destroys the contained object.
  ~any() { reset(); }

  // Assignment

  /**
   * @brief Copy assignment.
   * @param rhs The any object to copy.
   * @throws std::logic_error if rhs contains a move-only type.
   */
  any &operator=(const any &rhs) {
    any(rhs).swap(*this);
    return *this;
  }

  /**
   * @brief Move assignment.
   * @param rhs The any object to move from.
   */
  any &operator=(any &&rhs) noexcept {
    any(std::move(rhs)).swap(*this);
    return *this;
  }

  /**
   * @brief Value assignment.
   * @tparam T The type of the value.
   * @param rhs The value to assign.
   */
  template <typename T, typename VT = std::decay_t<T>,
            typename = std::enable_if_t<!std::is_same_v<VT, any>>>
  any &operator=(T &&rhs) {
    any(std::forward<T>(rhs)).swap(*this);
    return *this;
  }

  // Modifiers

  /**
   * @brief Emplaces a new value into the any.
   * @tparam T The type to construct.
   * @param args Arguments for T's constructor.
   * @return Reference to the constructed value.
   */
  template <typename T, typename... Args>
  std::decay_t<T> &emplace(Args &&...args) {
    reset();
    using VT = std::decay_t<T>;
    using Mgr = ManagerImpl<VT>;
    Mgr::Create(storage, std::forward<Args>(args)...);
    manager = &Mgr::Manage;
    return *static_cast<VT *>(Mgr::Access(storage));
  }

  /**
   * @brief Emplaces a new value with initializer_list.
   * @tparam T The type to construct.
   * @param il Initializer list.
   * @param args Additional arguments.
   * @return Reference to the constructed value.
   */
  template <typename T, typename U, typename... Args>
  std::decay_t<T> &emplace(std::initializer_list<U> il, Args &&...args) {
    reset();
    using VT = std::decay_t<T>;
    using Mgr = ManagerImpl<VT>;
    Mgr::Create(storage, il, std::forward<Args>(args)...);
    manager = &Mgr::Manage;
    return *static_cast<VT *>(Mgr::Access(storage));
  }

  /// @brief Destroys the contained object and makes the any empty.
  void reset() noexcept {
    if (has_value()) {
      manager(Op::Destroy, this, nullptr);
      manager = nullptr;
    }
  }

  /// @brief Swaps the content of this any with another.
  void swap(any &other) noexcept {
    if (!has_value() && !other.has_value()) {
      return;
    }
    if (has_value() && other.has_value()) {
      if (this == &other)
        return;
      any tmp;
      Arg arg;
      arg.any_ptr = &tmp;
      manager(Op::Xfer, this, &arg);

      arg.any_ptr = this;
      other.manager(Op::Xfer, &other, &arg);

      arg.any_ptr = &other;
      tmp.manager(Op::Xfer, &tmp, &arg);
    } else {
      any *empty = !has_value() ? this : &other;
      any *full = !has_value() ? &other : this;
      Arg arg;
      arg.any_ptr = empty;
      full->manager(Op::Xfer, full, &arg);
    }
  }

  // Observers

  /// @brief Checks if the any holds a value.
  bool has_value() const noexcept { return manager != nullptr; }

  /// @brief Returns the type_info of the contained value, or typeid(void) if
  /// empty.
  const std::type_info &type() const noexcept {
    if (!has_value()) {
      return typeid(void);
    }
    Arg arg;
    manager(Op::GetTypeInfo, this, &arg);
    return *arg.typeinfo;
  }

  // Non-member cast needs access to internals
  template <typename T> friend T *any_cast(any *) noexcept;

  template <typename T> friend const T *any_cast(const any *) noexcept;

private:
  /**
   * @brief Operation codes for the manager function.
   */
  enum class Op {
    Access,      ///< Access the contained object.
    GetTypeInfo, ///< Get type_info of the contained object.
    Clone,       ///< Clone the contained object (copy).
    Destroy,     ///< Destroy the contained object.
    Xfer         ///< Transfer ownership (move).
  };

  /**
   * @brief Union to pass arguments to/from the manager function.
   */
  union Arg {
    void *obj;                      ///< Pointer to the contained object.
    const std::type_info *typeinfo; ///< Pointer to type_info.
    any *any_ptr; ///< Pointer to another any object (for cloning/xfer).
  };

  static constexpr size_t BufferSize = 4 * sizeof(void *);
  static constexpr size_t Alignment = alignof(void *);

  /**
   * @brief Storage for the contained object.
   *
   * Uses a union to support Small Value Optimization (SVO).
   * If the object fits in `buffer` and is nothrow move constructible, it is
   * stored in `buffer`. Otherwise, it is allocated on the heap and the pointer
   * is stored in `ptr`.
   */
  union Storage {
    constexpr Storage() : ptr(nullptr) {}
    void *ptr; ///< Pointer to heap-allocated object.
    std::aligned_storage_t<BufferSize, Alignment>
        buffer; ///< Inline buffer for SVO.
  };

  /**
   * @brief Function pointer type for the type-erasure manager.
   *
   * The manager function handles all type-specific operations (access, copy,
   * move, destroy).
   */
  using ManagerType = void (*)(Op, const any *, Arg *);

  ManagerType manager =
      nullptr;     ///< Pointer to the manager function for the contained type.
  Storage storage; ///< Storage for the contained object.

  template <typename T>
  static constexpr bool IsSmall =
      sizeof(T) <= BufferSize && alignof(T) <= Alignment &&
      std::is_nothrow_move_constructible_v<T>;

  /**
   * @brief Implementation of the manager function for a specific type T.
   * @tparam T The type of the contained object.
   */
  template <typename T> struct ManagerImpl {
    /**
     * @brief The manager function.
     * @param op The operation to perform.
     * @param src The source any object.
     * @param arg Argument/Result union.
     */
    static void Manage(Op op, const any *src, Arg *arg) {
      switch (op) {
      case Op::Access:
        arg->obj = Access(const_cast<Storage &>(src->storage));
        break;
      case Op::GetTypeInfo:
        arg->typeinfo = &typeid(T);
        break;
      case Op::Clone:
        if constexpr (std::is_copy_constructible_v<T>) {
          const T &source_val = *static_cast<const T *>(Access(src->storage));
          Create(arg->any_ptr->storage, source_val);
          arg->any_ptr->manager = &Manage;
        } else {
          throw std::logic_error("nstd::any: Copying a move-only type");
        }
        break;
      case Op::Destroy:
        Destroy(const_cast<Storage &>(src->storage));
        break;
      case Op::Xfer: {
        T &source_val =
            *static_cast<T *>(Access(const_cast<Storage &>(src->storage)));
        Create(arg->any_ptr->storage, std::move(source_val));
        arg->any_ptr->manager = &Manage;
        Destroy(const_cast<Storage &>(src->storage));
        const_cast<any *>(src)->manager = nullptr;
      } break;
      }
    }

    /**
     * @brief Accesses the contained object.
     * @param s The storage.
     * @return void* Pointer to the object.
     */
    static void *Access(const Storage &s) {
      if constexpr (IsSmall<T>) {
        return const_cast<void *>(static_cast<const void *>(&s.buffer));
      } else {
        return s.ptr;
      }
    }

    /**
     * @brief Destroys the contained object.
     * @param s The storage.
     */
    static void Destroy(Storage &s) {
      if constexpr (IsSmall<T>) {
        T *ptr = static_cast<T *>(static_cast<void *>(&s.buffer));
        ptr->~T();
      } else {
        T *ptr = static_cast<T *>(s.ptr);
        delete ptr;
      }
    }

    /**
     * @brief Creates the object in storage.
     * @param s The storage.
     * @param args Arguments for construction.
     */
    template <typename... Args> static void Create(Storage &s, Args &&...args) {
      if constexpr (IsSmall<T>) {
        new (&s.buffer) T(std::forward<Args>(args)...);
      } else {
        s.ptr = new T(std::forward<Args>(args)...);
      }
    }
  };
};

/**
 * @brief Swaps two any objects.
 */
inline void swap(any &x, any &y) noexcept { x.swap(y); }

/**
 * @brief Performs a type-safe access to the contained object.
 * @tparam T The type to cast to.
 * @param operand Pointer to the any object.
 * @return Pointer to the contained object if types match, nullptr otherwise.
 */
template <typename T> T *any_cast(any *operand) noexcept {
  if (operand && operand->type() == typeid(T)) {
    any::Arg arg;
    operand->manager(any::Op::Access, operand, &arg);
    return static_cast<T *>(arg.obj);
  }
  return nullptr;
}

/**
 * @brief Performs a type-safe access to the contained object (const overload).
 * @tparam T The type to cast to.
 * @param operand Pointer to the any object.
 * @return Const pointer to the contained object if types match, nullptr
 * otherwise.
 */
template <typename T> const T *any_cast(const any *operand) noexcept {
  if (operand && operand->type() == typeid(T)) {
    any::Arg arg;
    operand->manager(any::Op::Access, operand, &arg);
    return static_cast<const T *>(arg.obj);
  }
  return nullptr;
}

/**
 * @brief Performs a type-safe cast to the contained object.
 * @tparam T The type to cast to.
 * @param operand The any object.
 * @return The contained object.
 * @throws bad_any_cast if types do not match.
 */
template <typename T> T any_cast(any &operand) {
  using U = std::remove_cv_t<std::remove_reference_t<T>>;
  static_assert(std::is_constructible_v<T, U &>, "Invalid cast");
  auto *ptr = any_cast<U>(&operand);
  if (!ptr)
    throw bad_any_cast();
  return static_cast<T>(*ptr);
}

/**
 * @brief Performs a type-safe cast to the contained object (const overload).
 * @tparam T The type to cast to.
 * @param operand The any object.
 * @return The contained object.
 * @throws bad_any_cast if types do not match.
 */
template <typename T> T any_cast(const any &operand) {
  using U = std::remove_cv_t<std::remove_reference_t<T>>;
  static_assert(std::is_constructible_v<T, const U &>, "Invalid cast");
  auto *ptr = any_cast<U>(&operand);
  if (!ptr)
    throw bad_any_cast();
  return static_cast<T>(*ptr);
}

/**
 * @brief Performs a type-safe cast to the contained object (rvalue overload).
 * @tparam T The type to cast to.
 * @param operand The any object.
 * @return The contained object (moved).
 * @throws bad_any_cast if types do not match.
 */
template <typename T> T any_cast(any &&operand) {
  using U = std::remove_cv_t<std::remove_reference_t<T>>;
  static_assert(std::is_constructible_v<T, U>, "Invalid cast");
  auto *ptr = any_cast<U>(&operand);
  if (!ptr)
    throw bad_any_cast();
  return static_cast<T>(std::move(*ptr));
}

} // namespace nstd

#endif // NSTD_ANY_HPP
