# PetCore（设备端）

PetCard V1 的 ZX297520V3 设备端程序：宠物显示、按键交互、本地养成、存档，以及给手机用的本地 HTTP 控制页。显示使用 `sleepout/blgon + 整帧 fb mmap 写入`；面板像素格式可用启动参数选择并需实机确认。

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

屏幕也会使用 `assets/` 中的 64×64 透明表情帧，程序在设备端将其合成到 RGB565 画布，再沿用 fb mmap 刷新。清理 4×5 表情图并重新生成运行素材：

```sh
python import_sprite_sheet.py assets/pet_emotions_64_sheet.png
python prepare_sprites.py
```

两条命令需要安装 Pillow；屏幕端使用随仓库提供的 `.rgba` 文件，不依赖设备端 PNG 解码器。

## 部署

### 开机接管（阶段 3）

`install_startup.sh` 用来让 PetCard 随设备启动：`/etc/rc` 在 `fs_check` 和设备节点创建后，先把 PetCard 启动/关机动画绑定到原厂 MMI 动画路径并预检 userdata 网页素材，再启动 `autostart.sh`。启动脚本等待 PetCard 首个完整 framebuffer 帧写成功后，才继续执行 `zte_mifi`；管理器随后挂起 MMI/按键 UI 线程。若 PetCard 未在 2 秒内就绪，系统继续原启动流程，管理器会在挂起 MMI 后回退启动 PetCard。原厂网络、看门狗和 USB/ADB 服务仍由 `/etc/rc` 启动。原厂 MMI 程序仍在固件中运行，不是固件删除或完整替换。

`prepare_boot.sh` 还会在挂载素材后尽早执行 `petcard --restore "$BASE/boot-frame.raw"`，在 userdata 可用时用 PetCard 启动图替换面板画面，缩短原厂 Power On 的可见时间。`boot-frame.raw` 是 128×128 RGB565 高字节优先的 32768 B 帧，由 `assets/boot/image_poweron_0.png` 生成；安装到 userdata 时需一并推送。该步骤不能移除 Linux 用户态之前由 bootloader/内核或面板保留的原厂画面。

虽然设备日志确认 PetCard 首帧先于 `zte_mifi` 且 MMI 动画资源被 PetCard 覆盖，用户仍报告启动时看到原厂 Power On。fb 时间线显示 3.2 s 后没有原厂帧进入 Linux framebuffer，当前判断该画面来自更早启动阶段或面板保留；当前工作流不写 MTD/NV/bootloader。

```sh
/mnt/userdata/petcard/install_startup.sh
```

脚本会先把原始 `/etc/rc` 和 `/etc/inittab` 备份到 `/mnt/userdata/petcard/rollback/`，再将 `/etc/rc` 改为可写、插入启动钩子并恢复只读。回滚：

```sh
/mnt/userdata/petcard/rollback_startup.sh
```

只回滚启动入口，不会删除 `/mnt/userdata/petcard` 中的程序、素材或存档。固件根分区保持只读；此流程不写 MTD/NV/bootloader。

设备端 `prepare_boot.sh`、`autostart.sh`、`install_startup.sh`、`rollback_startup.sh` 和 `generate_boot_assets.py` 随仓库 `petcard/` 提供。启动素材在 `assets/boot/`，由生成脚本从已有宠物帧生成。

```sh
adb push petcard /tmp/petcard
adb push ui_hold /tmp/ui_hold
adb push petcard.sh /tmp/petcard.sh
adb push resume.sh /tmp/petcard_resume.sh
adb shell 'mkdir -p /tmp/petcard-www/assets'
adb push companion/index.html /tmp/petcard-www/index.html
adb push assets/happy_1.png assets/happy_2.png assets/happy_3.png assets/happy_4.png assets/hungry_1.png assets/hungry_2.png assets/hungry_3.png assets/hungry_4.png assets/sleepy_1.png assets/sleepy_2.png assets/sleepy_3.png assets/sleepy_4.png assets/grumpy_1.png assets/grumpy_2.png assets/grumpy_3.png assets/grumpy_4.png assets/sleep_1.png assets/sleep_2.png assets/sleep_3.png assets/sleep_4.png /tmp/petcard-www/assets/
adb push assets/*.rgba /tmp/petcard-www/assets/
adb shell 'chmod 755 /tmp/petcard /tmp/ui_hold /tmp/petcard.sh /tmp/petcard_resume.sh'
```

