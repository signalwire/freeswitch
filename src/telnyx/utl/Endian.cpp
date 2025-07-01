// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.


#include "Telnyx/UTL/Endian.h"
#include "Poco/ByteOrder.h"

namespace Telnyx {

using Poco::ByteOrder;

Telnyx::Int16 endian_flip_bytes(Telnyx::Int16 value)
{
  return ByteOrder::flipBytes(value);
}

Telnyx::UInt16 endian_flip_bytes(Telnyx::UInt16 value)
{
  return ByteOrder::flipBytes(value);
}

Telnyx::Int32 endian_flip_bytes(Telnyx::Int32 value)
{
  return ByteOrder::flipBytes(value);
}

Telnyx::UInt32 endian_flip_bytes(Telnyx::UInt32 value)
{
  return ByteOrder::flipBytes(value);
}

#if defined(TELNYX_HAVE_INT64)
Telnyx::Int64 endian_flip_bytes(Telnyx::Int64 value)
{
  return ByteOrder::flipBytes((Poco::Int64)value);
}

Telnyx::UInt64 endian_flip_bytes(Telnyx::UInt64 value)
{
  return ByteOrder::flipBytes((Poco::UInt64)value);
}

#endif

Telnyx::Int16 endian_to_big_endian(Telnyx::Int16 value)
{
  return ByteOrder::toBigEndian(value);
}

Telnyx::UInt16 endian_to_big_endian (Telnyx::UInt16 value)
{
  return ByteOrder::toBigEndian(value);
}

Telnyx::Int32 endian_to_big_endian(Telnyx::Int32 value)
{
  return ByteOrder::toBigEndian(value);
}

Telnyx::UInt32 endian_to_big_endian (Telnyx::UInt32 value)
{
  return ByteOrder::toBigEndian(value);
}

#if defined(TELNYX_HAVE_INT64)
Telnyx::Int64 endian_to_big_endian(Telnyx::Int64 value)
{
  return ByteOrder::toBigEndian((Poco::Int64)value);
}

Telnyx::UInt64 endian_to_big_endian (Telnyx::UInt64 value)
{
  return ByteOrder::toBigEndian((Poco::UInt64)value);
}

#endif

Telnyx::Int16 endian_from_big_endian(Telnyx::Int16 value)
{
  return ByteOrder::fromBigEndian(value);
}

Telnyx::UInt16 endian_from_big_endian (Telnyx::UInt16 value)
{
  return ByteOrder::fromBigEndian(value);
}

Telnyx::Int32 endian_from_big_endian(Telnyx::Int32 value)
{
  return ByteOrder::fromBigEndian(value);
}

Telnyx::UInt32 endian_from_big_endian (Telnyx::UInt32 value)
{
  return ByteOrder::fromBigEndian(value);
}

#if defined(TELNYX_HAVE_INT64)
Telnyx::Int64 endian_from_big_endian(Telnyx::Int64 value)
{
  return ByteOrder::fromBigEndian((Poco::Int64)value);
}

Telnyx::UInt64 endian_from_big_endian (Telnyx::UInt64 value)
{
  return ByteOrder::fromBigEndian((Poco::UInt64)value);
}
#endif

Telnyx::Int16 endian_to_little_endian(Telnyx::Int16 value)
{
  return ByteOrder::toLittleEndian(value);
}

Telnyx::UInt16 endian_to_little_endian (Telnyx::UInt16 value)
{
  return ByteOrder::toLittleEndian(value);
}

Telnyx::Int32 endian_to_little_endian(Telnyx::Int32 value)
{
  return ByteOrder::toLittleEndian(value);
}

Telnyx::UInt32 endian_to_little_endian (Telnyx::UInt32 value)
{
  return ByteOrder::toLittleEndian(value);
}

#if defined(TELNYX_HAVE_INT64)
Telnyx::Int64 endian_to_little_endian(Telnyx::Int64 value)
{
  return ByteOrder::toLittleEndian((Poco::Int64)value);
}

Telnyx::UInt64 endian_to_little_endian (Telnyx::UInt64 value)
{
  return ByteOrder::toLittleEndian((Poco::UInt64)value);
}
#endif

Telnyx::Int16 endian_from_little_endian(Telnyx::Int16 value)
{
  return ByteOrder::fromLittleEndian(value);
}

Telnyx::UInt16 endian_from_little_endian (Telnyx::UInt16 value)
{
  return ByteOrder::fromLittleEndian(value);
}

Telnyx::Int32 endian_from_little_endian(Telnyx::Int32 value)
{
  return ByteOrder::fromLittleEndian(value);
}

Telnyx::UInt32 endian_from_little_endian (Telnyx::UInt32 value)
{
  return ByteOrder::fromLittleEndian(value);
}

#if defined(TELNYX_HAVE_INT64)
Telnyx::Int64 endian_from_little_endian(Telnyx::Int64 value)
{
  return ByteOrder::fromLittleEndian((Poco::Int64)value);
}

Telnyx::UInt64 endian_from_little_endian (Telnyx::UInt64 value)
{
  return ByteOrder::fromLittleEndian((Poco::UInt64)value);
}
#endif

Telnyx::Int16 endian_to_network_order (Telnyx::Int16 value)
{
  return ByteOrder::toNetwork(value);
}

Telnyx::UInt16 endian_to_network_order(Telnyx::UInt16 value)
{
  return ByteOrder::toNetwork(value);
}

Telnyx::Int32 endian_to_network_order(Telnyx::Int32 value)
{
  return ByteOrder::toNetwork(value);
}

Telnyx::UInt32 endian_to_network_order (Telnyx::UInt32 value)
{
  return ByteOrder::toNetwork(value);
}

#if defined(TELNYX_HAVE_INT64)
Telnyx::Int64 endian_to_network_order(Telnyx::Int64 value)
{
  return ByteOrder::toNetwork((Poco::Int64)value);
}

Telnyx::UInt64 endian_to_network_order (Telnyx::UInt64 value)
{
  return ByteOrder::toNetwork((Poco::UInt64)value);
}
#endif

Telnyx::Int16 endian_from_network_order(Telnyx::Int16 value)
{
  return ByteOrder::fromNetwork(value);
}

Telnyx::UInt16 endian_from_network_order (Telnyx::UInt16 value)
{
  return ByteOrder::fromNetwork(value);
}

Telnyx::Int32 endian_from_network_order(Telnyx::Int32 value)
{
  return ByteOrder::fromNetwork(value);
}

Telnyx::UInt32 endian_from_network_order (Telnyx::UInt32 value)
{
  return ByteOrder::fromNetwork(value);
}

#if defined(TELNYX_HAVE_INT64)
Telnyx::Int64 endian_from_network_order(Telnyx::Int64 value)
{
  return ByteOrder::fromNetwork((Poco::Int64)value);
}

Telnyx::UInt64 endian_from_network_order (Telnyx::UInt64 value)
{
  return ByteOrder::fromNetwork((Poco::UInt64)value);
}
#endif

} // OSS





