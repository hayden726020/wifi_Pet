# D623+ / PetCard V1 交接

## 最新状态：2026-11-11 手机页面故障排查（优先于下文历史快照）

- 用户确认手机连接设备热点 `adol-2D5025`，但页面最初打不开；启动尝试前设备屏幕显示原厂 UI。ADB 设备序列 `1234567890ABCDEF` 可连接。
- 设备刚重启后 `/tmp` 清空；`/proc/uptime` 曾为 64.91 秒，`dmesg`/`logread` 不存在，重启原因未知。未执行整进程 SIGSTOP 或主动设备重启。之后按用户授权重新部署程序、恢复脚本、页面和 44 个素材。
- 设备没有 `nohup`。后台启动会在 ADB shell 关闭时收到 `Hangup`，包装脚本按设计停止 PetCard 并释放 UI 线程；改为保持 ADB shell 前台连接后启动成功。
- **当前运行**：`ui_hold` 报告 `HOLDING 8 threads`，PetCore 日志显示 `HTTP listening on 0.0.0.0:8420`；`zte_mifi` 与 `hostapd` 保持运行。USB ADB 临时转发验证 `/state` 返回 JSON，主页和动画 PNG 分别返回 HTTP 200；临时转发已移除。用户手机反馈页面和按钮操作正常；设备日志记录 feed/play/pet/sleep 请求成功，`interactions` 从 17 增至 21。检查期间 uptime 从约 325 秒增长到约 906 秒，无再次重启。阶段 3 Companion 手机视觉/触控验收通过。
- **保持启动用的 ADB shell 连接打开**：该设备没有 nohup/setsid；shell 断开会触发包装脚本清理、恢复原厂 UI。电脑端 ADB 位于 `C:\Users\SPcuteAG\AppData\Local\Android\Sdk\platform-tools\adb.exe`。下列颜色修正中的 PID 与运行状态属于旧快照，不代表当前。

## 此前状态：2026-11-11 颜色格式修正

- 用户确认 `--freeze-frame` 静止画面仍有彩色色块，排除动画切帧为主因。此前 framebuffer 回读预览干净不能证明面板颜色正确；预览使用了发送端对应的 B-R-G 解码，缺少独立校准。
- 将原始回读按 **RGB565 高字节在前** 解码，可复现照片中的蓝底、绿状态条和人物彩色斑块。高度怀疑历史 B-R-G 颜色映射结论错误；纯原色校准不足以验证中间色和位序。
- 用户已确认固定帧在 RGB565 高字节优先模式下色块消失。该格式现设为默认，旧 B-R-G 保留为 `--panel-bgr565` 回退选项；帧刷新仍走 fb mmap/write，快照恢复逻辑不变。
- 默认格式构建、宿主模拟测试（原色/暗底/灰色/黑白/旧格式）和独立复核通过。已重新部署程序、ui_hold、启动脚本和 20 张 RGBA 帧。
- **此前运行状态（当前已过时）**：PetCard 曾运行默认 RGB565 BE 动画版（PID 1789，wrapper PID 1777，ui_hold PID 1786，HOLDING 8 threads）；`zte_mifi` PID 650 与当时的 `hostapd` PID 1085 正常。间隔 300 ms 的 framebuffer 回读不同，人物区域变化，面板 `normal`、背光 `on`。
- 历史安全提醒：ADB transport 丢失本身不能证明设备重启或断电；应结合 uptime 等证据。禁止整进程 SIGSTOP zte_mifi，禁止杀 hostapd，显示必须 fb mmap 写。

更新：2026-11-11。本次提交推送后成为 PetCard 当前基线；具体提交号以 `git log -1` 为准。清理素材、设备端 Companion 与 RGB565 字节序修复已部署验证。

## 一句话状态

PetCore 本地电子宠物已完成阶段 1/2 实机验收：128×128 宠物显示、单键短按/长按/双击、离线养成、存档、退出恢复原厂 UI。清理后的 64×64 透明帧按五类情绪四帧播放并随成长放大；面板现用用户实机确认的 RGB565 高字节优先格式，经 fb mmap 写入。阶段 3 Companion 页面及手机 Wi‑Fi 视觉/喂食/玩耍/摸摸/睡觉触控验收已通过，USB ADB 也验证 `/state`、主页和 PNG 路由。PetCard 当前运行；启动用 ADB shell 必须保持打开，否则设备会向包装脚本发送 Hangup 并恢复原厂 UI。Android App 和长连接/WebSocket 尚未决定或开发。未使用付费上网或运营商流量；未发现 BLE 硬件。阶段 4 云端 AI、阶段 5 常驻安装与外壳仍待做。

