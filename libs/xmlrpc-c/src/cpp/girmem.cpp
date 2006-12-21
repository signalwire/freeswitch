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
    if (this->objectP) {
        bool dead;
        this->objectP->decref(&dead);
        if (dead)
            delete(this->objectP);
    }
}


 
void
autoObjectPtr::instantiate(autoObject * const objectP) {

    this->objectP = objectP;
    objectP->incref();
}



autoObjectPtr
autoObjectPtr::operator=(autoObjectPtr const& autoObjectPtr) {

    if (this->objectP != NULL)
        throw(error("Already instantiated"));
    this->objectP = autoObjectPtr.objectP;
    this->objectP->incref();

    return *this;
}

   

autoObject *
autoObjectPtr::operator->() const {
    return this->objectP;
}



} // namespace
