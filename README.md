# nstd

`nstd` is a C++ library designed to resemble and extend the standard library, providing robust implementations of useful components like `nstd::any` and efficient memory management utilities.

## Chapter 1: Any

### nstd::any

`nstd::any` is a robust, single-header C++17 implementation of a type-safe container for single values of any type. It is designed as a superior alternative to `std::any`, offering enhanced flexibility and performance characteristics.

#### Rationale

The standard `std::any` has a significant limitation: it requires contained types to be **copy-constructible**. This prevents it from holding move-only types like `std::unique_ptr`, `std::thread`, or `std::future`.

`nstd::any` addresses this by implementing a **hybrid copy logic**:
- It **supports move-only types** fully.
- It behaves like `std::any` for copyable types.
- It throws `std::logic_error` only when you attempt to *copy* an `nstd::any` that holds a move-only type.

This design allows `nstd::any` to be used in a much wider range of scenarios where ownership transfer (move semantics) is preferred or required.

#### Features

- **Move-Only Support**: Can store and manage move-only types (e.g., `std::unique_ptr`).
- **Small Value Optimization (SVO)**: Avoids heap allocation for small types (up to `4 * sizeof(void*)`) that are nothrow move constructible.
- **Standard API**: Drop-in replacement for `std::any` with a familiar API (`emplace`, `reset`, `has_value`, `type`, `any_cast`).
- **Type Safety**: Throws `nstd::bad_any_cast` on invalid casts.
- **Single Header**: Easy integration; just include `nstd/data/any.hpp`.

#### Usage Example

```cpp
#include "nstd/data/any.hpp"
#include <iostream>
#include <vector>
#include <memory>
#include <string>

int main() {
    // 1. Store simple types (SVO optimized)
    nstd::any a = 42;
    std::cout << nstd::any_cast<int>(a) << "\n"; // Output: 42

    // 2. Store complex types
    std::string hello = "Hello, nstd::any!";
    nstd::any b = hello;
    std::cout << nstd::any_cast<std::string>(b) << "\n";

    // 3. Store Move-Only types (e.g., std::unique_ptr)
    auto ptr = std::make_unique<int>(100);
    nstd::any c = std::move(ptr);

    // Note: You cannot copy 'c' now because it holds a unique_ptr.
    // nstd::any d = c; // This would throw std::logic_error

    // But you can move it!
    nstd::any e = std::move(c);

    // 4. Extracting Move-Only types
    // To extract a move-only type, you must cast from an rvalue reference of the any object
    if (e.has_value()) {
        auto extracted_ptr = nstd::any_cast<std::unique_ptr<int>>(std::move(e));
        std::cout << "Extracted value: " << *extracted_ptr << "\n"; // Output: 100
    }

    return 0;
}
```

## Chapter 2: Memory

The `nstd::memory` namespace contains helpers to manage memory structures, including Smart Buffers and a Memory Pool.

### Smart Buffers

Smart Buffers are containers that wrap arrays of data in a manner that can be described as a hybrid between smart pointers, `std::vector`, and `std::span`.

This library provides three classes that can manage a (dynamically) allocated buffer, providing clear ownership (RAII) and the ability to "release" a buffer to use the underlying pointer if need be.

Both `unique_buffer` and `shared_buffer` contain `view()` functions that should be used by client code when access to the underlying data is needed without owner transference. The `view()` function returns a `buffer_base` object that allows access to the pointer, size, and location (host / device) of the buffer's memory. `buffer_base` also provides a `span()` function to view the buffer as a non-owning container. **Client code should prefer receiving `std::span` if details of the memory ownership are not required.**

#### Released Buffer
Think of Released Buffer as a lightweight wrapper around a buffer. It contains a pointer to the start of the buffer, its length, and a deleter function that provides the means to free the memory of the buffer. This class **Does not manage the buffer's memory!** It gives the holder of the class the responsibility to free the buffer. Released Buffers are usually returned by other Smart Buffers when calling the `release()` function.

#### Unique Buffer
A move-only owning container for a contiguous block of type `T` elements, similar to `std::unique_ptr`. Use a `unique_buffer` when you don't need shared ownership of the buffer's data.

#### Shared Buffer
A thread-safe, reference-counted owning container similar to `std::shared_ptr` for a contiguous block of data. Use a `shared_buffer` when you need shared ownership of the buffer's data.

### Mempool

A Memory Pool, or MemPool for short, allocates a block of memory and efficiently manages many small, frequent memory allocations and deallocations. Instead of repeatedly calling the system allocator (`malloc` / `free`), the pool provides fixed-size chunks of memory from a reserved region, improving performance.

The implemented Mempool is thread-safe and returns a `unique_buffer` with a size that was provided during construction of the pool. The `unique_buffer` can be cast into a `shared_buffer` if the client wishes.

## Build Instructions

This project uses CMake. To build and run the tests:

```bash
mkdir build
cd build
cmake ..
make
./tests
```

## Integrating into your project

### Using Conan

1. Add `nstd/0.1.0` to your `conanfile.txt` or `conanfile.py`.

   **conanfile.txt**
   ```ini
   [requires]
   nstd/0.1.0

   [generators]
   CMakeDeps
   CMakeToolchain
   ```

2. In your `CMakeLists.txt`:
   ```cmake
   find_package(nstd REQUIRED)
   target_link_libraries(your_target PRIVATE nstd::nstd)
   ```

### Manual Install

1. Build and install:
   ```bash
   cmake -B build
   cmake --install build --prefix /usr/local
   ```
2. In your `CMakeLists.txt`:
   ```cmake
   find_package(nstd REQUIRED)
   target_link_libraries(your_target PRIVATE nstd::nstd)
   ```
