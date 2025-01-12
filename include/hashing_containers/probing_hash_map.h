#ifndef PROBING_HASH_MAP_H
#define PROBING_HASH_MAP_H

#include <memory>
#include <utility>
#include <optional>

template <typename K, typename V, typename Hasher = std::hash<K>,
          typename Allocator = std::allocator<std::pair<K, V>>>
class ProbingHashMap {
    enum class State : uint8_t {
        Occupied,
        Empty,
        Deleted
    } *metadata = nullptr;
    std::pair<K, V>* slots = nullptr;
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

            auto starting_index = Hasher{}(slots[i].first) % new_capacity;
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
    
    bool contains(const K &key) const {
        if (curr_capacity == 0) {
            return false;
        }

        for (auto starting_index = Hasher{}(key) % curr_capacity, i = 0; i < curr_capacity; ++i) {
            auto curr_index = (starting_index + i) % curr_capacity;

            if (metadata[curr_index] == State::Empty) {
                return false;
            } else if (metadata[curr_index] == State::Occupied && slots[curr_index].first == key) {
                return true;
            }
        }

        return false;
    }

    auto get_value(const K &key) const -> std::optional<V> {
        if (curr_capacity == 0) {
            return {};
        }

        for (auto starting_index = Hasher{}(key) % curr_capacity, i = 0; i < curr_capacity; ++i) {
            auto curr_index = (starting_index + i) % curr_capacity;

            if (metadata[curr_index] == State::Empty) {
                return {};
            } else if (metadata[curr_index] == State::Occupied && slots[curr_index].first == key) {
                return slots[curr_index].second;
            }
        }

        return {};
    }

    void assign_or_insert(const K &key, const V &value) {
        if (curr_size + 1 > curr_capacity / 2) {
            reserve(std::max((size_t) 2, curr_capacity * 2));
        }

        std::optional<size_t> first_deleted_index;
        for (auto starting_index = Hasher{}(key) % curr_capacity, i = 0; i < curr_capacity; ++i) {
            auto curr_index = (starting_index + i) % curr_capacity;

            if (metadata[curr_index] == State::Empty) {  // Can't be deleted, because otherwise `value` might still exist in the table
                auto insert_index = first_deleted_index.value_or(curr_index);
                std::allocator_traits<Allocator>::construct(
                    allocator,
                    slots + insert_index,
                    key, value
                );
                metadata[insert_index] = State::Occupied;
                ++curr_size;
                return;
            } else if (metadata[curr_index] == State::Occupied && slots[curr_index].first == key) {
                slots[curr_index].second = value;
                return;
            } else if (metadata[curr_index] == State::Deleted && !first_deleted_index) {
                first_deleted_index = curr_index;
            }
        }
    }

    void erase(const K &key) {
        if (curr_capacity == 0) {
            return;
        }

        for (auto starting_index = Hasher{}(key) % curr_capacity, i = 0; i < curr_capacity; ++i) {
            auto curr_index = (starting_index + i) % curr_capacity;

            if (metadata[curr_index] == State::Empty) {
                return;
            } else if (metadata[curr_index] == State::Occupied && slots[curr_index].first == key) {
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

    /* --- CONSTRUCTORS --- */

    ProbingHashMap(const Allocator &allocator_ = {}) : allocator{allocator_} {}

    /* --- NAMED (static) CONSTRUCTORS --- */

    static auto with_capacity(size_t capacity) {
        ProbingHashMap result;
        result.reserve(capacity);
        return result;
    }

    ~ProbingHashMap() {
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