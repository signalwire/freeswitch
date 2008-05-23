#include <cassert>
#include <string>
#include <memory>
#include <algorithm>

#include "xmlrpc-c/girerr.hpp"
using girerr::throwf;
#include "xmlrpc-c/girmem.hpp"
using girmem::autoObject;
using girmem::autoObjectPtr;
#include "xmlrpc-c/base.h"
#include "xmlrpc-c/base.hpp"
#include "env_wrap.hpp"

#include "xmlrpc-c/registry.hpp"

using namespace std;
using namespace xmlrpc_c;


namespace {

void
throwIfError(env_wrap const& env) {

    if (env.env_c.fault_occurred)
        throw(girerr::error(env.env_c.fault_string));
}


} // namespace

namespace xmlrpc_c {


method::method() : 
        _signature("?"),
        _help("No help is available for this method")
        {};



method::~method() {}



methodPtr::methodPtr(method * const methodP) {
    this->point(methodP);
}



method *
methodPtr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<method *>(p);
}



defaultMethod::~defaultMethod() {}



defaultMethodPtr::defaultMethodPtr() {}


defaultMethodPtr::defaultMethodPtr(defaultMethod * const methodP) {
    this->point(methodP);
}



defaultMethod *
defaultMethodPtr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<defaultMethod *>(p);
}



defaultMethod *
defaultMethodPtr::get() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<defaultMethod *>(p);
}



registry::registry() {

    env_wrap env;

    this->c_registryP = xmlrpc_registry_new(&env.env_c);

    throwIfError(env);
}



registry::~registry(void) {

    xmlrpc_registry_free(this->c_registryP);
}



registryPtr::registryPtr() {}



registryPtr::registryPtr(registry * const registryP) {
    this->point(registryP);
}



registry *
registryPtr::operator->() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<registry *>(p);
}



registry *
registryPtr::get() const {

    autoObject * const p(this->objectP);
    return dynamic_cast<registry *>(p);
}



static xmlrpc_c::paramList
pListFromXmlrpcArray(xmlrpc_value * const arrayP) {
/*----------------------------------------------------------------------------
   Convert an XML-RPC array in C (not C++) form to a parameter list object
   that can be passed to a method execute method.

   This is glue code to allow us to hook up C++ Xmlrpc-c code to 
   C Xmlrpc-c code.
-----------------------------------------------------------------------------*/
    env_wrap env;

    XMLRPC_ASSERT_ARRAY_OK(arrayP);

    unsigned int const arraySize = xmlrpc_array_size(&env.env_c, arrayP);

    assert(!env.env_c.fault_occurred);

    xmlrpc_c::paramList paramList(arraySize);
    
    for (unsigned int i = 0; i < arraySize; ++i) {
        xmlrpc_value * arrayItemP;

        xmlrpc_array_read_item(&env.env_c, arrayP, i, &arrayItemP);
        assert(!env.env_c.fault_occurred);

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
        } catch (xmlrpc_c::fault const& fault) {
            xmlrpc_env_set_fault(envP, fault.getCode(), 
                                 fault.getDescription().c_str()); 
        }
        if (!envP->fault_occurred) {
            if (result.isInstantiated())
                retval = result.cValue();
            else
                throwf("Xmlrpc-c user's xmlrpc_c::method object's "
                       "'execute method' failed to set the RPC result "
                       "value.");
        }
    } catch (exception const& e) {
        xmlrpc_faultf(envP, "Unexpected error executing code for "
                      "particular method, detected by Xmlrpc-c "
                      "method registry code.  Method did not "
                      "fail; rather, it did not complete at all.  %s",
                      e.what());
    } catch (...) {
        xmlrpc_env_set_fault(envP, XMLRPC_INTERNAL_ERROR,
                             "Unexpected error executing code for "
                             "particular method, detected by Xmlrpc-c "
                             "method registry code.  Method did not "
                             "fail; rather, it did not complete at all.");
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
    defaultMethod * const methodP = 
        static_cast<defaultMethod *>(methodPtr);
    paramList const paramList(pListFromXmlrpcArray(paramArrayP));

    xmlrpc_value * retval;

    try {
        xmlrpc_c::value result;
        
        try {
            methodP->execute(methodName, paramList, &result);
        } catch (xmlrpc_c::fault const& fault) {
            xmlrpc_env_set_fault(envP, fault.getCode(), 
                                 fault.getDescription().c_str()); 
        }
        if (!envP->fault_occurred) {
            if (result.isInstantiated())
                retval = result.cValue();
            else
                throwf("Xmlrpc-c user's xmlrpc_c::defaultMethod object's "
                       "'execute method' failed to set the RPC result "
                       "value.");
        }
    } catch (exception const& e) {
        xmlrpc_faultf(envP, "Unexpected error executing default "
                      "method code, detected by Xmlrpc-c "
                      "method registry code.  Method did not "
                      "fail; rather, it did not complete at all.  %s",
                      e.what());
    } catch (...) {
        xmlrpc_env_set_fault(envP, XMLRPC_INTERNAL_ERROR,
                             "Unexpected error executing default "
                             "method code, detected by Xmlrpc-c "
                             "method registry code.  Method did not "
                             "fail; rather, it did not complete at all.");
    }
    return retval;
}
 


