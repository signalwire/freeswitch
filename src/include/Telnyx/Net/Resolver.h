#pragma once

#include <boost/asio.hpp>

#ifdef HAVE_CARES

// c-ares available - use async DNS resolution
#include <ares.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/bind/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/system/error_code.hpp>

#include <cstring>
#include <map>
#include <memory>
#include <set>
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

    // Prevent copying
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
            // Safe dereference - caller must ensure not at end
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
            // Check if both are end iterators
            bool this_at_end = !endpoints_ || index_ >= endpoints_->size();
            bool other_at_end = !other.endpoints_ || other.index_ >= other.endpoints_->size();

            // If both at end, they're equal (regardless of which vector)
            if (this_at_end && other_at_end) {
                return true;
            }

            // If only one is at end, they're not equal
            if (this_at_end || other_at_end) {
                return false;
            }

            // Both valid - must point to same vector and same index
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

    explicit resolver(boost::asio::io_service& io_service)
        : channel_(nullptr), initialized_(false), io_service_(io_service)
    {
        init();
    }

    ~resolver()
    {
        cleanup();
    }

    iterator resolve(const query& q)
    {
        boost::system::error_code ec;
        iterator result = resolve(q, ec);
        if (ec) {
            throw boost::system::system_error(ec);
        }
        return result;
    }

    struct socket_wrapper {
        std::shared_ptr<boost::asio::posix::stream_descriptor> descriptor;
        bool read_pending;
        bool write_pending;

        socket_wrapper(boost::asio::io_service* io_service, ares_socket_t fd)
            : descriptor(std::make_shared<boost::asio::posix::stream_descriptor>(*io_service, fd)),
              read_pending(false), write_pending(false)
        {
        }
    };

    struct resolve_context {
        std::vector<endpoint_type>* endpoints;
        unsigned short port;
        bool done;
        int status;
        boost::asio::io_service* io_service;
        ares_channel channel;
        std::map<ares_socket_t, std::shared_ptr<socket_wrapper>> sockets;
        std::shared_ptr<boost::asio::deadline_timer> timeout_timer;
    };

    static void addrinfo_callback(void* arg, int status, int /* timeouts */, struct ares_addrinfo* result)
    {
        resolve_context* ctx = static_cast<resolve_context*>(arg);
        ctx->status = status;

        if (status == ARES_SUCCESS && result) {
            for (struct ares_addrinfo_node* node = result->nodes; node != nullptr; node = node->ai_next) {
                if (node->ai_family == AF_INET && node->ai_addr) {
                    struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(node->ai_addr);

                    boost::asio::ip::address_v4::bytes_type bytes;
                    memcpy(bytes.data(), &addr_in->sin_addr, sizeof(struct in_addr));

                    ctx->endpoints->push_back(
                        endpoint_type(boost::asio::ip::address_v4(bytes), ctx->port)
                    );
                }
                else if (node->ai_family == AF_INET6 && node->ai_addr) {
                    struct sockaddr_in6* addr_in6 = reinterpret_cast<struct sockaddr_in6*>(node->ai_addr);

                    boost::asio::ip::address_v6::bytes_type bytes;
                    memcpy(bytes.data(), &addr_in6->sin6_addr, sizeof(struct in6_addr));

                    ctx->endpoints->push_back(
                        endpoint_type(boost::asio::ip::address_v6(bytes), ctx->port)
                    );
                }
            }
            ares_freeaddrinfo(result);
        }

        ctx->done = true;
    }

    static int socket_create_callback(int socket_fd, int type, void* user_data)
    {
        // This callback is called when c-ares needs to create a socket
        // We return 0 to indicate success, socket_fd is already created
        (void)socket_fd;
        (void)type;
        (void)user_data;
        return 0;
    }

    static void setup_socket_operations(std::shared_ptr<resolve_context> ctx)
    {
        if (ctx->done) {
            return;
        }

        ares_socket_t sockets[ARES_GETSOCK_MAXNUM];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        int bitmask = ares_getsock(ctx->channel, sockets, ARES_GETSOCK_MAXNUM);
#pragma GCC diagnostic pop

        std::set<ares_socket_t> active_sockets;

        for (int i = 0; i < ARES_GETSOCK_MAXNUM; i++) {
            bool readable = ARES_GETSOCK_READABLE(bitmask, i);
            bool writable = ARES_GETSOCK_WRITABLE(bitmask, i);

            if (!readable && !writable) {
                continue;
            }

            ares_socket_t socket_fd = sockets[i];
            active_sockets.insert(socket_fd);

            // Get or create socket wrapper. The stream_descriptor constructor
            // or async operations can throw if the fd is invalid or in a bad
            // state (e.g. c-ares closed it between ares_getsock and here).
            try {
                std::shared_ptr<socket_wrapper> wrapper;
                auto it = ctx->sockets.find(socket_fd);
                if (it == ctx->sockets.end()) {
                    wrapper = std::make_shared<socket_wrapper>(ctx->io_service, socket_fd);
                    ctx->sockets[socket_fd] = wrapper;
                } else {
                    wrapper = it->second;
                }

                // Setup async wait for read
                if (readable && !wrapper->read_pending) {
                    wrapper->read_pending = true;
                    wrapper->descriptor->async_read_some(
                        boost::asio::null_buffers(),
                        boost::bind(&resolver::handle_socket_read, ctx, socket_fd, boost::asio::placeholders::error)
                    );
                }

                // Setup async wait for write
                if (writable && !wrapper->write_pending) {
                    wrapper->write_pending = true;
                    wrapper->descriptor->async_write_some(
                        boost::asio::null_buffers(),
                        boost::bind(&resolver::handle_socket_write, ctx, socket_fd, boost::asio::placeholders::error)
                    );
                }
            } catch (const boost::system::system_error&) {
                ctx->sockets.erase(socket_fd);
                active_sockets.erase(socket_fd);
            }
        }

        // Remove sockets that are no longer active
        for (auto it = ctx->sockets.begin(); it != ctx->sockets.end();) {
            if (active_sockets.find(it->first) == active_sockets.end()) {
                // Cancel any pending operations — cancel itself may throw
                // if the descriptor is already in a bad state
                try {
                    if (it->second->read_pending || it->second->write_pending) {
                        it->second->descriptor->cancel();
                    }
                } catch (const boost::system::system_error&) {
                }
                it = ctx->sockets.erase(it);
            } else {
                ++it;
            }
        }
    }

    static void update_timeout(std::shared_ptr<resolve_context> ctx)
    {
        if (ctx->done) {
            return;
        }

        struct timeval tv;
        struct timeval* tvp = ares_timeout(ctx->channel, nullptr, &tv);

        if (!ctx->timeout_timer) {
            ctx->timeout_timer = std::make_shared<boost::asio::deadline_timer>(*ctx->io_service);
        }

        if (tvp && (tvp->tv_sec > 0 || tvp->tv_usec > 0)) {
            long milliseconds = tvp->tv_sec * 1000 + tvp->tv_usec / 1000;
            ctx->timeout_timer->expires_from_now(boost::posix_time::milliseconds(milliseconds));
            ctx->timeout_timer->async_wait(
                boost::bind(&resolver::handle_timeout, ctx, boost::asio::placeholders::error)
            );
        } else {
            ctx->timeout_timer->cancel();
        }
    }

    static void handle_socket_read(std::shared_ptr<resolve_context> ctx, ares_socket_t socket_fd, const boost::system::error_code& ec)
    {
        if (ctx->done) {
            return;
        }

        auto it = ctx->sockets.find(socket_fd);
        if (it != ctx->sockets.end()) {
            it->second->read_pending = false;
        }

        if (!ec) {
            ares_process_fd(ctx->channel, socket_fd, ARES_SOCKET_BAD);
            // Re-setup socket operations for next iteration
            setup_socket_operations(ctx);
        } else if (ec != boost::asio::error::operation_aborted) {
            // Error occurred, process anyway
            ares_process_fd(ctx->channel, socket_fd, ARES_SOCKET_BAD);
            setup_socket_operations(ctx);
        }
    }

    static void handle_socket_write(std::shared_ptr<resolve_context> ctx, ares_socket_t socket_fd, const boost::system::error_code& ec)
    {
        if (ctx->done) {
            return;
        }

        auto it = ctx->sockets.find(socket_fd);
        if (it != ctx->sockets.end()) {
            it->second->write_pending = false;
        }

        if (!ec) {
            ares_process_fd(ctx->channel, ARES_SOCKET_BAD, socket_fd);
            // Re-setup socket operations for next iteration
            setup_socket_operations(ctx);
        } else if (ec != boost::asio::error::operation_aborted) {
            // Error occurred, process anyway
            ares_process_fd(ctx->channel, ARES_SOCKET_BAD, socket_fd);
            setup_socket_operations(ctx);
        }
    }

    static void handle_timeout(std::shared_ptr<resolve_context> ctx, const boost::system::error_code& ec)
    {
        if (ctx->done) {
            return;
        }

        if (!ec) {
            // Timeout occurred, process with timeout
            ares_process_fd(ctx->channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
            update_timeout(ctx);
        }
    }

    iterator resolve(const query& q, boost::system::error_code& ec)
    {
        if (!initialized_) {
            ec = boost::asio::error::not_connected;
            return iterator();
        }

        std::vector<endpoint_type> endpoints;
        unsigned short port = static_cast<unsigned short>(atoi(q.service_name().c_str()));

        // Try parsing as IP address first
        boost::system::error_code parse_ec;
        boost::asio::ip::address addr = boost::asio::ip::address::from_string(q.host_name(), parse_ec);

        if (!parse_ec) {
            // It's already an IP address
            endpoints.push_back(endpoint_type(addr, port));
            ec.clear();
            return iterator(endpoints, 0);
        }

        // Need DNS resolution via c-ares
        // Use a private io_service for the DNS event loop to avoid
        // stealing handlers from the caller's shared io_service, which
        // would cause race conditions (e.g. mod_xml_http's semaphore handlers)
        boost::asio::io_service dns_io_service;

        // Heap-allocate context so async handlers can safely reference it
        // even after this function returns (e.g. cancellation handlers)
        auto ctx = std::make_shared<resolve_context>();
        ctx->endpoints = &endpoints;
        ctx->port = port;
        ctx->done = false;
        ctx->status = ARES_SUCCESS;
        ctx->io_service = &dns_io_service;
        ctx->channel = channel_;

        // Setup socket create callback (optional)
        ares_set_socket_callback(channel_, socket_create_callback, ctx.get());

        // Setup hints for getaddrinfo based on protocol type
        struct ares_addrinfo_hints hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;  // Support both IPv4 and IPv6

        // Set socket type based on protocol
        hints.ai_socktype = get_socktype();
        hints.ai_protocol = get_protocol();

        // Start async resolution using modern API
        ares_getaddrinfo(channel_, q.host_name().c_str(), nullptr, &hints,
            addrinfo_callback, ctx.get());

        // Initial socket operations and timeout setup
        setup_socket_operations(ctx);
        update_timeout(ctx);

        // Process events using private DNS event loop only
        while (!ctx->done && !dns_io_service.stopped()) {
            dns_io_service.run_one();
        }

        // Clear socket callback
        ares_set_socket_callback(channel_, nullptr, nullptr);

        // Cleanup: destroy all io_service-dependent objects (timers, stream
        // descriptors) while dns_io_service is still alive, then save results
        if (ctx->timeout_timer) {
            ctx->timeout_timer->cancel();
            ctx->timeout_timer.reset();
        }
        ctx->sockets.clear();

        int status = ctx->status;

        // Release our reference so ctx can be fully destroyed now while
        // dns_io_service is still valid (pending cancellation handlers in
        // dns_io_service also hold references and will be dropped when
        // dns_io_service is destroyed)
        ctx.reset();

        if (status != ARES_SUCCESS) {
            switch (status) {
                case ARES_ENOTFOUND:
                case ARES_ENODATA:
                    ec = boost::asio::error::host_not_found;
                    break;
                case ARES_ETIMEOUT:
                    ec = boost::asio::error::timed_out;
                    break;
                default:
                    ec = boost::system::error_code(status, boost::system::system_category());
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
        if (channel_) {
            ares_cancel(channel_);
        }
    }

private:
    // Protocol-specific helpers using template specialization
    static int get_socktype()
    {
        // Default to DGRAM for UDP/ICMP-like protocols
        return SOCK_DGRAM;
    }

    static int get_protocol()
    {
        // Default to UDP
        return IPPROTO_UDP;
    }

    void init()
    {
        // Ensure global c-ares library is initialized (once per process)
        if (!ares_library_initializer::instance().is_initialized()) {
            initialized_ = false;
            return;
        }

        struct ares_options options;
        memset(&options, 0, sizeof(options));
        options.timeout = 5000;  // 5 second timeout
        options.tries = 3;       // 3 retries

        int status = ares_init_options(&channel_, &options, ARES_OPT_TIMEOUT | ARES_OPT_TRIES);
        if (status != ARES_SUCCESS) {
            initialized_ = false;
            return;
        }

        initialized_ = true;
    }

    void cleanup()
    {
        if (channel_) {
            ares_destroy(channel_);
            channel_ = nullptr;
        }
        initialized_ = false;
    }

    ares_channel channel_;
    bool initialized_;
    boost::asio::io_service& io_service_;
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
