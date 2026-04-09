#pragma once

#include "mind_map/services/mind_map_service.hpp"
#include <boost/asio.hpp>

#include <vector>
#include <thread>

namespace mind_map::network {
    class NetworkServer {
    public:
        NetworkServer(MindMapService &service, unsigned short port, int threads);

        void start();

        void stop();

    private:
        MindMapService &service_;
        boost::asio::io_context ioc_;
        unsigned short port_;
        int threads_;
        std::vector<std::thread> thread_pool_;
    };
}
