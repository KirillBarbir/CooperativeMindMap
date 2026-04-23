#include "../../include/mind_map/network/sessions.hpp"
#include <boost/json.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <regex>

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

namespace mind_map::network {
    void http_session::send_response(http::response<http::string_body> &&res) {
        auto shared_res = std::make_shared<http::response<http::string_body> >(std::move(res));
        http::async_write(stream_, *shared_res,
                          asio::bind_executor(strand_, [self = shared_from_this(), shared_res](beast::error_code ec, std::size_t) {
                              if (!ec) {
                                  if (shared_res->need_eof()) {
                                      self->stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
                                  } else {
                                      self->do_read();
                                  }
                              }
                          }));
    }

    void http_session::on_read(beast::error_code ec, std::size_t) {
        if (ec == http::error::end_of_stream) {
            return;
        }

        if (ec) {
            return fail(ec, "read");
        }

        if (websocket::is_upgrade(req_)) {
            std::make_shared<websocket_session>(stream_.release_socket(), service_, manager_)
                ->run(std::move(req_), std::move(buffer_));
            return;
        }

        handle_request();
    }

    void http_session::handle_request() {
        http::response<http::string_body> res{http::status::ok, req_.version()};
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req_.keep_alive());

        try {
            auto target_sv = req_.target();
            std::string target(target_sv.data(), target_sv.size());
            std::string path = target;
            std::string query;
            size_t query_pos = target.find('?');
            if (query_pos != std::string::npos) {
                path = target.substr(0, query_pos);
                query = target.substr(query_pos + 1);
            }

            auto method = req_.method();

            if (method == http::verb::post && path == "/spaces") {
                try {
                    json::value jv = json::parse(req_.body());

                    if (!jv.is_object() || !jv.as_object().contains("user_id") || !jv.as_object().at("user_id").is_string()) {
                        res.result(http::status::bad_request);
                        res.body() = "{\"error\": \"Missing or invalid user_id\"}";
                    } else {
                        UserId owner_id = std::string(jv.as_object().at("user_id").as_string());
                        SpaceId space_id;
                        OperationResult or_ = service_.create_space(owner_id, space_id);
                        if (or_.code == StatusCode::Ok) {
                            json::object result;
                            result["space_id"] = space_id;
                            result["revision"] = or_.space_revision;
                            res.body() = json::serialize(result);
                        } else {
                            res.result(http::status::bad_request);
                            res.body() = "{\"error\": \"Failed to create space\"}";
                        }
                    }
                } catch (...) {
                    res.result(http::status::bad_request);
                    res.body() = "{\"error\": \"Invalid JSON\"}";
                }
            } else if (method == http::verb::get && std::regex_match(path, std::regex("/spaces/[^/]+"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)"));
                SpaceId space_id = match[1];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());
                Space out;
                StatusCode status = service_.get_space(actor_id, space_id, out);
                if (status == StatusCode::Ok) {
                    json::object obj;
                    obj["id"] = out.id;
                    obj["owner_id"] = out.owner_id;
                    obj["revision"] = out.revision;

                    json::array members;
                    for (const auto &m: out.memberships) {
                        members.push_back(json::object{{"user_id", m.user_id}, {"role", static_cast<int>(m.role)}});
                    }
                    obj["members"] = members;

                    std::vector<Node> nodes;
                    if (service_.list_nodes(actor_id, space_id, nodes) == StatusCode::Ok) {
                        json::array nodes_arr;
                        for (const auto &n: nodes) {
                            nodes_arr.push_back(json::object{
                                {"id", n.id},
                                {"title", n.title},
                                {"content", n.content},
                                {"content_version", n.content_version},
                                {"x", n.x},
                                {"y", n.y}
                            });
                        }
                        obj["nodes"] = nodes_arr;
                    }

                    std::vector<Edge> edges;
                    if (service_.list_edges(actor_id, space_id, edges) == StatusCode::Ok) {
                        json::array edges_arr;
                        for (const auto &e: edges) {
                            edges_arr.push_back(json::object{
                                {"id", e.id},
                                {"from", e.from},
                                {"to", e.to}
                            });
                        }
                        obj["edges"] = edges_arr;
                    }

                    json::array comments_arr;
                    std::vector<Node> all_nodes;
                    if (service_.list_nodes(actor_id, space_id, all_nodes) == StatusCode::Ok) {
                        for (const auto& node : all_nodes) {
                            std::vector<Comment> node_comments;
                            if (service_.list_comments(actor_id, space_id, node.id, node_comments) == StatusCode::Ok) {
                                for (const auto& c : node_comments) {
                                    json::object c_obj;
                                    c_obj["id"] = c.id;
                                    c_obj["node_id"] = c.anchor.node_id;
                                    c_obj["author_id"] = c.author_id;
                                    c_obj["text"] = c.text;
                                    comments_arr.push_back(c_obj);
                                }
                            }
                        }
                    }
                    obj["comments"] = comments_arr;

                    res.body() = json::serialize(obj);
                } else if (status == StatusCode::AccessDenied) {
                    res.result(http::status::forbidden);
                } else if (status == StatusCode::NotFound) {
                    res.result(http::status::not_found);
                } else {
                    res.result(http::status::internal_server_error);
                }
            } else if (method == http::verb::post && std::regex_match(path, std::regex("/spaces/[^/]+/invites"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)/invites"));
                SpaceId space_id = match[1];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());
                
                try {
                    json::value jv = json::parse(req_.body());
                    if (!jv.is_object() || !jv.as_object().contains("user_id") || !jv.as_object().contains("role")) {
                        res.result(http::status::bad_request);
                        res.body() = "{\"error\": \"Missing user_id or role\"}";
                    } else {
                        UserId invitee_id = jv.as_object().at("user_id").as_string().c_str();
                        Role role = static_cast<Role>(jv.as_object().at("role").as_int64());
                        OperationResult or_ = service_.invite_user(actor_id, space_id, invitee_id, role);
                        if (or_.code == StatusCode::Ok) {
                            json::object result;
                            result["revision"] = or_.space_revision;
                            res.body() = json::serialize(result);
                        } else {
                            res.result(http::status::bad_request);
                        }
                    }
                } catch (...) {
                    res.result(http::status::bad_request);
                    res.body() = "{\"error\": \"Invalid JSON\"}";
                }
            } else if (method == http::verb::post && std::regex_match(path, std::regex("/spaces/[^/]+/nodes"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)/nodes"));
                SpaceId space_id = match[1];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());
                NodeId node_id;
                OperationResult or_ = service_.create_node(actor_id, space_id, node_id);
                if (or_.code == StatusCode::Ok) {
                    json::object result;
                    result["node_id"] = node_id;
                    result["revision"] = or_.space_revision;
                    res.body() = json::serialize(result);
                    
                    json::object update;
                    update["type"] = "node_created";
                    update["node_id"] = node_id;
                    update["revision"] = or_.space_revision;
                    manager_.broadcast(space_id, json::serialize(update));
                } else {
                    res.result(http::status::forbidden);
                }
            } else if (method == http::verb::put && std::regex_match(
                           path, std::regex("/spaces/[^/]+/nodes/[^/]+/content"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)/nodes/([^/]+)/content"));
                SpaceId space_id = match[1];
                NodeId node_id = match[2];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());
                
                try {
                    json::value jv = json::parse(req_.body());
                    if (!jv.is_object() || !jv.as_object().contains("content")) {
                        res.result(http::status::bad_request);
                    } else {
                        std::string new_content = jv.as_object().at("content").as_string().c_str();
                        OperationResult or_ = service_.set_node_content(actor_id, space_id, node_id, new_content);
                        if (or_.code == StatusCode::Ok) {
                            json::object result;
                            result["revision"] = or_.space_revision;
                            res.body() = json::serialize(result);

                            json::object update;
                            update["type"] = "node_updated";
                            update["node_id"] = node_id;
                            update["content"] = new_content;
                            update["revision"] = or_.space_revision;
                            manager_.broadcast(space_id, json::serialize(update));
                        } else {
                            res.result(http::status::forbidden);
                        }
                    }
                } catch (...) {
                    res.result(http::status::bad_request);
                }
            } else if (method == http::verb::put && std::regex_match(
                           path, std::regex("/spaces/[^/]+/nodes/[^/]+/title"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)/nodes/([^/]+)/title"));
                SpaceId space_id = match[1];
                NodeId node_id = match[2];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());

                try {
                    json::value jv = json::parse(req_.body());
                    if (!jv.is_object() || !jv.as_object().contains("title")) {
                        res.result(http::status::bad_request);
                    } else {
                        std::string new_title = jv.as_object().at("title").as_string().c_str();
                        OperationResult or_ = service_.set_node_title(actor_id, space_id, node_id, new_title);
                        if (or_.code == StatusCode::Ok) {
                            json::object result;
                            result["revision"] = or_.space_revision;
                            res.body() = json::serialize(result);

                            json::object update;
                            update["type"] = "node_title_updated";
                            update["node_id"] = node_id;
                            update["title"] = new_title;
                            update["revision"] = or_.space_revision;
                            manager_.broadcast(space_id, json::serialize(update));
                        } else {
                            res.result(http::status::forbidden);
                        }
                    }
                } catch (...) {
                    res.result(http::status::bad_request);
                }
            } else if (method == http::verb::put && std::regex_match(
                           path, std::regex("/spaces/[^/]+/nodes/[^/]+/position"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)/nodes/([^/]+)/position"));
                SpaceId space_id = match[1];
                NodeId node_id = match[2];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());

                try {
                    json::value jv = json::parse(req_.body());
                    if (!jv.is_object() || !jv.as_object().contains("x") || !jv.as_object().contains("y")) {
                        res.result(http::status::bad_request);
                    } else {
                        double x = 0;
                        try {
                            if (jv.as_object().at("x").is_double()) {
                                x = jv.as_object().at("x").as_double();
                            } else if (jv.as_object().at("x").is_int64()) {
                                x = static_cast<double>(jv.as_object().at("x").as_int64());
                            }
                        } catch (...) {}

                        double y = 0;
                        try {
                            if (jv.as_object().at("y").is_double()) {
                                y = jv.as_object().at("y").as_double();
                            } else if (jv.as_object().at("y").is_int64()) {
                                y = static_cast<double>(jv.as_object().at("y").as_int64());
                            }
                        } catch (...) {}

                        OperationResult or_ = service_.set_node_position(actor_id, space_id, node_id, x, y);
                        if (or_.code == StatusCode::Ok) {
                            json::object result;
                            result["revision"] = or_.space_revision;
                            res.body() = json::serialize(result);

                            json::object update;
                            update["type"] = "node_moved";
                            update["node_id"] = node_id;
                            update["x"] = x;
                            update["y"] = y;
                            update["revision"] = or_.space_revision;
                            manager_.broadcast(space_id, json::serialize(update));
                        } else {
                            res.result(http::status::forbidden);
                        }
                    }
                } catch (...) {
                    res.result(http::status::bad_request);
                }
            } else if (method == http::verb::delete_ &&
                       std::regex_match(path, std::regex("/spaces/[^/]+/nodes/[^/]+"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)/nodes/([^/]+)"));
                SpaceId space_id = match[1];
                NodeId node_id = match[2];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());
                OperationResult or_ = service_.delete_node(actor_id, space_id, node_id);
                if (or_.code == StatusCode::Ok) {
                    json::object result;
                    result["revision"] = or_.space_revision;
                    res.body() = json::serialize(result);

                    json::object update;
                    update["type"] = "node_deleted";
                    update["node_id"] = node_id;
                    update["revision"] = or_.space_revision;
                    manager_.broadcast(space_id, json::serialize(update));
                } else {
                    res.result(http::status::forbidden);
                }
            } else if (method == http::verb::put && std::regex_match(
                           path, std::regex("/spaces/[^/]+/members/[^/]+/role"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)/members/([^/]+)/role"));
                SpaceId space_id = match[1];
                UserId member_id = match[2];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());
                
                try {
                    json::value jv = json::parse(req_.body());
                    if (!jv.is_object() || !jv.as_object().contains("role")) {
                        res.result(http::status::bad_request);
                    } else {
                        Role role = static_cast<Role>(jv.as_object().at("role").as_int64());
                        OperationResult or_ = service_.set_member_role(actor_id, space_id, member_id, role);
                        if (or_.code == StatusCode::Ok) {
                            json::object result;
                            result["revision"] = or_.space_revision;
                            res.body() = json::serialize(result);
                        } else {
                            res.result(http::status::forbidden);
                        }
                    }
                } catch (...) {
                    res.result(http::status::bad_request);
                }
            } else if (method == http::verb::post && std::regex_match(path, std::regex("/spaces/[^/]+/edges"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)/edges"));
                SpaceId space_id = match[1];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());
                
                try {
                    json::value jv = json::parse(req_.body());
                    if (!jv.is_object() || !jv.as_object().contains("from") || !jv.as_object().contains("to")) {
                        res.result(http::status::bad_request);
                    } else {
                        NodeId from = jv.as_object().at("from").as_string().c_str();
                        NodeId to = jv.as_object().at("to").as_string().c_str();
                        EdgeId edge_id;
                        OperationResult or_ = service_.create_edge(actor_id, space_id, from, to, edge_id);
                        if (or_.code == StatusCode::Ok) {
                            json::object result;
                            result["edge_id"] = edge_id;
                            result["revision"] = or_.space_revision;
                            res.body() = json::serialize(result);

                            json::object update;
                            update["type"] = "edge_created";
                            update["edge_id"] = edge_id;
                            update["from"] = from;
                            update["to"] = to;
                            update["revision"] = or_.space_revision;
                            manager_.broadcast(space_id, json::serialize(update));
                        } else {
                            res.result(http::status::forbidden);
                        }
                    }
                } catch (...) {
                    res.result(http::status::bad_request);
                }
            } else if (method == http::verb::delete_ &&
                       std::regex_match(path, std::regex("/spaces/[^/]+/edges/[^/]+"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)/edges/([^/]+)"));
                SpaceId space_id = match[1];
                EdgeId edge_id = match[2];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());
                OperationResult or_ = service_.delete_edge(actor_id, space_id, edge_id);
                if (or_.code == StatusCode::Ok) {
                    json::object result;
                    result["revision"] = or_.space_revision;
                    res.body() = json::serialize(result);

                    json::object update;
                    update["type"] = "edge_deleted";
                    update["edge_id"] = edge_id;
                    update["revision"] = or_.space_revision;
                    manager_.broadcast(space_id, json::serialize(update));
                } else {
                    res.result(http::status::forbidden);
                }
            } else if (method == http::verb::get && std::regex_match(path, std::regex("/spaces/[^/]+/edges"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)/edges"));
                SpaceId space_id = match[1];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());
                std::vector<Edge> edges;
                StatusCode status = service_.list_edges(actor_id, space_id, edges);
                if (status == StatusCode::Ok) {
                    json::array edge_list;
                    for (const auto &e: edges) {
                        json::object edge_obj;
                        edge_obj["id"] = e.id;
                        edge_obj["from"] = e.from;
                        edge_obj["to"] = e.to;
                        edge_list.push_back(edge_obj);
                    }
                    res.body() = json::serialize(edge_list);
                } else {
                    res.result(http::status::forbidden);
                }
            } else if (method == http::verb::post && std::regex_match(path, std::regex("/spaces/[^/]+/comments"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)/comments"));
                SpaceId space_id = match[1];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());
                
                try {
                    json::value jv = json::parse(req_.body());
                    if (!jv.is_object() || !jv.as_object().contains("text") || !jv.as_object().contains("node_id")) {
                        res.result(http::status::bad_request);
                    } else {
                        std::string text = jv.as_object().at("text").as_string().c_str();
                        CommentAnchor anchor;
                        anchor.node_id = jv.as_object().at("node_id").as_string().c_str();
                        if (jv.as_object().contains("range_start")) {
                            anchor.kind = CommentTargetKind::TextRange;
                            anchor.range_start = static_cast<std::size_t>(jv.as_object().at("range_start").as_int64());
                            anchor.range_end = static_cast<std::size_t>(jv.as_object().at("range_end").as_int64());
                        } else {
                            anchor.kind = CommentTargetKind::Node;
                        }

                        CommentId comment_id;
                        OperationResult or_ = service_.add_comment(actor_id, space_id, anchor, text, comment_id);
                        if (or_.code == StatusCode::Ok) {
                            json::object result;
                            result["comment_id"] = comment_id;
                            result["revision"] = or_.space_revision;
                            res.body() = json::serialize(result);

                            json::object update;
                            update["type"] = "comment_added";
                            update["comment_id"] = comment_id;
                            update["node_id"] = anchor.node_id;
                            update["text"] = text;
                            update["revision"] = or_.space_revision;
                            manager_.broadcast(space_id, json::serialize(update));
                        } else {
                            res.result(http::status::forbidden);
                        }
                    }
                } catch (...) {
                    res.result(http::status::bad_request);
                }
            } else if (method == http::verb::get && std::regex_match(path, std::regex("/spaces/[^/]+/comments"))) {
                std::smatch match;
                std::regex_search(path, match, std::regex("/spaces/([^/]+)/comments"));
                SpaceId space_id = match[1];
                auto auth_sv = req_[http::field::authorization];
                UserId actor_id = std::string(auth_sv.data(), auth_sv.size());

                std::string node_id;
                size_t pos = query.find("node_id=");
                if (pos != std::string::npos) {
                    node_id = query.substr(pos + 8);
                    size_t amp = node_id.find('&');
                    if (amp != std::string::npos) node_id = node_id.substr(0, amp);
                }

                std::vector<Comment> comments;
                StatusCode status = service_.list_comments(actor_id, space_id, node_id, comments);
                if (status == StatusCode::Ok) {
                    json::array comment_list;
                    for (const auto &c: comments) {
                        json::object c_obj;
                        c_obj["id"] = c.id;
                        c_obj["text"] = c.text;
                        c_obj["node_id"] = c.anchor.node_id;
                        comment_list.push_back(c_obj);
                    }
                    res.body() = json::serialize(comment_list);
                } else {
                    res.result(http::status::forbidden);
                }
            } else {
                res.result(http::status::not_found);
                res.body() = "Not Found";
            }
        } catch (const std::exception &e) {
            res.result(http::status::internal_server_error);
            res.body() = std::string("Error: ") + e.what();
        }

        res.prepare_payload();
        send_response(std::move(res));
    }

    void http_session::fail(beast::error_code ec, char const *what) {
        std::cerr << what << ": " << ec.message() << std::endl;
    }
}
