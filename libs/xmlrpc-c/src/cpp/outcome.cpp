#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "xmlrpc-c/base.hpp"

using namespace std;

namespace xmlrpc_c {

rpcOutcome::rpcOutcome() : valid(false) {}

rpcOutcome::rpcOutcome(xmlrpc_c::value const result) :
    valid(true), _succeeded(true), result(result) 
    {}



rpcOutcome::rpcOutcome(xmlrpc_c::fault const fault) :
    valid(true), _succeeded(false), fault(fault)
    {}



bool
rpcOutcome::succeeded() const {
    if (!valid)
        throw(error("Attempt to access rpcOutcome object before setting it"));
    return _succeeded;
}



fault
rpcOutcome::getFault() const {
    
    if (!valid)
        throw(error("Attempt to access rpcOutcome object before setting it"));
    if (_succeeded)
        throw(error("Attempt to get fault description from a non-failure "
                    "RPC outcome"));
    return fault;
}



value
rpcOutcome::getResult() const {
    
    if (!valid)
        throw(error("Attempt to access rpcOutcome object before setting it"));
    if (!_succeeded)
        throw(error("Attempt to get result from an unsuccessful RPC outcome"));
    return result;
}


} // namespace

