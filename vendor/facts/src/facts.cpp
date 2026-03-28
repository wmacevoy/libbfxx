// facts.cpp — C++ build: compile this instead of facts.c (never both).
// Includes the full C core (compiled as C++ with extern "C" linkage)
// plus the C++ tracing helpers.

#include "facts.c"

FactsTrace* FactsTrace::top = 0;
Facts* FactsTrace::facts = 0;

FactsTrace::FactsTrace(std::function<void(Facts *_facts, std::ostream&)> _note) : up(top), note(_note) { top = this; }
FactsTrace::~FactsTrace() { top = up; }

void FactsTrace::reset(Facts *_facts) { facts=_facts; }

void FactsTrace::notes() {
    FactsTrace *at = top;
    while (at != 0) {
        at->note(facts,std::cout);
        at=at->up;
    }
}
