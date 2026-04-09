#pragma once

#include "mind_map/services/mind_map_service.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>
#include <memory>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

namespace mind_map::network {
    class websocket_session;

    class http_session : public std::enable_shared_from_this<http_session> {
        beast::tcp_stream stream_;
        beast::flat_buffer buffer_;
        MindMapService &service_;
        http::request<http::string_body> req_;

    public:
        http_session(tcp::socket &&socket, MindMapService &service) : stream_(std::move(socket)), service_(service) {
        }

        void run() {
            do_read();
        }

    private:
        void do_read() {
            req_ = {};
            stream_.expires_after(std::chrono::seconds(30));
            http::async_read(stream_, buffer_, req_,
                             beast::bind_front_handler(&http_session::on_read, shared_from_this()));
        }

        void on_read(beast::error_code ec, std::size_t bytes_transferred);

        void handle_request();

        void send_response(http::response<http::string_body> &&res);

        static void fail(beast::error_code ec, char const *what);
    };


    class websocket_session : public std::enable_shared_from_this<websocket_session> {
        websocket::stream<beast::tcp_stream> ws_;
        beast::flat_buffer buffer_;
        MindMapService &service_;
        SpaceId space_id_;
        UserId user_id_;

    public:
        websocket_session(tcp::socket &&socket, MindMapService &service) : ws_(std::move(socket)), service_(service) {
        }

        template<class Body, class Fields>
        void run(http::request<Body, Fields> req) {
            ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
            ws_.async_accept(req, beast::bind_front_handler(&websocket_session::on_accept, shared_from_this()));
        }

    private:
        void on_accept(beast::error_code ec);

        void do_read();

        void on_read(beast::error_code ec, std::size_t bytes_transferred);

        void on_write(beast::error_code ec, std::size_t bytes_transferred);

        static void fail(beast::error_code ec, char const *what);
    };
}