## 交接快照（2026-11-10 23:15 交接时）

- **Git**：`main` 代码基线 `15a4e29`（PetCore + 本地链路 + 5 个 P1 修复）；本交接文件提交后以 `git log -1` 为准。
- **远端**：`origin https://github.com/hayden726020/wifi_Pet.git`，分支 `main`；推送状态以 `git status` 的 ahead/behind 为准。
- **设备**：已停回原厂 UI；`zte_mifi` 为 `S`；`hostapd`/`udhcpd` 运行；面板 `normal`、背光 `on`；无 `petcard`/`ui_hold` 进程；存档在 `/mnt/userdata/petcard`（约 40 B）；`/tmp` 中有修复版 `petcard`、`ui_hold`、`petcard.sh`、`petcard_resume.sh`（重启清空）。
- **结论**：阶段 0/1/2 与手机本地链路已完成；阶段 3/4/5 未开始。下一对话可直接从文末“新对话启动包”开始。

## 本轮新增文件

- `petcard/petcard.c`：设备端主程序（显示、状态、按键、HTTP、存档）。
- `petcard/ui_hold.c`：只挂起原厂 UI/按键线程的 ptrace 工具，必须与 `petcard.sh` 一起用。
- `petcard/sim.c`：主机模拟器 + 自测（渲染各状态帧、养成/存档/按键逻辑）。
- `petcard/petcard.sh`、`petcard/resume.sh`、`petcard/build.sh`、`petcard/README.md`。
- `docs/stage0-capability-report.md`：阶段 0 只读检测与能力结论（内存、存储、Wi‑Fi、BLE、看门狗）。
- `docs/known-issues.md`、`docs/function-matrix.md`、`docs/adr-0001-local-link-and-ui-hold.md`：已知问题、功能矩阵、关键决策。

## 已验证事实（2026-11-10 晚，实机）

- 设备：adol D623+，M2_V1.1，ZX297520V3，ARMv7 Cortex-A7，soft-float，内核 3.4.110-rt140，uClibc。
- ADB root 跨重启保留；`/dev/fb0` 128×128 16bpp；唯一按键 `KEY_POWER` code 116。
- **显示必须走 fb mmap**：把帧复制进 `mmap(/dev/fb0)` 再从 mmap `write`，驱动才推屏；直接 write 普通用户缓冲区即使返回 32768，屏幕仍是旧内容。
- 面板字段 **B[15:11] R[10:5] G[4:0]**；写像素前必须做 B-R-G 转换（PetCore `flush_fb`）。
- 面板会 sleep；写前 `sleepout` + `blgon`，运行期每 2 秒保活；写返回 0 时唤醒并重试。
- 整帧写入实测平均 **21.6–22.0 ms**，最大约 23.5 ms（`--debug` 的 `FLUSH ... avg_us`）。
- 宠物画面、短按/长按/双击、退出恢复均已由用户在物理屏幕确认。
- 手机连原厂热点 `adol-2D5025`（使用原厂默认密码，见设备标签，不写入仓库）后，`http://192.168.0.1:8420/` 页面和按钮正常；用户确认可用，无需缴费、无运营商流量。
- 存档 `/mnt/userdata/petcard/state.bin` 已生成（`.bak` 同目录），重启后能加载并继续年龄/经验。
- 手机直连只走本地 `192.168.0.1/24`；未启用 NAT/转发，不绕过任何上网计费。
- 长稳测试：`ui_hold` 只挂 UI 线程，`zte_watchdog`/`hostap`/`wlan` 保持运行；UI 线程挂起约 **11.4 分钟、3510 帧无重启**（超过此前整进程 SIGSTOP 约 10 分钟的重启窗口），整帧平均 21.7 ms。更长时间仍需持续观察。
- 包装脚本 `petcard.sh` 实测：`HOLDING` 检查、启动、`killall petcard` 停止、UI 线程释放、快照恢复原厂 UI 均通过。

## 关键坑与安全边界

1. **不能对整个 `zte_mifi` 做 SIGSTOP。** 它内部有 `zte_watchdog` 线程；整进程冻结约 10 分钟后设备重启（两次复现）。必须用 `ui_hold` 只挂 UI 线程：
   - 挂：`mmi_lcd_main` ×4、`mmihandlekey`、`mmi` ×2、`mmihandlemsg`；
   - 不挂：`zte_watchdog`、`zte_mifi` 主线程、`hostap`、`wlan`、`udhcpd`。
