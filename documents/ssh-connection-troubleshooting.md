# SSH 连接问题排查总结

## 问题现象

VSCode Remote-SSH 连接虚拟机 `192.168.153.129` 失败，报错：
```
ssh: connect to host 192.168.153.129 port 22: Connection timed out
```

## 排查过程

### 1. 网络层诊断

| 检查项 | 结果 | 分析 |
|--------|------|------|
| Windows ping 虚拟机 | 100% 丢包 | 网络层不可达 |
| 虚拟机 ping 自己 | 通 | 协议栈正常 |
| `ip link show ens160` | `NO-CARRIER`, `state DOWN` | 链路层断开 |
| `ethtool ens160` | `Link detected: no` | 无载波信号 |
| Windows VMnet 适配器 | Up | 宿主机端正常 |
| 路由表 | `linkdown` 标记 | 链路层故障确认 |

### 2. 尝试的解决方案

| 方案 | 操作 | 结果 |
|------|------|------|
| 重启虚拟机网络服务 | `systemctl restart NetworkManager` | 无效 |
| 重新插拔虚拟网卡 | VMware UI → 可移动设备 → 网络适配器 | 无效 |
| 切换网络模式 | NAT → 桥接 → NAT | 无效 |
| **重启 VMware NAT 服务** | `Restart-Service "VMware NAT Service"` | **有效** |

## 根本原因

**VMware NAT Service 状态异常**，导致：

1. 虚拟交换机 (vSwitch) 链路状态不同步
2. 虚拟机网卡收不到载波信号 (`Link detected: no`)
3. 即使 VMware UI 显示"已连接"，底层物理链路实际是断开的

## 最终解决方案

### Windows 端（管理员 PowerShell）
```powershell
Restart-Service "VMware NAT Service" -Force
```

### 虚拟机端
```bash
sudo ip link set ens160 down
sudo ip link set ens160 up
```

执行后 `ethtool ens160` 输出 `Link detected: yes`，SSH 连接恢复。

## 问题本质

```
┌─────────────────────────────────────────────────────────────┐
│                      问题层级架构                            │
├─────────────────────────────────────────────────────────────┤
│  应用层  │  SSH 连接失败                                      │
│  网络层  │  Ping 超时，路由表 linkdown                        │
│  链路层  │  NO-CARRIER, Link detected: no ← 问题所在        │
│  服务层  │  VMware NAT Service 状态异常 ← 根本原因          │
└─────────────────────────────────────────────────────────────┘
```

## 复发场景

以下情况可能触发此问题：
- Windows 主机休眠/唤醒后
- VMware 长时间运行后
- 网络配置变更（如切换 WiFi/有线）后
- VMware 服务崩溃或无响应后

## 快速修复命令

```powershell
# Windows（管理员）
Restart-Service "VMware NAT Service" -Force

# 虚拟机
sudo ip link set ens160 down && sudo ip link set ens160 up
```

## 预防建议

1. 避免 Windows 休眠后直接使用 VMware 虚拟机
2. 定期重启 VMware NAT Service（如每周一次）
3. 可将重启脚本加入 Windows 开机自启

---

**文档生成时间**: 2026-04-20  
**问题环境**: Windows 11 + VMware Workstation + CentOS/RHEL Linux 虚拟机
