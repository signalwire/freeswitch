
#ifndef STRINGTOARGV_H_INLCUDED
#define STRINGTOARGV_H_INLCUDED

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <string>

#include <cstdlib>
#include <cstring>


void stringToArgcArgv(const std::string& str, int* argc, char*** argv);

#endif
