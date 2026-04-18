#include "block_queue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <type_traits>
#include <vector>

using namespace gobang;

TEST(BlockQueueTest, PushAndTimedPopWorks) {
    BlockQueue<int> queue;
    queue.push(7);

    int out = 0;
    ASSERT_TRUE(queue.timed_pop(out, 10));
    ASSERT_EQ(out, 7);
    ASSERT_TRUE(queue.empty());
}

TEST(BlockQueueTest, TimedPopTimeoutReturnsFalseWithoutShutdown) {
    BlockQueue<int> queue;
    int out = 0;

    ASSERT_FALSE(queue.timed_pop(out, 20));
    ASSERT_FALSE(queue.is_shutdown());
}

TEST(BlockQueueTest, ShutdownMakesTimedPopReturnFalse) {
    BlockQueue<int> queue;
    queue.shutdown();

    int out = 0;
    ASSERT_FALSE(queue.timed_pop(out, 20));
    ASSERT_TRUE(queue.is_shutdown());
}

TEST(BlockQueueTest, MultiThreadProducerConsumerWorks) {
    BlockQueue<int> queue;
    const int total = 200;
    std::atomic<int> consumed(0);
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    for (int i = 0; i < 2; ++i) {
        producers.push_back(std::thread([&queue, i, total]() {
            for (int n = 0; n < total / 2; ++n) {
                queue.push(i * 1000 + n);
            }
        }));
    }

    for (int i = 0; i < 2; ++i) {
        consumers.push_back(std::thread([&queue, &consumed, total]() {
            int value = 0;
            while (consumed.load() < total) {
                if (queue.timed_pop(value, 50)) {
                    ++consumed;
                }
            }
        }));
    }

    for (size_t i = 0; i < producers.size(); ++i) {
        producers[i].join();
    }

    while (consumed.load() < total) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    queue.shutdown();

    for (size_t i = 0; i < consumers.size(); ++i) {
        consumers[i].join();
    }

    ASSERT_EQ(consumed.load(), total);
}

TEST(BlockQueueTest, ShutdownWakesBlockedConsumer) {
    BlockQueue<int> queue;
    std::atomic<bool> woke(false);

    std::thread worker([&queue, &woke]() {
        int value = 0;
        woke = !queue.timed_pop(value, 5000);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    queue.shutdown();
    worker.join();

    ASSERT_TRUE(woke.load());
    ASSERT_TRUE(queue.is_shutdown());
}

TEST(BlockQueueTest, QueueIsNotCopyable) {
    ASSERT_FALSE(std::is_copy_constructible<BlockQueue<int> >::value);
    ASSERT_FALSE(std::is_copy_assignable<BlockQueue<int> >::value);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
