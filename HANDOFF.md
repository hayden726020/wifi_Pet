# D623+ 单键贪吃蛇 / PetCard 显示链路交接

更新：2026-11-10（软件基线）。硬件与私有备份交接见 Codex 2026-11-10 会话目录下的
`outputs/HANDOFF.md`（原始 16 MiB 闪存镜像与分区清单不在本仓库，禁止提交/上传/送远端服务）。

## 一句话状态

自定义程序、唯一按键、128×128 SPI 屏都已实机打通；单键贪吃蛇已显示并通过颜色校准，当前可作为 PetCard 的显示/输入基线。设备上的原厂 UI 在游戏期间被临时暂停，退出后恢复。

## 已验证事实（2026-11-10 实机）

- 设备：adol D623+，主板 M2_V1.1，SoC ZX297520V3；ARMv7 soft-float，uClibc 0.9.33.2，内核 3.4.110-rt140。
- ADB：调试模式下 `uid=0(admin) gid=0(root)`，设备节点 `/dev/event0`、`/dev/fb0`。
- 唯一按键：`KEY_POWER` code 116；按下 value=1，长按自动重复 value=2。游戏只取 value=1。
- 屏幕：fb0 128×128、16 bpp，SPI 面板驱动 `lead_t15ds26`；单缓冲，`virtual_size/stride/pan` 读 0，`FBIOPAN_DISPLAY` 返回 EINVAL。
- 原厂 UI：`zte_mifi`（当时 PID 650；PID 以 `ps` 为准）同时占用 `/dev/event0` 和 `/dev/fb0`；会按超时把面板置为 sleep。
- 面板调试接口：`/sys/kernel/debug/lcd_zx29/lcd_lead`，读取示例：
  ```
  debugfs_lcd_show  lcd_id = 0x33,
  debugfs_lcd_show  sleep mode  sleep,   (或 normal)
  debugfs_lcd_show  g_sLcd_Backlight  on,
  ```
- 驱动字符串中的命令词：`reinit`、`update`、`sleepin`、`sleepout`、`blgon`、`blgoff`、`brn`、`simspion`、`simspioff`。
- **关键坑 1：面板会 sleep。** sleep 时向 fb 写入无效（`write` 返回 0）。先 `echo sleepout > lcd_lead`、`echo blgon > lcd_lead`，状态变为 `normal` 后再写。
- **关键坑 2：`update` 不是推帧。** 写 `update` 会在 fb/面板上生成驱动自带的彩条测试图（带中文标签），不是把我们的帧推上去。游戏不要使用 `update`。
- **正确刷新路径（已实机确认）：**
  1. `sleepout` + `blgon` 唤醒面板；
  2. 绘制到 fb mmap；
  3. `write(fb_fd, fb_ptr, 32768)`。面板醒着时返回 32768；sleep 时返回 0。
  4. 每 2 秒重复 `sleepout`/`blgon` 保活，防止自动熄屏。
- **颜色格式（实机校准）：** 面板 16 位字段为 `B[15:11] R[10:5] G[4:0]`（B-R-G 565），不是常见 RGB565。写像素前必须做字段转换；当前 `snake.c` 的 `fill_rect()` 已转换：
  - `r5=(c>>11)&31; g6=(c>>5)&63; b5=c&31;`
  - `panel=(b5<<11)|(((r5<<1)|(r5>>4))<<5)|(g6>>1)`
  - 用户实机确认 TEST 四角：左上红、右上绿、左下蓝、右下白。
- 备份/恢复：进入游戏前 `kill -STOP zte_mifi`，快照 32 KiB fb；退出时用 `snake --restore` 写回快照、`sleepout`/`blgon`、再 `kill -CONT zte_mifi`。
- **不要用 `cat snapshot > /dev/fb0`**：驱动 write 返回 0 时 `cat` 会死循环烧 CPU。
- 游戏：单键贪吃蛇，16×14 格（8 px/格），顶部状态栏，READY/PLAY/OVER 三态；按键开始、顺时针转向、结束重开；速度 230 ms→130 ms；15 分钟 alarm 自动退出恢复。

## 构建与部署

- 工具链（WSL Ubuntu 22.04）：`gcc-arm-linux-gnueabi`（soft-float），静态链接：
  ```
  arm-linux-gnueabi-gcc -O2 -static -s -o snake snake.c
  arm-linux-gnueabi-gcc -O2 -static -s -o tracer tracer.c
  arm-linux-gnueabi-gcc -O2 -static -s -o sysloop sysloop.c
  ```
- 部署（`/tmp` 是 tmpfs，重启后清空，需重新 push）：
  ```
  adb push snake /tmp/snake
  adb push snake.sh /tmp/snake.sh
  adb push resume.sh /tmp/resume.sh
  adb shell 'chmod 755 /tmp/snake /tmp/snake.sh /tmp/resume.sh'
  ```
- 运行游戏：`adb shell '/tmp/snake.sh'`（自行暂停原厂 UI、快照、运行、恢复）。
  - `snake.sh` 使用 `/tmp/snake.lock` 防止重复运行；快照 `/tmp/fb0.orig` 是 write-once（不存在/无效时才创建）。要强制重新快照，先删除该文件。
  - 游戏子进程在后台运行；wrapper 收到 HUP/INT/TERM 时会杀掉子进程、恢复快照并 `SIGCONT` 原厂 UI。
