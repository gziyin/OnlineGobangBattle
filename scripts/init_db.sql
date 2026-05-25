-- Online Gobang Battle - Database Initialization
-- Idempotent: safe to run multiple times
-- Usage: mysql -u root -p < scripts/init_db.sql

CREATE DATABASE IF NOT EXISTS gobang_db
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

USE gobang_db;

CREATE TABLE IF NOT EXISTS user (
    id INT UNSIGNED PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(32) UNIQUE NOT NULL COMMENT '用户名',
    password_hash VARCHAR(512) NOT NULL COMMENT 'PBKDF2 密码哈希 (algorithm$iterations$salt$hash)',
    score INT UNSIGNED DEFAULT 1500 COMMENT '天梯分数',
    total_count INT UNSIGNED DEFAULT 0 COMMENT '总场次',
    win_count INT UNSIGNED DEFAULT 0 COMMENT '胜场',
    status TINYINT DEFAULT 0 COMMENT '0-离线 1-大厅 2-房间',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_score (score),
    INDEX idx_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
