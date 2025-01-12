#include "vector_variations/stack_assisted_vector.h"
#include "vector_variations/fixed_capacity_vector.h"
#include "vector_variations/bounds_checked_vector.h"
#include "hashing_containers/probing_hash_set.h"
#include "hashing_containers/probing_hash_map.h"
#include <iostream>
#include <fstream>
#include <format>
#include <algorithm>
#include <random>
#include <unordered_set>
#include <unordered_map>
#include <format>

using namespace std::literals;

constexpr bool SILENCE_NDCC_DEBUG_PRINTS = true;

template <typename T>
void expect_equal(
    const T &actual, const T &expected,
    const std::source_location &sl = std::source_location::current()
) {
    if (actual != expected) {
        std::cerr << std::format(
            "expect_equal failed at {}:{}:{}\n"
            "Expected {}, got {}\n",
            sl.file_name(), sl.line(), sl.column(),
            expected, actual
        );
        std::exit(-1);
    }
}

template <typename... Args>
auto print(std::format_string<Args...> format_str, Args&&... args) {
    if (SILENCE_NDCC_DEBUG_PRINTS) { return; }
    std::cout << std::format(format_str, std::forward<Args>(args)...) << std::flush;
}

/* `NonDefaultConstructibleClass` is, as the name implies, a class that is not
default-constructible. Its construction involves dynamic memory allocation, making it useful
for allowing tools (ASan, valgrind) to detect memory leaks for custom vectors with element type
`NonDefaultConstructibleClass`.*/
struct NonDefaultConstructibleClass {
    int *field;

    /* Delete the default constructor */
    NonDefaultConstructibleClass() = delete;

    NonDefaultConstructibleClass(int field_) : field{new int(field_)} {
        static int counter = 0;
        print("NonDefaultConstructibleClass(int field_ = {}) called (occurrence #{})\n", field_, ++counter);
    }

    NonDefaultConstructibleClass(const NonDefaultConstructibleClass &other) {
        print("NDCC Copy constructor called\n");
        field = new int(*other.field);
    }

    auto& operator= (const NonDefaultConstructibleClass &other) {
        print("NDCC Copy assignment operator called\n");
        delete field;
        field = new int(*other.field);
        return *this;
    }

    auto& operator= (NonDefaultConstructibleClass &&other) {
        print("NDCC Move assignment operator called\n");
        delete field;
        field = other.field;
        other.field = nullptr;
        return *this;
    }

    NonDefaultConstructibleClass(NonDefaultConstructibleClass&& other) {
        print("NDCC Move constructor called\n");
        field = other.field;
        other.field = nullptr;
    }

    ~NonDefaultConstructibleClass() {
        print("NDCC Destructor called for {}\n", (!field ? "(nullptr)"s : std::to_string(*(field))));
        delete field;
    }
};

template <>
struct std::formatter<NonDefaultConstructibleClass> : public std::formatter<std::string> {
    auto format(const NonDefaultConstructibleClass &i, std::format_context &format_context) const {
        if (i.field) {
            return std::format_to(format_context.out(), "{}", *(i.field));
        } else {
            return std::format_to(format_context.out(), "(nullptr)");
        }
    }
};

