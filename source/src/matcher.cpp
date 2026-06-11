#include "matcher.hpp"

namespace gobang {

Matcher::Matcher()
    : _online_mgr(nullptr),
      _running(false),
      _request_id_counter(0),
      _room_id_counter(1000) {
    for (size_t i = 0; i < _incoming_queues.size(); ++i) {
        _incoming_queues[i].reset(new BlockQueue<MatchRequest>());
    }
}

Matcher::~Matcher() {
    stop();
}

void Matcher::init(OnlineManager* online_mgr) {
    _online_mgr = online_mgr;
}

void Matcher::set_match_callback(MatchCallback cb) {
    _match_callback = cb;
}

bool Matcher::enqueue(int64_t user_id, int score) {
    if (_online_mgr == nullptr) {
        return false;
    }
    if (_online_mgr->get_status(user_id) != OnlineStatus::HALL_IDLE) {
        return false;
    }

    MatchRequest req;
    req.user_id = user_id;
    req.score = score;
    req.enqueue_time = now_ms();
    req.request_id = ++_request_id_counter;
    req.cancelled = false;

    const int tier = tier_index(score_to_tier(score));

    {
        std::lock_guard<std::mutex> lock(_user_map_mtx);
        if (_user_request_map.find(user_id) != _user_request_map.end()) {
            return false;
        }
        RequestMeta meta;
        meta.tier = tier;
        meta.request_id = req.request_id;
        meta.enqueue_time = req.enqueue_time;
        _user_request_map[user_id] = meta;
    }

    if (!_online_mgr->set_status(user_id, OnlineStatus::MATCHING)) {
        std::lock_guard<std::mutex> lock(_user_map_mtx);
        _user_request_map.erase(user_id);
        return false;
    }

    _incoming_queues[tier]->push(req);
    return true;
}

bool Matcher::cancel(int64_t user_id) {
    bool erased = false;
    {
        std::lock_guard<std::mutex> lock(_user_map_mtx);
        erased = (_user_request_map.erase(user_id) > 0);
    }
    if (erased && _online_mgr != nullptr && _online_mgr->is_online(user_id)) {
        _online_mgr->set_status(user_id, OnlineStatus::HALL_IDLE);
    }
    return erased;
}

void Matcher::on_disconnect(int64_t user_id) {
    (void)cancel(user_id);
}

void Matcher::start() {
    bool expected = false;
    if (!_running.compare_exchange_strong(expected, true)) {
        return;
    }

    for (size_t i = 0; i < _incoming_queues.size(); ++i) {
        _incoming_queues[i].reset(new BlockQueue<MatchRequest>());
    }
    for (size_t i = 0; i < _pending_queues.size(); ++i) {
        _pending_queues[i].clear();
    }

    _match_thread = std::thread(&Matcher::match_loop, this);
}

void Matcher::stop() {
    bool expected = true;
    if (!_running.compare_exchange_strong(expected, false)) {
        return;
    }

    for (size_t i = 0; i < _incoming_queues.size(); ++i) {
        _incoming_queues[i]->shutdown();
    }

    if (_match_thread.joinable()) {
        _match_thread.join();
    }

    {
        std::lock_guard<std::mutex> lock(_user_map_mtx);
        for (std::unordered_map<int64_t, RequestMeta>::const_iterator it = _user_request_map.begin();
             it != _user_request_map.end(); ++it) {
            if (_online_mgr != nullptr && _online_mgr->is_online(it->first) &&
                _online_mgr->get_status(it->first) == OnlineStatus::MATCHING) {
                _online_mgr->set_status(it->first, OnlineStatus::HALL_IDLE);
            }
        }
        _user_request_map.clear();
    }

    for (size_t i = 0; i < _pending_queues.size(); ++i) {
        _pending_queues[i].clear();
    }
}

MatchTier Matcher::score_to_tier(int score) {
    if (score < 1300) {
        return MatchTier::BRONZE;
    }
    if (score < 1700) {
        return MatchTier::SILVER;
    }
    return MatchTier::GOLD;
}

int Matcher::tier_index(MatchTier tier) {
    return static_cast<int>(tier);
}

int64_t Matcher::now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

void Matcher::match_loop() {
    while (_running.load()) {
        drain_incoming_requests();

        for (int tier = 0; tier < 3; ++tier) {
            try_match_same_tier(tier);
        }

        try_match_cross_tier(0);
        try_match_cross_tier(1);

        std::this_thread::sleep_for(std::chrono::milliseconds(MATCH_LOOP_IDLE_MS));
    }

    drain_incoming_requests();
    for (int tier = 0; tier < 3; ++tier) {
        try_match_same_tier(tier);
    }
    try_match_cross_tier(0);
    try_match_cross_tier(1);
}

void Matcher::drain_incoming_requests() {
    for (int tier = 0; tier < 3; ++tier) {
        drain_tier(tier);
    }
}

void Matcher::drain_tier(int tier) {
    MatchRequest req;
    while (_incoming_queues[tier]->timed_pop(req, 0)) {
        _pending_queues[tier].push_back(req);
    }
}

void Matcher::try_match_same_tier(int tier) {
    std::deque<MatchRequest>& queue = _pending_queues[tier];
    while (true) {
        while (discard_invalid_front(tier)) {
        }

        if (queue.size() < 2) {
            return;
        }

        MatchRequest first = queue.front();
        queue.pop_front();

        while (discard_invalid_front(tier)) {
        }

        if (queue.empty()) {
            queue.push_front(first);
            return;
        }

        MatchRequest second = queue.front();
        queue.pop_front();

        if (!is_request_valid(first)) {
            continue;
        }
        if (!is_request_valid(second)) {
            queue.push_front(first);
            continue;
        }

        finalize_match(first, second);
    }
}

void Matcher::try_match_cross_tier(int tier) {
    std::deque<MatchRequest>& left = _pending_queues[tier];
    std::deque<MatchRequest>& right = _pending_queues[tier + 1];

    while (discard_invalid_front(tier)) {
    }
    while (discard_invalid_front(tier + 1)) {
    }

    if (left.empty() || right.empty()) {
        return;
    }

    const MatchRequest& left_front = left.front();
    const MatchRequest& right_front = right.front();
    if (!should_allow_cross_tier(left_front, right_front)) {
        return;
    }

    MatchRequest p1 = left_front;
    MatchRequest p2 = right_front;
    left.pop_front();
    right.pop_front();

    if (!is_request_valid(p1) || !is_request_valid(p2)) {
        return;
    }

    finalize_match(p1, p2);
}

bool Matcher::discard_invalid_front(int tier) {
    std::deque<MatchRequest>& queue = _pending_queues[tier];
    if (queue.empty()) {
        return false;
    }

    if (is_request_valid(queue.front())) {
        return false;
    }

    queue.pop_front();
    return true;
}

bool Matcher::is_request_valid(const MatchRequest& req) const {
    if (_online_mgr == nullptr) {
        return false;
    }
    if (req.cancelled) {
        return false;
    }
    if (!_online_mgr->is_online(req.user_id)) {
        return false;
    }
    if (_online_mgr->get_status(req.user_id) != OnlineStatus::MATCHING) {
        return false;
    }

    std::lock_guard<std::mutex> lock(_user_map_mtx);
    std::unordered_map<int64_t, RequestMeta>::const_iterator it = _user_request_map.find(req.user_id);
    if (it == _user_request_map.end()) {
        return false;
    }
    return it->second.request_id == req.request_id;
}

bool Matcher::should_allow_cross_tier(const MatchRequest& left,
                                      const MatchRequest& right) const {
    const int64_t now = now_ms();
    return (now - left.enqueue_time >= MATCH_TIMEOUT_MS) ||
           (now - right.enqueue_time >= MATCH_TIMEOUT_MS);
}

void Matcher::finalize_match(const MatchRequest& p1, const MatchRequest& p2) {
    {
        std::lock_guard<std::mutex> lock(_user_map_mtx);
        _user_request_map.erase(p1.user_id);
        _user_request_map.erase(p2.user_id);
    }

    if (_online_mgr != nullptr) {
        _online_mgr->set_status(p1.user_id, OnlineStatus::IN_ROOM);
        _online_mgr->set_status(p2.user_id, OnlineStatus::IN_ROOM);
    }

    MatchResult result;
    result.room_id = generate_room_id();
    result.player1_id = p1.user_id;
    result.player2_id = p2.user_id;
    result.player1_color = 1;  // BLACK，与 handle_game_start 一致
    result.player2_color = 2;  // WHITE

    if (_match_callback) {
        _match_callback(result);
    }
}

int64_t Matcher::generate_room_id() {
    return ++_room_id_counter;
}

} // namespace gobang
