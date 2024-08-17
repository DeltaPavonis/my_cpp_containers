/*
@file fixed_capacity_vector.h
@brief Defines and implements `FixedCapacityVector<T, Capacity, Allocator>`, a variation on
`std::vector` that has fixed compile-time capacity.

This file includes the following types:
- `FixedCapacityVector<T, Capacity, Allocator>`
*/

#ifndef FIXED_CAPACITY_VECTOR_H
#define FIXED_CAPACITY_VECTOR_H

#include <cstdint>
#include <memory>
#include <cassert>
#include <format>
#include <string>

/* `FixedCapacityVector<T, Capacity, Allocator>` is a dynamically-resizable array with fixed
compile-time capacity `Capacity`. Based on the upcoming C++26 feature, `std::inplace_vector`
(see the proposal: https://tinyurl.com/2ezrtjyp). */
template <typename T, size_t Capacity, typename Allocator = std::allocator<T>>
struct FixedCapacityVector {
    using value_type             = T;
    using allocator_type         = Allocator;
    using pointer                = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer          = typename std::allocator_traits<Allocator>::const_pointer;
    using reference              = value_type&;
    using const_reference        = value_type const&;
    using size_type              = size_t;
    using difference_type        = ptrdiff_t;
    using iterator               = pointer;
    using const_iterator         = const_pointer;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    /* --- ITERATORS --- */
    constexpr iterator begin() { return elements; }
    constexpr const_iterator begin() const { return elements; } 
    constexpr const_iterator cbegin() const { return elements; }

    constexpr iterator end() { return elements + current_size; } 
    constexpr const_iterator end() const { return elements + current_size; } 
    constexpr const_iterator cend() const { return elements + current_size; } 

    constexpr reverse_iterator rbegin() { return reverse_iterator(end()); }
    constexpr const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator crbegin() const { return rbegin(); }

    constexpr reverse_iterator rend() { return reverse_iterator(begin()); }
    constexpr const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    constexpr const_reverse_iterator crend() const { return rend(); }

    /* --- GETTERS --- */

    /* Returns true iff this `FixedCapacityVector` contains zero elements. */
    constexpr bool empty() const { return size() == 0; }

    /* Returns the current number of elements stored in this `FixedCapacityVector`. */
    constexpr size_type size() const { return current_size; }

    /* Returns the maximum number of elements this `FixedCapacityVector` could theoretically
    hold. */
    constexpr size_type max_size() const {
        /* The size of a `FixedCapacityVector` can never exceed its fixed capacity */
        return capacity();
    }

    /* Returns the current capacity of this `FixedCapacityVector`. */
    constexpr size_type capacity() const {
        return Capacity;
    }

    /* --- RESIZING METHODS --- */

    /* Resizes this `FixedCapacityVector` to contain exactly `new_size` elements.
    - Does nothing if `new_size` is equal to the current `size()`
    - If the current `size()` is greater than `new_size`, then this `FixedCapacityVector` will be
    reduced to its first `new_size` elements. Note that this will never cause the capacity of this
    `FixedCapacityVector` to be reduced.
    - If the current `size()` is less than `new_size`, then additional default-inserted elements
    will be appended to achieve that `new_size`. */
    constexpr void resize(size_type new_size) {
        assert(new_size <= capacity());

        if (new_size < size()) {
            /* If the current `size()` is greater than `new_size`, then destroy all but the first
            `new_size` elements. */
            for (size_t i = new_size; i < size(); ++i) {
                std::allocator_traits<Allocator>::destroy(
                    allocator,
                    begin() + i
                );
            }
        } else if (new_size > size()) {
            /* If the current `size()` is less than the `new_size`, then append default-inserted
            elements until we have exactly `new_size` elements in total. */
            for (size_t i = size(); i < new_size; ++i) {
                std::allocator_traits<Allocator>::construct(
                    allocator,
                    begin() + i
                );
            }
        }

        /* Finally, update `current_size` */
        current_size = new_size;
    }

    /* --- ELEMENT ACCESS OPERATORS/FUNCTIONS --- */
    constexpr reference operator[] (size_type index) { return elements[index]; }
    constexpr const_reference operator[] (size_type index) const { return elements[index]; }
    constexpr reference at(size_type index) {
        check_if_out_of_bounds(index);
        return elements[index];
    }
    constexpr const_reference at(size_type index) const {
        check_if_out_of_bounds(index);
        return elements[index];
    }
    constexpr reference front() { return *begin(); }
    constexpr const_reference front() const { return *begin(); }
    constexpr reference back() { return *(end() - 1); }
    constexpr const_reference back() const { return *(end() - 1); }