template <>
struct std::formatter<std::vector<NonDefaultConstructibleClass>> : public std::formatter<std::string> {
    auto format(const std::vector<NonDefaultConstructibleClass> &v, std::format_context &format_context) const {
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

void test_bcv() {
    BoundsCheckedVector<int> v{1, 2, 3};
    BoundsCheckedVector<int> v2(v.begin(), v.end());  // Most recent construction/initialization

    std::cout << std::format("v: {}\nv2: {}\n", v, v2);

    /* Test out-of-bounds access on `v2` */
    v2.push_back(4);  // Now, v2 = {1, 2, 3, 4}
    v2[3];  // In-bounds access; all good
    swap(v, v2);  // Now, v2 = {1, 2, 3}; this is the most recent size change to `v2`
    v2[3];  // Out-of-bounds access; a detailed error will be raised, referencing to the most
            // recent construction/initialization of `v2`, as well as the most recent size change
}

consteval auto test_fcv_constant_evaluation() {
    FixedCapacityVector<int, 100> v;
    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
    }

    int sum = 0;
    for (auto i : v) {
        sum += i;
    }
    return sum;
}

void sav_test_insert_uses_move() {
    StackAssistedVector<NonDefaultConstructibleClass, 4> sav;
    std::cout << "--- 5 push_back's\n";
    for (int i = 0; i < 100; ++i) {
        sav.push_back(i);
    }
    std::cout << std::format("sav: {}\n", sav);

    std::cout << "--- 1 insert\n";
    sav.insert(sav.begin() + 75, 100);
    std::cout << std::format("sav: {}\n", sav);

    std::cout << "--- insert at end\n";
    sav.insert(sav.end(), 101);
    std::cout << std::format("sav: {}\n", sav);
}

void sav_test_move_constructor() {
    /* First test move-constructing from a SAV who has moved to the heap */
    {
        StackAssistedVector<NonDefaultConstructibleClass, 5> sav;
        for (int i = 0; i < 10; ++i) {
            sav.push_back(i);
        }

        StackAssistedVector<NonDefaultConstructibleClass, 5> sav2(std::move(sav));
        std::cout << std::format("{}\n", sav2);
    }

    /* Then, test move-constructing from a SAV that is still on the stack */
    {
        StackAssistedVector<NonDefaultConstructibleClass, 5> sav;
        for (int i = 0; i < 5; ++i) {
            sav.push_back(i);
        }

        StackAssistedVector<NonDefaultConstructibleClass, 5> sav2(std::move(sav));
        std::cout << std::format("{}\n", sav2);
    }
}

void sav_test_iterator_constructor() {
    std::vector<int> nums{1, 2, 3, 4, 5, 6};
    StackAssistedVector<NonDefaultConstructibleClass, 5> sav(nums.begin(), nums.end());
    std::cout << std::format("Iterators constructor: {}\n", sav);
}

void sav_test_initializer_list_constructor() {
    StackAssistedVector<NonDefaultConstructibleClass, 5> sav = {2, 1, 3, 4, 5, 6};
    std::cout << std::format("Initializer list: {}\n", sav);
}

void sav_test_copy_constructor() {
    StackAssistedVector<NonDefaultConstructibleClass, 5> sav = {2, 1, 3, 4, 5, 6};
    StackAssistedVector<NonDefaultConstructibleClass, 5> sav2(sav);
    std::cout << std::format("Copy constructor: {}\n", sav2);
}

bool vectors_equal(auto &sav, auto &vec) {
    return (sav.size() == vec.size()) && 
            std::equal(
                sav.begin(), sav.end(), vec.begin(),
                [](const auto &a, const auto &b) {
                    return (a.field == nullptr && b.field == nullptr) ||
                           (*(a.field) == *(b.field));
                }
            );
}

template <size_t StackCapacity>
void sav_test_erase_with_capacity(bool exceed_stack_capacity) {
    StackAssistedVector<NonDefaultConstructibleClass, StackCapacity> initial_sav;
    for (size_t i = 0; i < (exceed_stack_capacity ? 3 * StackCapacity : StackCapacity); ++i) {
        initial_sav.push_back(i);
    }

    /* Test erasing a single element */
    for (size_t i = 0; i < initial_sav.size(); ++i) {
        std::vector<NonDefaultConstructibleClass> vec(initial_sav.begin(), initial_sav.end());
        auto vec_it = vec.erase(vec.begin() + i);

        StackAssistedVector<NonDefaultConstructibleClass, StackCapacity> curr_sav(initial_sav);
        auto curr_sav_it = curr_sav.erase(curr_sav.begin() + i);

        expect_equal(vectors_equal(curr_sav, vec), true);
        expect_equal(vec_it - vec.begin(), curr_sav_it - curr_sav.begin());
    }

    /* Test erasing a range of iterators */
    for (size_t l = 0; l < initial_sav.size(); ++l) {
        for (size_t r = l; r <= initial_sav.size(); ++r) {  /* Second iterator can be `end()` */
            std::vector<NonDefaultConstructibleClass> vec(initial_sav.begin(), initial_sav.end());
            auto vec_it = vec.erase(vec.begin() + l, vec.begin() + r);

            StackAssistedVector<NonDefaultConstructibleClass, StackCapacity> curr_sav(initial_sav);
            auto curr_sav_it = curr_sav.erase(curr_sav.begin() + l, curr_sav.begin() + r);

            expect_equal(vectors_equal(curr_sav, vec), true);
            expect_equal(vec_it - vec.begin(), curr_sav_it - curr_sav.begin());
        }
    }
}

void sav_test_erase() {
    for (auto b : {false, true}) {
        sav_test_erase_with_capacity<1>(b);
        sav_test_erase_with_capacity<2>(b);
        sav_test_erase_with_capacity<5>(b);
        sav_test_erase_with_capacity<10>(b);
        sav_test_erase_with_capacity<50>(b);
        sav_test_erase_with_capacity<100>(b);
    }
}

template <size_t StackCapacity>
void sav_test_insert_with_capacity(bool exceed_stack_capacity) {
    StackAssistedVector<NonDefaultConstructibleClass, StackCapacity> initial_sav;
    for (size_t i = 0; i < (exceed_stack_capacity ? 3 * StackCapacity : StackCapacity); ++i) {
        initial_sav.push_back(i);
    }

    /* Test inserting a single element */
    for (size_t i = 0; i <= initial_sav.size(); ++i) {  /* can pass end() to `insert` */
        std::vector<NonDefaultConstructibleClass> vec(initial_sav.begin(), initial_sav.end());
        auto vec_it = vec.insert(vec.begin() + i, -1);

        StackAssistedVector<NonDefaultConstructibleClass, StackCapacity> curr_sav(initial_sav);
        auto curr_sav_it = curr_sav.insert(curr_sav.begin() + i, -1);

        expect_equal(vectors_equal(curr_sav, vec), true);
        expect_equal(vec_it - vec.begin(), curr_sav_it - curr_sav.begin());
    }

    /* Test inserting `n` copies of a single element */
    for (auto n : std::vector<size_t>{0, 1, 2, 5, StackCapacity, StackCapacity + 1}) {
        for (size_t i = 0; i <= initial_sav.size(); ++i) {
            std::vector<NonDefaultConstructibleClass> vec(initial_sav.begin(), initial_sav.end());
            auto vec_it = vec.insert(vec.begin() + i, n, -1);

            StackAssistedVector<NonDefaultConstructibleClass, StackCapacity> curr_sav(initial_sav);
            auto curr_sav_it = curr_sav.insert(curr_sav.begin() + i, n, -1);

            expect_equal(vectors_equal(curr_sav, vec), true);
            expect_equal(vec_it - vec.begin(), curr_sav_it - curr_sav.begin());
        }
    }

    /* Test inserting a range of iterators */
    for (auto n : {0, 1, 2, 5}) {
        std::vector<int> range_to_be_inserted(n);
        for (int i = 0; i < n; ++i) {
            range_to_be_inserted[i] = -i;
        }

        /* Test inserting a range of iterators */
        for (size_t l = 0; l < range_to_be_inserted.size(); ++l) {
            for (size_t r = l; r <= range_to_be_inserted.size(); ++r) {  /* Second iterator can be `end()` */
                for (size_t i = 0; i <= initial_sav.size(); ++i) {
                    /* Try inserting `range_to_be_inserted[l..r)` before `sav.begin() + i` */
                    std::vector<NonDefaultConstructibleClass> vec(initial_sav.begin(), initial_sav.end());
                    auto vec_it = vec.insert(
                        vec.begin() + i,
                        range_to_be_inserted.begin() + l, range_to_be_inserted.begin() + r
                    );

                    StackAssistedVector<NonDefaultConstructibleClass, StackCapacity> curr_sav(initial_sav);
                    auto curr_sav_it = curr_sav.insert(
                        curr_sav.begin() + i,
                        range_to_be_inserted.begin() + l, range_to_be_inserted.begin() + r
                    );

                    expect_equal(vectors_equal(curr_sav, vec), true);
                    expect_equal(vec_it - vec.begin(), curr_sav_it - curr_sav.begin());
                }
            }
        }
    }
}

void sav_test_insert() {
    for (auto b : {false, true}) {
        sav_test_insert_with_capacity<1>(b);
        sav_test_insert_with_capacity<2>(b);
        sav_test_insert_with_capacity<5>(b);
        sav_test_insert_with_capacity<10>(b);
        sav_test_insert_with_capacity<50>(b);
        sav_test_insert_with_capacity<100>(b);
    }
}

void test_sav() {
    std::cout << "Testing SAV... " << std::flush;
    sav_test_insert();
    sav_test_erase();
    sav_test_move_constructor();
    sav_test_initializer_list_constructor();
    sav_test_iterator_constructor();
    sav_test_copy_constructor();
    std::cout << "Success" << std::endl;
}

template <size_t Capacity>
void fcv_test_insert_with_capacity() {
    FixedCapacityVector<NonDefaultConstructibleClass, Capacity> initial_fcv;
    for (size_t i = 0; i < Capacity / 2; ++i) {
        initial_fcv.push_back(i);
    }

    if (initial_fcv.size() + 1 <= initial_fcv.capacity()) {
        /* Test inserting a single element */
        for (size_t i = 0; i <= initial_fcv.size(); ++i) {  /* can pass end() to `insert` */
            std::vector<NonDefaultConstructibleClass> vec(initial_fcv.begin(), initial_fcv.end());
            auto vec_it = vec.insert(vec.begin() + i, -1);

            auto curr_fcv(initial_fcv);
            auto curr_fcv_it = curr_fcv.insert(curr_fcv.begin() + i, -1);

            expect_equal(vectors_equal(curr_fcv, vec), true);
            expect_equal(vec_it - vec.begin(), curr_fcv_it - curr_fcv.begin());
        }
    }

    /* Test inserting `n` copies of a single element */
    for (auto n : std::vector<size_t>{0, 1, 2, 5, Capacity, Capacity + 1}) {
        if (initial_fcv.size() + n > initial_fcv.capacity()) { continue; }
        for (size_t i = 0; i <= initial_fcv.size(); ++i) {
            std::vector<NonDefaultConstructibleClass> vec(initial_fcv.begin(), initial_fcv.end());
            auto vec_it = vec.insert(vec.begin() + i, n, -1);

            auto curr_fcv(initial_fcv);
            auto curr_fcv_it = curr_fcv.insert(curr_fcv.begin() + i, n, -1);

            expect_equal(vectors_equal(curr_fcv, vec), true);
            expect_equal(vec_it - vec.begin(), curr_fcv_it - curr_fcv.begin());
        }
    }

    /* Test inserting a range of iterators */
    for (auto n : {0, 1, 2, 5}) {
        if (initial_fcv.size() + n > initial_fcv.capacity()) { continue; }

        std::vector<int> range_to_be_inserted(n);
        for (int i = 0; i < n; ++i) {
            range_to_be_inserted[i] = -i;
        }

        /* Test inserting a range of iterators */
        for (size_t l = 0; l < range_to_be_inserted.size(); ++l) {
            for (size_t r = l; r <= range_to_be_inserted.size(); ++r) {  /* Second iterator can be `end()` */
                for (size_t i = 0; i <= initial_fcv.size(); ++i) {
                    /* Try inserting `range_to_be_inserted[l..r)` before `sav.begin() + i` */
                    std::vector<NonDefaultConstructibleClass> vec(initial_fcv.begin(), initial_fcv.end());
                    auto vec_it = vec.insert(
                        vec.begin() + i,
                        range_to_be_inserted.begin() + l, range_to_be_inserted.begin() + r
                    );

                    auto curr_fcv(initial_fcv);
                    auto curr_fcv_it = curr_fcv.insert(
                        curr_fcv.begin() + i,
                        range_to_be_inserted.begin() + l, range_to_be_inserted.begin() + r
                    );

                    expect_equal(vectors_equal(curr_fcv, vec), true);
                    expect_equal(vec_it - vec.begin(), curr_fcv_it - curr_fcv.begin());
                }
            }
        }
    }
}

void fcv_test_insert() {
    fcv_test_insert_with_capacity<1>();
    fcv_test_insert_with_capacity<2>();
    fcv_test_insert_with_capacity<5>();
    fcv_test_insert_with_capacity<10>();
    fcv_test_insert_with_capacity<50>();
    fcv_test_insert_with_capacity<100>();
}

template <size_t Capacity>
void fcv_test_erase_with_capacity() {
    StackAssistedVector<NonDefaultConstructibleClass, Capacity> initial_fcv;
    for (size_t i = 0; i < Capacity / 2; ++i) {
        initial_fcv.push_back(i);
    }

    if (initial_fcv.size() > 0) {
        /* Test erasing a single element */
        for (size_t i = 0; i < initial_fcv.size(); ++i) {
            std::vector<NonDefaultConstructibleClass> vec(initial_fcv.begin(), initial_fcv.end());
            auto vec_it = vec.erase(vec.begin() + i);
            
            auto curr_fcv(initial_fcv);
            auto curr_fcv_it = curr_fcv.erase(curr_fcv.begin() + i);

            expect_equal(vectors_equal(curr_fcv, vec), true);
            expect_equal(vec_it - vec.begin(), curr_fcv_it - curr_fcv.begin());
        }
    }

    /* Test erasing a range of iterators */
    for (size_t l = 0; l < initial_fcv.size(); ++l) {
        for (size_t r = l; r <= initial_fcv.size(); ++r) {  /* Second iterator can be `end()` */
            std::vector<NonDefaultConstructibleClass> vec(initial_fcv.begin(), initial_fcv.end());
            auto vec_it = vec.erase(vec.begin() + l, vec.begin() + r);
            
            auto curr_fcv(initial_fcv);
            auto curr_fcv_it = curr_fcv.erase(curr_fcv.begin() + l, curr_fcv.begin() + r);

            expect_equal(vectors_equal(curr_fcv, vec), true);
            expect_equal(vec_it - vec.begin(), curr_fcv_it - curr_fcv.begin());
        }
    }
}

void fcv_test_erase() {
    fcv_test_erase_with_capacity<1>();
    fcv_test_erase_with_capacity<2>();
    fcv_test_erase_with_capacity<5>();
    fcv_test_erase_with_capacity<10>();
    fcv_test_erase_with_capacity<50>();
    fcv_test_erase_with_capacity<100>();
}

void test_fcv() {
    std::cout << "Testing FCV... " << std::flush;
    fcv_test_insert();
    fcv_test_erase();
    std::cout << "Success" << std::endl;
}

struct S {
    auto& operator=(const S&) {std::cout << "S copy assignment operator" << std::endl; return *this;}
    auto& operator=(S&&) {std::cout << "S move assignment operator" << std::endl; return *this;}
    S(const S&) {std::cout << "S copy constructor" << std::endl;}
    S(S&&) {std::cout << "S move constructor" << std::endl;}
    S() {std::cout << "S default constructor" << std::endl;}
    ~S() {std::cout << "S destructor" << std::endl;}
};

template <typename T, typename Hasher>
void expect_eq(const ProbingHashSet<T, Hasher> &H, const std::unordered_set<T, Hasher> &S) {
    if (H.size() != S.size()) {
        std::cout << "hashset sizes differ: mine is " << H.size() << ", theirs is " << S.size() << '\n';
    }
    for (auto i : S) {
        expect_equal(H.find(i), true);
    }
}

template <typename K, typename V, typename Hasher>
void expect_eq(const ProbingHashMap<K, V, Hasher> &H, const std::unordered_map<K, V, Hasher> &M) {
    expect_equal(H.size(), M.size());
    for (auto [k, v] : M) {
        expect_equal(H.contains(k), true);
        auto val = H.get_value(k);
        expect_equal(val.has_value(), true);
        expect_equal(*val, v);
    }
}

template <typename T>
struct CollidingHasher {
    uint64_t operator() (const T &value) const {
        return 12;
    }
};

void test_hashset() {
    std::mt19937 generator{std::random_device{}()};
    std::uniform_int_distribution op_dist(0, 2), num_dist(0, 0);

    struct Op {
        int type;
        int value;
    };

    constexpr int NUM_OPERATIONS = 10000;
    std::vector<Op> operations(NUM_OPERATIONS);
    for (auto &[type, value] : operations) {
        type = op_dist(generator);
        value = num_dist(generator);
    }

    operations = {
        {2, 0}, {0, 0},
        {2, 0}, {0, 0},
        {2, 0}, {0, 0}
    };

    std::unordered_set<double, CollidingHasher<double>> S;
    ProbingHashSet<double, CollidingHasher<double>> H;
    for (int op_num = 0; const auto &[type, value] : operations) {
        std::cout << std::format("--- Operation {} {}\n", type, value);
        switch(type) {
            case 0:
                S.insert(value);
                H.insert(value);
                break;
            default:
                S.erase(value);
                H.erase(value);
                break;
        }

        H.dump();

        expect_eq(H, S);
        ++op_num;
    }
}

template <typename T, typename Hasher = std::hash<T>, typename Allocator = std::allocator<T>>
struct TestVec {
    T* ptr;
    [[no_unique_address]] Hasher h;
    [[no_unique_address]] Allocator a;
};

struct EmptyType {};

template <typename T1, typename T2>
struct CP {
    [[no_unique_address]] T1 first;
    [[no_unique_address]] T2 second;
};

int main()
{
    sizeof(std::pair<EmptyType, double>);
    sizeof(CP<EmptyType, EmptyType>);

    sizeof(TestVec<int>);

    test_hashset();

    ProbingHashMap<std::string, int> H;
    H.assign_or_insert("hello!", 1);
    H.assign_or_insert("hi!", 2);
    expect_equal(H.get_value("hello!").value_or(-1), 1);
    expect_equal(H.get_value("hi!").value_or(-1), 2);
    expect_equal(H.size(), (size_t) 2);
    H.erase("hi!");
    expect_equal(H.get_value("hi!").has_value(), false);

    // using Allocator = std::allocator<S>;
    // using AllocatorTraits = std::allocator_traits<Allocator>;
    // auto allocator = Allocator{};
    
    // S *array = AllocatorTraits::allocate(allocator, 5);
    // AllocatorTraits::construct(allocator, array + 1);
    return 0;

    test_fcv();
    test_sav();
    expect_equal(test_fcv_constant_evaluation(), 4950);
    test_bcv();  /* Will terminate the program if all goes well */

    return 0;
}