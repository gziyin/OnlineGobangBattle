#include "matcher.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

using namespace gobang;

class MatcherTest : public ::testing::Test {
protected:
    void SetUp() override {
        matcher_.init(&online_mgr_);
        matcher_.set_match_callback([this](const MatchResult& result) {
            std::lock_guard<std::mutex> lock(results_mtx_);
            results_.push_back(result);
        });
        matcher_.start();
    }

    void TearDown() override {
        matcher_.stop();
    }

    void wait_for_results(size_t expected, int timeout_ms = 2000) {
        const std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lock(results_mtx_);
                if (results_.size() >= expected) {
                    return;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    std::vector<MatchResult> snapshot_results() {
        std::lock_guard<std::mutex> lock(results_mtx_);
        return results_;
    }

    OnlineManager online_mgr_;
    Matcher matcher_;
    std::mutex results_mtx_;
    std::vector<MatchResult> results_;
};

TEST_F(MatcherTest, NonHallIdleUserCannotEnqueue) {
    online_mgr_.user_online(1);
    online_mgr_.set_status(1, OnlineStatus::IN_ROOM);

    ASSERT_FALSE(matcher_.enqueue(1, 1200));
}

TEST_F(MatcherTest, EnqueueMovesUserToMatching) {
    online_mgr_.user_online(1);

    ASSERT_TRUE(matcher_.enqueue(1, 1200));
    ASSERT_EQ(online_mgr_.get_status(1), OnlineStatus::MATCHING);
}

TEST_F(MatcherTest, SameTierUsersMatchSuccessfully) {
    online_mgr_.user_online(1);
    online_mgr_.user_online(2);

    ASSERT_TRUE(matcher_.enqueue(1, 1200));
    ASSERT_TRUE(matcher_.enqueue(2, 1250));

    wait_for_results(1);
    std::vector<MatchResult> results = snapshot_results();

    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0].player1_id, 1);
    ASSERT_EQ(results[0].player2_id, 2);
    ASSERT_GT(results[0].room_id, 1000);
    ASSERT_EQ(online_mgr_.get_status(1), OnlineStatus::IN_ROOM);
    ASSERT_EQ(online_mgr_.get_status(2), OnlineStatus::IN_ROOM);
}

TEST_F(MatcherTest, CancelledUserWillNotReceiveMatch) {
    online_mgr_.user_online(1);
    online_mgr_.user_online(2);
    online_mgr_.user_online(3);

    ASSERT_TRUE(matcher_.enqueue(1, 1200));
    ASSERT_TRUE(matcher_.cancel(1));
    ASSERT_EQ(online_mgr_.get_status(1), OnlineStatus::HALL_IDLE);

    ASSERT_TRUE(matcher_.enqueue(2, 1200));
    ASSERT_TRUE(matcher_.enqueue(3, 1200));

    wait_for_results(1);
    std::vector<MatchResult> results = snapshot_results();

    ASSERT_EQ(results.size(), 1u);
    ASSERT_NE(results[0].player1_id, 1);
    ASSERT_NE(results[0].player2_id, 1);
}

TEST_F(MatcherTest, DisconnectedUserDoesNotRemainMatchable) {
    online_mgr_.user_online(1);
    online_mgr_.user_online(2);
    online_mgr_.user_online(3);

    ASSERT_TRUE(matcher_.enqueue(1, 1200));
    matcher_.on_disconnect(1);
    online_mgr_.user_offline(1);

    ASSERT_TRUE(matcher_.enqueue(2, 1200));
    ASSERT_TRUE(matcher_.enqueue(3, 1200));

    wait_for_results(1);
    std::vector<MatchResult> results = snapshot_results();

    ASSERT_EQ(results.size(), 1u);
    ASSERT_NE(results[0].player1_id, 1);
    ASSERT_NE(results[0].player2_id, 1);
}

TEST_F(MatcherTest, FairnessKeepsFirstTwoUsersTogether) {
    online_mgr_.user_online(1);
    online_mgr_.user_online(2);
    online_mgr_.user_online(3);
    online_mgr_.user_online(4);

    ASSERT_TRUE(matcher_.enqueue(1, 1200));
    ASSERT_TRUE(matcher_.enqueue(2, 1201));
    ASSERT_TRUE(matcher_.enqueue(3, 1202));
    ASSERT_TRUE(matcher_.enqueue(4, 1203));

    wait_for_results(2);
    std::vector<MatchResult> results = snapshot_results();

    ASSERT_EQ(results.size(), 2u);
    ASSERT_EQ(results[0].player1_id, 1);
    ASSERT_EQ(results[0].player2_id, 2);
}

TEST_F(MatcherTest, ConcurrentEnqueueProducesNoDuplicatePairs) {
    const int total_users = 10;
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);

    for (int i = 0; i < total_users; ++i) {
        online_mgr_.user_online(100 + i);
    }

    for (int i = 0; i < total_users; ++i) {
        threads.push_back(std::thread([this, i, &success_count]() {
            if (matcher_.enqueue(100 + i, 1200 + (i % 2))) {
                ++success_count;
            }
        }));
    }

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }

    ASSERT_EQ(success_count.load(), total_users);
    wait_for_results(total_users / 2, 3000);
    std::vector<MatchResult> results = snapshot_results();

    ASSERT_EQ(results.size(), static_cast<size_t>(total_users / 2));
    std::set<int64_t> paired_users;
    for (size_t i = 0; i < results.size(); ++i) {
        ASSERT_TRUE(paired_users.insert(results[i].player1_id).second);
        ASSERT_TRUE(paired_users.insert(results[i].player2_id).second);
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
