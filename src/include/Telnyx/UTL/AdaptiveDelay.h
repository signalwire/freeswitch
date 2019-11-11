// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#ifndef TELNYX_ADAPTIVEDELAY_H_INCLUDED
#define	TELNYX_ADAPTIVEDELAY_H_INCLUDED

#include "Telnyx/Telnyx.h"


namespace Telnyx {
namespace UTL {

class AdaptiveDelay
  /// This class implements an adaptive recurring syncrhonous timer.
  /// It monitors the delta between waits and adjusts the wait time
  /// to compensate for the skew in between waits.  This guaranties
  /// that the timer stays within range of the real total wait time
  /// even if it is running for a very long time.
{
public:
  AdaptiveDelay(unsigned long milliseconds);
  /// Creates an adaptive deley timer
  ~AdaptiveDelay();
  /// Destroyes the timer
  void wait();
  /// Adaptive syncrhonous wait
  void reset();
  /// Reset the timers internals states
  Telnyx::UInt64 getTime();
  /// Return microseconds elapsed since class creation
private:
  struct timeval _bootTime;
  Telnyx::UInt64 _lastTick;
  unsigned long _duration;
};


} } // Telnyx::UTL



#endif	// TELNYX_ADAPTIVEDELAY_H_INCLUDED

