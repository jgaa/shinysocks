#pragma once


//#include <iostream>
#include <atomic>
#include <thread>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <memory>

namespace shinysocks {

class Closer
{
public:
    Closer(bool& closed) : closed_{closed} {}
    ~Closer() {
        closed_ = true;
    }
private:
    bool& closed_;
};


class Manager
{
    class Thread {
    public:
        Thread() = default;

        void Start();

        void Stop() {
            if (!io_service_.stopped()) {
                io_service_.stop();
            }
        }

        boost::asio::io_service& GetIoService() { return io_service_; }
        std::thread& GetThread() { return *thread_; }

    private:
        void Run();
        std::unique_ptr<std::thread> thread_;
        boost::asio::io_service io_service_;
    };

public:
    struct Conf {
        int io_threads = 2;
    };

    Manager(Conf& conf);

    boost::asio::io_service& GetSomeIoService() {
        return threads_[++next_io_service_ % threads_.size()]->GetIoService();
        //return threads_[0]->GetIoService();
    }

    void Shutdown() {
        for(auto& th: threads_) {
            th->Stop();
        }
    }

    void WaitForAllThreads();

private:

    std::vector<std::unique_ptr<Thread>> threads_;
    std::atomic_int next_io_service_;
};


class Listener
{
public:
    Listener(Manager& manager,
             boost::asio::ip::tcp::endpoint endpoint);


    void StartAccepting();

private:
    void StartAcceptingInt(boost::asio::io_service& ios,
                           boost::asio::yield_context yield);

    Manager& manager_;
    const boost::asio::ip::tcp::endpoint endpoint_;
};


class Proxy : public std::enable_shared_from_this<Proxy>
{
public:
    enum class Commands { CONNECT = 1 };
    enum class ProtocolVer { INVALID = 0, SOCKS4 = 4, SOCKS5 = 5};
    enum class ReplyVal { OK, REJECTED, ADDR_NOT_SUPPORTED, CONN_REFUSED,
        RESOLVER_FAILED
    };

    Proxy(boost::asio::ip::tcp::socket&& sck);
    ~Proxy();

    void Run(boost::asio::yield_context yield);

private:
    void ParseV4Header(const char *buffer,
                         const size_t len,
                         boost::asio::yield_context& yield);
    void ParseV5Header(const char *buffer,
                         const size_t len,
                         boost::asio::yield_context& yield);

    void RunInt(boost::asio::yield_context& yield);
    void RelayRoot(boost::asio::ip::tcp::socket& from,
                   boost::asio::ip::tcp::socket& to,
                   uint64_t& counter,
                   boost::asio::yield_context yield);
    void Relay(boost::asio::ip::tcp::socket& from,
               boost::asio::ip::tcp::socket& to,
               uint64_t& counter,
               boost::asio::yield_context& yield);
    void Reply(ReplyVal replyCode,
               boost::asio::ip::tcp::endpoint ep,
               boost::asio::yield_context& yield);

    int MapReplyToV4(ReplyVal code);
    int MapReplyToV5(ReplyVal code);

    boost::asio::ip::tcp::socket client_;
    boost::asio::ip::tcp::socket server_;
    bool closed_ = false;
    uint64_t bytes_relayed_to_server_ = 0;
    uint64_t bytes_relayed_to_client_ = 0;
    int command_ = -1;
    uint16_t port_ = 0;
    uint32_t ipv4_ = 0;
    boost::asio::ip::tcp::endpoint endpoint_;
    std::string name_;
    std::vector<char> remaining_buffer_;
    ProtocolVer protocol_ver_ = ProtocolVer::INVALID;;
};

} // namespace



