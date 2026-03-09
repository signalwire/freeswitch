#pragma once

#include <boost/asio.hpp>

#ifdef HAVE_CARES

// c-ares available - use async DNS resolution
#include <ares.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <boost/system/error_code.hpp>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace Telnyx {
namespace Net {

/**
 * Global c-ares library initialization.
 * Ensures ares_library_init() is called exactly once per process.
 */
class ares_library_initializer
{
public:
    static ares_library_initializer& instance()
    {
        static ares_library_initializer init;
        return init;
    }

    bool is_initialized() const { return initialized_; }

private:
    ares_library_initializer()
    {
        int status = ares_library_init(ARES_LIB_INIT_ALL);
        initialized_ = (status == ARES_SUCCESS);
    }

    ~ares_library_initializer() = default;

    ares_library_initializer(const ares_library_initializer&) = delete;
    ares_library_initializer& operator=(const ares_library_initializer&) = delete;

    bool initialized_;
};

/**
 * Generic c-ares based resolver that mimics boost::asio resolver interface.
 * Template parameter InternetProtocol should be one of:
 *   - boost::asio::ip::tcp
 *   - boost::asio::ip::udp
 *   - boost::asio::ip::icmp
 *
 * Usage:
 *   Telnyx::Net::resolver<boost::asio::ip::tcp> tcp_resolver(io_service);
 *   Telnyx::Net::resolver<boost::asio::ip::udp> udp_resolver(io_service);
 *   Telnyx::Net::resolver<boost::asio::ip::icmp> icmp_resolver(io_service);
 */
template <typename InternetProtocol>
class resolver
{
public:
    using protocol_type = InternetProtocol;
    using endpoint_type = typename InternetProtocol::endpoint;

    class query
    {
    public:
        query(const std::string& host, const std::string& service)
            : protocol_(protocol_type::v4()), host_(host), service_(service)
        {
        }

        query(const protocol_type& protocol, const std::string& host, const std::string& service)
            : protocol_(protocol), host_(host), service_(service)
        {
        }

        const protocol_type& protocol() const { return protocol_; }
        const std::string& host_name() const { return host_; }
        const std::string& service_name() const { return service_; }

    private:
        protocol_type protocol_;
        std::string host_;
        std::string service_;
    };

    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = endpoint_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const endpoint_type*;
        using reference = const endpoint_type&;

        // Default constructor creates an "end" iterator
        iterator() : endpoints_(), index_(0) {}

        // Construct from vector - shares ownership via shared_ptr
        iterator(const std::vector<endpoint_type>& endpoints, size_t index = 0)
            : endpoints_(std::make_shared<std::vector<endpoint_type>>(endpoints)), index_(index)
        {
        }

        const endpoint_type& operator*() const
        {
            return (*endpoints_)[index_];
        }

        const endpoint_type* operator->() const
        {
            return &(*endpoints_)[index_];
        }

        iterator& operator++()
        {
            ++index_;
            return *this;
        }

        iterator operator++(int)
        {
            iterator tmp = *this;
            ++index_;
            return tmp;
        }

        bool operator==(const iterator& other) const
        {
            bool this_at_end = !endpoints_ || index_ >= endpoints_->size();
            bool other_at_end = !other.endpoints_ || other.index_ >= other.endpoints_->size();

            if (this_at_end && other_at_end) {
                return true;
            }
            if (this_at_end || other_at_end) {
                return false;
            }
            return endpoints_.get() == other.endpoints_.get() && index_ == other.index_;
        }

        bool operator!=(const iterator& other) const
        {
            return !(*this == other);
        }

        // EndpointSequence support: begin()/end() allow this iterator
        // to be passed directly to boost::asio::connect/async_connect
        iterator begin() const
        {
            if (!endpoints_ || endpoints_->empty()) {
                return iterator();
            }
            return iterator(*endpoints_, 0);
        }

        iterator end() const
        {
            return iterator();
        }

    private:
        std::shared_ptr<std::vector<endpoint_type>> endpoints_;
        size_t index_;
    };

    explicit resolver(boost::asio::io_service& /* io_service */)
    {
        ares_library_initializer::instance();
    }

    ~resolver() = default;

    iterator resolve(const query& q)
    {
        boost::system::error_code ec;
        iterator result = resolve(q, ec);
        if (ec) {
            throw boost::system::system_error(ec);
        }
        return result;
    }

