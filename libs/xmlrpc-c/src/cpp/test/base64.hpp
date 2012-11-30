#include "tools.hpp"

class base64TestSuite : public testSuite {

public:
    virtual std::string suiteName();
    virtual void runtests(unsigned int const indentation);
};

