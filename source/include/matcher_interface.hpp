#pragma once
#include <cstdint>
#include <functional>

namespace gobang {

/**
 * @brief 匹配结果结构
 *
 * 由 Matcher 生成，通过回调传递给 WebSocketHandler
 * 包含房间ID、双方玩家ID和执子颜色
 */
struct MatchResult {
    int64_t room_id;
    int64_t player1_id;
    int64_t player2_id;
    int     player1_color; // 1 = black, 2 = white
    int     player2_color;

    MatchResult()
        : room_id(0), player1_id(0), player2_id(0), player1_color(0), player2_color(0) {
    }
};

using MatchCallback = std::function<void(const MatchResult&)>;

/**
 * @brief Matcher 接口定义
 *
 * WebSocketHandler 通过此接口与 Matcher 交互
 * 实现类：Matcher (matcher.hpp)
 */
class MatcherInterface {
public:
    virtual ~MatcherInterface() = default;

    /**
     * @brief 将用户加入匹配队列
     * @param user_id 用户ID
     * @param score 用户分数（用于分段匹配）
     * @return 成功入队返回 true
     */
    virtual bool enqueue(int64_t user_id, int score) = 0;

    /**
     * @brief 取消用户匹配
     * @param user_id 用户ID
     * @return 成功取消返回 true
     */
    virtual bool cancel(int64_t user_id) = 0;

    /**
     * @brief 用户断开连接处理
     * @param user_id 用户ID
     *
     * 断线顺序：conn_mgr.remove -> online_mgr.user_offline -> matcher.on_disconnect
     */
    virtual void on_disconnect(int64_t user_id) = 0;

    /**
     * @brief 设置匹配成功回调
     * @param cb 回调函数
     *
     * 匹配成功后通过此回调通知 WebSocketHandler 发送 match.success 消息
     */
    virtual void set_match_callback(MatchCallback cb) = 0;
};

} // namespace gobang
