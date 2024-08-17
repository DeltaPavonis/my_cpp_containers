/*
@file bounds_checked_vector.h
@brief Defines and implements `BoundsCheckedVector<T, Allocator>`, a bounds-checked version of
`std::vector<T, Allocator>`.

This file includes the following types:
- `BoundsCheckedVector<T, Allocator>`
*/

#ifndef BOUNDS_CHECKED_VECTOR_H
#define BOUNDS_CHECKED_VECTOR_H

#include <memory>
#include <vector>  
#include <source_location>
#include <iostream>
#include <format>
#include <optional>
#include <utility>

/* `BoundsCheckedVector<T, Allocator>` is a bounds-checked version of `std::vector<T, Allocator>`.
All calls to `operator[]`, `front()`, or `back()` will now raise a runtime error with detailed
diagnostic information on the event of an out-of-bounds access. The information includes:
1. The value of the index that was out-of-bounds,
2. The file/function/line/column at which the out-of-bounds access occurs,
3. The file/function/line/column at which the `BoundsCheckedVector` was last initialized, and
4. The file/function/line/column at which the size for this `BoundsCheckedVector` last changed,
along with information about the size change itself. */
template <typename T, typename Allocator = std::allocator<T>>
class BoundsCheckedVector : public std::vector<T, Allocator> {
    using SourceLoc = std::source_location;  /* Allows shortening code */

    /* `operator[]` takes exactly one argument, but for `BoundsCheckedVector`, we need it to
    take in not only the index, but also a `std::source_location` constructed at the call site.
    This motivates us to define a type which wraps an index along with a `std::source_location`;
    this is the type `IndexWithSourceLoc` below. */
    struct IndexWithSourceLoc {
        /* The reason `IndexType` is not `size_t` is so that when the user passes in a negative
        index, the error prints will contain that original index (rather than a value very
        close to `(size_t)-1` due to unsigned integer overflow); this aids debugging. */
        using IndexType = long long;

        IndexType index;  /* The passed-in index */
        SourceLoc sl;  /* The `std::source_location` for the call to `operator[]` */

        /* ALlow implicit conversions of `IndexWithSourceLoc` to the index itself, which is
        used when checking whether an `IndexWithSourceLoc` is out-of-bounds */
        operator IndexType() const { return index; }

        /* Constructs an `IndexWithSourceLoc` from the original index `index_`, and the
        current `std::source_location` (by default). */
        IndexWithSourceLoc(IndexType index_, const SourceLoc &sl_ = SourceLoc::current())
        : index{index_}, sl{sl_}
        {}
    };

    /* `last_construction_info` = The `std::source_location` corresponding to the location
    where this `BoundsCheckedVector<T>` was last constructed or initialized. */
    SourceLoc last_construction_info;
    /* `last_size_change` = The size before and after the most recent change to the size of
    this `BoundsCheckedVector<T>`. If the size was never changed, `last_size_change` will
    remain unset. */
    std::optional<std::pair<int, int>> last_size_change;
    /* `last_size_change_info` = The `std::source_location` corresponding to the location of
    the last size change. */
    SourceLoc last_size_change_info;

    /* `format_source_location` is an utility function which pretty-formats the information
    contained within a given `std::source_location` `sl`. In VSCode, the outputted locations
    will become clickable hyperlinks that take the user directly to the file/line/column at
    which an out-of-bounds access occurred. */
    static auto format_source_location(const SourceLoc &sl) -> std::string {
        return std::format(
            "File {}:{}:{} `{}`",
            sl.file_name(), sl.line(), sl.column(), sl.function_name()
        );
    }

    /* Checks if the index `index` passed to `operator[]` is out-of-bounds; if it is, then
    prints detailed diagnostic output and calls `std::exit(-1)`. */
    auto check_if_out_of_bounds(const IndexWithSourceLoc &index) const {
        if (index < 0 || index >= static_cast<long long>(std::vector<T>::size())) {
            /* Print the out-of-bounds index, and provide information about where this
            `BoundsCheckedVector<T>` was most recently constructed or initialized */
            std::cerr << std::format(
                "{}: Index out of bounds; {} for a std::vector of size {}\n"
                "Help: The std::vector was most recently constructed at {}\n",
                format_source_location(index.sl), index.index, std::vector<T>::size(),
                format_source_location(last_construction_info)
            );

            /* If this `BoundsCheckedVector<T>` has had its size changed, print information
            about where that size change occurred, and the size before and after that change. */
            if (last_size_change.has_value()) {
                auto [old_size, new_size] = *last_size_change;

                std::cerr << std::format(
                    "Help: This std::vector's most recent size change was from {} to {} at {}\n"
                    "This does not include size changes from std::erase/std::erase_if, however.\n",
                    old_size, new_size, format_source_location(last_size_change_info)
                );
            } else {
                std::cerr << "Note: This std::vector has no recorded size changes after its most "
                             "recent construction/initialization.\n";
            }

            std::exit(-1);
        }
        return;
    }

public:

    /* --- IMPLEMENT operator[] --- */

