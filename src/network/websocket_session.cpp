#include "../../include/mind_map/network/sessions.hpp"
#include <boost/json.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <regex>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace json = boost::json;

namespace mind_map::network {
    void websocket_session::on_accept(beast::error_code ec) {
        if (ec) {
            return fail(ec, "accept");
        }
        do_read();
    }

    void websocket_session::do_read() {
        ws_.async_read(buffer_, beast::bind_front_handler(&websocket_session::on_read, shared_from_this()));
    }

    void websocket_session::on_read(beast::error_code ec, std::size_t) {
        if (ec == websocket::error::closed) {
            return;
        }

        if (ec) {
            return fail(ec, "read");
        }

        try {
            std::string data = beast::buffers_to_string(buffer_.data());
            json::value jv = json::parse(data);
            json::object const &obj = jv.as_object();

            std::string type = obj.at("type").as_string().c_str();

            if (type == "presence_update") {
                UserId actor_id = obj.at("user_id").as_string().c_str();
                SpaceId space_id = obj.at("space_id").as_string().c_str();

                std::optional<NodeId> focused_node;
                if (obj.contains("node_id") && !obj.at("node_id").is_null()) {
                    focused_node = obj.at("node_id").as_string().c_str();
                }

                std::size_t cursor_offset = 0;
                if (obj.contains("cursor_offset")) {
                    cursor_offset = static_cast<std::size_t>(obj.at("cursor_offset").as_int64());
                }

                service_.update_presence(actor_id, space_id, focused_node, cursor_offset);

                std::vector<Presence> presences;
                service_.list_presence(actor_id, space_id, presences);

                json::array presence_list;
                for (const auto &p: presences) {
                    json::object presence_obj;
                    presence_obj["user_id"] = p.user_id;
                    if (p.focused_node.has_value()) {
                        presence_obj["node_id"] = *p.focused_node;
                    } else {
                        presence_obj["node_id"] = nullptr;
                    }
                    presence_obj["cursor_offset"] = p.cursor_offset;
                    presence_list.push_back(presence_obj);
                }

                json::object res;
                res["type"] = "presence_list";
                res["presences"] = std::move(presence_list);

                auto s = json::serialize(res);
                ws_.text(ws_.got_text());
                ws_.async_write(asio::buffer(s),
                                beast::bind_front_handler(&websocket_session::on_write, shared_from_this()));
            }
        } catch (const std::exception &e) {
            std::cerr << "WebSocket error: " << e.what() << std::endl;
        }

        buffer_.consume(buffer_.size());
        do_read();
    }

    void websocket_session::on_write(beast::error_code ec, std::size_t) {
        if (ec) {
            return fail(ec, "write");
        }
    }

    void websocket_session::fail(beast::error_code ec, char const *what) {
        std::cerr << what << ": " << ec.message() << std::endl;
    }
}
