#include "tools.hpp"

class xmlTestSuite : public testSuite {

public:
    virtual std::string suiteName();
    virtual void runtests(unsigned int const indentation);
};

