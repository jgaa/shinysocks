#include <iostream>
#include <boost/program_options.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>


#include "shinysocks.h"


using namespace std;
using namespace shinysocks;
using boost::asio::ip::tcp;

void SleepUntilDoomdsay()
{
    boost::asio::io_service main_thread_service;

    boost::asio::signal_set signals(main_thread_service, SIGINT, SIGTERM
#ifdef SIGQUIT
        ,SIGQUIT
#endif
        );
    signals.async_wait([](boost::system::error_code /*ec*/, int signo) {

        BOOST_LOG_TRIVIAL(info) << "Reiceived signal " << signo << ". Shutting down";
    });

    BOOST_LOG_TRIVIAL(debug) << "Main thread going to sleep - waiting for shtudown signal";
    main_thread_service.run();
    BOOST_LOG_TRIVIAL(debug) << "Main thread is awake";
}

int main(int argc, char **argv) {

    namespace po = boost::program_options;
    namespace pt = boost::property_tree;
    bool run_as_daemon = false;

    pt::ptree opts;
    vector<unique_ptr<Listener>> listeners;

    {
        std::string conf_file = "shinysocks.conf";

        po::options_description general("General Options");

        general.add_options()
            ("help,h", "Print help and exit")
            ("config-file,c",
                po::value<string>(&conf_file)->default_value(conf_file),
                "Configuration file")
#ifndef WIN32
            ("daemon",  po::value<bool>(&run_as_daemon), "Run as a system daemon")
#endif
            ;

        po::options_description cmdline_options;
        cmdline_options.add(general);

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, cmdline_options), vm);
        po::notify(vm);

        if (vm.count("help")) {
            cout << cmdline_options << endl;
            return -1;
        }

        if (!boost::filesystem::exists(conf_file)) {
            BOOST_LOG_TRIVIAL(error) << "*** The configuration-file '"
                << conf_file
                << "' does not exist.";
            return -1;
        }
        read_info(conf_file, opts);

        if (auto log_file = opts.get_optional<string>("log.file")) {
            cout << "Opening log-file: " << *log_file << endl;
            //boost::log::add_file_log(log_file->c_str());

            boost::log::add_file_log(
                boost::log::keywords::file_name = *log_file,
                boost::log::keywords::target = "boost_logs",
                boost::log::keywords::format = "%TimeStamp% %ThreadID% %Severity%: %Message%",
                boost::log::keywords::auto_flush = true
            );

            boost::log::add_common_attributes();

            boost::log::core::get()->set_filter (
                boost::log::trivial::severity >= boost::log::trivial::debug);
        }
    }

    Manager::Conf conf;
    if (auto threads = opts.get_optional<int>("system.io-threads")) {
        conf.io_threads = *threads;
    }

    // Start acceptor(s)
    Manager manager(conf);
    {
        boost::asio::io_service io_service;
        for(auto &node :  opts.get_child("interfaces")) {

            auto host = node.second.get<string>("hostname");
            auto port = node.second.get<string>("port");

            BOOST_LOG_TRIVIAL(error) << "Resolving host=" << host << ", port=" << port;

            tcp::resolver resolver(io_service);
            auto address_it = resolver.resolve({host, port});
            decltype(address_it) addr_end;

            for(; address_it != addr_end; ++address_it) {
                auto iface = make_unique<Listener>(manager, *address_it);
                iface->StartAccepting();
                listeners.push_back(move(iface));
            }
        }
    }

#ifndef WIN32
    if (run_as_daemon) {
        BOOST_LOG_TRIVIAL(info) << "Switching to system daemon mode";
        daemon(1, 0);
    }
#endif


    // Wait for signal
    SleepUntilDoomdsay();

    manager.Shutdown();

    // Just wait for the IO thread(s) to end.
    manager.WaitForAllThreads();

    return 0;
}
