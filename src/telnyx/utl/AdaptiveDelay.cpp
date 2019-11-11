// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#include <sys/select.h>
#include <sys/time.h>
#include "Telnyx/UTL/CoreUtils.h"
#include "Telnyx/UTL/AdaptiveDelay.h"


namespace Telnyx {
namespace UTL {


AdaptiveDelay::AdaptiveDelay(unsigned long milliseconds) :
  _lastTick(0),
  _duration(milliseconds * 1000)
{
  gettimeofday(&_bootTime, NULL);
}

AdaptiveDelay::~AdaptiveDelay()
{
}

Telnyx::UInt64 AdaptiveDelay::getTime()
{
  struct timeval sTimeVal, ret;
	gettimeofday( &sTimeVal, NULL );
  timersub(&sTimeVal, &_bootTime, &ret);
  return (Telnyx::UInt64)( (sTimeVal.tv_sec * 1000000) + sTimeVal.tv_usec );
}

void AdaptiveDelay::wait()
{
  unsigned long nextWait = _duration;
  if (!_lastTick)
  {
    _lastTick = getTime();
  }
  else
  {
    Telnyx::UInt64 now = getTime();
    Telnyx::UInt64 accuracy = now - _lastTick;

    _lastTick = now;
    if (accuracy < _duration)
    {
      //
      // Timer fired too early.  compensate by adding more ticks
      //
      nextWait = _duration + (_duration - accuracy);
      _lastTick = _lastTick + (_duration - accuracy);
    }
    else if (accuracy > _duration)
    {
      //
      // Timer fired too late.  compensate by removing some ticks
      //
      if ((accuracy - _duration) < _duration)
      {
        nextWait = _duration - (accuracy - _duration);
        _lastTick = _lastTick - (accuracy - _duration);
      }
      else
      {
        //
        // We are late by more than the duration value.
        // remove the entire duration value + the delta
        //
        nextWait = 0;
        //
        // We will fire now and offset the differce to the next iteration
        //
        _lastTick = _lastTick - ((accuracy - _duration) - _duration);
      }
    }
  }

  if (nextWait)
  {
#if TELNYX_OS == TELNYX_OS_MAC_OS_X
    timeval sTimeout = { (int)(nextWait / 1000000), (int)( nextWait % 1000000 ) };
#else
    timeval sTimeout = { (long int)(nextWait / 1000000), (long int)( nextWait % 1000000 ) };
#endif
    select( 0, 0, 0, 0, &sTimeout );
  }
}


void AdaptiveDelay::reset()
{
  _lastTick = 0;
}



} } // Telnyx::UTL



