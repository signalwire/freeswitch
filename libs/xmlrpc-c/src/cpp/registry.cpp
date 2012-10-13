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



method2::method2() {}



method2::~method2() {}


void
method2::execute(xmlrpc_c::paramList const& paramList,
                 xmlrpc_c::value *   const  resultP) {

    callInfo const nullCallInfo;

    execute(paramList, &nullCallInfo, resultP);
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



struct registry_impl {

    xmlrpc_registry * c_registryP;
        // Pointer to the C registry object we use to implement this
        // object.

    std::list<xmlrpc_c::methodPtr> methodList;
        // This is a list of all the method objects (actually, pointers
        // to them).  But since the real registry is the C registry object,
        // all this list is for is to maintain references to the objects
        // to which the C registry points so that they continue to exist.

    xmlrpc_c::defaultMethodPtr defaultMethodP;
        // The real identifier of the default method is the C registry
        // object; this member exists only to maintain a reference to the
        // object to which the C registry points so that it will continue
        // to exist.

    registry_impl();

    ~registry_impl();
};



registry_impl::registry_impl() {

    env_wrap env;

    this->c_registryP = xmlrpc_registry_new(&env.env_c);

    throwIfError(env);
}



registry_impl::~registry_impl() {

    xmlrpc_registry_free(this->c_registryP);
}


registry::registry() {

    this->implP = new registry_impl();
}



registry::~registry(void) {

    delete(this->implP);
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
                void *         const methodPtr,
                void *         const callInfoPtr) {
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

   This function is of type 'xmlrpc_method2'.
-----------------------------------------------------------------------------*/
    method * const methodP(static_cast<method *>(methodPtr));
    paramList const paramList(pListFromXmlrpcArray(paramArrayP));
    callInfo * const callInfoP(static_cast<callInfo *>(callInfoPtr));

    xmlrpc_value * retval;
    retval = NULL; // silence used-before-set warning

    try {
        value result;

        try {
            method2 * const method2P(dynamic_cast<method2 *>(methodP));
            if (method2P)
                method2P->execute(paramList, callInfoP, &result);
            else 
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
    retval = NULL; // silence used-before-set warning

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

    this->implP->methodList.push_back(methodP);

    struct xmlrpc_method_info3 methodInfo;
    env_wrap env;

    methodInfo.methodName      = name.c_str();
    methodInfo.methodFunction  = &c_executeMethod;
    methodInfo.serverInfo      = methodP.get();
    methodInfo.stackSize       = 0;
    string const signatureString(methodP->signature());
    methodInfo.signatureString = signatureString.c_str();
    string const help(methodP->help());
    methodInfo.help            = help.c_str();
    
	xmlrpc_registry_add_method3(&env.env_c, this->implP->c_registryP,
                                &methodInfo);

    throwIfError(env);
}



void
registry::setDefaultMethod(defaultMethodPtr const methodP) {

    this->implP->defaultMethodP = methodP;

    env_wrap env;
    
    xmlrpc_registry_set_default_method(
        &env.env_c, this->implP->c_registryP,
        &c_executeDefaultMethod, (void*) methodP.get());

    throwIfError(env);
}



void
registry::disableIntrospection() {

    xmlrpc_registry_disable_introspection(this->implP->c_registryP);
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

    xmlrpc_registry_set_shutdown(this->implP->c_registryP,
                                 &shutdownServer,
                                 context);
}



void
registry::setDialect(xmlrpc_dialect const dialect) {

    env_wrap env;

    xmlrpc_registry_set_dialect(&env.env_c, this->implP->c_registryP, dialect);

    throwIfError(env);
}



void
registry::processCall(string           const& callXml,
                      const callInfo * const  callInfoP,
                      string *         const  responseXmlP) const {
/*----------------------------------------------------------------------------
   Process an XML-RPC call whose XML is 'callXml'.

   Return the response XML as *responseXmlP.

   If we are unable to execute the call, we throw an error.  But if
   the call executes and the method merely fails in an XML-RPC sense, we
   don't.  In that case, *responseXmlP indicates the failure.
-----------------------------------------------------------------------------*/
    env_wrap env;
    xmlrpc_mem_block * response;

    // For the pure C++ version, this will have to parse 'callXml'
    // into a method name and parameters, look up the method name in
    // the registry, call the method's execute() method, then marshall
    // the result into XML and return it as *responseXmlP.  It will
    // also have to execute system methods (e.g. introspection)
    // itself.  This will be more or less like what
    // xmlrpc_registry_process_call() does.

    xmlrpc_registry_process_call2(
        &env.env_c, this->implP->c_registryP,
        callXml.c_str(), callXml.length(),
        const_cast<callInfo *>(callInfoP),
        &response);

    throwIfError(env);

    *responseXmlP = string(XMLRPC_MEMBLOCK_CONTENTS(char, response),
                           XMLRPC_MEMBLOCK_SIZE(char, response));
    
    xmlrpc_mem_block_free(response);
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
        &env.env_c, this->implP->c_registryP, NULL,
        callXml.c_str(), callXml.length());

    throwIfError(env);

    *responseXmlP = string(XMLRPC_MEMBLOCK_CONTENTS(char, output),
                           XMLRPC_MEMBLOCK_SIZE(char, output));
    
    xmlrpc_mem_block_free(output);
}



#define PROCESS_CALL_STACK_SIZE 256
    // This is our liberal estimate of how much stack space
    // registry::processCall() needs, not counting what
    // the call the to C registry uses.



size_t
registry::maxStackSize() const {

    return xmlrpc_registry_max_stackSize(this->implP->c_registryP) +
        PROCESS_CALL_STACK_SIZE;
}



}  // namespace


registry::shutdown::~shutdown() {}
