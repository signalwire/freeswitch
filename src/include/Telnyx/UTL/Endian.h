// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#ifndef TELNYX_ENDIAN_H_INCLUDED
#define TELNYX_ENDIAN_H_INCLUDED


#include "Telnyx/Telnyx.h"


namespace Telnyx {

	Int16 TELNYX_API endian_flip_bytes(Int16 value);

	UInt16 TELNYX_API endian_flip_bytes(UInt16 value);
	
  Int32 TELNYX_API endian_flip_bytes(Int32 value);
	
  UInt32 TELNYX_API endian_flip_bytes(UInt32 value);

#if defined(TELNYX_HAVE_INT64)
	Int64 TELNYX_API endian_flip_bytes(Int64 value);
	
  UInt64 TELNYX_API endian_flip_bytes(UInt64 value);
#endif

	Int16 TELNYX_API endian_to_big_endian(Int16 value);
	
  UInt16 TELNYX_API endian_to_big_endian (UInt16 value);
	
  Int32 TELNYX_API endian_to_big_endian(Int32 value);
	
  UInt32 TELNYX_API endian_to_big_endian (UInt32 value);

#if defined(TELNYX_HAVE_INT64)
	Int64 TELNYX_API endian_to_big_endian(Int64 value);
	
  UInt64 TELNYX_API endian_to_big_endian (UInt64 value);
#endif

	Int16 TELNYX_API endian_from_big_endian(Int16 value);
	
  UInt16 TELNYX_API endian_from_big_endian (UInt16 value);
	
  Int32 TELNYX_API endian_from_big_endian(Int32 value);
	
  UInt32 TELNYX_API endian_from_big_endian (UInt32 value);

#if defined(TELNYX_HAVE_INT64)
	Int64 TELNYX_API endian_from_big_endian(Int64 value);
	
  UInt64 TELNYX_API endian_from_big_endian (UInt64 value);
#endif

	Int16 TELNYX_API endian_to_little_endian(Int16 value);
	
  UInt16 TELNYX_API endian_to_little_endian (UInt16 value);
	
  Int32 TELNYX_API endian_to_little_endian(Int32 value);
	
  UInt32 TELNYX_API endian_to_little_endian (UInt32 value);

#if defined(TELNYX_HAVE_INT64)
	Int64 TELNYX_API endian_to_little_endian(Int64 value);
	
  UInt64 TELNYX_API endian_to_little_endian (UInt64 value);
#endif

	Int16 TELNYX_API endian_from_little_endian(Int16 value);
	
  UInt16 TELNYX_API endian_from_little_endian (UInt16 value);
	
  Int32 TELNYX_API endian_from_little_endian(Int32 value);
	
  UInt32 TELNYX_API endian_from_little_endian (UInt32 value);

#if defined(TELNYX_HAVE_INT64)
	Int64 TELNYX_API endian_from_little_endian(Int64 value);
	
  UInt64 TELNYX_API endian_from_little_endian (UInt64 value);
#endif

	Int16 TELNYX_API endian_to_network_order (Int16 value);
	
  UInt16 TELNYX_API endian_to_network_order(UInt16 value);
	
  Int32 TELNYX_API endian_to_network_order(Int32 value);
	
  UInt32 TELNYX_API endian_to_network_order (UInt32 value);
#if defined(TELNYX_HAVE_INT64)
	Int64 TELNYX_API endian_to_network_order(Int64 value);
	
  UInt64 TELNYX_API endian_to_network_order (UInt64 value);
#endif

	Int16 TELNYX_API endian_from_network_order(Int16 value);
	
  UInt16 TELNYX_API endian_from_network_order (UInt16 value);
	
  Int32 TELNYX_API endian_from_network_order(Int32 value);
	
  UInt32 TELNYX_API endian_from_network_order (UInt32 value);

#if defined(TELNYX_HAVE_INT64)
	Int64 TELNYX_API endian_from_network_order(Int64 value);
	
  UInt64 TELNYX_API endian_from_network_order (UInt64 value);
#endif

} // OSS


#endif // TELNYX_ENDIAN_H_INCLUDED


