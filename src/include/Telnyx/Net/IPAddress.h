// Based on original code from https://github.com/joegen/oss_core
// Original work is done by Joegen Baclor and hereby allows use of this code 
// to be republished as part of the freeswitch project under 
// MOZILLA PUBLIC LICENSE Version 1.1 and above.

#ifndef OSS_IPAddress_H_INCLUDED
#define OSS_IPAddress_H_INCLUDED

#include <boost/asio.hpp>

#include "Telnyx/Telnyx.h"


namespace Telnyx {
namespace Net {
  
class IPAddress
  /// This is a helper class on top of the asio address_v4 and address_v6 objects
{
public:
  enum Protocol
  {
    UDP,
    TCP,
    TLS,
    WS,
    WSS,
    SCTP,
    UnknownTransport
  };
  
  typedef boost::asio::ip::address_v6::bytes_type v6_byte_type;
  
  IPAddress();
    /// Default constructor

  explicit IPAddress(const std::string& address);
    /// Create an address from a string

  explicit IPAddress(unsigned long address);
    /// Create an address from an unsigned long
  
  explicit IPAddress(v6_byte_type bytes);
    /// Create an address from an array of bytes 

  explicit IPAddress(const boost::asio::ip::address_v4& address);
    /// Create and IP address from an asio address_v4

  explicit IPAddress(const boost::asio::ip::address_v6& address);
    /// Create and IP address from an asio address_v6

  IPAddress(const std::string& address, unsigned short port);
    /// Create a new IP address with port
  
  IPAddress(const std::string& address, unsigned short port, Protocol protocol);
    /// Create a new IP address with port and protocol

  IPAddress(const IPAddress& address);
    /// Copy constructor

  IPAddress& operator = (const std::string& address);
    /// Copy an address from a string

  IPAddress& operator = (unsigned long address);
    /// Copy an address from an unsigned long

  IPAddress& operator = (const boost::asio::ip::address& address);
    /// Copy and IP address from an asio address_v4 or address_v6

  IPAddress& operator = (const IPAddress& address);
    /// Copy and IP address from another IPAddress

  bool operator == (const boost::asio::ip::address& address) const;
    /// Equality operator against another address

  bool operator == (const IPAddress& address) const;
    /// Equality operator against another address

  bool operator < (const boost::asio::ip::address& address) const;
    /// Lessthan operator against another address

  bool operator < (const IPAddress& address) const;
    /// Lessthan operator against another address

  bool operator > (const boost::asio::ip::address& address) const;
    /// Greater operator against another address

  bool operator > (const IPAddress& address) const;
    /// Greater operator against another address

  bool operator != (const boost::asio::ip::address& address) const;
    /// Non-equality operator against another address

  bool operator != (const IPAddress& address) const;
    /// Non-equality operator against another address

  bool compare(const IPAddress& address, bool includePort) const;
    /// Compare hosts and optional port

  void swap(IPAddress& address);
    /// Exchanges the content of two messages.
  
  std::string toString() const;
    /// Get the address in its string format

  unsigned short& cidr();
    /// Return the CIDR segment of a CIDR address

  const unsigned short& getPort() const;
    /// Return the port element of an address:port tuple

  void setPort(unsigned short port);
    /// Set the port number

  std::string toCidrString() const;
    /// Get the address in CIDR format

  std::string toIpPortString() const;
    /// Get the address in ip:port format

  bool isValid() const;
    /// Return true if the address is valid

  boost::asio::ip::address& address();
    /// Return a direct reference to the asio address
  
  const boost::asio::ip::address& address() const;
    /// Return a direct reference to the asio address

  std::string& externalAddress();
    /// Return the external address.  This is a custom property used
    /// by applications that would want to retain a map between an internal
    /// and external addresses in cases of servers deplyed within NAT
  
  const std::string& externalAddress() const;
    /// Return the external address.  This is a custom property used
    /// by applications that would want to retain a map between an internal
    /// and external addresses in cases of servers deplyed within NAT
  
  std::string& alias();
    /// alternative name for this address
  
  const std::string& alias() const;
    /// alternative name for this address
  
  Protocol getProtocol() const;
    /// Returns the protocol.  Default is UnknownTransport.
  
  void setProtocol(Protocol protocol);
    /// Set the transport

  bool isPrivate() const;
    /// Returns true if the IP address is of private type.  eg 192.168.x.x
  
  bool isInaddrAny() const;
  /// Returns true if the IP address is 0.0.0.0 or "::"
  
  bool isVirtual() const;
    /// Returns true if the IP address is a virtual address.  This is used for CARP address identification
  
  void setVirtual(bool isVirtual = true);
    /// Flag this IP address as virtual

  bool isLocalAddress() const;
    /// Returns true if this IP Address is bound to a local interface

  static IPAddress fromHost(const char* host);
    /// Returns an IP address from a host

  static IPAddress fromV4IPPort(const char* ipPortTuple);
    /// Returns an IP Address from IP:PORT tupple

  static IPAddress fromV4DWORD(Telnyx::UInt32 ip4);
    /// Returns an IP Addres from a DWORD

  static bool isV4Address(const std::string& address);
    /// Returns true if address is a valid IPV4 address
  
  static bool isV6Address(const std::string& address);
    /// Returns true if address is a valid IPV6 address
  
  static bool isIPAddress(const std::string& address);
    /// Returns true if the address is either IPV4 or IPV6 address
  
  static const std::vector<IPAddress>& getLocalAddresses();
    /// Returns all available local IP address
  
  static const IPAddress& getDefaultAddress();
    /// Returns the address of the default interface
  
public:
  static std::vector<IPAddress> _localAddresses;
  static IPAddress _defaultAddress;
  
protected:
  boost::asio::ip::address _address;
  std::string _externalAddress;
  unsigned short _port;
  unsigned short _cidr;
  bool _isVirtual;
  Protocol _protocol;
  std::string _alias;
};

//
// Inlines
//

template <typename Elem, typename Traits>
std::basic_ostream<Elem, Traits>& operator<<(
    std::basic_ostream<Elem, Traits>& os, const IPAddress& addr)
{
  os << addr.toString();
  return os;
}

inline boost::asio::ip::address& IPAddress::address()
{
  return _address;
}

inline const boost::asio::ip::address& IPAddress::address() const
{
  return _address;
}

inline std::string& IPAddress::externalAddress()
{
  return _externalAddress;
}

inline const std::string& IPAddress::externalAddress() const
{
  return _externalAddress;
}

inline std::string& IPAddress::alias()
{
  return _alias;
}

inline const std::string& IPAddress::alias() const
{
  return _alias;
}


inline const unsigned short& IPAddress::getPort() const
{
  return _port;
}

inline void IPAddress::setPort(unsigned short port)
{
  _port = port;
}

inline unsigned short& IPAddress::cidr()
{
  return _cidr;
}

inline IPAddress& IPAddress::operator = (const std::string& address)
{
  try
  {
    this->_address = boost::asio::ip::address(boost::asio::ip::address::from_string(address));
  }
  catch(...)
  {
    this->_address = boost::asio::ip::address();
  }
  return *this;
}

inline IPAddress& IPAddress::operator = (unsigned long address)
{
  this->_address = boost::asio::ip::address(boost::asio::ip::address_v4(address));
  return *this;
}

inline IPAddress& IPAddress::operator = (const boost::asio::ip::address& address)
{
  this->_address = address;
  return *this;
}

inline IPAddress& IPAddress::operator = (const IPAddress& address)
{
  IPAddress swapable(address);
  swap(swapable);
  return *this;
}

inline bool IPAddress::operator == (const boost::asio::ip::address& address) const
{
  return _address == address;
}

inline bool IPAddress::operator == (const IPAddress& address) const
{
  return _address == address._address && _port == address._port;
}

inline bool IPAddress::compare(const IPAddress& address, bool includePort) const
{
  if (includePort)
    return _address == address._address && _port == address._port;
  else
    return _address == address._address;
}

inline bool IPAddress::operator < (const boost::asio::ip::address& address) const
{
  return _address < address;
}

inline bool IPAddress::operator < (const IPAddress& address) const
{
  if (_address == address._address)
    return _port < address._port;
  return _address < address._address;
}

inline bool IPAddress::operator > (const boost::asio::ip::address& address) const
{
  return !IPAddress::operator < (address);
}

inline bool IPAddress::operator > (const IPAddress& address) const
{
  return !IPAddress::operator < (address);
}

inline bool IPAddress::operator != (const boost::asio::ip::address& address) const
{
  return _address != address;
}

inline bool IPAddress::operator != (const IPAddress& address) const
{
  return _address != address._address || _port != address._port;
}
  
inline std::string IPAddress::toCidrString() const
{
  boost::system::error_code e;
  std::string ipString = _address.to_string(e);
  if (e)
    return "";

  std::stringstream strm;
  strm << ipString << "/" << _cidr;
  return strm.str();
}

inline std::string IPAddress::toIpPortString() const
{
  boost::system::error_code e;
  std::string ipString = _address.to_string(e);
  if (e)
    return "";
  std::stringstream strm;
  
  if (_port != 0)
  {
    if (_address.is_v6())
    {
      strm << "[" << ipString << "]:" << _port;
    }
    else
    {
      strm << ipString << ":" << _port;
    }
  }
  else
  {
    strm << ipString;
  }
  return strm.str();
}

inline bool IPAddress::isValid() const
{
  return !isInaddrAny();
}

inline std::string IPAddress::toString() const
{
  try
  {
    return _address.to_string();
  }
  catch(...)
  {
    return "";
  }
}

inline IPAddress IPAddress::fromV4DWORD(Telnyx::UInt32 ip4)
{
  return IPAddress(ip4);
}

inline bool IPAddress::isVirtual() const
{
  return _isVirtual;
}
  
inline void IPAddress::setVirtual(bool isVirtual)
{
  _isVirtual = isVirtual;
}

inline IPAddress::Protocol IPAddress::getProtocol() const\
{
  return _protocol;
}
  
inline void IPAddress::setProtocol(Protocol protocol)
{
  _protocol = protocol;
}

inline const std::vector<IPAddress>& IPAddress::getLocalAddresses()
{
  return IPAddress::_localAddresses;
}

inline const IPAddress& IPAddress::getDefaultAddress()
{
  return IPAddress::_defaultAddress;
}

} } // Telnyx::Net

#endif // OSS_IPAddress_H_INCLUDED

