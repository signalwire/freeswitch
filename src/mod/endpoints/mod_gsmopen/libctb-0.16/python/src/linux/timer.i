%{
#include "ctb-0.16/linux/timer.h"
%}

%include cpointer.i

// lets create new fuctions for pointer handling in python (for int *exitflag)
%pointer_functions(int, intp);

namespace ctb {

// perhaps we doesn''t need timer_control to export
// but we need if we want to inherit from timer in python
struct timer_control
{
    unsigned int usecs;
    int *exitflag;
    void* (*exitfnc)(void*);
};

class Timer
{
protected:
    timer_control control;
    int stopped;
    pthread_t tid;
    unsigned int timer_secs;
public:
    Timer(unsigned int msec,int* exitflag,void*(*exitfnc)(void*)=NULL);
    ~Timer();
    int start();
    int stop();
};

void sleepms(unsigned int ms);

};
