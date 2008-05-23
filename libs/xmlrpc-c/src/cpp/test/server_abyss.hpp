#include "tools.hpp"

class serverAbyssTestSuite : public testSuite {

public:
    virtual std::string suiteName();
    virtual void runtests(unsigned int const);
};
