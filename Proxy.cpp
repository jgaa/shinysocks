
#include <boost/log/trivial.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include "shinysocks.h"

using namespace std;
using boost::asio::ip::tcp;

namespace shinysocks {

Proxy::Proxy(tcp::socket&& sck)
: client_(move(sck)), server_(client_.get_io_service())
{
}

Proxy::~Proxy() {
    BOOST_LOG_TRIVIAL(debug) << "Leaving ~Proxy()";
}

void Proxy::Run(boost::asio::yield_context yield) {
    try {
        RunInt(yield);
    } catch(const exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Proxy: Caught exception: " << ex.what();
    } catch(const boost::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Proxy: Caught exception: "
            << boost::diagnostic_information(ex);
    }


    BOOST_LOG_TRIVIAL(info) << "Proxy done. Sent "
        << bytes_relayed_to_server_
        << " bytes, received " << bytes_relayed_to_client_ << " bytes.";

    if (client_.is_open())
        client_.close();
    if (server_.is_open())
        server_.close();

    BOOST_LOG_TRIVIAL(debug) << "Proxy::Run coroutine is done";
}

void Proxy::RunInt(boost::asio::yield_context& yield) {
    BOOST_LOG_TRIVIAL(info) << "Proxy starting on socket " << client_.local_endpoint()
        << "<-->" << client_.remote_endpoint();

    client_.set_option(tcp::no_delay(true));

    // Get and parse the initial buffer
    {
        char buffer[512] = {};
        const auto len = client_.async_read_some(
            boost::asio::buffer(buffer, sizeof(buffer)), yield);

        if (len < 2) {
            BOOST_LOG_TRIVIAL(error) << "Received too few bytes";
            throw runtime_error("Invalid connect header");
        }

        if (buffer[0] == static_cast<int>(ProtocolVer::SOCKS4))
            protocol_ver_ = ProtocolVer::SOCKS4;
        else if (buffer[0] == static_cast<int>(ProtocolVer::SOCKS5))
            protocol_ver_ = ProtocolVer::SOCKS5;

        command_ = buffer[1];

        if (protocol_ver_ == ProtocolVer::SOCKS4) {
            BOOST_LOG_TRIVIAL(debug) << "Client is requesting SOCKS4";
            ParseV4Header(buffer, len, yield);
        } else if (protocol_ver_ == ProtocolVer::SOCKS5) {
            BOOST_LOG_TRIVIAL(debug) << "Client is requesting SOCKS5";
            ParseV5Header(buffer, len, yield);
        } else {
            BOOST_LOG_TRIVIAL(error) << "Invalid protocol version "
                << static_cast<int>(buffer[0]);

            throw runtime_error("Invalid protocol version");
        }
    }

    // Do what we are asked
    if (command_ == static_cast<int>(Commands::CONNECT)) {
        // Connect to the requested endpoint
        BOOST_LOG_TRIVIAL(info) << "Connecting to endpoint " << endpoint_;

        try {
            server_.async_connect(endpoint_, yield);
        } catch(...) {
            // Tell the client that we failed.
            Reply(ReplyVal::CONN_REFUSED, server_.local_endpoint(), yield);
            throw;
        }

        server_.set_option(tcp::no_delay(true));
    } else {
        Reply(ReplyVal::REJECTED, server_.local_endpoint(), yield);
        throw runtime_error("Unimplemented command requested");
    }

    // Send reply package
    Reply(ReplyVal::OK, server_.local_endpoint(), yield);


    // Forward traffic from server to client
    boost::asio::spawn(client_.get_io_service(),
                        bind(&Proxy::RelayRoot,
                            shared_from_this(),
                            ref(server_), ref(client_),
                            ref(bytes_relayed_to_client_),
                            std::placeholders::_1));

    // Send whatever that was left of data after the header was parsed
    if (!remaining_buffer_.empty()) {

        BOOST_LOG_TRIVIAL(info) << "Sending " << remaining_buffer_.size()
            << " remaining bytes";

        boost::asio::async_write(server_, boost::asio::buffer(
            &remaining_buffer_[0], remaining_buffer_.size()),
                                    yield);

        bytes_relayed_to_server_ += remaining_buffer_.size();
        remaining_buffer_.clear();
    }

    // Forward traffic from client to server
    Relay(client_, server_, bytes_relayed_to_server_, yield);
}

void Proxy::ParseV4Header(const char *buffer,
                          const size_t len,
                          boost::asio::yield_context& yield) {
    size_t header_len = 9; // Minimum header len in v4 of the protocol
    const char *buffer_end = buffer + len;

    if (len < header_len) {
        BOOST_LOG_TRIVIAL(error) << "ParseV4Header: Received too few bytes";
        throw runtime_error("Invalid connect header");
    }

    // Get the port
    const uint16_t *port = reinterpret_cast<const uint16_t *>(&buffer[2]);
    port_ = htons(*port);

    // Get the IP
    const uint32_t *ip = reinterpret_cast<const uint32_t *>(&buffer[4]);
    ipv4_ = ntohl(*ip);

    // Get the name
    const char *name_end = &buffer[8];
    while((name_end < buffer_end) && *name_end) {
        ++name_end;
        ++header_len;
    }

    name_.assign(static_cast<const char *>(&buffer[8]), name_end);

    if (buffer[4] == 0 && buffer[5] == 0 && buffer[6] == 0) {
        // 4a hostname - We need to resolve the address
        const char *host_start = name_end + 1;
        if (host_start >= buffer_end) {
            throw runtime_error("Missing 4a domain name - buffer too short");
        }
        const char *host_end = host_start;
        while((host_end < buffer_end) && *host_end) {
            ++buffer_end;
            ++header_len;
        }

        const string host(host_start, host_end);
        BOOST_LOG_TRIVIAL(info) << "Will try to connect to: " << host;

        tcp::resolver resolver(client_.get_io_service());
        auto address_it = resolver.async_resolve({host, to_string(port_)},
                                                    yield);
        decltype(address_it) addr_end;

        if (address_it == addr_end) {
            Reply(ReplyVal::RESOLVER_FAILED, {}, yield);
            throw runtime_error("Failed to lookup domain-name");
        }

        endpoint_ = *address_it;

    } else {
        boost::asio::ip::address_v4 addr(ipv4_);
        endpoint_ = tcp::endpoint(addr, port_);

        BOOST_LOG_TRIVIAL(debug) << "Will try to connect to ipv4 supplied address: "
            << endpoint_;
    }

    if (len > header_len) {
        const size_t remaining = len - header_len;
        remaining_buffer_.resize(remaining);
        memcpy(&remaining_buffer_[0], &buffer[header_len], remaining);
    }
}

void Proxy::ParseV5Header(const char *buffer,
                            const size_t len,
                            boost::asio::yield_context& yield) {
    size_t header_len = 2; // Minimum header len in v5 of the protocol (auth hdr)

    // Authentication negotiation
    const unsigned num_methods = static_cast<unsigned char>(buffer[1]);
    header_len += num_methods;
    bool can_skip_authentication = false;
    for(unsigned m = 0; m < num_methods; ++m) {
        if (buffer[2 + m] == 0) {
            can_skip_authentication = true;
            break;
        }
    }

    if (!can_skip_authentication) {
        unsigned char rbuf[2] = {5, 0xff}; // No acceptable authentications
        boost::asio::async_write(client_, boost::asio::buffer(rbuf, 2), yield);
        throw runtime_error("No acceptable authentication methods");
    }

    {
        unsigned char rbuf[2] = {5, 0}; // OK, skip authentication
        boost::asio::async_write(client_, boost::asio::buffer(rbuf, 2), yield);
    }

    vector<char> hdr_buffer;
    {
        const size_t remaining = len - header_len;
        hdr_buffer.reserve(1024);
        hdr_buffer.resize(remaining);
        memcpy(&hdr_buffer[0], buffer + header_len, remaining);
    }

    bool need_more_data = hdr_buffer.size() < 7;
    bool done = false;

again:
    while(!done) { // Parse the header
        if (need_more_data) {
            const size_t max_chunk_size = 512;
            const size_t current_size = hdr_buffer.size();
            hdr_buffer.resize(current_size + max_chunk_size);

            const size_t read_bytes = client_.async_read_some(
                boost::asio::buffer(&hdr_buffer[current_size], max_chunk_size),
                                                              yield);
            hdr_buffer.resize(current_size + read_bytes);
            need_more_data = false;
        }

        header_len = 6; // Minimum header length, excluding address field

        if (hdr_buffer[0] != 5) {
            BOOST_LOG_TRIVIAL(error) << "ParseV5Header: Invalid protocol version "
                << static_cast<int>(hdr_buffer[0]);

            throw runtime_error("Invalid protocol version");
        }

        command_ = static_cast<int>(hdr_buffer[1]);

        switch(hdr_buffer[3]) { // address type
            case 1: // IPv4 address
                header_len += 4;
                if (header_len > hdr_buffer.size()) {
                    need_more_data = true;
                    goto again; // get more data
                } else {
                    const uint32_t *ip = reinterpret_cast<const uint32_t *>(&hdr_buffer[4]);
                    ipv4_ = ntohl(*ip);
                    // Get the port
                    const uint16_t *port = reinterpret_cast<const uint16_t *>(&hdr_buffer[8]);
                    port_ = htons(*port);
                    boost::asio::ip::address_v4 addr(ipv4_);
                    endpoint_ = tcp::endpoint(addr, port_);

                    BOOST_LOG_TRIVIAL(debug) << "ParseV5Header: IPv4 endpoint: "
                        << endpoint_;
                    done = true;
                }
                break;
            case 3: // Domainname
                header_len += 1; // name len
                if (header_len > hdr_buffer.size()) {
                    need_more_data = true;
                    goto again; // get more data
                } else {
                    const size_t name_len = static_cast<unsigned char>(hdr_buffer[4]);
                    header_len += name_len;

                    if (header_len > hdr_buffer.size()) {
                        need_more_data = true;
                        goto again; // get more data
                    }

                    std::string hostname(&hdr_buffer[5], name_len);
                    // TODO: Validate the name

                    if (hostname.empty()) {
                        BOOST_LOG_TRIVIAL(debug) << "ParseV5Header: No hostname!";
                        Reply(ReplyVal::REJECTED, {}, yield);
                        throw runtime_error("No hostname!");
                    }

                    const uint16_t *port = reinterpret_cast<const uint16_t *>(&hdr_buffer[header_len-2]);
                    port_ = htons(*port);

                    BOOST_LOG_TRIVIAL(info)
                        << "ParseV5Header: Will try to connect to: " << hostname;

                    tcp::resolver resolver(client_.get_io_service());
                    auto address_it = resolver.async_resolve({hostname, to_string(port_)},
                                                             yield);
                    decltype(address_it) addr_end;

                    if (address_it == addr_end) {
                        Reply(ReplyVal::RESOLVER_FAILED, {}, yield);
                        throw runtime_error("Failed to lookup domain-name");
                    }

                    endpoint_ = *address_it;

                    BOOST_LOG_TRIVIAL(debug) << "ParseV5Header: host-lookup endpoint: "
                        << endpoint_;
                    done = true;
                }
                break;
            case 4: // IPv6 address
                Reply(ReplyVal::ADDR_NOT_SUPPORTED, {}, yield);
                throw runtime_error("IPv6 not supported at this time");
            default:
                BOOST_LOG_TRIVIAL(error) << "ParseV5Header: Invalid address type "
                << static_cast<int>(hdr_buffer[3]);

                throw runtime_error("Invalid address type");
        }
    }

    if (hdr_buffer.size() > header_len) {
        const size_t remaining = hdr_buffer.size() - header_len;
        remaining_buffer_.resize(remaining);
        memcpy(&remaining_buffer_[0], &hdr_buffer[header_len], remaining);
    }
}

// Top-level on the extra coroutine for forwarding
void Proxy::RelayRoot(tcp::socket& from,
            tcp::socket& to,
            uint64_t& counter,
            boost::asio::yield_context yield) {
    try {
        Relay(from, to, counter, yield);
    } catch(const exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Proxy::RelayRoot: Caught exception: "
            << ex.what();
    } catch(const boost::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "Proxy::RelayRoot: Caught boost exception: "
            << boost::diagnostic_information(ex);
    }

    BOOST_LOG_TRIVIAL(debug) << "Proxy::RelayRoot coroutine is done";
}

void Proxy::Relay(tcp::socket& from,
            tcp::socket& to,
            uint64_t& counter,
            boost::asio::yield_context& yield) {

    Closer closer(closed_);
    array<char, 1024 * 4> buffer;
    while(!closed_ && from.is_open() && to.is_open()) {
        auto bytes = from.async_read_some(
            boost::asio::buffer(&buffer[0], buffer.size()), yield);

        boost::asio::async_write(to,
                                 boost::asio::buffer(&buffer[0], bytes),
                                 yield);

        counter += bytes;
    }
}

int Proxy::MapReplyToV4(ReplyVal code) {
    switch(code) {
        case ReplyVal::OK:
            return 90;
        case ReplyVal::REJECTED:
        default:
            return 91;
    }
}

int Proxy::MapReplyToV5(ReplyVal code) {
     switch(code) {
        case ReplyVal::OK:
            return 0;
        case ReplyVal::REJECTED:
            return 1;
        case ReplyVal::ADDR_NOT_SUPPORTED:
            return 8;
        case ReplyVal::CONN_REFUSED:
            return 4; // host unreachable
        case ReplyVal::RESOLVER_FAILED:
            return 4; // Network unreachable
        default:
            return 1;
    }
}


void Proxy::Reply(ReplyVal ReplyVal,
                  boost::asio::ip::tcp::endpoint ep,
                  boost::asio::yield_context& yield) {

    char buffer[16] = {};
    uint16_t *port = nullptr;
    uint32_t *ip = nullptr;
    size_t bytes = 0;

    if (protocol_ver_ == ProtocolVer::SOCKS5) {
        bytes = 10;
        buffer[1] = MapReplyToV5(ReplyVal);
        buffer[0] = 5;
        buffer[3] = 1; // ipv4 address
        port = reinterpret_cast<uint16_t *>(&buffer[8]);
        ip = reinterpret_cast<uint32_t *>(&buffer[4]);

    } else {
        bytes = 8;
        buffer[1] = MapReplyToV4(ReplyVal);
        port = reinterpret_cast<uint16_t *>(&buffer[2]);
        ip = reinterpret_cast<uint32_t *>(&buffer[4]);
    }

    *port = htons(ep.port());
    if (ep.address().is_v4()) {
        *ip = htonl(ep.address().to_v4().to_ulong());
    } else {
         // TODO: ipv6/hostname/SOCKS5
        *ip = 0;
    }

    assert(bytes < sizeof(buffer));

    boost::asio::async_write(client_,
                            boost::asio::buffer(buffer, bytes),
                            yield);

}

} // namespace