2. **不要杀/替换原厂 `hostapd`。** 杀掉或重配 Wi‑Fi 会触发设备重启。保留原厂 AP；本地 HTTP 只在 `192.168.0.1` 上服务。
3. 显示路径不要用 `update`（驱动彩条测试图），不要只写普通缓冲区；必须 mmap。
4. 面板 sleep 时写 fb 无效；先唤醒，失败重试，并每 2 秒保活。
5. `/tmp` 是 tmpfs，重启清空；当前二进制（petcard 约 588 KB + ui_hold 约 502 KB）放在 `/tmp` 只适合开发。长期运行应安装到 `/mnt/userdata` 并评估 jffs2 执行、内存和开机自启。
6. 私有闪存镜像、固件分块、NV、fb 原始转储在 `work/`（gitignore），禁止提交/上传。
7. 电池焊盘未修；当前 USB‑C 外接供电。外接移动电源的低负载断电/启动电流仍需实测。
8. BLE：无 `/sys/class/bluetooth`、无 HCI、无 BT 固件，V1 手机控制走 Wi‑Fi 本地 HTTP。若 BLE 是硬性要求，需要重新评估硬件。
9. 不要用 `cat snapshot > /dev/fb0` 写回：面板 sleep 时 `write` 返回 0，`cat` 会死循环烧 CPU。用 `petcard --restore`。
10. `ui_hold` 被 SIGKILL 可能留下 ptrace 停止的 UI 线程；优先 SIGTERM，异常时用 `resume.sh`，必要时重启设备。

## 网络与付费边界

- 原厂 AP：`hostapd` + `udhcpd` + `dnsmasq`，`br0` = wlan0 + usblan0，`192.168.0.1/24`，SSID `adol-2D5025`，密码为原厂默认值（不写入仓库）。
- 付费限制在**上网出口**，不在 Wi‑Fi 关联或本地入站；`iptables INPUT` 对 br0 放行。PetCore 只做 `192.168.0.1:8420` 本地 HTTP，不启用转发/NAT。
- 设备没有可用 `wpa_supplicant` 二进制，STA 方案暂不可行；保留原厂 AP + 手机蜂窝上网（方案 B）是当前路线。
- 手机 App 若同时需要本地 HTTP 和云端 AI，Android 侧应使用 `ConnectivityManager` 绑定 Wi‑Fi 网络走本地、蜂窝走云端。

## 构建 / 部署 / 运行

WSL（Ubuntu，已装 `gcc-arm-linux-gnueabi`）：

```sh
cd petcard
./build.sh
# 产物：petcard（ARM 静态）、ui_hold（ARM 静态）、petcard_sim（主机自测）
```

部署（Git Bash 注意 `MSYS_NO_PATHCONV=1`，源路径用 Windows 形式）：

```sh
adb push petcard /tmp/petcard
adb push ui_hold /tmp/ui_hold
adb push petcard.sh /tmp/petcard.sh
adb push resume.sh /tmp/petcard_resume.sh
adb shell 'chmod 755 /tmp/petcard /tmp/ui_hold /tmp/petcard.sh /tmp/petcard_resume.sh'
```

运行：

```sh
adb shell '/tmp/petcard.sh --debug'
```

停止：`adb shell 'killall petcard'`。异常恢复：`adb shell '/tmp/petcard_resume.sh'`。

`petcard.sh` 会先快照原厂 fb，启动 `ui_hold` 并确认 `HOLDING`，再启动 PetCore；退出时杀 PetCore/`ui_hold`、写回快照、唤醒面板。`ui_hold` 每隔 5 秒重扫线程，原厂 UI 线程若被重新拉起会再次挂起。

常用调试：

```sh
adb shell '/tmp/petcard --status'     # 读存档 JSON
adb shell '/tmp/petcard --reset'      # 删存档
adb shell '/tmp/ui_hold --list 650'   # 列出 zte_mifi 线程
adb shell 'cat /sys/kernel/debug/lcd_zx29/lcd_lead'
```

## 历史设备状态（2026-11-10 收尾时，已被本文顶部状态取代）

- 清理帧显示测试已完成：设备运行时 UI holder 只挂起 8 个 UI 线程；fb mmap 刷新约 22 ms；framebuffer 回读 32768 B，预览确认新轮廓。
- 当时 PetCore 已退出并恢复原厂 UI；随后设备按用户要求关机。当前运行状态见本文顶部最新状态。
- 当时程序和素材位于 `/tmp`；本次已重新部署，设备重启后仍会清空。
- 存档保留在 `/mnt/userdata/petcard/`。

