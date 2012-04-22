#include "tools.hpp"

class registryTestSuite : public testSuite {

public:
    virtual std::string suiteName();
    virtual void runtests(unsigned int const indentation);
};

