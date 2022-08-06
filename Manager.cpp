
#include "shinysocks.h"
#include "logging.h"

/* TODO: Add handling of all open connections so we can output some nice stats
 */

using namespace std;

namespace shinysocks {

Manager::Manager(Conf& conf)
{
    if (conf.io_threads < 1) {
        throw runtime_error("Invalid thread-count");
    }

    for(int i = 0; i < conf.io_threads; ++i) {
        threads_.push_back(make_unique<Thread>());
        threads_.back()->Start();
    }
}

void Manager::Thread::Start() {
    thread_ = make_unique<thread>(bind(&Manager::Thread::Run, this));
}

void Manager::Thread::Run() {
    LOG_DEBUG << "Run: Starting io_service in one thread";

    // Keep the io-service running, also when the queue is empty
    boost::asio::io_service::work work(io_service_);

    // Run the io-service
    io_service_.run();

    LOG_DEBUG << "Run: Ended io_service in one thread";
}

void Manager::WaitForAllThreads() {
    for(auto& th : threads_) {
        th->GetThread().join();
    }
}


} // namespace