    /* Accesses the `index`th (0-indexed) element of this `BoundsCheckedVector<T>`, terminating
    the program if `index` is out of bounds (non-const version). */
    auto& operator[] (const IndexWithSourceLoc &index) {
        check_if_out_of_bounds(index);
        return std::vector<T>::operator[](index);
    }

    /* Accesses the `index`th (0-indexed) element of this `BoundsCheckedVector<T>`, terminating
    the program if `index` is out of bounds (const version). */
    const auto& operator[] (const IndexWithSourceLoc &index) const {
        check_if_out_of_bounds(index);
        return std::vector<T>::operator[](index);
    }


    /* --- IMPLEMENT OTHER RANDOM ACCESS FUNCTIONS --- */

    auto& front(const SourceLoc &curr_info = SourceLoc::current()) {
        /* Delegate to `operator[]`. Explicitly construct the `IndexWithSourceLoc` using
        the `std::source_location` for the actual call to `front()`, rather than using the
        `std::source_location` for the internal call to `operator[]` here. */
        return operator[]({0, curr_info});
    }

    const auto& front(const SourceLoc &curr_info = SourceLoc::current()) const {
        /* Delegate to `operator[]`. Explicitly construct the `IndexWithSourceLoc` using
        the `std::source_location` for the actual call to `front()`, rather than using the
        `std::source_location` for the internal call to `operator[]` here. */
        return operator[]({0, curr_info});
    }

    auto& back(const SourceLoc &curr_info = SourceLoc::current()) {
        /* Delegate to `operator[]`. Explicitly construct the `IndexWithSourceLoc` using
        the `std::source_location` for the actual call to `back()`, rather than using the
        `std::source_location` for the internal call to `operator[]` here. */
        return operator[]({
            static_cast<IndexWithSourceLoc::IndexType>(std::vector<T>::size()) - 1,
            curr_info
        });
    }

    const auto& back(const SourceLoc &curr_info = SourceLoc::current()) const {
        /* Delegate to `operator[]`. Explicitly construct the `IndexWithSourceLoc` using
        the `std::source_location` for the actual call to `back()`, rather than using the
        `std::source_location` for the internal call to `operator[]` here. */
        return operator[]({
            static_cast<IndexWithSourceLoc::IndexType>(std::vector<T>::size()) - 1,
            curr_info
        });
    }

    /*
    --- IMPLEMENT SIZE MODIFIERS ---
    We implement every size-modifying function for the base class `std::vector<T>` here. Each such
    function differs from the original function in exactly two ways:
    1. Each function now takes in the `std::source_location` corresponding to its call site, for
    debugging purposes; and,
    2. Each function updates `last_size_change`/`last_size_change_info` with information about the
    size-change it causes.
    This is very straightforward. The only nuances show up in the `swap` functions, in which we
    need to remember to update information for both the current and the other `BoundsCheckedVector`
    passed in.
    */

    void clear(const SourceLoc &curr_info = SourceLoc::current()) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::clear();
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    auto insert(
        std::vector<T>::const_iterator pos, const T &value,
        const SourceLoc &curr_info = SourceLoc::current()
    ) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::insert(pos, value);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    auto insert(
        std::vector<T>::const_iterator pos, T &&value,
        const SourceLoc &curr_info = SourceLoc::current()
    ) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::insert(pos, std::move(value));
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    auto insert(
        std::vector<T>::const_iterator pos, size_t count, const T &value,
        const SourceLoc &curr_info = SourceLoc::current()
    ) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::insert(pos, count, value);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    template <typename InputIterator>
    auto insert(
        std::vector<T>::const_iterator pos, InputIterator first, InputIterator last,
        const SourceLoc &curr_info = SourceLoc::current()
    ) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::insert(pos, first, last);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    auto insert(
        std::vector<T>::const_iterator pos, std::initializer_list<T> init,
        const SourceLoc &curr_info = SourceLoc::current()
    ) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::insert(pos, init);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    template <typename... Args>
    auto emplace(
        std::vector<T>::const_iterator pos, Args&&... args,
        const SourceLoc &curr_info = SourceLoc::current()
    ) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::insert(pos, std::forward<Args>(args)...);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    auto erase(
        std::vector<T>::const_iterator pos,
        const SourceLoc &curr_info = SourceLoc::current()
    ) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::erase(pos);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    auto erase(
        std::vector<T>::const_iterator first, std::vector<T>::const_iterator last,
        const SourceLoc &curr_info = SourceLoc::current()
    ) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::erase(first, last);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    void push_back(const T &value, const SourceLoc &curr_info = SourceLoc::current()) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::push_back(value);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    void push_back(T &&value, const SourceLoc &curr_info = SourceLoc::current()) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::push_back(std::move(value));
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    template <typename... Args>
    auto emplace_back(Args&&... args, const SourceLoc &curr_info = SourceLoc::current()) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::emplace_back(std::forward<Args>(args)...);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    void pop_back( const SourceLoc &curr_info = SourceLoc::current()) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::pop_back();
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    void resize(size_t count, const SourceLoc &curr_info = SourceLoc::current()) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::resize(count);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    void resize(size_t count, const T &element, const SourceLoc &curr_info = SourceLoc::current()) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::resize(count, element);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    void assign(size_t count, const T &element, const SourceLoc &curr_info = SourceLoc::current()) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::assign(count, element);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    template <typename InputIterator>
    void assign(
        InputIterator first, InputIterator last, 
        const SourceLoc &curr_info = SourceLoc::current()
    ) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::assign(first, last);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    void assign(std::initializer_list<T> init, const SourceLoc &curr_info = SourceLoc::current()) {
        auto old_size = std::vector<T>::size();
        std::vector<T>::assign(init);
        auto new_size = std::vector<T>::size();
        last_size_change = {old_size, new_size};
        last_size_change_info = curr_info;
    }

