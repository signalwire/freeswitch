#ifndef GIRMEM_HPP_INCLUDED
#define GIRMEM_HPP_INCLUDED

#include <xmlrpc-c/config.h>
#include <xmlrpc-c/c_util.h>

/* The following pthread crap mirrors what is in pthreadx.h, which is
   what girmem.cpp uses to declare the lock interface.  We can't simply
   include pthreadx.h here, because it's an internal Xmlrpc-c header file,
   and this is an external one.

   This is a stopgap measure until we do something cleaner, such as expose
   pthreadx.h as an external interface (class girlock, maybe?) or create
   a lock class with virtual methods.

   The problem we're solving is that class autoObject contains 
   a pthread_mutex_t member, and on Windows, there's no such type.
*/
   
#if XMLRPC_HAVE_PTHREAD
#  include <pthread.h>
   typedef pthread_mutex_t girmem_lock;
#else
#  include <windows.h>
   typedef CRITICAL_SECTION girmem_lock;
#endif

namespace girmem {

class XMLRPC_DLLEXPORT autoObjectPtr;

class XMLRPC_DLLEXPORT autoObject {
    friend class autoObjectPtr;

public:
    void incref();
    void decref(bool * const unreferencedP);
    
protected:
    autoObject();
    virtual ~autoObject();

private:
    girmem_lock refcountLock;
    unsigned int refcount;
};

class XMLRPC_DLLEXPORT autoObjectPtr {
public:
    autoObjectPtr();
    autoObjectPtr(girmem::autoObject * objectP);
    autoObjectPtr(girmem::autoObjectPtr const& autoObjectPtr);
    
    ~autoObjectPtr();
    
    void
    point(girmem::autoObject * const objectP);

    void
    unpoint();

    autoObjectPtr
    operator=(girmem::autoObjectPtr const& objectPtr);
    
    girmem::autoObject *
    operator->() const;
    
    girmem::autoObject *
    get() const;

protected:
    girmem::autoObject * objectP;
};

} // namespace

#endif
