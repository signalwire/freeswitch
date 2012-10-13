#include <string>
#include <iostream>
#include <vector>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "xmlrpc-c/base64.hpp"

#include "tools.hpp"

#include "base64.hpp"

using namespace xmlrpc_c;
using namespace std;



string
base64TestSuite::suiteName() {
    return "base64TestSuite";
}



void
base64TestSuite::runtests(unsigned int const) {

    unsigned char const bytes0Data[] = "This is a test";

    vector<unsigned char> bytes0(&bytes0Data[0],
                                 &bytes0Data[sizeof(bytes0Data)]);

    string const base64_0("VGhpcyBpcyBhIHRlc3QA");

    string const expectedBase64_0(base64_0 + "\r\n");

    TEST(base64FromBytes(bytes0) == expectedBase64_0);

    TEST(bytesFromBase64(base64_0) == bytes0);

    unsigned char const bytes1Data[] = {0x80, 0xff};

    vector<unsigned char> bytes1(&bytes1Data[0],
                                 &bytes1Data[sizeof(bytes1Data)]);

    string const base64_1("gP8=");

    string const expectedBase64_1(base64_1 + "\r\n");

    TEST(base64FromBytes(bytes1) == expectedBase64_1);

    TEST(bytesFromBase64(base64_1) == bytes1);

}