    constexpr T* data() { return (empty() ? nullptr : begin()); }
    constexpr const T* data() const { return (empty() ? nullptr : begin()); }


    /* --- MUTATORS --- */

    /* Appends a new element to the end of this `FixedCapacityVector`. Unlike `push_back`, the
    element will be constructed in-place using the arguments `args` if the `Allocator` for this
    `FixedCapacityVector` is capable of doing so. */
    template <typename... Ts>
    constexpr void emplace_back(Ts&&... args) {
        assert(current_size <= Capacity);
        std::allocator_traits<Allocator>::construct(
            allocator,
            elements + (current_size++),
            std::forward<Ts>(args)...
        );
    }

    /* Appends a copy of the given element `element` to the end of this `FixedCapacityVector`. */
    constexpr void push_back(const T& element) {
        assert(current_size <= Capacity);
        std::allocator_traits<Allocator>::construct(
            allocator,
            elements + (current_size++),
            element
        );
    }

    /* Moves and appends the given element `element` to the end of this `FixedCapacityVector`. */
    constexpr void push_back(T&& element) {
        assert(current_size <= Capacity);
        std::allocator_traits<Allocator>::construct(
            allocator,
            elements + (current_size++),
            std::move(element)
        );
    }

    /* Removes the last element of this `FixedCapacityVector`. */
    constexpr void pop_back() {
        assert(!empty());

        std::allocator_traits<Allocator>::destroy(
            allocator,
            elements + (--current_size)
        );
    }

    /* Erases all elements from the container, after which `size()` will return zero. */
    constexpr void clear() {

        /* Destroy every element, then set `current_size` to 0. */
        auto begin_ptr = begin();
        for (size_t i = 0; i < current_size; ++i) {
            std::allocator_traits<Allocator>::destroy(
                allocator,
                begin_ptr + i
            );
        }

        current_size = 0;
    }

    constexpr void swap(FixedCapacityVector &other) {
        static_assert(false, "Unimplemented, sorry!");
    }

    /* Inserts a copy of `element` immediately before `position`. */
    constexpr iterator insert(const_iterator position, const T& element) {
        assert(size() <= Capacity);

        /* Non-const equivalent of `position`, needed for mutation */
        auto pos = begin() + (position - begin());

        /* First, we make space for the inserted element by shifting all elements after it one to
        the right. To do this, we iterate backwards from `end()` to just after the position of
        insertion, and move-construct each element into the next position. */
        for (auto it = end(); it != pos; --it) {
            /* Move-construct the element at `it` from the element stored at `it - 1`. */
            std::allocator_traits<Allocator>::construct(
                allocator,
                it,
                std::move(*(it - 1))
            );
        }

        /* Finally, copy `element` to the position of insertion. */
        std::allocator_traits<Allocator>::construct(
            allocator,
            pos,
            element
        );

        ++current_size;

        /* Return an iterator to the inserted element, which is just the position of insertion */
        return pos;
    }

