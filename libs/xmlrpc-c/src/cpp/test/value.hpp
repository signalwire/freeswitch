#include "tools.hpp"

class valueTestSuite : public testSuite {

public:
    virtual std::string suiteName();
    virtual void runtests(unsigned int const indentation);
};

