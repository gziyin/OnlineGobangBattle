# M2 Phase 1: db.hpp 数据库连接池实现计划

## Context

M1 基础设施已完成（logger.hpp, util.hpp），test_base 全部通过。现在实现 M2 阶段的第一个模块：数据库连接池。

**需求**：实现线程安全的 MySQL 连接池，支持并发请求和超时机制，防止数据库连接资源耗尽。

---

## 实现方案

### 1. 创建文件：`source/include/db.hpp`

实现 `DBPool` 类，包含：
- 连接池队列（`std::queue<MYSQL*>`）
- 互斥锁 + 条件变量（支持超时等待）
- 初始化方法 `init()` 从 Config 读取数据库配置
- 获取连接 `get_connection(timeout_ms)` - 超时返回 nullptr
- 归还连接 `return_connection()`
- 析构函数关闭所有连接

**关键点**：
- 使用 `std::condition_variable::wait_for()` 实现 3 秒超时
- 连接池大小默认 10（可配置）
- 线程安全，无死锁

### 2. 修改：`source/CMakeLists.txt`

添加 MySQL 客户端库依赖：
```cmake
pkg_check_modules(MYSQL REQUIRED mysqlclient)
```

### 3. 创建测试：`source/tests/test_db_pool.cpp`

测试用例：
- [ ] 连接池初始化测试
- [ ] 获取/归还连接测试
- [ ] 并发获取连接测试（10 线程，池大小 5，验证 3 秒超时）

### 4. 修改：`source/config/server.conf`

确保包含正确的 MySQL 配置（host, port, user, password, db_name, pool_size）

---

## 关键文件路径

- `source/include/db.hpp` - 新增
- `source/tests/test_db_pool.cpp` - 新增
- `source/CMakeLists.txt` - 修改
- `source/config/server.conf` - 可能需要调整

---

## 复用现有代码

- `gobang::util::Config` - 已定义数据库配置字段（util.hpp:170-192）
- `gobang::Logger` - 日志记录（logger.hpp）
- `gobang::util::load_config()` - 配置加载（util.hpp:212）

---

## 验证步骤

1. 虚拟机环境检查：
   ```bash
   pkg-config --exists mysqlclient && echo "mysqlclient: OK" || echo "mysqlclient: MISSING"
   ```

2. 在 Windows 本地编译（CMake 会同步到虚拟机）

3. 运行 test_db_pool 测试，验证：
   - 连接池初始化成功
   - 并发请求无死锁
   - 超时机制正常工作

---

## 风险与注意

- MySQL 连接需要正确的驱动库（mysqlclient）
- 虚拟机需要安装 `mysql-devel` 包
- Windows 本地编译可能需要配置 MySQL 路径（如果本地测试的话）