    void swap(BoundsCheckedVector &other, const SourceLoc &curr_info = SourceLoc::current()) {
        /* For `BoundsCheckedVector<T>::swap()`, we make sure to update the size-change info for
        both the current and the other `BoundsCheckedVector<T>`. */
        auto old_size_this = std::vector<T>::size(), old_size_other = other.size();
        std::vector<T>::swap(other);
        auto new_size_this = std::vector<T>::size(), new_size_other = other.size();
        last_size_change = {old_size_this, new_size_this};
        other.last_size_change = {old_size_other, new_size_other};
        last_size_change_info = other.last_size_change_info = curr_info;
    }

    friend void swap(
        BoundsCheckedVector &a, BoundsCheckedVector &b,
        const SourceLoc &curr_info = SourceLoc::current()
    ) {
        /* Delegate to `BoundsCheckedVector<T>::swap()` */
        a.swap(b);

        /* For the non-member `swap` function on `BoundsCheckedVector<T>`s, we make sure to
        set the size-change info to reflect the call to that non-member `swap` function,
        rather than the internal call to `a.swap(b)` in the line above (which is what it would
        be set to if we did not add the line below). */
        a.last_size_change_info = b.last_size_change_info = curr_info;
    }


    /*
    --- CONSTRUCTORS ---
    We implement every constructor for the base class `std::vector<T>` here. Each such function
    differs from the original function in one way: they now also take in the `std::source_location`
    corresponding to the call site of the constructor, which is used to initialize the debugging
    variable `last_construction_info`. In short, every constructor now records information about
    where it was called to `last_construction_info`.
    */

    BoundsCheckedVector(
        const Allocator &alloc = {},
        const SourceLoc &curr_info = SourceLoc::current()
    ) : std::vector<T>(alloc), last_construction_info{curr_info}
    {}

    BoundsCheckedVector(
        size_t size_,
        const Allocator &alloc = {},
        const SourceLoc &curr_info = SourceLoc::current()
    ) : std::vector<T>(size_, alloc), last_construction_info{curr_info}
    {}

    BoundsCheckedVector(
        size_t size_, const T &element, const Allocator &alloc = {},
        const SourceLoc &curr_info = SourceLoc::current()
    ) : std::vector<T>(size_, element, alloc), last_construction_info{curr_info}
    {}

    BoundsCheckedVector(
        std::initializer_list<T> init, const Allocator &alloc = {},
        const SourceLoc &curr_info = SourceLoc::current()
    ) : std::vector<T>(init, alloc), last_construction_info{curr_info}
    {}

    template <typename InputIterator>
    BoundsCheckedVector(
        InputIterator first, InputIterator last, const Allocator &alloc = {},
        const SourceLoc &curr_info = SourceLoc::current()
    ) : std::vector<T>(first, last, alloc), last_construction_info{curr_info}
    {}

    BoundsCheckedVector(
        const BoundsCheckedVector &other,
        const SourceLoc &curr_info = SourceLoc::current()
    ) : std::vector<T>(other), last_construction_info{curr_info}
    {}

    BoundsCheckedVector(
        const BoundsCheckedVector &other, const Allocator &alloc,
        const SourceLoc &curr_info = SourceLoc::current()
    ) : std::vector<T>(other, alloc), last_construction_info{curr_info}
    {}

    BoundsCheckedVector(
        BoundsCheckedVector &&other,
        const SourceLoc &curr_info = SourceLoc::current()
    ) : std::vector<T>(std::move(other)), last_construction_info{curr_info}
    {}

    BoundsCheckedVector(
        BoundsCheckedVector &&other, const Allocator &alloc,
        const SourceLoc &curr_info = SourceLoc::current()
    ) : std::vector<T>(std::move(other), alloc), last_construction_info{curr_info}
    {}
};

/* Specialize `std::formatter` for `BoundsCheckedVector<T, Allocator>` */
template <typename T, typename Allocator>
struct std::formatter<BoundsCheckedVector<T, Allocator>> : public std::formatter<std::string> {
    auto format(
        const BoundsCheckedVector<T, Allocator> &v,
        std::format_context &format_context
    ) const {
        auto output = format_context.out();
        std::format_to(output, "{{");
        if (!v.empty()) {
            std::format_to(output, "{}", v.front());
            for (size_t i = 1; i < v.size(); ++i) {
                std::format_to(output, ", {}", v[i]);
            }
        }
        std::format_to(output, "}}");
        return output;
    }
};

#endif