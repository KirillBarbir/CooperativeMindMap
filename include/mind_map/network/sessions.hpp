#pragma once

#include "mind_map/services/mind_map_service.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <boost/asio/strand.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

namespace mind_map::network {
    class websocket_session;

    class session_manager {
        std::mutex mutex_;
        std::unordered_map<SpaceId, std::vector<std::shared_ptr<websocket_session>>> sessions_;

    public:
        void join(SpaceId space_id, std::shared_ptr<websocket_session> session);
        void leave(SpaceId space_id, std::shared_ptr<websocket_session> session);
        void broadcast(SpaceId space_id, const std::string& message);
    };

    class http_session : public std::enable_shared_from_this<http_session> {
        beast::tcp_stream stream_;
        beast::flat_buffer buffer_;
        MindMapService &service_;
        session_manager &manager_;
        http::request<http::string_body> req_;
        asio::any_io_executor strand_;

    public:
        explicit http_session(tcp::socket &&socket, MindMapService &service, session_manager &manager) 
            : stream_(std::move(socket)), service_(service), manager_(manager),
              strand_(asio::make_strand(stream_.get_executor())) {
        }

        void run() {
            do_read();
        }

    private:
        void do_read() {
            req_ = {};
            stream_.expires_after(std::chrono::seconds(30));
            http::async_read(stream_, buffer_, req_,
                             asio::bind_executor(strand_, beast::bind_front_handler(&http_session::on_read, shared_from_this())));
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
        session_manager &manager_;
        SpaceId space_id_;
        UserId user_id_;
        std::vector<std::shared_ptr<const std::string>> write_queue_;
        asio::any_io_executor strand_;

    public:
        websocket_session(tcp::socket &&socket, MindMapService &service, session_manager &manager) 
            : ws_(std::move(socket)), service_(service), manager_(manager),
              strand_(asio::make_strand(ws_.get_executor())) {
        }

        void send(const std::shared_ptr<const std::string>& ss);

        template<class Body, class Allocator>
        void run(http::request<Body, http::basic_fields<Allocator>> req, beast::flat_buffer&& buffer) {
            buffer_ = std::move(buffer);
            ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
            ws_.async_accept(req, asio::bind_executor(strand_, beast::bind_front_handler(&websocket_session::on_accept, shared_from_this())));
        }

    private:
        void on_accept(beast::error_code ec);

        void do_read() {
            ws_.async_read(buffer_, asio::bind_executor(strand_, beast::bind_front_handler(&websocket_session::on_read, shared_from_this())));
        }

        void on_read(beast::error_code ec, std::size_t bytes_transferred);

        void on_write(beast::error_code ec, std::size_t bytes_transferred);

        static void fail(beast::error_code ec, char const *what);
    };
}
