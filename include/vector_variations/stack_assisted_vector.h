/*
@file stack_assisted_vector.h
@brief Defines and implements `StackAssistedVector<T, StackCapacity, Allocator>`, a variation on
`std::vector` that preallocates stack space for the first `StackCapacity` elements.

This file includes the following types:
- `StackAssistedVector<T, StackCapacity, Allocator>`
*/

#ifndef STACK_ASSISTED_VECTOR_H
#define STACK_ASSISTED_VECTOR_H

#include <cstddef>
#include <memory>
#include <limits>
#include <cassert>
#include <format>
#include <string>

/* `StackAssistedVector<T, StackCapacity, Allocator>` is a dynamically-resizable array which
preallocates stack space for exactly `StackCapacity` elements, and which guarantees
zero dynamic memory allocations until that `StackCapacity` is exceeded. */
template <typename T, size_t Capacity, typename Allocator = std::allocator<T>>
struct StackAssistedVector {
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

    constexpr iterator begin() { return (dynamic_array ? dynamic_array : fixed_array); }
    constexpr const_iterator begin() const { return (dynamic_array ? dynamic_array : fixed_array); }
    constexpr const_iterator cbegin() const { return begin(); }

    constexpr iterator end() { return begin() + current_size; }
    constexpr const_iterator end() const { return begin() + current_size; }
    constexpr const_iterator cend() const { return end(); }

    constexpr reverse_iterator rbegin() { return reverse_iterator(end()); }
    constexpr const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    constexpr const_reverse_iterator crbegin() const { return rbegin(); }

    constexpr reverse_iterator rend() { return reverse_iterator(begin()); }
    constexpr const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    constexpr const_reverse_iterator crend() const { return rend(); }


    /* --- GETTERS --- */

    /* Returns true iff this `StackAssistedVector` contains zero elements. */
    constexpr bool empty() const { return size() == 0; }

    /* Returns the current number of elements stored in this `StackAssistedVector`. */
    constexpr size_type size() const { return current_size; }

    /* Returns the maximum number of elements this `StackAssistedVector` could theoretically
    hold. */
    constexpr size_type max_size() const {
        /* `std::distance(begin(), end())` cannot be larger than the maximum `ptrdiff_t` value */
        auto limit_from_ptrdiff_type = static_cast<size_t>(
            std::numeric_limits<difference_type>::max() / sizeof(T)
        );

        /* At the same time, the chosen `Allocator` type comes with its own bound on the number
        of objects of type `T` that can be allocated. */
        auto limit_from_allocator = std::allocator_traits<Allocator>::max_size(allocator);

        /* Take the minimum of those two values */
        return std::min(limit_from_ptrdiff_type, limit_from_allocator);
    }

    /* Returns the current capacity of this `StackAssistedVector`. */
    constexpr size_type capacity() const {
        return (dynamic_array ? current_dynamic_capacity : Capacity);
    }


    /* --- CAPACITY-CHANGING METHODS (resize, reserve, shrink_to_fit) --- */