    /* Move-constructs an element from `element`, and inserts it immediately before `position`. */
    constexpr iterator insert(const_iterator position, T&& element) {
        /* Identical to the implementation of `insert(const_iterator, const T&)`, except that the
        inserted element is now move-constructed (rather than copy-constructed) from `element`. */

        assert(size() <= Capacity);

        /* Non-const equivalent of `position`, needed for mutation */
        auto pos = begin() + (position - begin());

        for (auto it = end(); it != pos; --it) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                it,
                std::move(*(it - 1))
            );
        }

        std::allocator_traits<Allocator>::construct(
            allocator,
            pos,
            std::move(element)  /* Move-construct the inserted element from the given `element`,
                                   instead of copy-constructing it as before. */
        );

        ++current_size;

        return pos;
    }

    /* Inserts `n` copies of `element` immediately before `position`. */
    constexpr iterator insert(const_iterator position, size_type n, const T& element) {
        /* Identical to the implementation of `insert(const_iterator, const T&)`, except that we
        now have to shift all elements after `position` by `n` to the right rather than by one,
        and that we have to perform `n` copy-constructs of `element` instead of one. */

        assert(size() + n <= Capacity);

        /* Non-const equivalent of `position`, needed for mutation */
        auto pos = begin() + (position - begin());

        /* If `n` is 0 for some reason, we just return the current position. */
        if (n == 0) {
            /* Note that we cannot just `return position`, because `position` is a `const_iterator`
            and not an `iterator`. `return pos`, however, does exactly what we want. */
            return pos;
        }

        /* Make space for the `n` elements to be inserted by shifting all elements after the
        position of insertion `n` to the right. */
        for (auto it = end(); it != pos; --it) {
            /* Move-construct the element at `it + n - 1` from the element stored at `it - 1`. */
            std::allocator_traits<Allocator>::construct(
                allocator,
                it + n - 1,
                std::move(*(it - 1))
            );
        }

        /* Finally, write `n` copies of `element` starting from the position of insertion. */
        for (iterator curr_pos = pos; curr_pos < pos + n; ++curr_pos) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                curr_pos,
                element
            );
        }

        current_size += n;

        /* Return the position of the first element inserted. */
        return pos;
    }

    /* Inserts the elements in the range `[first, last)` immediately before `position`. */
    template<class InputIt>
    /* To avoid ambiguity with `insert(const_iterator, size_type, const T&)`, we check that
    `InputIt` is not an integral type. According to https://tinyurl.com/yzj79ufr, this is
    the minimal check needed to conform to the standard. */
    requires (!std::is_integral_v<InputIt>)
    constexpr iterator insert(const_iterator position, InputIt first, InputIt last) {
        /* Identical to the implementation of `insert(const_iterator, size_type n, const T&)`,
        except that `n` is now given by `std::distance(first, last)`, and that the elements
        to be inserted are now given by dereferencing iterators in the range `[first, last)`. */

        /* Non-const equivalent of `position`, needed for mutation */
        auto pos = begin() + (position - begin());

        if (first == last) {
            /* Note that we cannot just `return position`, because `position` is a `const_iterator`
            and not an `iterator`. `return pos`, however, does exactly what we want. */
            return pos;
        }

        /* Compute `n`, the number of elements we will be inserting */
        auto n = std::distance(first, last);

        assert(size() + n <= Capacity);

        for (auto it = end(); it != pos; --it) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                it + n - 1,
                std::move(*(it - 1))
            );
        }

        /* Finally, write the `n` elements in the range `[first, last)` to the range starting
        from the position of insertion. */
        for (auto curr_pos = pos; first != last; ++curr_pos, ++first) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                curr_pos,
                *first
            );
        }

        current_size += n;

        /* Return the position of the first element inserted. */
        return pos;
    }

    /* Removes the element at `position` from this `FixedCapacityVector`. */
    constexpr iterator erase(const_iterator position) {
        assert(position != end());

        /* `position` is a `const_iterator`, so we cannot use it for the mutation we need to
        perform. Thus, we compute `pos`, which is the non-`const` equivalent of `position`. */
        iterator pos = begin() + (position - begin());

        /* First, we delete the element at `position` by shifting all elements after it one to
        the left. To do this, we iterate forwards from the position of insertion to just before
        `end() - 1`, and move-assign each element to the next element. */
        while (pos != (end() - 1)) {
            /* Move-assign the element at `pos` to the element at `pos + 1`. */
            *pos = std::move(*(pos + 1));
            ++pos;
        }

        /* This leaves us with an extra moved-from element at the position `end() - 1`, which
        we now destroy. */
        std::allocator_traits<Allocator>::destroy(
            allocator,
            pos  /* `pos` will always equal `end() - 1` after the above loop terminates */
        );
        
        --current_size;

        /* Return the iterator to the position immediately following the removed element. This is
        given by the iterator that is the same distance away from `begin()` as the position of the
        original, now-removed element was. */
        return begin() + (position - begin());
    }

    /* Removes the element(s) in the range `[first, last)` from this `FixedCapacityVector`. */
    constexpr iterator erase(const_iterator first, const_iterator last) {
        /* Identical to the implementation of `erase(const_iterator)`, except that we now have to
        shift all elements after `last` by `n` to the left instead of one, where `n` is the number
        of elements in the range `[first, last)`. */

        /* Non-const equivalent of `first`, needed for mutation */
        auto pos = begin() + (first - begin());

        if (first == last) {
            return pos;
        }

        /* Compute `n`, the number of elements we will be erasing. There is no need to use
        `std::distance` here, as both `first` and `last` are assumed to be iterators into
        this `FixedCapacityVector`. */
        auto n = last - first;

        /* Delete the range `[first, last)` by shifting all elements in `[last, end())` `n` to
        the left. */
        while (pos != (end() - n)) {
            /* Move-assign the element at `pos` to the element at `pos + n`. */
            *pos = std::move(*(pos + n));
            ++pos;
        }

        /* This leaves us with `n` extra moved-from elements starting from `end() - n`, which
        we now destroy. */
        while (pos != end()) {
            std::allocator_traits<Allocator>::destroy(
                allocator,
                pos++
            );
        }

        current_size -= n;

        /* Return the iterator to the position immediately following the last removed element. This
        is given by the iterator that is the same distance away from `begin()` as the position of
        the last removed element was. */
        return begin() + (first - begin());
    }

    constexpr FixedCapacityVector(const Allocator &allocator_ = {}) : allocator{allocator_} {}

    /* Constructs a `FixedCapacityVector` with `initial_size` default-inserted instances of `T`. */
    constexpr FixedCapacityVector(size_type initial_size, const Allocator &allocator_ = {})
    : current_size{initial_size},
      allocator{allocator_} {
        for (size_type i = 0; i < current_size; ++i) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                begin() + i
            );
        }
    }

    /* Constructs a `FixedCapacityVector` with `initial_size` copies of `value`. */
    constexpr FixedCapacityVector(
        size_type initial_size, const T &value, const Allocator &allocator_ = {}
    ) : current_size{initial_size},
        allocator{allocator_} {
        for (size_type i = 0; i < current_size; ++i) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                begin() + i,
                value
            );
        }
    }

    /* Constructs a `FixedCapacityVector` with the contents of the range `[first, last)`. */
    template <typename InputIt>
    requires (!std::is_integral_v<InputIt>)
    constexpr FixedCapacityVector(InputIt first, InputIt last, const Allocator &allocator_ = {})
    : current_size(std::distance(first, last)),
      allocator{allocator_} {
        for (size_type i = 0; i < current_size; ++i) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                begin() + i,
                *(first++)
            );
        }
    }

    /* Copy constructor */
    constexpr FixedCapacityVector(
        const FixedCapacityVector &other, const Allocator &allocator_ = {}
    ) : allocator{allocator_} {
        current_size = other.size();
        for (size_type i = 0; i < current_size; ++i) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                begin() + i,
                other[i]
            );
        }
    }

    /* Move constructor */
    constexpr FixedCapacityVector(FixedCapacityVector &&other)
    : current_size{other.current_size},
      allocator{other.allocator}
    {
        /* The responsibility for cleaning up the elements is now transferred to us */
        other.current_size = 0;  /* Prevent double-destructor calls for elements */

        for (size_type i = 0; i < current_size; ++i) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                begin() + i,
                std::move(other[i])
            );
        }
    }

    /* Initializer-list constructor */
    constexpr FixedCapacityVector(std::initializer_list<T> init, const Allocator &allocator_ = {})
    : FixedCapacityVector(init.begin(), init.end(), allocator_)
    {}

    /* Destructor */
    constexpr ~FixedCapacityVector() {
        /* Call the destructor on every element in this `FixedCapacityVector`. */
        clear();
    }

private:
    union {
        /* `elements` stores the at-most `Capacity` elements in this `FixedCapacityVector` on the
        stack. It is kept in an union because doing so prevents it from being automatically
        initialized, which in turn allows the element type `T` to be non-default-constructible. */
        T elements[Capacity];
    };
    /* `current_size` = The current number of elements stored within this `FixedCapacityVector`. */
    size_t current_size = 0;
    /* `allocator` = An instance of type `Allocator`, used to allocate/construct/destroy/deallocate
    elements. */
    Allocator allocator;

    /* Throws `std::out_of_range` if `index` is out of bounds for this `FixedCapacityVector`. */
    constexpr void check_if_out_of_bounds(size_type index) {
        if (index >= size()) {
            throw std::out_of_range(
                std::format(
                    "FixedCapacityVector: index ({}) >= size ({})\n",
                    index, size()
                )
            );
        }
    }
};

/* Specialize `std::formatter` for `FixedCapacityVector<T, Capacity, Allocator>` */
template <typename T, size_t Capacity, typename Allocator>
struct std::formatter<FixedCapacityVector<T, Capacity, Allocator>>
: public std::formatter<std::string>
{
    auto format(
        const FixedCapacityVector<T, Capacity, Allocator> &v,
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