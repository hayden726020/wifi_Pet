# 阶段 0 硬件能力报告（D623+ / PetCard V1）

更新：2026-11-10。全部结论来自本机 ADB 只读检查或实机运行，不包含推测。

## 1. 系统与资源

| 项目 | 结果 |
|---|---|
| 设备 | adol D623+，主板 M2_V1.1 |
| SoC | ZX297520V3，ARMv7 Cortex-A7（`CPU part 0xd03`），soft-float |
| 内核 | 3.4.110-rt140，uClibc 0.9.33.2 |
| ADB | `uid=0(admin) gid=0(root)`，调试模式跨重启保留 |
| RAM | 22,040 KiB；空闲约 1.7–2.5 MiB，另有约 5 MB cache |
| 根文件系统 | `/dev/mtdblock5` 6.9 MB，已用 97% |
| 持久可写 | `/mnt/userdata`（`/dev/mtdblock7`，jffs2）3.2 MB，空闲约 3.0 MB |
| 只读分区 | `/mnt/imagefs`（jffs2 ro）、`/mnt/nvrofs`（jffs2 ro） |
| 时间 | RTC `/dev/rtc0`，重启后时间正确 |

PetCore 静态二进制 588,244 B；画布/面板/快照各 32 KiB；HTTP 服务常驻。当前可用内存可支撑，但不要同时保留多份大二进制在 `/tmp`。

## 2. 显示

- `/dev/fb0`：128×128，16 bpp，SPI 面板 `lead_t15ds26`，单缓冲。
- 面板 16 位字段为 **B[15:11] R[10:5] G[4:0]**，不是常见 RGB565；PetCore 在整帧刷新时转换。
- 面板会自动 sleep。刷新前必须 `sleepout` + `blgon`（`/sys/kernel/debug/lcd_zx29/lcd_lead`），运行期间每 2 秒保活。
- **驱动只把 fb mmap 内存推到屏幕。** 直接 `write(fd, 用户缓冲区, 32768)` 即使返回 32768，屏幕仍显示旧内容；必须 `mmap /dev/fb0`，把帧复制进 mmap，再从 mmap 写。
- 整帧写入实测：平均 **21.6–22.0 ms**，最大约 23.5 ms（30 帧统计，`--debug` 日志 `FLUSH ... avg_us`）。
- `update` 是驱动自带彩条测试图，不是推帧命令，禁止使用。
- 退出时写回原厂 UI 快照；面板状态恢复为 `normal`。背光可能被原厂 UI 的休眠逻辑关掉，必要时补 `blgon`。

## 3. 按键

- 唯一按键 `KEY_POWER`，code 116；按下 value=1，长按自动重复 value=2，释放 value=0。
- PetCore 只取 0/1，实现短按、长按（≥650 ms）、双击（330 ms 窗口）。
- 实机确认：短按切换动作、长按执行、双击摸摸均正常，长按约 1 秒不会触发关机。

## 4. Wi‑Fi 与手机直连

- 无线芯片 AIC8800D80（`aic8818_fdrv`，mac80211/cfg80211，固件 `fmacfw_8800d80`）。
- 原厂 AP：`hostapd` + `udhcpd` + `dnsmasq`，网桥 `br0`（wlan0 + usblan0），地址 `192.168.0.1/24`，SSID `adol-2D5025`，密码为设备原厂默认值（不写入仓库，见设备标签）。
- 付费限制在**上网出口**，不在 Wi‑Fi 关联或本地入站：`iptables INPUT` 对 br0 全部放行，手机连上后可访问设备本地 TCP 端口。
- 实测：手机连接原厂热点后打开 `http://192.168.0.1:8420/` 正常，PetCore 页面和按钮可用；无需缴费、无运营商流量。
- 未启用 NAT/转发；PetCore 只做本地通信，不绕过任何上网计费。
- 设备没有可用 STA 工具（`wpa_supplicant` 二进制缺失），方案 B（保留原厂 AP + 手机蜂窝上网）是当前可行路线。
- 不要杀掉或替换原厂 `hostapd`：上一轮实验在杀 hostapd 后设备重启。保留原厂 AP，只在需要时挂起 UI 线程。

## 5. 蓝牙

- 无 `/sys/class/bluetooth`、无 HCI 设备、无 BT 固件/驱动，未发现 BLE 硬件。
- V1 手机控制走 Wi‑Fi 本地 HTTP；BLE 需作为硬件能力验收项单独重新确认。

## 6. 看门狗与 UI 接管（关键风险）

- `zte_mifi` 有 36 个线程，其中包括 `zte_watchdog`、`hostap`、`wlan`、`mmi_lcd_main`、`mmihandlekey` 等。
- 对整个进程 `SIGSTOP` 会连 `zte_watchdog` 一起冻结；实测约 10 分钟后设备重启（两次复现）。
- 当前方案：用 `ui_hold`（ptrace）只挂起 UI/按键线程：
  - `mmi_lcd_main` ×4、`mmihandlekey`、`mmi` ×2、`mmihandlemsg`；
  - 保持 `zte_watchdog`、`zte_mifi` 主线程、`hostap`、`wlan`、`udhcpd` 运行。
- 这样屏幕归 PetCore、按键归 PetCore，同时 AP 和看门狗继续工作。实测 UI 线程挂起约 **11.4 分钟、3510 帧无重启**（整进程 SIGSTOP 约 10 分钟即重启）；包装脚本启动/停止/快照恢复通过。更长运行时间仍需持续观察。
- 退出时 `ui_hold` 收到 SIGTERM 会 `PTRACE_DETACH`，原厂 UI 线程恢复；再写回快照并唤醒面板。

## 7. 资源与持久化

- 存档：`/mnt/userdata/petcard/state.bin`（原子写入，保留 `.bak`），约 40 B；每 2 分钟或退出时保存。
- 离线结算：启动时按 `last_seen` 补算，最多 24 小时；断网/断电后继续基本养成。
- `/tmp` 是 tmpfs，重启清空；二进制和临时脚本需要重新 push。PetCard 长期安装应放到 `/mnt/userdata`，但执行前要评估 jffs2 与内存。

## 8. 结论

- 阶段 1（显示/按键原型）已完成实机验收：宠物画面、三种按键操作、退出恢复。
- 阶段 2（离线养成）已完成：饥饿/心情/精力/亲密/经验/阶段、喂食/玩耍/摸摸/睡觉、存档与离线结算。
- 手机本地控制页（HTTP 8420）已实机可用，为阶段 3 提供协议基础。
- 阶段 3 的正式手机 App、阶段 4 云端 AI、阶段 5 常驻安装与外壳仍待开发。
- BLE 未发现硬件；若 V1 必须 BLE，需要重新评估需求。