- 调试模式：`--probe`（四色块+按键日志）、`--lcdtest`（唤醒+整帧写入测试）、`--demo`（自动演示）、`--watch`（只读按键）、`--fbinfo`、`--peek PID ADDR [N]`（读目标进程内存）、`--restore FILE`（恢复快照）。
- 停止游戏：`adb shell 'killall snake'`（SIGTERM，包装脚本恢复原厂 UI 并 `SIGCONT`）。异常恢复：`adb shell '/tmp/resume.sh'`（先 kill 游戏，再唤醒面板/恢复快照/`SIGCONT`，并清锁）。
- 状态检查：
  ```
  adb shell 'ps | grep snake; cat /sys/kernel/debug/lcd_zx29/lcd_lead; grep State /proc/650/status'
  ```

## 当前运行状态（基线提交时）

- 已停止游戏并恢复原厂 UI：`zte_mifi` 处于 `S (sleeping)`，面板 `normal`、背光 `on`，`/tmp/snake.lock` 不存在。
- 启动游戏：`adb shell '/tmp/snake.sh'`；停止：`adb shell 'killall snake'`；异常恢复：`adb shell '/tmp/resume.sh'`。
- 以下测试已通过（2026-11-10）：
  - wrapper 收到 SIGTERM：子进程停止、快照恢复、UI `SIGCONT`，锁清理。
  - 游戏运行中执行 `resume.sh`：游戏停止、UI 恢复、返回码 0。
  - 强制 `echo sleepin > lcd_lead` 后 3 秒内，游戏自动 `sleepout` 并回到 `normal`。
- `/tmp` 中的文件与进程在设备重启后全部消失。

## 已知问题与风险

1. 暂停 `zte_mifi` 会冻结原厂 UI；正常退出会恢复。wrapper 已改为后台子进程 + trap；若 wrapper 或游戏被 SIGKILL、ADB 会话异常断开，可能留下 UI 停止/面板亮着：先跑 `/tmp/resume.sh`，必要时重启设备（调试模式已验证跨重启保留）。
2. 面板 sleep 是主要坑：任何新程序都必须先 `sleepout`/`blgon`，并在运行期间保活；否则 `write` 返回 0、屏幕只发光。`flush_fb` 在 `write` 非 32768 时会重新唤醒并保持 dirty，下一轮重试。
3. `update` 是驱动测试图命令，不是帧刷新；不要混用。
4. 颜色字段 B-R-G 是本机实机校准结果；换屏/换固件后需重新校准。
5. 设备内存约 22 MiB、`/tmp` 约 1 MiB、`/mnt/userdata` 约 3 MiB；当前静态二进制约 0.5 MiB（514,500 B）。后续 PetCard 常驻需做内存/存储预算。
6. 私有闪存镜像、固件分块、NV 文件、fb 原始转储均在 `work/`（已 gitignore），不得提交。`work/` 中的 `cpuap_*.lzma` 解出的内核/固件仅用于只读分析。
7. `/tmp` 不是持久化位置；PetCard 若要开机自启，需要设计 `/mnt/userdata` 安装与启动脚本，并评估与 `zte_mifi` 的共存。
8. `tracer` 正常收到 SIGINT/TERM/HUP 会 `PTRACE_DETACH`；若被 SIGKILL，目标进程可能留在 ptrace stop，需要 `kill -CONT <pid>`（本机 `zte_mifi` 为 650）。

## 下一步（PetCard V1）

1. 先做本局游戏的人工试玩：确认转向、吃到食物、撞墙/撞自己、结束后重开、15 分钟自动恢复原厂 UI。
2. 按 `PetCard_V1_项目实施方案.md` 开发 PetCore：显示引擎、养成状态、按键管理、与手机通信；直接复用已验证的 `sleepout/blgon + 整帧 write + B-R-G 颜色转换`。
3. 先做 32×32/64×64 宠物精灵放大到 128×128，测刷新率、内存与 SPI 写入耗时，再决定动画帧率。
4. 需要产品决策/验证：常驻方式（`/mnt/userdata`/开机脚本）、是否停 `zte_mifi`、Wi-Fi STA 与 BLE 能力、手机端协议。
5. 验收门槛：**实机屏幕 + 实体按键 + 退出后原厂 UI 恢复**；不要把 fb 回读或 `update` 测试图当成面板证据。

## 本次会话证据与复现

- 按键探针日志：`EV type=1 code=116 value=1`，确认唯一按键为 KEY_POWER。
- 显示测试日志：`FBWRITE n=32768 errno=0`（面板醒着）；sleep 时同一写返回 0。
- 用户实机确认（颜色校准）：TEST 四角为左上红、右上绿、左下蓝、右下白。
- 面板状态：`sleep mode normal`、`g_sLcd_Backlight on`。
- 恢复测试：wrapper SIGTERM 后无残留 `snake`、`zte_mifi` 为 `S`、面板 `normal`、锁已清理；`resume.sh` 在游戏运行中恢复并返回 0；强制 `sleepin` 后 3 秒内自动回到 `normal`。
- 构建：WSL `arm-linux-gnueabi-gcc -O2 -static -s`；部署：adb push 到 `/tmp`。
- 会话耗时约 70 分钟；精确 Token/检索次数未统计。返工主要集中在面板 sleep、`update` 测试图误判和颜色字段顺序。
