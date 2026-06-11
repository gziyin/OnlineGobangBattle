# 个人项目极简流程（直接推送 main）

本文档用于单人开发场景，目标是最少流程但可构建、可部署、可回滚。

## 1. 分支与提交规范

- 仅使用 `main` 分支。
- 每次改动直接提交并推送 `main`。
- 提交格式：`英文标志位: 中文说明`
- 推荐标志位：`feat|fix|docs|chore|refactor|test|build|ci|perf|style|revert`
- 示例：`feat: 修改mysql配置`

## 2. Windows 开发步骤

```powershell
git checkout main
git pull origin main
```

开发完成后：

```powershell
git status --short
git add <files>
git commit -m "fix: 中文说明"
git push origin main
```

## 3. Ubuntu 构建测试步骤

```bash
git checkout main
git pull origin main

cmake -S source -B source/build
cmake --build source/build -j
ctest --test-dir source/build --output-on-failure
```

说明：
- `ctest` 未通过时禁止部署。
- 线上禁止手改代码，所有修复必须回到本地提交后再拉取。

## 4. 部署与回滚

仓库提供两个脚本：
- `ops/deploy.sh <git-ref>`：部署指定分支/标签/提交（默认 `main`）
- `ops/rollback.sh [release_name]`：回滚到上一个版本或指定版本

首次部署前建议创建目录：

```bash
sudo mkdir -p /srv/online-gobang/releases
sudo chown -R "$(whoami)":"$(whoami)" /srv/online-gobang
```

部署命令：

```bash
bash ops/deploy.sh main
```

部署产物二进制：`/srv/online-gobang/current/source/build/bin/gobang_server`（见 `ops/deploy.sh` 输出）。

首次部署后安装 systemd（可选）：

```bash
sudo cp ops/gobang_server.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now gobang_server
```

Nginx 反代示例见 `ops/nginx-gobang.conf.example`。

回滚命令：

```bash
bash ops/rollback.sh
```

## 5. 版本标记（发布节点）

每次准备发布时建议打标签：

```bash
git tag v0.3.0
git push origin v0.3.0
```

如果发布失败，可结合标签快速定位并回滚。
