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
    void session_manager::join(SpaceId space_id, std::shared_ptr<websocket_session> session) {
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_[space_id].push_back(std::move(session));
    }

    void session_manager::leave(SpaceId space_id, std::shared_ptr<websocket_session> session) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(space_id);
        if (it != sessions_.end()) {
            auto& v = it->second;
            v.erase(std::remove(v.begin(), v.end(), session), v.end());
            if (v.empty()) {
                sessions_.erase(it);
            }
        }
    }

    void session_manager::broadcast(SpaceId space_id, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(space_id);
        if (it != sessions_.end()) {
            auto ss = std::make_shared<const std::string>(message);
            for (auto const& session : it->second) {
                session->send(ss);
            }
        }
    }

    void websocket_session::send(const std::shared_ptr<const std::string>& ss) {
        asio::post(strand_, [self = shared_from_this(), ss]() {
            self->write_queue_.push_back(ss);
            if (self->write_queue_.size() > 1) {
                return;
            }

            self->ws_.text(true);
            self->ws_.async_write(asio::buffer(*self->write_queue_.front()),
                                  asio::bind_executor(self->strand_, beast::bind_front_handler(&websocket_session::on_write, self)));
        });
    }

    void websocket_session::on_write(beast::error_code ec, std::size_t) {
        if (ec) {
            return fail(ec, "write");
        }

        write_queue_.erase(write_queue_.begin());
        if (!write_queue_.empty()) {
            ws_.text(true);
            ws_.async_write(asio::buffer(*write_queue_.front()),
                            asio::bind_executor(strand_, beast::bind_front_handler(&websocket_session::on_write, shared_from_this())));
        }
    }

    void websocket_session::on_accept(beast::error_code ec) {
        if (ec) {
            return fail(ec, "accept");
        }
        buffer_.consume(buffer_.size());
        do_read();
    }

    void websocket_session::on_read(beast::error_code ec, std::size_t) {
        if (ec == websocket::error::closed) {
            if (!space_id_.empty()) {
                manager_.leave(space_id_, shared_from_this());
            }
            return;
        }

        if (ec) {
            if (!space_id_.empty()) {
                manager_.leave(space_id_, shared_from_this());
            }
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

                if (space_id_ != space_id) {
                    if (!space_id_.empty()) {
                        manager_.leave(space_id_, shared_from_this());
                    }
                    space_id_ = space_id;
                    user_id_ = actor_id;
                    manager_.join(space_id_, shared_from_this());
                }

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

                manager_.broadcast(space_id_, json::serialize(res));
            } else if (!space_id_.empty()) {
                manager_.broadcast(space_id_, data);
            }
        } catch (const std::exception &e) {
            std::cerr << "WebSocket error: " << e.what() << std::endl;
        }

        buffer_.consume(buffer_.size());
        do_read();
    }

    void websocket_session::fail(beast::error_code ec, char const *what) {
        std::cerr << what << ": " << ec.message() << std::endl;
    }
}