    /* Resizes this `StackAssistedVector` to contain exactly `new_size` elements.
    - Does nothing if `new_size` is equal to the current `size()`
    - If the current `size()` is greater than `new_size`, then this `StackAssistedVector` will be
    reduced to its first `new_size` elements. Note that this will never cause the capacity of this
    `StackAssistedVector` to be reduced; use `shrink_to_fit()` after this function for that purpose.
    - If the current `size()` is less than `new_size`, then additional default-inserted elements
    will be appended to achieve that `new_size`. */
    constexpr void resize(size_type new_size) {
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
            /* If the current `size()` is less than the `new_size`, then increase the capacity of
            this `StackAssistedVector` to at least `new_size` if necessary... */
            reserve(new_size);

            /* ...then append default-inserted elements until we have exactly `new_size` elements
            in total. */
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

    /* Equivalent to `resize(new_size)`, except that rather than default-inserted elements,
    additional copies of `filler_value` will be appended in the case that `new_size` is greater
    than the current `size()`. */
    constexpr void resize(size_type new_size, const T& filler_value) {
        /* Identical to the implementation of `resize(size_t new_size)`, except for the one
        difference marked below. See the comments there. */
        if (new_size < size()) {
            for (size_t i = new_size + 1; i < size(); ++i) {
                std::allocator_traits<Allocator>::destroy(
                    allocator,
                    begin() + i
                );
            }
        } else if (new_size > size()) {
            reserve(new_size);
            for (size_t i = size(); i < new_size; ++i) {
                std::allocator_traits<Allocator>::construct(
                    allocator,
                    begin() + i,
                    filler_value  /* Append additional copies of `filler_value` instead of default-
                                     inserted values. */
                );
            }
        }

        current_size = new_size;
    }

    /* Increases the capacity of this `StackAssistedVector` to at least `count` elements. */
    constexpr void reserve(size_type new_capacity) {

        /* If the current capacity is already at least the requested capacity, return. */
        if (new_capacity <= capacity()) {
            return;
        }

        /* Otherwise, allocate a new dynamic array of size `n`... */
        auto new_dynamic_array = std::allocator_traits<Allocator>::allocate(
            allocator, new_capacity
        );

        /* ...move the currently-stored elements to that new dynamic array and destroy the
        original elements... */
        move_from_then_destroy_range(begin(), current_size, new_dynamic_array);

        /* ...then deallocate the original dynamic array, if we were using one. */
        deallocate_dynamic_array();

        /* Update `dynamic_array` and `current_dynamic_capacity` */
        dynamic_array = new_dynamic_array;
        current_dynamic_capacity = new_capacity;
    }

    /* Requests the removal of unused capacity; that is, makes a NON-BINDING request to decrease
    the capacity of this `StackAssistedVector` to its `size()`. Here, we provide the guarantee
    that if the currently-stored elements could fit within the `StackCapacity` of this
    `StackAssistedVector` (i.e. if `size()` <= `StackCapacity`), then those elements will be moved
    back onto the stack. */
    constexpr void shrink_to_fit() {
        if (current_size == capacity()) {
            /* Do nothing if the current `size()` and the current `capacity()` are equal. */
            return;
        } else if (current_size <= Capacity) {
            /* If the current size is at most the `StackCapacity` of this `StackAssistedVector`
            and if elements are currently stored on the heap (that is, in `dynamic_array`), then
            we will move those elements back to the `fixed_array` on the stack.
            
            If elements were already stored on the stack, then there is nothing to do; we always
            have to keep `fixed_array` around anyways. */
            if (dynamic_array) {

                /* First, save the value of `current_dynamic_capacity` in `dynamic_capacity`.
                This is needed because it is in an `union` with `fixed_array`, which we write
                to below (thus invalidating `current_dynamic_capacity`, which we need later). */
                auto dynamic_capacity = current_dynamic_capacity;

                /* Now, we move all currently-stored elements from `dynamic_array` back to
                `fixed_array`... */
                move_from_then_destroy_range(dynamic_array, current_size, fixed_array);

                /* ...and then deallocate the dynamic array we were using. Note that we cannot use
                `deallocate_dynamic_array` here, since that uses the `current_dynamic_capacity`
                field, which was invalidated by the writes to `fixed_array` above (as `fixed_array`
                and `current_dynamic_capacity` are members of the same `union`). */
                std::allocator_traits<Allocator>::deallocate(dynamic_array, dynamic_capacity);

                /* All elements are back on the stack, so we set `dynamic_array` to `nullptr`. */
                dynamic_array = nullptr;
            }
        } else {
            /* If the current size is smaller than the current capacity, but not small enough to fit
            on the stack (inside `fixed_array`), then we will allocate a smaller dynamic array on
            the heap and move all the elements there. */

            assert(current_size < capacity());  /* Sanity check */

            /* First, allocate a new dynamic array with size equal to `current_size`... */
            auto new_dynamic_array = std::allocator_traits<Allocator>::allocate(
                allocator, current_size
            );

            /* ...move the currently-stored elements from `dynamic_array` to `new_dynamic_array`
            and destroy the original elements... */
            move_from_then_destroy_range(dynamic_array, current_size, new_dynamic_array);
            
            /* ...then deallocate the original dynamic array. */
            deallocate_dynamic_array();

            /* Update `dynamic_array` and `current_dynamic_capacity` */
            dynamic_array = new_dynamic_array;
            current_dynamic_capacity = current_size;
        }
    }


    /* --- ELEMENT ACCESS OPERATORS/FUNCTIONS --- */

    constexpr reference operator[] (size_type index) { return begin()[index]; }
    constexpr const_reference operator[] (size_type index) const { return begin()[index]; }
    constexpr reference at(size_type index) {
        check_if_out_of_bounds(index);
        return (*this)[index];
    }
    constexpr const_reference at(size_type index) const {
        check_if_out_of_bounds(index);
        return (*this)[index];
    }
    constexpr reference front() { return *begin(); }
    constexpr const_reference front() const { return *begin(); }
    constexpr reference back() { return *(end() - 1); }
    constexpr const_reference back() const { return *(end() - 1); }

    constexpr T* data() { return (empty() ? nullptr : begin()); }
    constexpr const T* data() const { return (empty() ? nullptr : begin()); }


    /* --- MUTATORS --- */

    /* Appends a new element to the end of this `StackAssistedVector`. Unlike `push_back`, the
    element will be constructed in-place using the arguments `args` if the `Allocator` for this
    `StackAssistedVector` is capable of doing so. */
    template <typename... Ts>
    constexpr void emplace_back(Ts&&... args) {

        /* If we have reached the current capacity, then increase the capacity using `reserve`.
        This involves reallocation, and so invalidates all existing iterators to this
        `StackAssistedVector`. By default, we always double the capacity when it is reached. */
        if (current_size == capacity()) {
            reserve(2 * capacity());
        }

        /* Construct the element in-place from `args` at the location one after the current end
        of the array (equivalent to appending it). The arguments `args` are `forward`ed to the
        corresponding constructor for the type `T`. */
        std::allocator_traits<Allocator>::construct(
            allocator,
            begin() + (current_size++),
            std::forward<Ts>(args)...
        );
    }

    /* Appends a copy of the given element `element` to the end of this `StackAssistedVector`. */
    constexpr void push_back(const T &element) {

        /* If we have reached the current capacity, then increase the capacity using `reserve`.
        This involves reallocation, and so invalidates all existing iterators to this
        `StackAssistedVector`. By default, we always double the capacity when it is reached. */
        if (current_size == capacity()) {
            reserve(2 * capacity());
        }

        /* Append a copy of `element` and increment `current_size` */
        std::allocator_traits<Allocator>::construct(
            allocator,
            begin() + (current_size++),
            element  /* Invokes the copy constructor of `T` */
        );
    }

    /* Moves and appends the given element `element` to the end of this `StackAssistedVector`. */
    constexpr void push_back(T&& element) {

        /* If we have reached the current capacity, then increase the capacity using `reserve`.
        This involves reallocation, and so invalidates all existing iterators to this
        `StackAssistedVector`. By default, we always double the capacity when it is reached. */
        if (current_size == capacity()) {
            reserve(2 * capacity());
        }

        /* Move and append `element`, and also increment `current_size` */
        std::allocator_traits<Allocator>::construct(
            allocator,
            begin() + (current_size++),
            std::move(element)  /* Invokes the move constructor of `T` */
        );
    }

    /* Removes the last element of this `StackAssistedVector`. */
    constexpr void pop_back() {
        assert(!empty());

        /* Destroy the last element and decrement `current_size` */
        std::allocator_traits<Allocator>::destroy(
            allocator,
            begin() + (--current_size)
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

    constexpr void swap(StackAssistedVector &other) {
        static_assert(false, "Unimplemented, sorry!");
    }

    /* Inserts a copy of `element` immediately before `position`. */
    constexpr iterator insert(const_iterator position, const T& element) {
        /* `offset` = the distance between `begin()` and `position`. This is needed because after
        the call to `reserve()` below, `begin()` may have changed, meaning that `position` may
        no longer represent a valid iterator in this `StackAssistedVector`. However, we can always
        find the position of insertion as `begin() + offset`. */
        auto offset = position - begin();

        /* If we have reached the current capacity, then increase the capacity using `reserve`.
        This involves reallocation, and so invalidates all existing iterators to this
        `StackAssistedVector`. By default, we always double the capacity when it is reached. */
        if (current_size == capacity()) {
            reserve(2 * size());
        }

        /* First, we make space for the inserted element by shifting all elements after it one to
        the right. To do this, we iterate backwards from `end()` to just after the position of
        insertion, and move-construct each element into the next position. */
        for (auto it = end(), insert_pos = begin() + offset; it != insert_pos; --it) {
            /* Move-construct the element at `it` from the element stored at `it - 1`. */
            std::allocator_traits<Allocator>::construct(
                allocator,
                it,
                std::move(*(it - 1))
            );
        }

        /* Finally, copy `element` to the position of insertion, which is given by
        `begin() + offset`. */
        std::allocator_traits<Allocator>::construct(
            allocator,
            begin() + offset,
            element
        );

        ++current_size;

        /* Return the position of insertion; that is, an iterator to the inserted element */
        return begin() + offset;
    }

    /* Move-constructs an element from `element`, and inserts it immediately before `position`. */
    constexpr iterator insert(const_iterator position, T&& element) {
        /* Identical to the implementation of `insert(const_iterator, const T&)`, except that the
        inserted element is now move-constructed (rather than copy-constructed) from `element`. */

        auto offset = position - begin();

        if (current_size == capacity()) {
            reserve(2 * size());
        }

        for (auto it = end(), insert_pos = begin() + offset; it != insert_pos; --it) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                it,
                std::move(*(it - 1))
            );
        }

        std::allocator_traits<Allocator>::construct(
            allocator,
            begin() + offset,
            std::move(element)  /* Move-construct the inserted element from the given `element`,
                                   instead of copy-constructing it as before. */
        );

        ++current_size;

        return begin() + offset;
    }

    /* Inserts `n` copies of `element` immediately before `position`. */
    constexpr iterator insert(const_iterator position, size_type n, const T& element) {
        /* Identical to the implementation of `insert(const_iterator, const T&)`, except that we
        now have to shift all elements after `position` by `n` to the right rather than by one,
        and that we have to perform `n` copy-constructs of `element` instead of one. */

        auto offset = position - begin();

        /* If `n` is 0 for some reason, we just return the current position. */
        if (n == 0) {
            /* Note that we cannot just `return position`, because `position` is a `const_iterator`
            and not an `iterator`. We can obtain an equivalent `iterator` as `begin() + offset`,
            however. */
            return begin() + offset;
        }

        if (current_size + n > capacity()) {

            /* Always double the capacity */
            auto new_capacity = capacity();
            while (new_capacity < current_size + n) {
                new_capacity *= 2;
            }

            reserve(new_capacity);
        }

        /* Make space for the `n` elements to be inserted by shifting all elements after the
        position of insertion `n` to the right. */
        auto insert_pos = begin() + offset;
        for (auto it = end(); it != insert_pos; --it) {
            /* Move-construct the element at `it + n - 1` from the element stored at `it - 1`. */
            std::allocator_traits<Allocator>::construct(
                allocator,
                it + n - 1,
                std::move(*(it - 1))
            );
        }

        /* Finally, write `n` copies of `element` starting from the position of insertion. */
        for (size_type i = 0; i < n; ++i) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                insert_pos++,
                element
            );
        }

