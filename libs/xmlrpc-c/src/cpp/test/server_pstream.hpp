#include "tools.hpp"

class serverPstreamTestSuite : public testSuite {

public:
    virtual std::string suiteName();
    virtual void runtests(unsigned int const);
};
