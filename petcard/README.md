# PetCore（设备端）

PetCard V1 的 ZX297520V3 设备端程序：宠物显示、按键交互、本地养成、存档，以及给手机用的本地 HTTP 控制页。复用了已验证的 `sleepout/blgon + 整帧 fb 写入 + B-R-G 颜色转换` 路径。

## 构建

在 WSL（Ubuntu，已安装 `gcc-arm-linux-gnueabi`）里：

```sh
cd petcard
./build.sh
```

产物：

- `petcard`：ARM 静态二进制，部署到设备。
- `ui_hold`：只挂起原厂 UI 线程的 ptrace 小工具，必须与 `petcard.sh` 一起部署。
- `petcard_sim`：主机模拟器，渲染各状态帧并跑逻辑自测。

## 部署

```sh
adb push petcard /tmp/petcard
adb push ui_hold /tmp/ui_hold
adb push petcard.sh /tmp/petcard.sh
adb push resume.sh /tmp/petcard_resume.sh
adb shell 'chmod 755 /tmp/petcard /tmp/ui_hold /tmp/petcard.sh /tmp/petcard_resume.sh'
```

Git Bash 下 `adb push` 的远端绝对路径会被 MSYS 改写；使用 Windows 源路径并加 `MSYS_NO_PATHCONV=1`：

```sh
MSYS_NO_PATHCONV=1 adb push "D:/Pet_wifi/petcard/petcard" /tmp/petcard
```

`/tmp` 是 tmpfs，设备重启后清空，需要重新 push。

## 运行与恢复

```sh
adb shell '/tmp/petcard.sh --debug'
```

- 包装脚本先快照原厂 fb，再启动 `ui_hold` 挂起原厂 LCD/按键/UI 消息线程。
- `ui_hold` 必须报告 `HOLDING`，否则包装脚本拒绝运行（避免整进程冻结触发看门狗重启）。
- `zte_watchdog`、`hostap`、`wlan`、`udhcpd` 保持运行，所以原厂热点和看门狗不受影响。
- 停止：`adb shell 'killall petcard'`。包装脚本会恢复快照、释放 UI 线程。
- 异常恢复：`adb shell '/tmp/petcard_resume.sh'`。先杀 PetCore/`ui_hold`，再唤醒面板、写回快照、删除锁。

**不要** 对整个 `zte_mifi` 执行 `SIGSTOP`：实测约 10 分钟后看门狗重启设备。也不要杀原厂 `hostapd`。

## 按键

唯一按键 `KEY_POWER`（code 116）：

- 短按：循环 `FEED → PLAY → SLEEP`。
- 长按（≥650 ms）：执行当前动作。
- 双击（330 ms 窗口）：摸摸。
- 睡觉时任意按键唤醒。

## 手机本地控制页

手机连接原厂热点（`adol-2D5025`，密码使用原厂默认值）后访问：

```
http://192.168.0.1:8420/
```

页面显示饱食/心情/精力，提供喂食、玩耍、摸摸、睡觉/唤醒按钮。只用本地链路，不需要付费上网，也不经过运营商流量。

HTTP 接口（JSON）：

- `GET /state` → `hunger/satiety/mood/energy/intimacy/exp/stage/sleeping/age_min/...`
- `GET /action?do=feed|play|pet|sleep|wake` → 执行动作并返回新状态。
- `GET /` → 手机控制页。
- `GET /favicon.ico` → 空响应。

注意：宠物睡觉时 `feed/play/pet` 只返回 `ZZZ...`，需先 `sleep`（切换）或 `wake`。

## 养成与存档

- 属性：饥饿（0–100，越高越饿）、心情、精力、亲密度、经验、年龄、阶段（BABY/KID/TEEN/ADULT）。
- 每 60 秒结算：饥饿上升；饥饿/疲惫时心情下降更快；清醒耗精力，睡觉回精力；精力满自动醒。
- 启动时按 `last_seen` 补算离线时间（最多 24 小时）。
- 存档：`/mnt/userdata/petcard/state.bin`，写入临时文件后原子重命名，并保留 `state.bak`；每 120 秒或退出时保存。
- 备用路径：`/tmp/petcard-state`（掉电丢失）；主路径失败时自动回退，并在后续保存时重试主路径。
- 包装脚本会监控 `ui_hold`；若它退出，会停止 PetCore 并恢复原厂 UI。

## 调试

- `petcard --debug`：打印启动状态、HTTP 动作、每 30 帧的整帧写入耗时。
- `petcard --status`：读取存档并输出 JSON，不打开屏幕/按键。
- `petcard --reset`：删除存档。
- `petcard --restore FILE`：把 32768 B 的面板快照写回 fb（会先唤醒面板）。
- `ui_hold --list [PID]`：列出 `zte_mifi` 的线程名和 TID。

## 已知限制

- **必须从 mmap 的 fb 写入**；直接 write 普通缓冲区会返回 32768 但屏幕不更新。
- 面板会睡；程序每 2 秒 `sleepout/blgon` 保活，写失败会重试。
- 原厂 UI 的背光休眠逻辑偶尔会把背光关掉；`resume.sh` 会补 `blgon`。
- 没有发现 BLE 硬件；V1 手机控制走 Wi‑Fi 本地 HTTP。
- 原厂热点未改；付费限制只影响上网，不影响本地 `192.168.0.1` 通信。
- `/tmp` 非持久；长期使用需要把程序和启动脚本安装到 `/mnt/userdata` 并评估开机自启。