void
registry::addMethod(string    const name,
                    methodPtr const methodP) {

    this->methodList.push_back(methodP);

    env_wrap env;
    
	xmlrpc_registry_add_method_w_doc(
        &env.env_c, this->c_registryP, NULL,
        name.c_str(), &c_executeMethod, 
        (void*) methodP.get(), 
        methodP->signature().c_str(), methodP->help().c_str());

    throwIfError(env);
}



void
registry::setDefaultMethod(defaultMethodPtr const methodP) {

    this->defaultMethodP = methodP;

    env_wrap env;
    
    xmlrpc_registry_set_default_method(
        &env.env_c, this->c_registryP,
        &c_executeDefaultMethod, (void*) methodP.get());

    throwIfError(env);
}



void
registry::disableIntrospection() {

    xmlrpc_registry_disable_introspection(this->c_registryP);
}



static xmlrpc_server_shutdown_fn shutdownServer;

static void
shutdownServer(xmlrpc_env * const envP,
               void *       const context,
               const char * const comment,
               void *       const callInfo) {

    registry::shutdown * const shutdownP(
        static_cast<registry::shutdown *>(context));

    assert(shutdownP != NULL);

    try {
        shutdownP->doit(string(comment), callInfo);
    } catch (exception const& e) {
        xmlrpc_env_set_fault(envP, XMLRPC_INTERNAL_ERROR, e.what());
    }
}



void
registry::setShutdown(const registry::shutdown * const shutdownP) {

    void * const context(const_cast<registry::shutdown *>(shutdownP));

    xmlrpc_registry_set_shutdown(this->c_registryP,
                                 &shutdownServer,
                                 context);
}



void
registry::setDialect(xmlrpc_dialect const dialect) {

    env_wrap env;

    xmlrpc_registry_set_dialect(&env.env_c, this->c_registryP, dialect);

    throwIfError(env);
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
    env_wrap env;
    xmlrpc_mem_block * output;

    // For the pure C++ version, this will have to parse 'callXml'
    // into a method name and parameters, look up the method name in
    // the registry, call the method's execute() method, then marshall
    // the result into XML and return it as *responseXmlP.  It will
    // also have to execute system methods (e.g. introspection)
    // itself.  This will be more or less like what
    // xmlrpc_registry_process_call() does.

    output = xmlrpc_registry_process_call(
        &env.env_c, this->c_registryP, NULL,
        callXml.c_str(), callXml.length());

    throwIfError(env);

    *responseXmlP = string(XMLRPC_MEMBLOCK_CONTENTS(char, output),
                           XMLRPC_MEMBLOCK_SIZE(char, output));
    
    xmlrpc_mem_block_free(output);
}

xmlrpc_registry *
registry::c_registry() const {

    return this->c_registryP;
}

}  // namespace


registry::shutdown::~shutdown() {}
