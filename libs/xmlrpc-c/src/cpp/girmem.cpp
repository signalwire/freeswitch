#include "pthreadx.h"
#include "xmlrpc-c/girerr.hpp"
using girerr::error;
#include "xmlrpc-c/girmem.hpp"

using namespace std;
using namespace girmem;


namespace girmem {


void
autoObject::incref() {
    pthread_mutex_lock(&this->refcountLock);
    ++this->refcount;
    pthread_mutex_unlock(&this->refcountLock);
}



void
autoObject::decref(bool * const unreferencedP) {

    if (this->refcount == 0)
        throw(error("Decrementing ref count of unreferenced object"));
    pthread_mutex_lock(&this->refcountLock);
    --this->refcount;
    *unreferencedP = (this->refcount == 0);
    pthread_mutex_unlock(&this->refcountLock);
}
 


autoObject::autoObject() {
    int rc;

    rc = pthread_mutex_init(&this->refcountLock, NULL);

    if (rc != 0)
        throw(error("Unable to initialize pthread mutex"));

    this->refcount   = 0;
}



autoObject::~autoObject() {
    if (this->refcount > 0)
        throw(error("Destroying referenced object"));

    int rc;

    rc = pthread_mutex_destroy(&this->refcountLock);

    if (rc != 0)
        throw(error("Unable to destroy pthread mutex"));
}



autoObjectPtr::autoObjectPtr() : objectP(NULL) {}



autoObjectPtr::autoObjectPtr(autoObject * const objectP) {

    // Note: When someone attempts to use this constructor with a null
    // argument, it's normally because a 'new' of the autoObject
    // failed, before calling the autoObject's constructor, thus
    // generating a null pointer.

    // E.g. the following code, where the system is out of memory:
    //
    //    class client    : public autoObject    { ... }
    //    class clientPtr : public autoObjectPtr { ... }
    //    clientPtr clientP(new client);

    if (objectP == NULL)
        throw(error("Object creation failed; trying to create autoObjectPtr "
                    "with a null autoObject pointer"));
        
    this->objectP = objectP;
    objectP->incref();
}



autoObjectPtr::autoObjectPtr(autoObjectPtr const& autoObjectPtr) {
    // copy constructor

    this->objectP = autoObjectPtr.objectP;
    if (this->objectP)
        this->objectP->incref();
}
    
 

autoObjectPtr::~autoObjectPtr() {

    this->unpoint();
}


 
void
autoObjectPtr::point(autoObject * const objectP) {

    if (this->objectP != NULL)
        throw(error("Already pointing"));
    this->objectP = objectP;
    objectP->incref();
}



void
autoObjectPtr::unpoint() {

    if (this->objectP) {
        bool dead;
        this->objectP->decref(&dead);
        if (dead) {
            delete(this->objectP);
            this->objectP = NULL;
        }
    }
}



autoObjectPtr
autoObjectPtr::operator=(autoObjectPtr const& source) {

    // If we're overwriting a variable that already points to something,
    // we have to unpoint it from what it points to now before we can point
    // it to what 'source' points to.  But if the source and destination
    // are the same object, we just want to leave the pointing alone.

    if (this == &source) {
        // Assignment of variable to itself; no-op
    } else {
        this->unpoint();
        this->objectP = source.objectP;
        if (this->objectP)
            this->objectP->incref();
    }
    return *this;
}

   

autoObject *
autoObjectPtr::operator->() const {
    if (this->objectP == NULL)
        throw(error("attempt to dereference autoObjectPtr "
                    "which does not point to anything"));
    return this->objectP;
}



autoObject *
autoObjectPtr::get() const {

    return this->objectP;
}

} // namespace
