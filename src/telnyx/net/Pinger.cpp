//
// ping.h
// ~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "Telnyx/Net/Pinger.h"


namespace Telnyx {
namespace Net {

static unsigned short get_identifier()
{
#if defined(BOOST_WINDOWS)
  return static_cast<unsigned short>(::GetCurrentProcessId());
#else
  return static_cast<unsigned short>(::getpid());
#endif
}

bool Pinger::ping_host(boost::asio::io_service& io_service, const std::string& destination, int sequence, int ttl, ping_host_handler handler, void* user_data)
{
  try {
    Pinger* pinger = new Pinger(io_service);
    icmp::resolver::query query(icmp::v4(), destination.c_str(), "");
    try 
    {
      pinger->destination_ = *pinger->resolver_.resolve(query);
    }
    catch(...)
    {
      delete pinger;
      return false;
    }
    pinger->handler_ = handler;
    pinger->user_data_ = user_data;
    pinger->sequence_number_ = sequence;
    return pinger->start_send(ttl) && pinger->start_receive();
  }
  catch(...)
  {
    return false;
  }
}

void Pinger::garbage_collect_socket(Pinger* pinger)
{
  delete pinger;
}

Pinger::Pinger(boost::asio::io_service& io_service)
  : io_service_(io_service), 
    resolver_(io_service), 
    socket_(io_service, icmp::v4()),
    timer_(io_service), 
    sequence_number_(0), 
    num_replies_(0),
    user_data_(0),
    timed_out_(false),
    errored_out_(false)
{
}

Pinger::~Pinger()
{
}


bool Pinger::start_send(int ttl_ms)
{
  std::string body("ping");

  // Create an ICMP header for an echo request.
  icmp_header echo_request;
  echo_request.type(icmp_header::echo_request);
  echo_request.code(0);
  echo_request.identifier(get_identifier());
  echo_request.sequence_number(sequence_number_);
  compute_checksum(echo_request, body.begin(), body.end());

  // Encode the request packet.
  boost::asio::streambuf request_buffer;
  std::ostream os(&request_buffer);
  os << echo_request << body;

  try
  {
    // Send the request.
    time_sent_ = posix_time::microsec_clock::universal_time();
    socket_.send_to(request_buffer.data(), destination_);

    // Wait up to five seconds for a reply.
    num_replies_ = 0;
    timer_.expires_at(time_sent_ + posix_time::milliseconds(ttl_ms));
    timer_.async_wait(boost::bind(&Pinger::handle_timeout, this));
  }
  catch(...)
  {
    errored_out_ = true;
    if (handler_) {
      handler_(destination_.address().to_string(), true, sequence_number_, 0, 0, 0, user_data_);
    }
    io_service_.post(boost::bind(Pinger::garbage_collect_socket, this));
    return false;
  }
  return true;
}

bool Pinger::start_receive()
{
  try
  {
    // Discard any data already in the buffer.
    reply_buffer_.consume(reply_buffer_.size());

    // Wait for a reply.
    socket_.async_receive(reply_buffer_.prepare(512),
        boost::bind(&Pinger::handle_receive, this, _2));
  }
  catch(...)
  {
    errored_out_ = true;
    if (handler_) {
      handler_(destination_.address().to_string(), true, sequence_number_, 0, 0, 0, user_data_);
    }
    io_service_.post(boost::bind(Pinger::garbage_collect_socket, this));
    return false;
  }
  return true;
}

void Pinger::handle_timeout()
{
  if (errored_out_) {
    return;
  }
  if (num_replies_)
  {
    io_service_.post(boost::bind(Pinger::garbage_collect_socket, this));
    return;
  }
  
  try
  {
    if (num_replies_ == 0)
    {
      timed_out_ = true;
      if (handler_)
      {
        handler_(destination_.address().to_string(), true, sequence_number_, 0, 0, 0, user_data_);
      }
      socket_.close();
    }
  }
  catch(...)
  {
  }
}

void Pinger::handle_receive(std::size_t length)
{
  if (errored_out_) {
    return;
  }
  if (timed_out_) {
    io_service_.post(boost::bind(garbage_collect_socket, this));
    return;
  }
  // The actual number of bytes received is committed to the buffer so that we
  // can extract it using a std::istream object.
  reply_buffer_.commit(length);

  // Decode the reply packet.
  std::istream is(&reply_buffer_);
  ipv4_header ipv4_hdr;
  icmp_header icmp_hdr;
  is >> ipv4_hdr >> icmp_hdr;

  // We can receive all ICMP packets received by the host, so we need to
  // filter out only the echo replies that match the our identifier and
  // expected sequence number.
  if (is && icmp_hdr.type() == icmp_header::echo_reply
        && icmp_hdr.identifier() == get_identifier()
        && icmp_hdr.sequence_number() == sequence_number_)
  {
    num_replies_++;
    timer_.cancel();
    // Print out some information about the reply packet.
    posix_time::ptime now = posix_time::microsec_clock::universal_time();
    if (handler_)
    {
      handler_(destination_.address().to_string(), false, sequence_number_, length - ipv4_hdr.header_length(), ipv4_hdr.time_to_live(), (now - time_sent_).total_milliseconds(), user_data_);
    }
  }
  else
  {
    start_receive();
  }
}


} } // OSS::Net
