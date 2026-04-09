#include <gtest/gtest.h>
#include "mind_map/network/server.hpp"
#include "mind_map/services/mind_map_service.hpp"
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
namespace json = boost::json;
using tcp = asio::ip::tcp;

class NetworkTest : public ::testing::Test {
protected:
    mind_map::MindMapService service;
    unsigned short port = 8080;
    std::unique_ptr<mind_map::network::NetworkServer> server;

    void SetUp() override {
        server = std::make_unique<mind_map::network::NetworkServer>(service, port, 4);
        server->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        server->stop();
    }

    http::response<http::string_body> send_request(http::verb method, std::string target, std::string body = "",
                                                   std::string auth = "") {
        asio::io_context ioc;
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);
        auto const results = resolver.resolve("127.0.0.1", std::to_string(port));
        stream.connect(results);

        http::request<http::string_body> req{method, target, 11};
        req.set(http::field::host, "127.0.0.1");
        req.set(http::field::content_type, "application/json");
        if (!auth.empty()) {
            req.set(http::field::authorization, auth);
        }
        req.body() = body;
        req.prepare_payload();

        http::write(stream, req);

        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);

        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);
        return res;
    }
};

TEST_F(NetworkTest, CreateSpaceAndGetIt) {
    json::object create_body;
    create_body["user_id"] = "owner1";
    auto res1 = send_request(http::verb::post, "/spaces", json::serialize(create_body));

    EXPECT_EQ(res1.result(), http::status::ok);
    auto jv1 = json::parse(res1.body());
    std::string space_id = jv1.as_object().at("space_id").as_string().c_str();
    EXPECT_FALSE(space_id.empty());

    auto res2 = send_request(http::verb::get, "/spaces/" + space_id, "", "owner1");
    EXPECT_EQ(res2.result(), http::status::ok);
    auto jv2 = json::parse(res2.body());
    EXPECT_EQ(jv2.as_object().at("id").as_string(), space_id);
    EXPECT_EQ(jv2.as_object().at("owner_id").as_string(), "owner1");
}

