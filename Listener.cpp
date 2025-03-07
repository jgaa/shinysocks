
#include "shinysocks.h"
#include "logging.h"
#include <boost/exception/diagnostic_information.hpp>

using namespace std;
using boost::asio::ip::tcp;

namespace shinysocks {

Listener::Listener(Manager& manager,
                   boost::asio::ip::tcp::endpoint endpoint)
: manager_{manager}, endpoint_{endpoint}
{
}

void Listener::StartAccepting() {

    LOG_INFO << "Listener on " << endpoint_;

    auto& ios = manager_.GetSomeIoService();

    boost::asio::spawn(ios,
                       bind(&Listener::StartAcceptingInt,
                            this, ref(ios),
                            std::placeholders::_1),
                       boost::asio::detached);
}


void Listener::StartAcceptingInt(boost::asio::io_context& ios,
                                 boost::asio::yield_context yield) {
    try {
        boost::asio::ip::tcp::acceptor acceptor(ios, endpoint_, true);

        while(acceptor.is_open()) {
            tcp::socket socket(manager_.GetSomeIoService());
            acceptor.async_accept(socket, yield);

            LOG_INFO << "Incoming connection on socket.";

            auto proxy = make_shared<Proxy>(move(socket));
            boost::asio::spawn(socket.GET_IO_CONTEXT_OR_EXECURTOR(),
                               bind(&Proxy::Run,
                                    proxy,
                                    std::placeholders::_1),
                               boost::asio::detached);
        }
    } catch (const boost::exception& ex) {
        LOG_ERROR << "StartAccepting: Caught boost exception: "
                  << boost::diagnostic_information(ex);
    } catch (const std::exception& ex) {
        LOG_ERROR << "StartAccepting: Caught standard exception: "
                  << ex.what();
    }
}

} // namespace


