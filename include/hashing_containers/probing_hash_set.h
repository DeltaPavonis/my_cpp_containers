#ifndef PROBING_HASH_SET_H
#define PROBING_HASH_SET_H

#include <memory>

template <typename T, typename Hasher = std::hash<T>, typename Allocator = std::allocator<T>>
class ProbingHashSet {
    enum class State : uint8_t {
        Occupied,
        Empty,
        Deleted
    } *metadata = nullptr;
    T* slots = nullptr;
    size_t curr_size = 0, curr_capacity = 0;
    [[no_unique_address]] Allocator allocator;

public:

    /* --- GETTERS --- */

    auto size() const -> size_t {
        return curr_size;
    }

    auto capacity() const -> size_t {
        return curr_capacity;
    }

    bool is_empty() const {
        return curr_size == 0;
    }

    void clear() {
        for (size_t i = 0; i < curr_capacity; ++i) {
            if (metadata[i] == State::Occupied) {
                std::allocator_traits<Allocator>::destroy(
                    allocator,
                    slots + i
                );
            }
        }

        curr_size = 0;
        // `clear()` leaves the capacity alone
    }

    void reserve(size_t new_capacity) {
        if (capacity() >= new_capacity) {
            return;
        }

        /* Allocate new slot and metadata arrays */
        auto new_slot_array = std::allocator_traits<Allocator>::allocate(allocator, new_capacity);
        auto new_metadata = new State[new_capacity];

        /* Set initial values of `new_metadata` all to `State::empty` */
        for (size_t i = 0; i < new_capacity; ++i) {
            new_metadata[i] = State::Empty;
        }

        /* Move every element from the original slots array to the `new_slot_array` */
        for (size_t i = 0; i < curr_capacity; ++i) {
            if (metadata[i] != State::Occupied) {
                continue;
            }

            auto starting_index = Hasher{}(slots[i]) % new_capacity;
            for (size_t j = 0; j < curr_capacity; ++j) {
                auto curr_index = (starting_index + j) % new_capacity;

                if (new_metadata[curr_index] != State::Occupied) {
                    new_metadata[curr_index] = State::Occupied;

                    std::allocator_traits<Allocator>::construct(
                        allocator,
                        new_slot_array + curr_index,
                        std::move(slots[i])
                    );

                    std::allocator_traits<Allocator>::destroy(
                        allocator,
                        slots + i
                    );

                    break;
                }
            }
        }

        /* Deallocate old metadata and slot arrays */
        if (slots) {
            std::allocator_traits<Allocator>::deallocate(
                allocator,
                slots,
                curr_capacity
            );
        }

        delete[] metadata;

        /* Set `slots` and `metadata` to the newly-allocated arrays and update the capacity */
        slots = new_slot_array;
        metadata = new_metadata;
        curr_capacity = new_capacity;
    }

    bool find(const T &value) const {
        if (curr_capacity == 0) {
            return false;
        }

        for (auto starting_index = Hasher{}(value) % curr_capacity, i = 0; i < curr_capacity; ++i) {
            auto curr_index = (starting_index + i) % curr_capacity;

            if (metadata[curr_index] == State::Empty) {
                return false;
            } else if (metadata[curr_index] == State::Occupied && slots[curr_index] == value) {
                return true;
            }
        }

        return false;
    }

    void insert(const T &value) {
        if (curr_size + 1 > curr_capacity / 2) {
            reserve(std::max((size_t) 2, curr_capacity * 2));
        }

        std::optional<size_t> first_deleted_index;
        for (auto starting_index = Hasher{}(value) % curr_capacity, i = 0; i < curr_capacity; ++i) {
            auto curr_index = (starting_index + i) % curr_capacity;

            if (metadata[curr_index] == State::Empty) { // Only stop on Empty slots because otherwise `value` might still exist in the table
                // Insert in the first deleted or empty slot that we found. Note that if you just always only
                // insert in the empty slot, that causes correctness violations, as the current hash table
                // does not correctly account for deleted slots in the load factor. thus, if every slot is deleted
                // (for example), inserting a value will fail even when it should succeed, as there exist no
                // empty slots at all. Can fix by adding a case after this loop ends. Should be automatically
                // fixed when we correctly account for load factor.
                auto insert_slot = first_deleted_index.value_or(curr_index);
                metadata[insert_slot] = State::Occupied;
                std::allocator_traits<Allocator>::construct(
                    allocator,
                    slots + insert_slot,
                    value
                );
                ++curr_size;
                return;
            } else if (metadata[curr_index] == State::Occupied && slots[curr_index] == value) {
                return;
            } else if (metadata[curr_index] == State::Deleted && !first_deleted_index) {
                first_deleted_index = curr_index;
            }
        }
    }

    void erase(const T &value) {
        if (curr_capacity == 0) {
            return;
        }

        for (auto starting_index = Hasher{}(value) % curr_capacity, i = 0; i < curr_capacity; ++i) {
            auto curr_index = (starting_index + i) % curr_capacity;

            if (metadata[curr_index] == State::Empty) {
                return;
            } else if (metadata[curr_index] == State::Occupied && slots[curr_index] == value) {
                std::allocator_traits<Allocator>::destroy(
                    allocator,
                    slots + curr_index
                );
                metadata[curr_index] = State::Deleted;
                --curr_size;
                return;
            }
        }
    }

    void dump() {
        size_t occ[3] = {};
        for (size_t i = 0; i < curr_capacity; ++i) {
            switch(metadata[i]) {
                case State::Occupied: ++occ[0]; break;
                case State::Empty: ++occ[1]; break;
                case State::Deleted: ++occ[2]; break;
            }
        }
        std::cout << "(O, E, D) = " << occ[0] << " " << occ[1] << " " << occ[2] << ", total: " << curr_capacity << '\n';
    }

    /* --- CONSTRUCTORS --- */

    ProbingHashSet(const Allocator &allocator_ = {}) : allocator{allocator_} {}

    /* --- NAMED (static) CONSTRUCTORS --- */

    static auto with_capacity(size_t capacity) {
        ProbingHashSet result;
        result.reserve(capacity);
        return result;
    }

    ~ProbingHashSet() {
        clear();

        std::allocator_traits<Allocator>::deallocate(
            allocator,
            slots,
            curr_capacity
        );

        delete[] metadata;
    }
};

#endif