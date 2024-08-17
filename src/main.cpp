#include "vector_variations/bounds_checked_vector.h"
#include <iostream>
#include <format>

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

int main()
{
    test_bcv();  /* Will terminate the program if all goes well */

    return 0;
}