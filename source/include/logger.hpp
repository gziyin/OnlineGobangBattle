#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <ctime>
#include <iomanip>
#include <cstring>   // strrchr
#include <sys/stat.h> // stat

namespace gobang {

// ─── 日志等级 ───────────────────────────────────────────────
enum class LogLevel {
    DEBUG = 0,
    INFO  = 1,
    WARN  = 2,
    ERROR = 3
};

// ─── 日志器（单例） ─────────────────────────────────────────
class Logger {
public:
    // 获取单例
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    // 初始化：指定日志文件路径、最低输出等级、单文件最大字节数
    void init(const std::string& filepath,
              LogLevel          min_level   = LogLevel::DEBUG,
              size_t            max_bytes   = 10 * 1024 * 1024) { // 默认 10MB 轮转
        _filepath  = filepath;
        _min_level = min_level;
        _max_bytes = max_bytes;

        _file.open(filepath, std::ios::app);
        if (!_file.is_open()) {
            throw std::runtime_error("Logger: cannot open file: " + filepath);
        }

        // 启动后台写入线程
        _running = true;
        _worker  = std::thread(&Logger::_consume, this);
    }

    // 投递一条日志（由业务线程调用，不阻塞）
    void log(LogLevel level, const std::string& msg,
             const char* file, int line) {
        if (level < _min_level) return;

        std::string entry = _format(level, msg, file, line);

        {
            std::lock_guard<std::mutex> lk(_mtx);
            // 锁内判断容量，避免数据竞争
            if (_queue.size() >= MAX_QUEUE_SIZE && level < LogLevel::ERROR) {
                _drop_count++;
                return;
            }
            _queue.push(std::move(entry));
        }
        _cv.notify_one(); // 唤醒后台线程
    }

    // 析构：优雅关闭后台线程，把队列里剩余的全部刷盘
    ~Logger() {
        _running = false;
        _cv.notify_one();
        if (_worker.joinable()) _worker.join();
        // 把队列里剩余的全部写完
        while (!_queue.empty()) {
            _file << _queue.front();
            _queue.pop();
        }
        _file.flush();

        // 报告丢弃的日志数
        if (_drop_count > 0) {
            _file << "[" << _now_str() << "] [INFO ] Logger shutdown. "
                  << "Dropped " << _drop_count << " low-priority messages.\n";
            _file.flush();
        }
        _file.close();
    }

private:
    Logger() : _running(false), _drop_count(0) {} // 私有构造，单例
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

    // 队列容量上限
    static constexpr size_t MAX_QUEUE_SIZE = 10000;

    // ── 后台消费线程 ─────────────────────────────────────
    void _consume() {
        while (true) {
            std::unique_lock<std::mutex> lk(_mtx);
            // 等待：队列非空 或 收到退出信号
            _cv.wait(lk, [this]{ return !_queue.empty() || !_running; });

            // 把当前队列里的全部取出来批量写（减少锁持有时间）
            std::queue<std::string> local;
            local.swap(_queue);
            lk.unlock();

            while (!local.empty()) {
                _check_rotate();          // 超大了就轮转
                _file << local.front();
                local.pop();
            }
            _file.flush();

            if (!_running) break; // 退出信号且队列已空
        }
    }

    // ── 日志轮转：超过 max_bytes 就把旧文件重命名 ────────
    void _check_rotate() {
        struct stat st{};
        if (stat(_filepath.c_str(), &st) != 0) return;
        if ((size_t)st.st_size < _max_bytes)   return;

        _file.close();
        std::string backup = _filepath + "." + _timestamp_str() + ".bak";
        ::rename(_filepath.c_str(), backup.c_str());
        _file.open(_filepath, std::ios::app);
    }

    // ── 格式化一条日志条目 ──────────────────────────────
    std::string _format(LogLevel level, const std::string& msg,
                        const char* src_file, int line) {
        static const char* LEVEL_STR[] = {"DEBUG","INFO ","WARN ","ERROR"};

        // 提取文件名（兼容 Windows 反斜杠）
        const char* filename = std::strrchr(src_file, '/');
#ifdef _WIN32
        if (!filename) filename = std::strrchr(src_file, '\\');
#endif
        filename = filename ? filename + 1 : src_file;

        std::ostringstream oss;
        oss << "[" << _now_str() << "] "
            << "[" << LEVEL_STR[(int)level] << "] "
            << "[" << filename << ":" << line << "] "
            << msg << "\n";
        return oss.str();
    }

    // ── 获取当前时间字符串 ───────────────────────────────
    std::string _now_str() {
        auto now = std::time(nullptr);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &now);
#else
        localtime_r(&now, &tm_buf);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
        return buf;
    }

    std::string _timestamp_str() {
        auto now = std::time(nullptr);
        std::tm tm_buf{};
#ifdef _WIN32
        localtime_s(&tm_buf, &now);
#else
        localtime_r(&now, &tm_buf);
#endif
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm_buf);
        return buf;
    }

    // ── 成员变量 ─────────────────────────────────────────
    std::string              _filepath;
    LogLevel                 _min_level = LogLevel::DEBUG;
    size_t                   _max_bytes = 0;

    std::ofstream            _file;
    std::queue<std::string>  _queue;
    std::mutex               _mtx;
    std::condition_variable  _cv;
    std::atomic<bool>        _running;
    std::atomic<size_t>      _drop_count;
    std::thread              _worker;
};

} // namespace gobang

// ─── 使用宏（__FILE__ / __LINE__ 由编译器自动填入）────────────
// 用法：LOG_INFO("user " << user_id << " login");
#define LOG_DEBUG(msg) \
    do { std::ostringstream _oss; _oss << msg; \
         gobang::Logger::instance().log(gobang::LogLevel::DEBUG, _oss.str(), __FILE__, __LINE__); } while(0)

#define LOG_INFO(msg) \
    do { std::ostringstream _oss; _oss << msg; \
         gobang::Logger::instance().log(gobang::LogLevel::INFO,  _oss.str(), __FILE__, __LINE__); } while(0)

#define LOG_WARN(msg) \
    do { std::ostringstream _oss; _oss << msg; \
         gobang::Logger::instance().log(gobang::LogLevel::WARN,  _oss.str(), __FILE__, __LINE__); } while(0)

#define LOG_ERROR(msg) \
    do { std::ostringstream _oss; _oss << msg; \
         gobang::Logger::instance().log(gobang::LogLevel::ERROR, _oss.str(), __FILE__, __LINE__); } while(0)
