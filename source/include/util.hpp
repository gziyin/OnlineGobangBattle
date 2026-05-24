#pragma once

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <json/json.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace gobang {
namespace util {

inline std::string json_to_str(const Json::Value& val) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    builder["emitUTF8"] = true;
    return Json::writeString(builder, val);
}

inline bool str_to_json(const std::string& s, Json::Value& out, std::string& errmsg) {
    Json::CharReaderBuilder builder;
    std::istringstream iss(s);
    return Json::parseFromStream(builder, iss, &out, &errmsg);
}

inline Json::Value make_response(int code, const std::string& message, const Json::Value& data = Json::Value()) {
    Json::Value resp;
    resp["code"] = code;
    resp["message"] = message;
    if (!data.isNull()) {
        resp["data"] = data;
    }
    return resp;
}

inline int64_t now_sec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

inline int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

inline std::string format_time(int64_t timestamp_sec) {
    std::time_t t = static_cast<std::time_t>(timestamp_sec);
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

inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delim)) {
        result.push_back(token);
    }
    return result;
}

inline bool is_alnum(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    return std::all_of(s.begin(), s.end(), [](unsigned char ch) {
        return std::isalnum(ch) != 0;
    });
}

inline bool valid_username(const std::string& s) {
    return s.size() >= 3 && s.size() <= 32 && is_alnum(s);
}

inline bool valid_password(const std::string& s) {
    return s.size() >= 6 && s.size() <= 64;
}

inline std::string board_to_str(const int board[15][15]) {
    std::string s(225, '0');
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 15; ++c) {
            s[r * 15 + c] = static_cast<char>('0' + board[r][c]);
        }
    }
    return s;
}

inline void str_to_board(const std::string& s, int board[15][15]) {
    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 15; ++c) {
            board[r][c] = s[r * 15 + c] - '0';
        }
    }
}

namespace detail {
inline bool _parse_line(const std::string& line, std::string& key, std::string& val) {
    std::string s = trim(line);
    if (s.empty() || s[0] == '#') {
        return false;
    }

    size_t eq = s.find('=');
    if (eq == std::string::npos) {
        return false;
    }

    key = trim(s.substr(0, eq));
    val = trim(s.substr(eq + 1));

    size_t slash_comment = val.find("//");
    if (slash_comment != std::string::npos) {
        val = trim(val.substr(0, slash_comment));
    }

    size_t hash_comment = val.find('#');
    if (hash_comment != std::string::npos) {
        val = trim(val.substr(0, hash_comment));
    }

    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
        val = val.substr(1, val.size() - 2);
    }

    return !key.empty();
}

inline bool has_letter(const std::string& s) {
    return std::any_of(s.begin(), s.end(), [](unsigned char ch) {
        return std::isalpha(ch) != 0;
    });
}

inline bool has_digit(const std::string& s) {
    return std::any_of(s.begin(), s.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
}

inline bool has_symbol(const std::string& s) {
    return std::any_of(s.begin(), s.end(), [](unsigned char ch) {
        return std::ispunct(ch) != 0;
    });
}

inline std::string lower_copy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return s;
}

inline bool is_weak_jwt_secret(const std::string& secret) {
    const std::string trimmed = trim(secret);
    if (trimmed.size() < 32) {
        return true;
    }

    const std::string lowered = lower_copy(trimmed);
    const std::vector<std::string> denylist = {
        "change_me",
        "change_me_use_random_string_at_least_32_chars",
        "123456",
        "123456789",
        "online_gobang",
        "jwt_secret"
    };
    for (const auto& item : denylist) {
        if (lowered.find(item) != std::string::npos) {
            return true;
        }
    }

    int classes = 0;
    classes += has_letter(trimmed) ? 1 : 0;
    classes += has_digit(trimmed) ? 1 : 0;
    classes += has_symbol(trimmed) ? 1 : 0;
    return classes < 2;
}
}

struct Config {
    int server_port = 8080;
    int server_threads = 4;

    std::string db_host = "127.0.0.1";
    int db_port = 3306;
    std::string db_user = "root";
    std::string db_password = "";
    std::string db_name = "gobang_db";
    int db_pool_size = 10;

    std::string log_file = "logs/server.log";
    std::string log_level = "INFO";
    size_t log_max_bytes = 10 * 1024 * 1024;

    int reconnect_timeout = 60;
    int match_timeout = 30;
    int heartbeat_interval = 30;

    std::string jwt_secret = "CHANGE_ME";
    std::string jwt_issuer = "online_gobang";
    int64_t jwt_expire = 86400;
};

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
    if (detail::is_weak_jwt_secret(cfg.jwt_secret)) {
        throw std::runtime_error("Config error: jwt_secret is weak; use a random secret with length >= 32 and mixed character classes");
    }
}

inline Config load_config(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filepath);
    }

    Config cfg;
    std::string line;
    std::string key;
    std::string val;

    while (std::getline(f, line)) {
        if (!detail::_parse_line(line, key, val)) {
            continue;
        }

        if (key == "server_port") {
            cfg.server_port = std::stoi(val);
        } else if (key == "server_threads") {
            cfg.server_threads = std::stoi(val);
        } else if (key == "db_host") {
            cfg.db_host = val;
        } else if (key == "db_port") {
            cfg.db_port = std::stoi(val);
        } else if (key == "db_user") {
            cfg.db_user = val;
        } else if (key == "db_password") {
            cfg.db_password = val;
        } else if (key == "db_name") {
            cfg.db_name = val;
        } else if (key == "db_pool_size") {
            cfg.db_pool_size = std::stoi(val);
        } else if (key == "log_file") {
            cfg.log_file = val;
        } else if (key == "log_level") {
            cfg.log_level = val;
        } else if (key == "log_max_bytes") {
            cfg.log_max_bytes = std::stoull(val);
        } else if (key == "reconnect_timeout") {
            cfg.reconnect_timeout = std::stoi(val);
        } else if (key == "match_timeout") {
            cfg.match_timeout = std::stoi(val);
        } else if (key == "heartbeat_interval") {
            cfg.heartbeat_interval = std::stoi(val);
        } else if (key == "jwt_secret") {
            cfg.jwt_secret = val;
        } else if (key == "jwt_issuer") {
            cfg.jwt_issuer = val;
        } else if (key == "jwt_expire") {
            cfg.jwt_expire = std::stoll(val);
        }
    }

    validate_config(cfg);
    return cfg;
}

} // namespace util
} // namespace gobang
