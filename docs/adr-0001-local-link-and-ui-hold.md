# ADR-0001：原厂 AP 本地链路 + 按线程挂起原厂 UI

- 状态：已接受（V1，2026-11-10）
- 相关：`HANDOFF.md`、`docs/stage0-capability-report.md`、`petcard/README.md`

## 背景

1. 设备的付费限制在**上网出口**，不在 Wi‑Fi 关联或本地入站；原厂 AP 的 `hostapd`/`udhcpd` 已在运行，`br0` 为 `192.168.0.1/24`。
2. 设备没有可用的 `wpa_supplicant` 二进制，无法直接做 STA 连手机热点。
3. 原厂 `zte_mifi` 有 `zte_watchdog` 线程；整进程 `SIGSTOP` 约 10 分钟会触发设备重启（两次复现）。
4. 显示驱动只把 `/dev/fb0` 的 mmap 内存推屏；直接写普通用户缓冲区返回 32768 但屏幕不更新。

## 决策

1. **保留原厂 AP，只做本地链路**：不杀/不改 `hostapd`，不启用 NAT/转发；Phone→`192.168.0.1:8420` 本地 HTTP。付费上网服务不使用，也不绕过。
2. **按线程挂起原厂 UI**：用 ptrace 只停 `mmi_lcd_main`、`mmihandlekey`、`mmi`、`mmihandlemsg`，保留 `zte_watchdog`、主线程、`hostap`/`wlan`/`udhcpd`。
3. **显示走 fb mmap**：把帧复制进 `mmap(/dev/fb0)` 再从 mmap 写；B-R-G 字段转换。
4. **存档放 `/mnt/userdata/petcard`**：原子写 + `.bak`，每 120 秒/退出保存；`/tmp` 仅开发用。

## 备选方案

- 自己起 hostapd/新 SSID：杀原厂 hostapd/重配 Wi‑Fi 后设备重启（已实测），放弃。
- 整进程 SIGSTOP `zte_mifi`：看门狗约 10 分钟重启，放弃。
- 设备 STA 连手机热点：缺 `wpa_supplicant`，需自编译，暂不作为 V1 路线。
- USB RNDIS 直连：设备已有 `usblan0` 并桥接 br0，但手机 OTG/USB 以太网兼容性与供电未验证；保留为备选。

## 后果

- 手机和设备只在本地网通信；手机可继续用蜂窝上网，App 需绑定 Wi‑Fi 网络走本地。
- 方案依赖原厂线程名与 ptrace；固件升级/线程重命名可能导致 `ui_hold` 失效，需要重新适配。
- 本地 HTTP 无认证；个人设备、WPA2 热点下接受，后续 App 可加 token/配对。
- BLE 缺失；若成为硬性需求，需重新评估硬件或接受外接方案。
