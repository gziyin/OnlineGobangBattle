#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <json/json.h>   // jsoncpp

namespace gobang {
namespace util {

// ══════════════════════════════════════════════════════════════
// ①  JSON 工具
// ══════════════════════════════════════════════════════════════

// 把 Json::Value 序列化为紧凑 JSON 字符串（无多余空格）
inline std::string json_to_str(const Json::Value& val) {
    Json::StreamWriterBuilder builder;
    builder["indentation"]  = "";      // 紧凑模式
    builder["emitUTF8"]     = true;    // 中文不转义
    return Json::writeString(builder, val);
}

// 把 JSON 字符串解析为 Json::Value
// 成功返回 true，失败返回 false 并在 errmsg 中写原因
inline bool str_to_json(const std::string& s,
                         Json::Value&       out,
                         std::string&       errmsg) {
    Json::CharReaderBuilder builder;
    std::istringstream iss(s);
    return Json::parseFromStream(builder, iss, &out, &errmsg);
}

// 快速构造一个响应 JSON（统一格式）
// 用法：make_response(200, "ok", data_obj)
inline Json::Value make_response(int         code,
                                  const std::string& message,
                                  const Json::Value& data = Json::Value()) {
    Json::Value resp;
    resp["code"]    = code;
    resp["message"] = message;
    if (!data.isNull()) resp["data"] = data;
    return resp;
}

// ══════════════════════════════════════════════════════════════
// ②  时间工具
// ══════════════════════════════════════════════════════════════

// 获取当前 Unix 时间戳（秒）
inline int64_t now_sec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// 获取当前 Unix 时间戳（毫秒），用于心跳等精确计时
inline int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

// 把时间戳格式化为可读字符串，如 "2026-03-31 12:00:00"
inline std::string format_time(int64_t timestamp_sec) {
    std::time_t t = (std::time_t)timestamp_sec;
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &t);
#else
    localtime_r(&t, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}

// ══════════════════════════════════════════════════════════════
// ③  字符串工具
// ══════════════════════════════════════════════════════════════

// 去除首尾空白字符
inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 按分隔符切割字符串
// split("a,b,c", ',') → {"a","b","c"}
inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delim)) {
        result.push_back(token);
    }
    return result;
}

// 检查字符串是否只包含字母和数字（用于用户名校验）
inline bool is_alnum(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), ::isalnum);
}

// 长度校验（注册时用）
inline bool valid_username(const std::string& s) {
    return s.size() >= 3 && s.size() <= 32 && is_alnum(s);
}

inline bool valid_password(const std::string& s) {
    return s.size() >= 6 && s.size() <= 64;
}

// ══════════════════════════════════════════════════════════════
// ④  棋盘工具
// ══════════════════════════════════════════════════════════════

// 把二维棋盘数组序列化为 225 字符字符串（0 空 1 黑 2 白）
// board[row][col] ∈ {0,1,2}
inline std::string board_to_str(const int board[15][15]) {
    std::string s(225, '0');
    for (int r = 0; r < 15; ++r)
        for (int c = 0; c < 15; ++c)
            s[r * 15 + c] = '0' + board[r][c];
    return s;
}

// 把 225 字符字符串还原为二维数组
inline void str_to_board(const std::string& s, int board[15][15]) {
    for (int r = 0; r < 15; ++r)
        for (int c = 0; c < 15; ++c)
            board[r][c] = s[r * 15 + c] - '0';
}

// ══════════════════════════════════════════════════════════════
// ⑤  配置文件读取
// ══════════════════════════════════════════════════════════════