        current_size += n;

        /* Return the position of the first element inserted. */
        return begin() + offset;
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
        auto offset = position - begin();

        if (first == last) {
            /* Note that we cannot just `return position`, because `position` is a `const_iterator`
            and not an `iterator`. We can obtain an equivalent `iterator` as `begin() + offset`,
            however. */
            return begin() + offset;
        }

        /* Compute `n`, the number of elements we will be inserting */
        auto n = std::distance(first, last);

        if (current_size + n > capacity()) {
            auto new_capacity = capacity();
            while (new_capacity < current_size + n) {
                new_capacity *= 2;
            }

            reserve(new_capacity);
        }

        auto insert_pos = begin() + offset;
        for (auto it = end(); it != insert_pos; --it) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                it + n - 1,
                std::move(*(it - 1))
            );
        }

        /* Finally, write the `n` elements in the range `[first, last)` to the range starting
        from the position of insertion. */
        for (size_type i = 0; i < n; ++i) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                insert_pos++,
                *(first++)
            );
        }

        current_size += n;

        /* Return the position of the first element inserted. */
        return begin() + offset;
    }

    /* Removes the element at `position` from this `StackAssistedVector`. */
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

    /* Removes the element(s) in the range `[first, last)` from this `StackAssistedVector`. */
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
        this `StackAssistedVector`. */
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

    /* --- CONSTRUCTORS --- */

    /* Constructs an empty `StackAssistedVector` with the given allocator `allocator_`. */
    constexpr StackAssistedVector(const Allocator &allocator_ = {}) : allocator{allocator_} {}

    /* Constructs a `StackAssistedVector` with `initial_size` default-inserted instances of `T`. */
    constexpr StackAssistedVector(size_type initial_size, const Allocator &allocator_ = {})
    : allocator{allocator_} {
        reserve(initial_size);
        
        current_size = initial_size;
        for (size_type i = 0; i < current_size; ++i) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                begin() + i
            );
        }
    }

    /* Constructs a `StackAssistedVector` with `initial_size` copies of `value`. */
    constexpr StackAssistedVector(
        size_type initial_size, const T &value, const Allocator &allocator_ = {}
    ) : allocator{allocator_} {
        reserve(initial_size);

        current_size = initial_size;
        for (size_type i = 0; i < current_size; ++i) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                begin() + i,
                value
            );
        }
    }

    /* Constructs a `StackAssistedVector` with the contents of the range `[first, last)`. */
    template <typename InputIt>
    requires (!std::is_integral_v<InputIt>)
    constexpr StackAssistedVector(InputIt first, InputIt last, const Allocator &allocator_ = {})
    : allocator{allocator_} {
        size_type initial_size(std::distance(first, last));
        reserve(initial_size);

        current_size = initial_size;
        for (size_type i = 0; i < current_size; ++i) {
            std::allocator_traits<Allocator>::construct(
                allocator,
                begin() + i,
                *(first++)
            );
        }

    }

    /* Copy constructor */
    constexpr StackAssistedVector(
        const StackAssistedVector &other, const Allocator &allocator_ = {}
    ) : allocator{allocator_} {
        reserve(other.size());

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
    constexpr StackAssistedVector(StackAssistedVector &&other)
    : dynamic_array{other.dynamic_array},
      current_size{other.current_size},
      allocator{other.allocator}
    {
        if (dynamic_array) {
            current_dynamic_capacity = other.current_dynamic_capacity;

            /* The responsibility for cleaning up `dynamic_array` is now transferred to us */
            other.current_size = 0;  /* Prevent double-destructor calls for elements */
            other.dynamic_array = nullptr;  /* Prevent double-free of `dynamic_array` */
        } else {
            for (size_type i = 0; i < current_size; ++i) {
                std::allocator_traits<Allocator>::construct(
                    allocator,
                    fixed_array + i,
                    std::move(other.fixed_array[i])
                );
            }
        }
    }

    /* Initializer-list constructor */
    constexpr StackAssistedVector(std::initializer_list<T> init, const Allocator &allocator_ = {})
    : StackAssistedVector(init.begin(), init.end(), allocator_)
    {}

    /* Destructor */
    ~StackAssistedVector() {
        /* Call the destructor on every element in this `StackAssistedVector`... */
        clear();

        /* ...then deallocate the dynamic array itself, if we were using one */
        deallocate_dynamic_array();
    }

private:

    /* If elements are being stored on the heap, then they will reside in `dynamic_array`.
    Otherwise, if elements are being stored on the stack, then `dynamic_array` will be set to
    `nullptr` and the elements will reside in `fixed_array` (defined below). Whether or not
    `dynamic_array` is `nullptr` serves as the discriminator for which of the two union members
    below is the active one. */
    T *dynamic_array = nullptr;

    union {
        /* `fixed_array` stores at most `StackCapacity` elements of type `T` on the stack; if
        elements are being kept on the stack, then they will always reside in `fixed_array`.
        Now, the reason that `fixed_array` is kept in an union (besides having it share the
        same memory as `current_dynamic_capacity` and hence save storage) is because whenever
        something is put inside an `union`, it is not automatically initialized. If `fixed_array`
        was made a regular member of `StackAssistedVector`, then initializing `StackAssistedVector`
        would also initialize `fixed_array`, which would always result in `StackCapacity` calls to
        the default constructor of `T`. Instead, by having `fixed_array` reside within an `union`,
        its elements are not automatically default-constructed; not only does this save the overhead
        of `StackCapacity` calls to `T::T()`, but it also allows for the type `T` to be not
        default-constructible, giving `StackCapacity` an ability that `std::array` does not
        have. Choosing to not initialize the elements of `fixed_array` by default, however,
        means that we must use placement-new and placement-delete to eventually construct the
        elements, in order to abide by C++'s object lifetime rules. */
        T fixed_array[Capacity];

        /* When `dynamic_array` is not `nullptr`, `current_dynamic_capacity` represents the
        current size of `dynamic_array`, which is equivalent to the current capacity we have
        for storing elements on the heap. Storing this in an union along with `fixed_array`
        saves storage. */
        size_type current_dynamic_capacity = 0;
    };

    /* `current_size` = The current number of elements stored within this `StackAssistedVector`. */
    size_type current_size = 0;

    /* `allocator` = An instance of type `Allocator`, used to allocate/construct/destroy/deallocate
    elements. */
    Allocator allocator;

    /* Throws `std::out_of_range` if `index` is out of bounds for this `StackAssistedVector`. */
    constexpr void check_if_out_of_bounds(size_type index) {
        if (index >= size()) {
            throw std::out_of_range(
                std::format(
                    "StackAssistedVector: index ({}) >= size ({})\n",
                    index, size()
                )
            );
        }
    }

    /* Move-constructs exactly `count` elements at `dest` from the `count` elements starting at
    `source`. Afterwards, destroys the original elements in the `source` range. */ 
    constexpr void move_from_then_destroy_range(
        iterator source, size_t count,
        iterator dest
    ) {
        /* Iterate over the elements to be moved */
        for (size_t i = 0; i < count; ++i) {
            /* Move the current element from `source` to `dest`; move `source[i]` to `dest + i` */
            std::allocator_traits<Allocator>::construct(
                allocator,
                dest + i,
                std::move(source[i])
            );

            /* Destroy the moved-from element at `source + i` */
            std::allocator_traits<Allocator>::destroy(
                allocator,
                source + i
            );
        }
    }

    /* Deallocates `dynamic_array` via `std::allocator_traits` if it is not `nullptr`. */
    constexpr void deallocate_dynamic_array() {
        if (dynamic_array) {
            std::allocator_traits<Allocator>::deallocate(
                allocator,
                dynamic_array,
                current_dynamic_capacity  /* = size of `dynamic_array` */
            );
        }
    }
};

/* Specialize `std::formatter` for `StackAssistedVector<T, StackCapacity, Allocator>` */
template <typename T, size_t StackCapacity, typename Allocator>
struct std::formatter<StackAssistedVector<T, StackCapacity, Allocator>>
: public std::formatter<std::string>
{
    auto format(
        const StackAssistedVector<T, StackCapacity, Allocator> &v,
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