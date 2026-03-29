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

    std::vector<numeric> nums;
    nums.push_back(numeric(sqrt(2.0)));
    nums.push_back(numeric(sqrt(BF(2))));
    nums.push_back(numeric(sqrt(BFDec(2))));
    
    std::sort(nums.begin(), nums.end());

    for (const auto &num : nums) {
        std::cout << num << std::endl;
    }
    return 0;
}
