
#include <boost/log/trivial.hpp>

#include "shinysocks.h"


using namespace std;
using boost::asio::ip::tcp;

namespace shinysocks {

Listener::Listener(Manager& manager,
                   boost::asio::ip::tcp::endpoint endpoint)
: manager_{manager}, endpoint_{endpoint}
{
}

void Listener::StartAccepting() {

    BOOST_LOG_TRIVIAL(info) << "Listener on " << endpoint_;

    auto& ios = manager_.GetSomeIoService();

    boost::asio::spawn(ios,
                       bind(&Listener::StartAcceptingInt,
                            this, ref(ios),
                            std::placeholders::_1));
}


void Listener::StartAcceptingInt(boost::asio::io_service& ios,
                                 boost::asio::yield_context yield) {
    try {
        boost::asio::ip::tcp::acceptor acceptor(ios, endpoint_, true);

        while(acceptor.is_open()) {
            tcp::socket socket(manager_.GetSomeIoService());
            acceptor.async_accept(socket, yield);

            BOOST_LOG_TRIVIAL(info) << "Incoming connection on socket.";

            auto& ios = socket.get_executor();
            auto proxy = make_shared<Proxy>(move(socket));
            boost::asio::spawn(ios,
                               bind(&Proxy::Run,
                                    proxy,
                                    std::placeholders::_1));
        }
    } catch(const std::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "StartAccepting: Caught exception: "
            << ex.what() ;
    } catch(const boost::exception& ex) {
        BOOST_LOG_TRIVIAL(error) << "StartAccepting: Caught boost exception: "
            << boost::diagnostic_information(ex);
    }
}

} // namespace


