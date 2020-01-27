//
// ping.h
// ~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef OSS_PINGER_H_INCLUDED
#define OSS_PINGER_H_INCLUDED

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <istream>
#include <iostream>
#include <ostream>

#include "icmp_header.h"
#include "ipv4_header.h"

using boost::asio::ip::icmp;
using boost::asio::deadline_timer;
namespace posix_time = boost::posix_time;


namespace Telnyx {
namespace Net {


class Pinger
{
public:
  typedef boost::function<void(const std::string& /*host*/, bool /*timeout*/, int /*sequence*/, int /*bytes*/, int /*ttl*/, int /*roundtrip*/, void* /*user_data*/)> ping_host_handler;
  static bool ping_host(boost::asio::io_service& io_service, const std::string& destination, int sequence, int ttl, ping_host_handler handler, void* user_data);

private:
  Pinger(boost::asio::io_service& io_service);
  ~Pinger();
  bool start_send(int ttl_ms);
  bool start_receive();
  void handle_timeout();
  void handle_receive(std::size_t length);
  static void garbage_collect_socket(Pinger* pinger);
  static void on_send_error(Pinger* pinger);

  boost::asio::io_service& io_service_;
  icmp::resolver resolver_;
  icmp::endpoint destination_;
  icmp::socket socket_;
  deadline_timer timer_;
  unsigned short sequence_number_;
  posix_time::ptime time_sent_;
  boost::asio::streambuf reply_buffer_;
  std::size_t num_replies_;
  ping_host_handler handler_;
  void* user_data_;
  bool timed_out_;
  bool errored_out_;
};

} } // OSS::Net

#endif 