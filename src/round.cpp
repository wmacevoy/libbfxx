#include <iostream>
#include <iomanip>
#include "bf.hpp"

using namespace bellard;

int main() {
    BFContext context(256, 34);
    retain<BFContext> use(&context);

    std::cout << std::setprecision(17);

    // Binary float: 0.1 is not exactly representable
    BF bf_tenth = BF(1) / BF(10);
    std::cout << "BF  1/10 = " << bf_tenth << std::endl;

    // Decimal float: 0.1 is exact
    BFDec bd_tenth("0.1");
    std::cout << "Dec 0.1  = " << bd_tenth << std::endl;

    // Decimal arithmetic: no binary rounding surprises
    BFDec price("19.99");
    BFDec tax("0.0725");
    BFDec total = price + price * tax;
    std::cout << "Dec $19.99 + 7.25% tax = $" << total << std::endl;

    // Rounding to double via free functions
    bf_rnd(BF_RNDN);
    std::cout << "Dec 0.1 → double RNDN = " << bd_tenth.to_double() << std::endl;
    bf_rnd(BF_RNDD);
    std::cout << "Dec 0.1 → double RNDD = " << bd_tenth.to_double() << std::endl;
    bf_rnd(BF_RNDU);
    std::cout << "Dec 0.1 → double RNDU = " << bd_tenth.to_double() << std::endl;

    // Change precision on the fly
    bf_dprec(50);
    BFDec third = BFDec(1) / BFDec(3);
    std::cout << "Dec 1/3 (50 digits) = " << third << std::endl;

    return 0;
}