## 下一步

1. 阶段 3 页面验收已完成：手机在设备 AP 下打开 Companion 页并确认视觉与喂食/玩耍/摸摸/睡觉按钮正常；设备日志与状态计数吻合。后续产品决策：是否包装为 Android App、是否需要长连接/WebSocket。
2. 阶段 4：云端 AI 代理、人格/记忆；情绪只作为建议，数值由本地养成引擎校验。
3. 阶段 5：安装到 `/mnt/userdata`、开机自启、与 `zte_mifi` 的 UI 线程挂起/恢复自动化；外壳、USB‑C 供电与移动电源测试。
4. 继续观察 `ui_hold` 的长期稳定性；如果原厂 UI 线程被重启或看门狗策略变化，需要重新评估常驻方案。
5. 产品决策：BLE 是否硬性要求；若必须，需要重新确认硬件或接受外接方案。

## 本次会话证据与指标

- 主机自测：`petcard_sim` 16 项全部 PASS（feed/play/pet/睡眠恢复/阶段/存档往返/短按/长按/双击/JSON）。
- 实机长稳：UI 线程挂起约 11.4 分钟、3510 帧、平均 21.7 ms、无重启；面板保持 `normal`。
- 实机功能：用户确认屏幕宠物、三种按键、手机 8420 页面/按钮均正常。
- 启停：`petcard.sh` 启动 `HOLDING 8 threads`；`killall petcard` 后无残留、UI 线程释放、快照/原厂 UI 恢复。
- 返工：显示首轮因直接写用户缓冲区不推屏，改为 fb mmap；看门狗因整进程 SIGSTOP 重启两次，改为按线程 ptrace 挂起。独立复核提出 5 个 P1（存档回退路径、清理信号、挂起进程失联、按键事件抢占/丢事件、HTTP 阻塞），均已修复并复测。
- 会话耗时约 60–70 分钟；精确 Token/检索次数未统计。

## 新对话启动包（可直接粘贴）

```text
项目：D:\Pet_wifi（adol D623+ 随身 WiFi 改 PetCard V1）。
先读：全局/项目 AGENTS.md、HANDOFF.md、.planning/STATE.md、petcard/README.md、
docs/function-matrix.md、docs/known-issues.md、docs/adr-0001-local-link-and-ui-hold.md。
当前基线：main 15a4e29（PetCore 阶段1/2 + 本地 HTTP），设备已停回原厂 UI，AP 正常；源码在 petcard/。

硬约束：
- 不要对 zte_mifi 整进程 SIGSTOP（zte_watchdog 约 10 分钟会重启设备）；用 ui_hold 只挂 UI 线程。
- 不要杀/重配 hostapd；本地链路 192.168.0.1:8420，不启用 NAT/转发，不使用付费上网。
- 显示必须 mmap(/dev/fb0) 后写；B-R-G；面板先 sleepout/blgon。
- 私有固件/镜像/转储在 work/，不得提交/上传；不写凭据。
- 每次完成按 vibe-coding-project-optimization 的完成清单；实机证据优先。

待决定/下一步：
1) 阶段3：手机 Companion 本地页面已实现待实机验收；先完成 /state + /action 与新动画帧验证，再决定 Android App 和长连接/WebSocket。
2) 阶段5：是否先做 /mnt/userdata 安装 + 开机自启（需处理 zte_mifi/看门狗）。
3) 阶段4：云端 AI provider/人格/记忆（API key 放手机或代理，不进固件）。
4) BLE 是否硬性要求；原板未发现 BLE 硬件。

设备操作：
- 启动：adb shell '/tmp/petcard.sh'
- 停止：adb shell 'killall petcard'
- 异常恢复：adb shell '/tmp/petcard_resume.sh'
- 运行前确认 zte_mifi 为 S、hostapd 在跑；结束后确认原厂 UI 恢复。
- 重启后 /tmp 清空：用 petcard/build.sh 重编，再 push petcard/ui_hold/脚本。
```

## 历史基线

单键贪吃蛇、显示/按键/颜色校准的详细记录保留在提交 `e082af1` 的 `HANDOFF.md` 历史版本；本文件已更新为 PetCard 当前状态。硬件只读检测的完整数据见 `docs/stage0-capability-report.md` 与 `work/probe/`（work 已 gitignore）。
