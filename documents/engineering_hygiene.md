# Engineering Hygiene

## 1. 可提交产物

- 源码：`source/include/`、`source/tests/`、`client/`
- 文档：`documents/`、`plan_and_review/`
- 构建配置：`source/CMakeLists.txt`、`.gitignore`

## 2. 不可提交产物

- 构建产物：`source/build/`
- 运行日志：`source/logs/`、`logs/`
- 本机目录：`%SystemDrive%/`、`.deepseek/`
- 临时文件：`*.log`、`*.bak`

## 3. 提交前检查命令

```bash
git status --short
git check-ignore -v source/build/* source/logs/* logs/* %SystemDrive%/* .deepseek/*
```

## 4. 原则

- 每次提交只做一类改动（编码/文档/测试/安全/治理分离）。
- 先保证构建与测试入口可复现，再考虑后续扩展。
- 文档必须描述真实代码状态，避免“计划领先于实现”。

## 5. 远程提交和拉取（GitHub + Gitee）

```bash
# 推送到 GitHub
git push origin main

# 推送到 Gitee
git push gitee main

# 两个都推
git push origin main && git push gitee main
```

```bash
# 从 Gitee 拉取
git pull origin main

# 从 GitHub 拉取
git pull github main

```