// 内部实现细节（不要直接使用）
namespace detail {
// 解析一行配置，去掉注释和空白
// 返回 false 表示这行是注释或空行，跳过即可
inline bool _parse_line(const std::string& line,
                         std::string& key, std::string& val) {
    std::string s = trim(line);
    if (s.empty() || s[0] == '#') return false;  // 注释行/空行

    size_t eq = s.find('=');
    if (eq == std::string::npos) return false;    // 没有等号，格式错误行

    key = trim(s.substr(0, eq));
    val = trim(s.substr(eq + 1));

    // 去掉行内注释：val 里 '#' 后面的部分
    size_t comment = val.find('#');
    if (comment != std::string::npos)
        val = trim(val.substr(0, comment));

    return !key.empty();
}
} // namespace detail

// 存放所有配置项的结构体（db.hpp 直接用里面的字段建连接）
struct Config {
    // 服务器
    int         server_port     = 8080;
    int         server_threads  = 4;

    // 数据库（由 db.hpp 消费）
    std::string db_host         = "127.0.0.1";
    int         db_port         = 3306;
    std::string db_user         = "root";
    std::string db_password     = "";
    std::string db_name         = "gobang_db";
    int         db_pool_size    = 10;

    // 日志
    std::string log_file        = "logs/server.log";
    std::string log_level       = "INFO";
    size_t      log_max_bytes   = 10 * 1024 * 1024;

    // 游戏规则
    int         reconnect_timeout   = 60;
    int         match_timeout       = 30;
    int         heartbeat_interval  = 30;

    // JWT 配置（Phase 3 新增）
    std::string jwt_secret      = "CHANGE_ME";
    std::string jwt_issuer     = "online_gobang";
    int64_t     jwt_expire     = 86400;
};

// 校验配置参数的合法性
inline void validate_config(const Config& cfg) {
    if (cfg.db_password.empty()) {
        throw std::runtime_error("Config error: db_password is required");
    }
    if (cfg.server_port < 1 || cfg.server_port > 65535) {
        throw std::runtime_error("Config error: server_port out of range (1-65535)");
    }
    if (cfg.db_pool_size < 1 || cfg.db_pool_size > 100) {
        throw std::runtime_error("Config error: db_pool_size must be 1-100");
    }
    if (cfg.heartbeat_interval < 1 || cfg.heartbeat_interval > 300) {
        throw std::runtime_error("Config error: heartbeat_interval must be 1-300");
    }
}

// 从文件路径加载配置，返回填好的 Config 结构体
// 如果文件打不开，抛出 std::runtime_error
inline Config load_config(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filepath);
    }

    Config cfg;
    std::string line, key, val;

    while (std::getline(f, line)) {
        if (!detail::_parse_line(line, key, val)) continue;

        // 逐一匹配 key，转换类型后写入结构体
        if      (key == "server_port")          cfg.server_port         = std::stoi(val);
        else if (key == "server_threads")        cfg.server_threads      = std::stoi(val);
        else if (key == "db_host")               cfg.db_host             = val;
        else if (key == "db_port")               cfg.db_port             = std::stoi(val);
        else if (key == "db_user")               cfg.db_user             = val;
        else if (key == "db_password")           cfg.db_password         = val;
        else if (key == "db_name")               cfg.db_name             = val;
        else if (key == "db_pool_size")          cfg.db_pool_size        = std::stoi(val);
        else if (key == "log_file")              cfg.log_file            = val;
        else if (key == "log_level")             cfg.log_level           = val;
        else if (key == "log_max_bytes")         cfg.log_max_bytes       = std::stoull(val);
        else if (key == "reconnect_timeout")     cfg.reconnect_timeout   = std::stoi(val);
        else if (key == "match_timeout")         cfg.match_timeout       = std::stoi(val);
        else if (key == "heartbeat_interval")    cfg.heartbeat_interval  = std::stoi(val);
        else if (key == "jwt_secret")            cfg.jwt_secret        = val;
        else if (key == "jwt_issuer")            cfg.jwt_issuer        = val;
        else if (key == "jwt_expire")            cfg.jwt_expire        = std::stoll(val);
        // 未知 key 直接跳过，不报错（方便以后加新字段）
    }

    // 校验配置参数
    validate_config(cfg);

    return cfg;
}

} // namespace util
} // namespace gobang
