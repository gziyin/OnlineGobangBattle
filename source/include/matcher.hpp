#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "block_queue.hpp"
#include "matcher_interface.hpp"
#include "online.hpp"

namespace gobang {

enum class MatchTier {
    BRONZE = 0,
    SILVER = 1,
    GOLD = 2,
    TIER_COUNT = 3
};

struct MatchRequest {
    int64_t user_id;
    int score;
    int64_t enqueue_time;
    uint64_t request_id;
    bool cancelled;

    MatchRequest()
        : user_id(0), score(0), enqueue_time(0), request_id(0), cancelled(false) {
    }
};

class Matcher : public MatcherInterface {
public:
    Matcher();
    ~Matcher() override;

    void init(OnlineManager* online_mgr);
    void set_match_callback(MatchCallback cb) override;

    bool enqueue(int64_t user_id, int score) override;
    bool cancel(int64_t user_id) override;
    void on_disconnect(int64_t user_id) override;

    void start();
    void stop();

private:
    struct RequestMeta {
        int tier;
        uint64_t request_id;
        int64_t enqueue_time;
    };

    static MatchTier score_to_tier(int score);
    static int tier_index(MatchTier tier);
    static int64_t now_ms();

    void match_loop();
    void drain_incoming_requests();
    void drain_tier(int tier);
    void try_match_same_tier(int tier);
    void try_match_cross_tier(int tier);
    bool discard_invalid_front(int tier);
    bool is_request_valid(const MatchRequest& req) const;
    bool should_allow_cross_tier(const MatchRequest& left, const MatchRequest& right) const;
    void finalize_match(const MatchRequest& p1, const MatchRequest& p2);
    int64_t generate_room_id();

    OnlineManager* _online_mgr;
    MatchCallback _match_callback;

    std::array<std::unique_ptr<BlockQueue<MatchRequest> >, 3> _incoming_queues;
    std::array<std::deque<MatchRequest>, 3> _pending_queues;

    mutable std::mutex _user_map_mtx;
    std::unordered_map<int64_t, RequestMeta> _user_request_map;

    std::thread _match_thread;
    std::atomic<bool> _running;
    std::atomic<uint64_t> _request_id_counter;
    std::atomic<int64_t> _room_id_counter;

    enum {
        MATCH_TIMEOUT_MS = 30000,
        MATCH_LOOP_IDLE_MS = 10
    };
};

} // namespace gobang
