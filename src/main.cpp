#include "vector_variations/fixed_capacity_vector.h"
#include "vector_variations/bounds_checked_vector.h"
#include <iostream>
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
    FixedCapacityVector<NonDefaultConstructibleClass, Capacity> initial_fcv;
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

int main()
{
    test_fcv();
    expect_equal(test_fcv_constant_evaluation(), 4950);
    test_bcv();  /* Will terminate the program if all goes well */

    return 0;
}