    iterator resolve(const query& q, boost::system::error_code& ec)
    {
        if (!ares_library_initializer::instance().is_initialized()) {
            ec = boost::asio::error::not_connected;
            return iterator();
        }

        std::vector<endpoint_type> endpoints;
        unsigned short port = static_cast<unsigned short>(atoi(q.service_name().c_str()));

        // Fast path: numeric IP addresses don't need DNS resolution
        boost::system::error_code parse_ec;
        boost::asio::ip::address addr = boost::asio::ip::address::from_string(q.host_name(), parse_ec);

        if (!parse_ec) {
            endpoints.push_back(endpoint_type(addr, port));
            ec.clear();
            return iterator(endpoints, 0);
        }

        // DNS resolution via c-ares with internal event thread.
        static const int DNS_TIMEOUT_MS = 1000;
        static const int DNS_TRIES = 2;

        ares_options options;
        memset(&options, 0, sizeof(options));
        options.evsys = ARES_EVSYS_DEFAULT;
        options.timeout = DNS_TIMEOUT_MS;
        options.tries = DNS_TRIES;

        ares_channel channel = nullptr;
        int init_status = ares_init_options(&channel, &options,
            ARES_OPT_EVENT_THREAD | ARES_OPT_TIMEOUT | ARES_OPT_TRIES);

        if (init_status != ARES_SUCCESS) {
            ec = boost::system::error_code(init_status, boost::system::system_category());
            return iterator();
        }

        resolve_result result_ctx;
        result_ctx.endpoints = &endpoints;
        result_ctx.port = port;
        result_ctx.status = ARES_SUCCESS;

        // Setup hints
        ares_addrinfo_hints hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = get_socktype();
        hints.ai_protocol = get_protocol();

        // Start async resolution — c-ares event thread handles all I/O
        ares_getaddrinfo(channel, q.host_name().c_str(), nullptr, &hints,
            addrinfo_callback, &result_ctx);

        // Block until done. The wait timeout is a safety backstop, not the
        // primary timeout — c-ares enforces timeout × tries internally.
        // Worst case: DNS_TIMEOUT_MS * DNS_TRIES + margin for processing.
        static const int DNS_WAIT_MS = DNS_TIMEOUT_MS * DNS_TRIES + 500;
        ares_status_t wait_status = ares_queue_wait_empty(channel, DNS_WAIT_MS);

        // Cleanup channel (closes all c-ares sockets)
        ares_destroy(channel);

        // If wait timed out, the callback may never have fired
        if (wait_status == ARES_ETIMEOUT) {
            ec = boost::asio::error::timed_out;
            return iterator();
        }

        // Map result status
        if (result_ctx.status != ARES_SUCCESS) {
            switch (result_ctx.status) {
                case ARES_ENOTFOUND:
                case ARES_ENODATA:
                    ec = boost::asio::error::host_not_found;
                    break;
                case ARES_ETIMEOUT:
                case ARES_EDESTRUCTION:
                    ec = boost::asio::error::timed_out;
                    break;
                default:
                    ec = boost::system::error_code(result_ctx.status, boost::system::system_category());
                    break;
            }
            return iterator();
        }

        if (endpoints.empty()) {
            ec = boost::asio::error::host_not_found;
            return iterator();
        }

        ec.clear();
        return iterator(endpoints, 0);
    }

    void cancel()
    {
        // No-op: each resolve() uses its own short-lived channel.
        // Cancellation is handled by ares_destroy() when resolve() completes.
    }

private:
    struct resolve_result {
        std::vector<endpoint_type>* endpoints;
        unsigned short port;
        int status;
    };

    static int get_socktype()
    {
        return SOCK_DGRAM;
    }

    static int get_protocol()
    {
        return IPPROTO_UDP;
    }

    static void addrinfo_callback(void* arg, int status, int /* timeouts */, ares_addrinfo* result)
    {
        resolve_result* ctx = static_cast<resolve_result*>(arg);
        ctx->status = status;

        if (status == ARES_SUCCESS && result) {
            for (ares_addrinfo_node* node = result->nodes; node != nullptr; node = node->ai_next) {
                if (node->ai_family == AF_INET && node->ai_addr) {
                    sockaddr_in* addr_in = reinterpret_cast<sockaddr_in*>(node->ai_addr);

                    boost::asio::ip::address_v4::bytes_type bytes;
                    memcpy(bytes.data(), &addr_in->sin_addr, sizeof(in_addr));

                    ctx->endpoints->push_back(
                        endpoint_type(boost::asio::ip::address_v4(bytes), ctx->port)
                    );
                }
                else if (node->ai_family == AF_INET6 && node->ai_addr) {
                    sockaddr_in6* addr_in6 = reinterpret_cast<sockaddr_in6*>(node->ai_addr);

                    boost::asio::ip::address_v6::bytes_type bytes;
                    memcpy(bytes.data(), &addr_in6->sin6_addr, sizeof(in6_addr));

                    ctx->endpoints->push_back(
                        endpoint_type(boost::asio::ip::address_v6(bytes), ctx->port)
                    );
                }
            }
            ares_freeaddrinfo(result);
        }

    }

};

// Template specializations for protocol-specific socket types

template <>
inline int resolver<boost::asio::ip::tcp>::get_socktype()
{
    return SOCK_STREAM;
}

template <>
inline int resolver<boost::asio::ip::tcp>::get_protocol()
{
    return IPPROTO_TCP;
}

template <>
inline int resolver<boost::asio::ip::udp>::get_socktype()
{
    return SOCK_DGRAM;
}

template <>
inline int resolver<boost::asio::ip::udp>::get_protocol()
{
    return IPPROTO_UDP;
}

template <>
inline int resolver<boost::asio::ip::icmp>::get_socktype()
{
    return SOCK_RAW;
}

template <>
inline int resolver<boost::asio::ip::icmp>::get_protocol()
{
    return IPPROTO_ICMP;
}

// Convenient type aliases matching boost::asio naming convention
using tcp_resolver = resolver<boost::asio::ip::tcp>;
using udp_resolver = resolver<boost::asio::ip::udp>;
using icmp_resolver = resolver<boost::asio::ip::icmp>;

} // namespace Net
} // namespace Telnyx

#else // !HAVE_CARES

// c-ares not available - fall back to boost::asio resolvers
namespace Telnyx {
namespace Net {

/**
 * Fallback resolver when c-ares is not available.
 * Simply aliases to boost::asio's native resolver.
 */
template <typename InternetProtocol>
using resolver = typename InternetProtocol::resolver;

// Convenient type aliases matching boost::asio naming convention
using tcp_resolver = boost::asio::ip::tcp::resolver;
using udp_resolver = boost::asio::ip::udp::resolver;
using icmp_resolver = boost::asio::ip::icmp::resolver;

} // namespace Net
} // namespace Telnyx

#endif // HAVE_CARES
