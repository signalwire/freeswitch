#include "xmlrpc-c/util.h"

#include "env_wrap.hpp"

namespace xmlrpc_c {

env_wrap::env_wrap() {
    xmlrpc_env_init(&this->env_c);
}



env_wrap::~env_wrap() {
    xmlrpc_env_clean(&this->env_c);
}


} // namespace
