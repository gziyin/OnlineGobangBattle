#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

#include <mysql/mysql.h>

#include "logger.hpp"
#include "util.hpp"

namespace gobang {

/**
 * @brief 数据库连接池
 *
 * 线程安全的 MySQL 连接池，支持并发请求和超时机制
 *
 * 使用示例:
 *   DBPool pool;
 *   pool.init(cfg);
 *
 *   // RAII 方式获取连接，析构时自动归还
 *   auto conn = pool.get_connection();
 *   if (conn) {
 *       // 使用 conn.get() 获取 MYSQL* 裸指针
 *   } else {
 *       // 超时或错误，返回 nullptr
 *   }
 */
class DBPool {
public:
    DBPool() : _running(false) {}

    ~DBPool() {
        shutdown();
    }

    // 禁止拷贝和移动
    DBPool(const DBPool&) = delete;
    DBPool& operator=(const DBPool&) = delete;

    /**
     * @brief 初始化连接池
     * @param cfg 配置对象（需包含 db_host, db_port, db_user, db_password, db_name, db_pool_size）
     * @throws std::runtime_error 当数据库连接失败时
     */
    void init(const util::Config& cfg) {
        _host = cfg.db_host;
        _port = cfg.db_port;
        _user = cfg.db_user;
        _password = cfg.db_password;
        _db_name = cfg.db_name;
        _pool_size = cfg.db_pool_size;

        LOG_INFO("DBPool: initializing with " << _pool_size << " connections to "
                << _host << ":" << _port << "/" << _db_name);

        // 预创建连接
        for (int i = 0; i < _pool_size; ++i) {
            MYSQL* conn = _create_connection();
            if (!conn) {
                throw std::runtime_error("DBPool: failed to create connection " + std::to_string(i + 1));
            }
            _pool.push(conn);
        }

        _running = true;
        LOG_INFO("DBPool: initialization completed");
    }

    /**
     * @brief 获取数据库连接（RAII 方式，析构时自动归还）
     * @param timeout_ms 超时时间（毫秒），默认 3000ms
     * @return ConnGuard 智能指针，成功返回非空，超时/失败返回 nullptr
     */
    using ConnGuard = std::unique_ptr<MYSQL, std::function<void(MYSQL*)>>;

    ConnGuard get_connection(int timeout_ms = 3000) {
        if (!_running) {
            LOG_ERROR("DBPool: get_connection called after shutdown");
            return nullptr;
        }

        std::unique_lock<std::mutex> lock(_mtx);

        // 使用 wait_for 实现超时等待
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

        while (_pool.empty()) {
            LOG_DEBUG("DBPool: pool empty, waiting for connection...");
            if (_cv.wait_until(lock, deadline) == std::cv_status::timeout) {
                LOG_WARN("DBPool: get_connection timeout after " << timeout_ms << "ms");
                return nullptr;
            }
            // 醒来后检查是否被中断或池已关闭
            if (!_running) {
                return nullptr;
            }
        }

        // 获取连接
        MYSQL* conn = _pool.front();
        _pool.pop();

        // 检查连接是否有效（ping 一下）
        if (mysql_ping(conn) != 0) {
            LOG_WARN("DBPool: connection dead, recreating...");
            // 连接已断开，尝试重连
            MYSQL* new_conn = _create_connection();
            if (new_conn) {
                conn = new_conn;
            } else {
                LOG_ERROR("DBPool: failed to recreate connection");
                // 连接池少了一个，但不阻塞调用方
            }
        }

        LOG_DEBUG("DBPool: connection acquired (remaining: " << _pool.size() << ")");

        // 创建 RAII 守卫，析构时自动归还
        return ConnGuard(conn, [this](MYSQL* conn) {
            this->return_connection(conn);
        });
    }

    /**
     * @brief 归还数据库连接（通常在 ConnGuard 析构时自动调用）
     * @param conn 要归还的连接
     */
    void return_connection(MYSQL* conn) {
        if (!conn) return;

        {
            std::lock_guard<std::mutex> lock(_mtx);
            _pool.push(conn);
        }
        _cv.notify_one();

        LOG_DEBUG("DBPool: connection returned (available: " << _pool.size() << ")");
    }

    /**
     * @brief 关闭连接池，释放所有连接
     */
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(_mtx);
            if (!_running) return; // 已关闭
            _running = false;
        }

        // 通知所有等待的线程
        _cv.notify_all();

        // 关闭所有连接
        std::lock_guard<std::mutex> lock(_mtx);
        while (!_pool.empty()) {
            MYSQL* conn = _pool.front();
            _pool.pop();
            mysql_close(conn);
            LOG_DEBUG("DBPool: connection closed");
        }

        LOG_INFO("DBPool: shutdown completed");
    }

    /**
     * @brief 获取当前可用连接数（仅用于调试/监控）
     */
    size_t available_connections() const {
        return _pool.size();
    }

private:
    /**
     * @brief 创建单个 MySQL 连接
     */
    MYSQL* _create_connection() {
        MYSQL* conn = mysql_init(nullptr);
        if (!conn) {
            LOG_ERROR("DBPool: mysql_init failed: " << mysql_error(conn));
            return nullptr;
        }

        // 设置连接选项
        unsigned int timeout = 3; // 3 秒连接超时
        mysql_options(conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

        // 连接数据库
        if (!mysql_real_connect(conn, _host.c_str(), _user.c_str(), _password.c_str(),
                                 _db_name.c_str(), _port, nullptr, 0)) {
            LOG_ERROR("DBPool: mysql_real_connect failed: " << mysql_error(conn));
            mysql_close(conn);
            return nullptr;
        }

        // 设置字符集
        if (mysql_set_character_set(conn, "utf8mb4") != 0) {
            LOG_WARN("DBPool: mysql_set_character_set failed: " << mysql_error(conn));
            // 不返回失败，字符集问题通常不影响功能
        }

        LOG_DEBUG("DBPool: new connection created to " << _host << ":" << _port);
        return conn;
    }

    // 配置
    std::string _host;
    int _port;
    std::string _user;
    std::string _password;
    std::string _db_name;
    int _pool_size;

    // 连接池状态
    std::queue<MYSQL*> _pool;
    mutable std::mutex _mtx;
    std::condition_variable _cv;
    std::atomic<bool> _running;
};

} // namespace gobang