Git Bash 下 `adb push` 的远端绝对路径会被 MSYS 改写；使用 Windows 源路径并加 `MSYS_NO_PATHCONV=1`：

```sh
MSYS_NO_PATHCONV=1 adb push "D:/Pet_wifi/petcard/petcard" /tmp/petcard
```

手机 Companion 页面由 PetCore 从 `/tmp/petcard-www` 提供。每次设备重启后，按上面的步骤推送页面、20 帧 PNG 和 20 个 RGBA 屏幕帧（目录位于 tmpfs）。页面使用现有 `/state`、`/action` 接口，不需要互联网；服务端只会公开 `/assets/` 下列出的 PNG 帧。屏幕直接使用 RGBA 帧素材；缺少素材时保留程序绘制的宠物作为回退。

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
- 按住 5 秒：显示 PetCard「是否关机？」确认页；松开后再短按一次确认关机。
- 确认页上再次长按或等待 15 秒：取消关机并回到宠物页面。
- 关机确认页的单击不会执行喂食/玩耍/睡觉等宠物操作。

关机请求默认进入**软关机**：保存状态、显示 GOOD NIGHT 页面、写入 fb 后执行 `sleepin`/`blgoff` 熄屏；不调用 `/sbin/poweroff`，不创建管理器抑制标记，PetCard 和管理器保持运行，按电源键唤醒并恢复宠物界面。`--hard-poweroff` 可切回真实断电路径：保留关机父进程和画面，`/tmp/petcard-shutdown` 抑制管理器重新拉起；等待期间每 2 秒唤醒面板，30 秒内仍未断电则记录 `timeout`、清理标记并恢复宠物界面。真实路径的退出不恢复旧 framebuffer，持久诊断记录位于 `/mnt/userdata/petcard/poweroff.log`。软关机不会改变系统电源状态；设备无电池、USB-C 供电时真实断电可能立即被供电唤醒。

`ui_hold` 启动前 10 秒每 100ms 检查新增 UI 线程，之后每秒检查。主机测试已验证迟建显示线程及时接管、非 UI 线程继续运行；原厂 Power On 是否完全消失仍需实机验证。

在 WSL/Linux 中运行关机和线程接管回归（只使用无害替代命令和合成进程，不操作设备）：

```sh
cd petcard
sh test_shutdown.sh
sh test_autostart_shutdown.sh
cc -O2 -o /tmp/petcard_ui_hold_host ui_hold.c
cc -std=c11 -O2 -pthread -o /tmp/petcard_ui_hold_test test_ui_hold.c
/tmp/petcard_ui_hold_test /tmp/petcard_ui_hold_host
```

`test_shutdown.sh` 不只检查退出码，还要求输出 `RESULT PASS`；若关机实现退回 `execl` 替换测试进程，输出缺失会直接判失败。当前关机回归 20 项。

## 手机本地控制页

手机连接原厂热点（`adol-2D5025`，密码使用原厂默认值）后访问：

```
http://192.168.0.1:8420/
```

页面是面向手机的 Companion 控制界面，按设备情绪播放 `assets/` 中的四帧宠物动画，并直接调用本地 `/state`、`/action`。只使用设备热点，不请求云端；设备重启后需按部署步骤重新推送页面和动画帧。

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
- `petcard --freeze-frame`：固定人物动画帧，供屏幕诊断。
- `petcard`：默认按 RGB565 高字节优先输出，实物屏确认该格式可清除色块；关机默认软关机。
- `petcard --hard-poweroff`：关机时改用真实 `/sbin/poweroff` 路径（调试/对比用）。
- `petcard --panel-bgr565`：回退到原 B-R-G 映射。
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