TEST_F(NetworkTest, CreateNodeAndSetContent) {
    json::object create_body;
    create_body["user_id"] = "owner1";
    auto res1 = send_request(http::verb::post, "/spaces", json::serialize(create_body));
    std::string space_id = json::parse(res1.body()).as_object().at("space_id").as_string().c_str();

    auto res2 = send_request(http::verb::post, "/spaces/" + space_id + "/nodes", "", "owner1");
    EXPECT_EQ(res2.result(), http::status::ok);
    std::string node_id = json::parse(res2.body()).as_object().at("node_id").as_string().c_str();
    json::object content_body;
    content_body["content"] = "Hello, Mind Map!";
    auto res3 = send_request(http::verb::put, "/spaces/" + space_id + "/nodes/" + node_id + "/content",
                             json::serialize(content_body), "owner1");
    EXPECT_EQ(res3.result(), http::status::ok);

    auto res4 = send_request(http::verb::get, "/spaces/" + space_id, "", "owner1");
    EXPECT_EQ(res4.result(), http::status::ok);
    auto jv4 = json::parse(res4.body());
    auto nodes = jv4.as_object().at("nodes").as_array();

    bool found = false;
    for (const auto &node_val: nodes) {
        auto node_obj = node_val.as_object();
        if (node_obj.at("id").as_string() == node_id) {
            EXPECT_EQ(node_obj.at("content").as_string(), "Hello, Mind Map!");
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(NetworkTest, EdgeCrud) {
    json::object create_body;
    create_body["user_id"] = "owner1";
    auto res1 = send_request(http::verb::post, "/spaces", json::serialize(create_body));
    std::string space_id = json::parse(res1.body()).as_object().at("space_id").as_string().c_str();

    auto res2 = send_request(http::verb::post, "/spaces/" + space_id + "/nodes", "", "owner1");
    std::string node1 = json::parse(res2.body()).as_object().at("node_id").as_string().c_str();
    auto res3 = send_request(http::verb::post, "/spaces/" + space_id + "/nodes", "", "owner1");
    std::string node2 = json::parse(res3.body()).as_object().at("node_id").as_string().c_str();

    json::object edge_body;
    edge_body["from"] = node1;
    edge_body["to"] = node2;
    auto res4 = send_request(http::verb::post, "/spaces/" + space_id + "/edges", json::serialize(edge_body), "owner1");
    EXPECT_EQ(res4.result(), http::status::ok);
    std::string edge_id = json::parse(res4.body()).as_object().at("edge_id").as_string().c_str();

    auto res5 = send_request(http::verb::get, "/spaces/" + space_id + "/edges", "", "owner1");
    EXPECT_EQ(res5.result(), http::status::ok);
    auto edges = json::parse(res5.body()).as_array();
    EXPECT_EQ(edges.size(), 1);
    EXPECT_EQ(edges[0].as_object().at("id").as_string(), edge_id);
}

TEST_F(NetworkTest, Comments) {
    json::object create_body;
    create_body["user_id"] = "owner1";
    auto res1 = send_request(http::verb::post, "/spaces", json::serialize(create_body));
    std::string space_id = json::parse(res1.body()).as_object().at("space_id").as_string().c_str();

    auto res2 = send_request(http::verb::post, "/spaces/" + space_id + "/nodes", "", "owner1");
    std::string node_id = json::parse(res2.body()).as_object().at("node_id").as_string().c_str();

    json::object comment_body;
    comment_body["node_id"] = node_id;
    comment_body["text"] = "Interesting node!";
    auto res3 = send_request(http::verb::post, "/spaces/" + space_id + "/comments", json::serialize(comment_body),
                             "owner1");
    EXPECT_EQ(res3.result(), http::status::ok);

    auto res4 = send_request(http::verb::get, "/spaces/" + space_id + "/comments?node_id=" + node_id, "", "owner1");
    EXPECT_EQ(res4.result(), http::status::ok);
    auto comments = json::parse(res4.body()).as_array();

    EXPECT_EQ(comments.size(), 1);
    EXPECT_EQ(comments[0].as_object().at("text").as_string(), "Interesting node!");
}

TEST_F(NetworkTest, DeleteNodeVerification) {
    json::object create_body;
    create_body["user_id"] = "owner1";
    auto res1 = send_request(http::verb::post, "/spaces", json::serialize(create_body));
    std::string space_id = json::parse(res1.body()).as_object().at("space_id").as_string().c_str();

    auto res2 = send_request(http::verb::post, "/spaces/" + space_id + "/nodes", "", "owner1");
    std::string node_id = json::parse(res2.body()).as_object().at("node_id").as_string().c_str();

    auto res3 = send_request(http::verb::delete_, "/spaces/" + space_id + "/nodes/" + node_id, "", "owner1");
    EXPECT_EQ(res3.result(), http::status::ok);

    auto res4 = send_request(http::verb::get, "/spaces/" + space_id, "", "owner1");
    auto nodes = json::parse(res4.body()).as_object().at("nodes").as_array();

    bool found = false;
    for (const auto &node_val: nodes) {
        if (node_val.as_object().at("id").as_string() == node_id) {
            found = true;
            break;
        }
    }
    EXPECT_FALSE(found);
}

TEST_F(NetworkTest, AccessDeniedForStranger) {
    json::object create_body;
    create_body["user_id"] = "owner1";
    auto res1 = send_request(http::verb::post, "/spaces", json::serialize(create_body));
    std::string space_id = json::parse(res1.body()).as_object().at("space_id").as_string().c_str();

    auto res2 = send_request(http::verb::get, "/spaces/" + space_id, "", "stranger1");
    EXPECT_EQ(res2.result(), http::status::forbidden);
}

TEST_F(NetworkTest, MultithreadedLoad) {
    json::object create_body;
    create_body["user_id"] = "owner1";
    auto res1 = send_request(http::verb::post, "/spaces", json::serialize(create_body));
    std::string space_id = json::parse(res1.body()).as_object().at("space_id").as_string().c_str();

    const int num_threads = 10;
    const int reqs_per_thread = 20;
    std::vector<std::thread> clients;
    std::atomic<int> success_count{0};

    for (int i = 0; i < num_threads; ++i) {
        clients.emplace_back([&, i]() {
            for (int j = 0; j < reqs_per_thread; ++j) {
                auto res = send_request(http::verb::get, "/spaces/" + space_id, "", "owner1");
                if (res.result() == http::status::ok) {
                    ++success_count;
                }
            }
        });
    }

    for (auto &t: clients) {
        t.join();
    }

    EXPECT_EQ(success_count, num_threads * reqs_per_thread);
}
