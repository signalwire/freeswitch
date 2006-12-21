#include <string>
#include <memory>
#include <algorithm>

#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "xmlrpc-c/girmem.hpp"
using girmem::autoObject;
using girmem::autoObjectPtr;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base.hpp"
#include "xmlrpc-c/registry.hpp"

using namespace std;
using namespace xmlrpc_c;

namespace xmlrpc_c {


method::method() : 
        _signature("?"),
        _help("No help is available for this method")
        {};



method *
method::self() {

    return this;
}



method::~method() {}



methodPtr::methodPtr(method * const methodP) {
    this->instantiate(methodP);
}



method *
methodPtr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<method *>(p);
}



defaultMethod::~defaultMethod() {}



defaultMethodPtr::defaultMethodPtr() {}


defaultMethodPtr::defaultMethodPtr(defaultMethod * const methodP) {
    this->instantiate(methodP);
}



defaultMethod *
defaultMethodPtr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<defaultMethod *>(p);
}



defaultMethod *
defaultMethod::self() {

    return this;
}



registry::registry() {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    this->c_registryP = xmlrpc_registry_new(&env);

    if (env.fault_occurred)
        throw(error(env.fault_string));
}



registry::~registry(void) {

    xmlrpc_registry_free(this->c_registryP);
}



static xmlrpc_c::paramList
pListFromXmlrpcArray(xmlrpc_value * const arrayP) {
/*----------------------------------------------------------------------------
   Convert an XML-RPC array in C (not C++) form to a parameter list object
   that can be passed to a method execute method.

   This is glue code to allow us to hook up C++ Xmlrpc-c code to 
   C Xmlrpc-c code.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;

    xmlrpc_env_init(&env);

    XMLRPC_ASSERT_ARRAY_OK(arrayP);

    unsigned int const arraySize = xmlrpc_array_size(&env, arrayP);

    xmlrpc_c::paramList paramList(arraySize);
    
    for (unsigned int i = 0; i < arraySize; ++i) {
        xmlrpc_value * arrayItemP;
        xmlrpc_array_read_item(&env, arrayP, i, &arrayItemP);

        paramList.add(xmlrpc_c::value(arrayItemP));
        
        xmlrpc_DECREF(arrayItemP);
    }
    return paramList;
}



static xmlrpc_value *
c_executeMethod(xmlrpc_env *   const envP,
                xmlrpc_value * const paramArrayP,
                void *         const methodPtr) {
/*----------------------------------------------------------------------------
   This is a function designed to be called via a C registry to
   execute an XML-RPC method, but use a C++ method object to do the
   work.  You register this function as the method function and a
   pointer to the C++ method object as the method data in the C
   registry.

   If we had a pure C++ registry, this would be unnecessary.

   Since we can't throw an error back to the C code, we catch anything
   the XML-RPC method's execute() method throws, and any error we
   encounter in processing the result it returns, and turn it into an
   XML-RPC method failure.  This will cause a leak if the execute()
   method actually created a result, since it will not get destroyed.
-----------------------------------------------------------------------------*/
    xmlrpc_c::method * const methodP = 
        static_cast<xmlrpc_c::method *>(methodPtr);
    xmlrpc_c::paramList const paramList(pListFromXmlrpcArray(paramArrayP));

    xmlrpc_value * retval;

    try {
        xmlrpc_c::value result;

        try {
            methodP->execute(paramList, &result);
        } catch (xmlrpc_c::fault caughtFault) {
            xmlrpc_env_set_fault(envP, caughtFault.getCode(), 
                                 caughtFault.getDescription().c_str()); 
        } catch (girerr::error caughtError) {
            xmlrpc_env_set_fault(envP, 0, caughtError.what());
        }
        if (envP->fault_occurred)
            retval = NULL;
        else
            retval = result.cValue();
    } catch (...) {
        xmlrpc_env_set_fault(envP, XMLRPC_INTERNAL_ERROR,
                             "Unexpected error executing the code for this "
                             "particular method, detected by the Xmlrpc-c "
                             "method registry code.  The method did not "
                             "fail; rather, it did not complete at all.");
        retval = NULL;
    }
    return retval;
}
 


