#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include "numeric.hpp"

using namespace bellard;

int main() {
    BFContext context(256, 34);
    retain<BFContext> use(&context);

    std::cout << std::setprecision(17);

    std::vector<bracket> nums;
    nums.push_back(bracket(sqrt(2.0)));
    nums.push_back(bracket(sqrt(BF(2))));
    nums.push_back(bracket(sqrt(BFDec(2))));

    std::sort(nums.begin(), nums.end());

    for (const auto &num : nums) {
        std::cout << num.value() << std::endl;
    }
    return 0;
}
