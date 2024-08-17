# cpp_containers

This repository contains C++20 implementations of various custom containers I've used in my personal projects (or that I've just found fun to implement). Each container is defined in its own corresponding header file, listed below:

## Variations on `std::vector`
### 1. `BoundsCheckedVector`
`BoundsCheckedVector` is an **automatically bounds-checked** dynamic array, designed to be a drop-in replacement for `std::vector` when debugging. In the event of any out-of-bounds access, it provides highly detailed debugging information:
* The index that was out-of-bounds and the size of the array at that point,
* The file/function/line/column at which the out-of-bounds access occurred,
* The file/function/line/column at which the `BoundsCheckedVector` was last constructed/initialized, and
* The file/function/line/column at which the size for this `BoundsCheckedVector` last changed, along with the size before and after that change.

Portably tracking the exact code locations of out-of-bounds accesses is done using [`std::source_location`](https://en.cppreference.com/w/cpp/utility/source_location).

### 2. `FixedCapacityVector`
`FixedCapacityVector` is a dynamically-resizable array with fixed compile-time capacity, based on the upcoming C++26 addition `std::inplace_vector`. As outlined in the [original proposal](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p0843r14.html#Motivation-and-Scope), such a container is very useful when
* **Non-default-constructible** objects must be stored (which `std::array` cannot do),
* A dynamically-resizable array is needed **during constant evaluation**;
* The performance penalty from memory allocation (due to growing or reallocating the array) is unacceptable; or, if
* Memory allocation is not possible (e.g. in embedded environments).

I provide my own simple proof-of-concept implementation here. I represent `FixedCapacityVector<T, Capacity, Allocator>` as a single-element `union { T array[Capacity]; }`, which allows for the storage of non-default-constructible objects. Allocator-aware element construction/destruction is then achieved using `std::allocator_traits`.

### 3. `StackAssistedVector`
`StackAssistedVector<T, StackCapacity, Allocator>` is a dynamically-resizable array with **pre-allocated stack storage** for up to `StackCapacity` elements, which guarantees **zero dynamic memory allocations** until that stack storage is exceeded. It is designed to be a drop-in replacement for `std::vector<T, Allocator>` in cases where the size is known to typically stay within some compile-time bound. In those scenarios, `StackAssistedVector` can be applied to reduce or eliminate the overhead incurred from dynamic memory allocations.

Overall, `StackAssistedVector<T, StackCapacity, Allocator>` can be thought of as the middle ground between `std::vector<T, Allocator>` and `FixedCapacityVector<T, StackCapacity, Allocator>`. While keeping small vectors entirely on the stack, it also allows falling back to dynamic allocation via the specified `Allocator` when the stack capacity is exceeded.

`StackAssistedVector` is inspired by the `InlinedVector` type from [pbrt-v4](https://github.com/mmp/pbrt-v4).