static xmlrpc_value *
c_executeDefaultMethod(xmlrpc_env *   const envP,
                       const char *   const , // host
                       const char *   const methodName,
                       xmlrpc_value * const paramArrayP,
                       void *         const methodPtr) {
/*----------------------------------------------------------------------------
   This is a function designed to be called via a C registry to
   execute an XML-RPC method, but use a C++ method object to do the
   work.  You register this function as the default method function and a
   pointer to the C++ default method object as the method data in the C
   registry.

   If we had a pure C++ registry, this would be unnecessary.

   Since we can't throw an error back to the C code, we catch anything
   the XML-RPC method's execute() method throws, and any error we
   encounter in processing the result it returns, and turn it into an
   XML-RPC method failure.  This will cause a leak if the execute()
   method actually created a result, since it will not get destroyed.
-----------------------------------------------------------------------------*/
    xmlrpc_c::defaultMethod * const methodP = 
        static_cast<xmlrpc_c::defaultMethod *>(methodPtr);
    xmlrpc_c::paramList const paramList(pListFromXmlrpcArray(paramArrayP));

    xmlrpc_value * retval;

    try {
        xmlrpc_c::value result;

        try {
            methodP->execute(methodName, paramList, &result);
        } catch (xmlrpc_c::fault caughtFault) {
            xmlrpc_env_set_fault(envP, caughtFault.getCode(), 
                                 caughtFault.getDescription().c_str()); 
        } catch (girerr::error caughtError) {
            xmlrpc_env_set_fault(envP, 0, caughtError.what());
        }
        if (envP->fault_occurred)
            retval = NULL;
        else
            retval = result.cValue();
    } catch (...) {
        xmlrpc_env_set_fault(envP, XMLRPC_INTERNAL_ERROR,
                             "Unexpected error executing the default "
                             "method code, detected by the Xmlrpc-c "
                             "method registry code.  The method did not "
                             "fail; rather, it did not complete at all.");
        retval = NULL;
    }
    return retval;
}
 


void
registry::addMethod(string              const name,
                    xmlrpc_c::methodPtr const methodPtr) {


    this->methodList.push_back(methodPtr);

    xmlrpc_env env;
    
    xmlrpc_env_init(&env);

    xmlrpc_c::method * const methodP(methodPtr->self());

	xmlrpc_registry_add_method_w_doc(
        &env, this->c_registryP, NULL,
        name.c_str(), &c_executeMethod, 
        (void*) methodP, 
        methodP->signature().c_str(), methodP->help().c_str());

    if (env.fault_occurred)
        throw(error(env.fault_string));
}



void
registry::setDefaultMethod(defaultMethodPtr const methodPtr) {

    xmlrpc_env env;
    
    xmlrpc_env_init(&env);

    this->defaultMethodP = methodPtr;

    xmlrpc_c::defaultMethod * const methodP(methodPtr->self());

    xmlrpc_registry_set_default_method(
        &env, this->c_registryP, &c_executeDefaultMethod, (void*) methodP);

    if (env.fault_occurred)
        throw(error(env.fault_string));
}



void
registry::disableIntrospection() {

    xmlrpc_registry_disable_introspection(this->c_registryP);
}



void
registry::processCall(string   const& callXml,
                      string * const  responseXmlP) const {
/*----------------------------------------------------------------------------
   Process an XML-RPC call whose XML is 'callXml'.

   Return the response XML as *responseXmlP.

   If we are unable to execute the call, we throw an error.  But if
   the call executes and the method merely fails in an XML-RPC sense, we
   don't.  In that case, *responseXmlP indicates the failure.
-----------------------------------------------------------------------------*/
    xmlrpc_env env;
    xmlrpc_mem_block * output;

    xmlrpc_env_init(&env);

    // For the pure C++ version, this will have to parse 'callXml'
    // into a method name and parameters, look up the method name in
    // the registry, call the method's execute() method, then marshall
    // the result into XML and return it as *responseXmlP.  It will
    // also have to execute system methods (e.g. introspection)
    // itself.  This will be more or less like what
    // xmlrpc_registry_process_call() does.

    output = xmlrpc_registry_process_call(
        &env, this->c_registryP, NULL, callXml.c_str(), callXml.length());

    if (env.fault_occurred)
        throw(error(env.fault_string));

    *responseXmlP = string(XMLRPC_MEMBLOCK_CONTENTS(char, output),
                           XMLRPC_MEMBLOCK_SIZE(char, output));
    
    xmlrpc_mem_block_free(output);
}

xmlrpc_registry *
registry::c_registry() const {

    return this->c_registryP;
}

}  // namespace
