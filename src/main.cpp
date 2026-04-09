#include "mind_map/network/server.hpp"
#include "mind_map/services/mind_map_service.hpp"
#include <iostream>
#include <thread>

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
        std::cout << "Server is running!" << std::endl;
        std::cout << "Press enter to stop..." << std::endl;
        std::cin.get();
        server.stop();
        std::cout << "Server stopped!" << std::endl;
    } catch (const std::exception &e) {
        std::cerr << "Error occured: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
