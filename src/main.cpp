#include "mind_map/network/server.hpp"
#include "mind_map/services/mind_map_service.hpp"
#include <iostream>
#include <thread>
#include <boost/asio/signal_set.hpp>

int main(int argc, char *argv[]) {
    try {
        unsigned short port = 8080;
        int threads = std::thread::hardware_concurrency();
        if (threads == 0) {
            threads = 2;
        }

        if (argc > 1) {
            port = static_cast<unsigned short>(std::atoi(argv[1]));
        }

        if (argc > 2) {
            threads = std::atoi(argv[2]);
        }

        mind_map::MindMapService service;
        mind_map::network::NetworkServer server(service, port, threads);
        server.start();

        asio::io_context signal_ioc;
        asio::signal_set signals(signal_ioc, SIGINT, SIGTERM);

        signals.async_wait([&](const boost::system::error_code &, int) {
            std::cout << "\nStopping server..." << std::endl;
            server.stop();
        });

        std::cout << "Server is running on port " << port << " with " << threads << " threads!" << std::endl;
        std::cout << "Press Ctrl+C to stop..." << std::endl;

        signal_ioc.run();
        std::cout << "Server stopped!" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error occured: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
