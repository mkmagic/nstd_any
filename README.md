# nstd::any

`nstd::any` is a robust, single-header C++17 implementation of a type-safe container for single values of any type. It is designed as a superior alternative to `std::any`, offering enhanced flexibility and performance characteristics.

## Rationale

The standard `std::any` has a significant limitation: it requires contained types to be **copy-constructible**. This prevents it from holding move-only types like `std::unique_ptr`, `std::thread`, or `std::future`.

`nstd::any` addresses this by implementing a **hybrid copy logic**:
- It **supports move-only types** fully.
- It behaves like `std::any` for copyable types.
- It throws `std::logic_error` only when you attempt to *copy* an `nstd::any` that holds a move-only type.

This design allows `nstd::any` to be used in a much wider range of scenarios where ownership transfer (move semantics) is preferred or required.

## Features

- **Move-Only Support**: Can store and manage move-only types (e.g., `std::unique_ptr`).
- **Small Value Optimization (SVO)**: Avoids heap allocation for small types (up to `4 * sizeof(void*)`) that are nothrow move constructible.
- **Standard API**: Drop-in replacement for `std::any` with a familiar API (`emplace`, `reset`, `has_value`, `type`, `any_cast`).
- **Type Safety**: Throws `nstd::bad_any_cast` on invalid casts.
- **Single Header**: Easy integration; just include `include/any/nstd_any.hpp`.

## Dependencies

- **C++17** compliant compiler.
- **Google Test** (for building tests only).

## Build Instructions

This project uses CMake. To build and run the tests:

```bash
mkdir build
cd build
cmake ..
make
./tests
```

## Usage Example

```cpp
#include "any/nstd_any.hpp"
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
