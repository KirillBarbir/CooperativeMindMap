#include "mind_map/services/mind_map_service.hpp"

#include <barrier>
#include <thread>

#include <atomic>
#include <chrono>
#include <gtest/gtest.h>

#include <vector>

using namespace mind_map;

TEST(Concurrency, ConcurrentReads) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);

    constexpr int kThreads = 8;
    constexpr int kIters = 500;
    std::barrier start(kThreads);
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&]() {
            start.arrive_and_wait();
            for (int i = 0; i < kIters; ++i) {
                std::vector<Node> nodes;
                if (svc.list_nodes("owner", sid, nodes) != StatusCode::Ok) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto &th: threads) {
        th.join();
    }
    EXPECT_EQ(failures.load(), 0);
}

TEST(Concurrency, ConcurrentWriterAndReaders) {
    MindMapService svc;
    SpaceId sid;
    ASSERT_EQ(svc.create_space("owner", sid).code, StatusCode::Ok);
    ASSERT_EQ(svc.invite_user("owner", sid, "writer", Role::Editor, 1).code, StatusCode::Ok);

    NodeId n;
    ASSERT_EQ(svc.create_node("owner", sid, n, 2).code, StatusCode::Ok);

    constexpr int kReaders = 6;
    constexpr int kDurationMs = 200;
    std::barrier start(kReaders + 1);
    std::atomic<bool> stop{false};
    std::atomic<int> failures{0};

    std::vector<std::thread> readers;
    for (int t = 0; t < kReaders; ++t) {
        readers.emplace_back([&]() {
            start.arrive_and_wait();
            while (!stop.load(std::memory_order_relaxed)) {
                Node node;
                if (svc.get_node("writer", sid, n, node) != StatusCode::Ok) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    std::thread writer([&]() {
        start.arrive_and_wait();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kDurationMs);
        int i = 0;
        while (std::chrono::steady_clock::now() < deadline) {
            Node cur;
            if (svc.get_node("writer", sid, n, cur) != StatusCode::Ok) {
                failures.fetch_add(1, std::memory_order_relaxed);
            } else if (svc.set_node_content("writer", sid, n, std::string("v") + std::to_string(i),
                                            std::nullopt, cur.content_version).code != StatusCode::Ok) {
                failures.fetch_add(1, std::memory_order_relaxed);
            }

            ++i;
        }

        stop.store(true, std::memory_order_relaxed);
    });

    for (auto &th: readers) {
        th.join();
    }

    writer.join();
    EXPECT_EQ(failures.load(), 0);
}
