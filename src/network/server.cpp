#include "mind_map/network/server.hpp"
#include "../../include/mind_map/network/sessions.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

namespace mind_map::network {
    class listener : public std::enable_shared_from_this<listener> {
        asio::io_context &ioc_;
        tcp::acceptor acceptor_;
        MindMapService &service_;

    public:
        listener(asio::io_context &ioc, const tcp::endpoint &endpoint,
                 MindMapService &service) : ioc_(ioc), acceptor_(ioc), service_(service) {
            beast::error_code ec;
            acceptor_.open(endpoint.protocol(), ec);
            if (ec) {
                fail(ec, "open");
                return;
            }

            acceptor_.bind(endpoint, ec);
            if (ec) {
                fail(ec, "bind");
                return;
            }

            acceptor_.listen(asio::socket_base::max_listen_connections, ec);
            if (ec) {
                fail(ec, "listen");
                return;
            }
        }

        void run() {
            do_accept();
        }

    private:
        static void fail(beast::error_code ec, char const *what) {
            std::cerr << what << ": " << ec.message() << std::endl;
        }

        void do_accept() {
            acceptor_.async_accept(ioc_, beast::bind_front_handler(&listener::on_accept, shared_from_this()));
        }

        void on_accept(beast::error_code ec, tcp::socket socket) {
            if (ec) {
                return fail(ec, "accept");
            }
            std::make_shared<http_session>(std::move(socket), service_)->run();
            do_accept();
        }
    };

    NetworkServer::NetworkServer(MindMapService &service, unsigned short port, int threads) : service_(service),
        ioc_(threads), port_(port), threads_(threads) {
    }

    void NetworkServer::start() {
        auto endpoint = tcp::endpoint{asio::ip::make_address("127.0.0.1"), port_};

        std::make_shared<listener>(ioc_, endpoint, service_)->run();

        thread_pool_.reserve(threads_);
        for (int i = 0; i < threads_; ++i) {
            thread_pool_.emplace_back([this] {
                ioc_.run();
            });
        }
    }

    void NetworkServer::stop() {
        ioc_.stop();
        for (auto &t: thread_pool_) {
            if (t.joinable()) {
                t.join();
            }
        }
        thread_pool_.clear();
    }
}
