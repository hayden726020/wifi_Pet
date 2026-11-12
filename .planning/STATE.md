# STATE

## 最新状态（2026-11-12 阶段 3 开关机画面问题排查）

- 用户需求：开机进入 PetCard；尽可能移除原厂界面；保留联网、看门狗和 USB/ADB 救援；启动/关机画面采用 PetCard 专属视觉。设备 monolithic `zte_mifi` 同时承载 MMI 与关键联网/看门狗线程，因此采用只挂起 MMI/按键 UI 线程并保留主进程的方式；原厂 UI 程序仍在固件中，不是从固件删除。
- 第一版 UI 接管钩子曾放在 `zte_mifi` 启动行之后；该方案约 10 秒启动 PetCard，但仍留下原厂 Power On 画面。
- 原始 `/etc/rc`、`/etc/inittab` 仍备份在 `/mnt/userdata/petcard/rollback/`，`rollback_startup.sh` 可回滚启动入口。未写 MTD、NV 或 bootloader。
- 关机交互：按住 KEY_POWER 5 秒显示 PetCard 确认页，松开后再短按才调用 `/sbin/poweroff`；确认页再次长按或 15 秒无操作取消。短按/650ms 长按宠物动作/双击保留。主机模拟器覆盖进入确认、释放不确认、单击确认、长按取消、超时取消；确认和关机画面已分别渲染检查。
- 用户最新描述关机/启动顺序：PetCard 关机页 → PetCard 开机页 → 原厂 Power On → PetCard 开机页。乱码已消失。该顺序表明关机后可能发生两次启动/复位，先前“只有一段早期原厂画面”的判断不充分。
- 用户补充：设备必须连接 USB-C 才能供电，关机后自动启动时线仍连接。外接电源触发启动是可能原因，但暂不能证明。
- 又调整管理器：UI hold 成功后将页面/素材目录软链接到 userdata；随后进一步改为直接执行 userdata 中的 PetCard/ui_hold 二进制，不复制二进制；管理器会在创建 ready 标记前验证 20 个 RGBA（各 16384 B）和对应 20 张 PNG 存在。
- 上一版普通重启中，`prepare_boot.sh` 在 `zte_mifi` 启动前完成启动图片挂载及网页素材检查/软链接；日志为 08:14:08。随后 `ui_hold` 与 PetCard 在 08:14:11 启动，约 12 秒 uptime 时 ADB、hostapd、DHCP/DNS 正常，根 JFFS2 只读。该优化把 userdata 网页素材校验移到了 MMI 启动前，但没能消除原厂 Power On。
- 用户重新插拔后 ADB 恢复。日志记录两轮启动：13:31:08 和时钟复位后的 08:00:08，两轮各启动一次 PetCard。第二轮发生在用户重新插拔前后，现有时间戳不足以判断它是原关机后的自动复位还是重新插拔造成；不能据此断言一次关机必然触发两轮启动。当前 `boot_reason=0`，设备无 `dmesg`/`logread`/`last_kmsg`/pstore 历史记录，无法确认复位原因。充电器 online=1、status=Discharging，不能单独证明原因。本轮未触发关机；普通重启用于验证启动时序；未写 MTD/NV/bootloader。
- 只读固件审查：`resource` 分区镜像 262144 B 全为 `0xFF`，不是有效的图像载荷；`zte_mifi` 中存在 `mmi_showPowerOnFrame`、`mmi_startFastPowerOnFrame` 及 `/etc_ro/mmi/image_poweron_*.png` 路径。文件级 PetCard 覆盖有效，但用户仍需确认最新普通重启时看到的 OEM Power On 是 MMI 画面还是更早阶段的显示。
- 本轮未再次触发关机。开关机过渡乱码已由用户报告消失；关机后出现两段启动画面的根因尚未定位，不能仅凭 USB-C 持续连接断定硬件原因。
- 用户在上一版本上仍看到约 5–7 秒 Power On。普通重启日志发现后台管理器虽提前启动，仍输给 `zte_mifi`；现已改为 `/etc/rc` 等待 PetCard 首个完整 framebuffer 帧成功写入后再启动 `zte_mifi`，等待上限 2 秒并保留原回退。
- 最新普通重启日志：PetCard 首帧 ready 08:49:22，PID 668 先于 `zte_mifi` PID 725；`ui_hold` 于 08:49:26 挂起 MMI。uptime 约 111 秒时 PetCard、ADB、hostapd、DHCP/DNS 正常，根 JFFS2 只读；Companion `/state` 和主页 HTTP 200。用户随后确认仍会看到约 5–7 秒原厂 Power On，因此首帧先于 MMI 启动并未解决可见启动画面；实际显示时序来源尚未分清，可能是 MMI 启动绘制与接管间隙，也可能是更早阶段的显示，当前证据不能定论。本轮工作区 ADB daemon 启动卡住，未取到新现场日志。
- **状态**：阶段 3 UI 接管仍在排查；脚本已更新并部署，阶段 3 改动已提交为本地基线 `e42d5dd`，未推送。阶段 4 Android 任务仍按下文历史状态进行；阶段 5 外壳/供电任务未开始。

## 最新状态（2026-11-11 手机页面故障排查，覆盖下文历史状态）

用户确认手机连接设备热点 `adol-2D5025`。设备之前重启过（`/proc/uptime` 曾为 64.91 秒，`/tmp` 已清空），原因未知；之后按授权重新部署程序、脚本、页面和 44 个素材。设备没有 `nohup`/`setsid`，后台启动收到 Hangup 后会按设计清理；目前通过保持 ADB shell 前台连接成功运行。当前 `ui_hold` 为 `HOLDING 8 threads`，PetCore 监听 `0.0.0.0:8420`，`zte_mifi` 和 `hostapd` 正常；USB ADB 转发实测 `/state` 返回 JSON、主页和动画 PNG 均 HTTP 200，临时转发已移除。用户手机报告页面和所有操作正常；日志显示 feed/play/pet/sleep 请求成功，`interactions` 从 17 增至 21。设备 uptime 从约 325 秒增长到 906 秒，无再次重启。阶段 3 手机视觉/触控验收通过。启动用 ADB shell 必须保持打开，关闭后包装脚本会收到 Hangup 并恢复 UI。ADB 路径为 `C:\Users\SPcuteAG\AppData\Local\Android\Sdk\platform-tools\adb.exe`。

此前用户确认固定帧下 RGB565 高字节优先模式已消除实物屏色块，程序曾恢复动画运行。该 PID 快照已过时，不代表当前运行状态。详见 HANDOFF 中“此前状态”与当前排查记录。

- 更新：2026-11-11
- 阶段：阶段 1/2 已完成实机验收；阶段 3 Companion 手机视觉和四种触控操作通过验收。阶段 4 已开始：Android 原生工程及 4A/4B 首个竖切已写入工作区；包括 DeepSeek adapter、Keystore 加密 key、本地 ¥5/日预算 gate、离线假模型、动作二次确认、本机记忆批准/清空、设备 Wi‑Fi 与蜂窝请求分流。Gradle `:app:assembleDebug` 成功，APK 为 `android/app/build/outputs/apk/debug/app-debug.apk`（26,694 B）；Java 源码编译通过。APK 已安装到 Android 16 手机并启动；离线假模型实际返回默认回复、happy、none，预算显示 ¥0.0000。手机未连接 PetCard 热点，故 `/state` 显示 timeout；尚未实测本地 Wi‑Fi/蜂窝分流、Key/预算写入，未发起真实 API 请求。用户配置：DeepSeek `deepseek-flash`，聊天发送需 App 内勾选确认，key 存 Android Keystore，每日 app 内预算 ¥5，长期记忆默认仅本地；详见 `.planning/ROADMAP.md`。PetCard 当前仍需保持 ADB 前台会话；阶段 5 未开始。
- 当前交接基线：本次提交推送后成为 `main`/`origin/main` HEAD；具体 SHA 以 `git log -1` 为准。

## 已完成

- ADB root、唯一按键 KEY_POWER(116)、fb0 128×128 16bpp 已确认。
- 面板 sleep 控制（debugfs `lcd_lead`: `sleepout`/`blgon`）、B-R-G 颜色转换、整帧写入已实机验证。
- **关键修正**：驱动只把 fb mmap 内存推屏；必须 `mmap(/dev/fb0)` 后从 mmap 写。直接写普通缓冲区返回 32768 但屏幕不更新。
- PetCore：
  - 宠物渲染（BABY/KID/TEEN/ADULT，六种情绪 + 吃/玩/摸摸动画、HUD、气泡）；
  - 按键短按/长按/双击；
  - 饥饿/心情/精力/亲密/经验/年龄/阶段、离线结算；
  - `/mnt/userdata/petcard/state.bin` 原子存档 + `.bak`；
  - 本地 HTTP `192.168.0.1:8420`（页面 + `/state` + `/action`）。
- 手机直连：保留原厂 AP（`adol-2D5025`，密码为原厂默认值，不写入仓库）；本次手机通过 `192.168.0.1:8420` 打开 Companion 并完成按钮验收。未缴费、无运营商流量，未启用 NAT/转发。
- 看门狗处理：整进程 SIGSTOP `zte_mifi` 约 10 分钟会重启设备（两次复现）；改为 `ui_hold` 只挂 UI/按键线程，保留 `zte_watchdog`/AP 线程。
- 验证证据：
  - 用户实机确认宠物画面、三种按键、手机页面均正常；
  - `FLUSH ... avg_us=21600~22040`（整帧约 21.6–22.0 ms）；
  - 存档文件存在且重启后加载（年龄/经验延续）；
  - `resume.sh` 退出恢复：`zte_mifi` 为 `S`、面板 `normal`、原厂 UI 恢复（用户确认）；
  - 长稳：`ui_hold` 只挂 UI 线程约 11.4 分钟、3510 帧无重启（超过整进程 SIGSTOP 约 10 分钟的重启窗口）；包装脚本启动/停止/快照恢复实测通过；更长时间仍需观察。

## 历史设备状态（2026-11-10，已被顶部最新状态取代）

- 清理帧实机检查完成：运行时 `ui_hold` 只挂起 8 个 UI 线程，fb mmap 刷新约 22 ms；回读 32768 B framebuffer 可见清理后的新图。
- 当时 PetCore 已退出并恢复原厂 UI，随后按用户要求关机；之后重新连接并运行 PetCard（这是历史状态，不代表当前）。
- `/tmp` 重启后会清空；本次已重新部署。存档仍在 `/mnt/userdata/petcard/`。

## 风险 / 阻塞

- 没有 BLE 硬件（无 HCI/BT 固件）；V1 手机控制走 Wi‑Fi 本地 HTTP。
- `ui_hold` 依赖原厂线程名与 ptrace；长期稳定性、线程被重新拉起、看门狗策略变化需持续观察。
- 原厂 UI 的背光休眠逻辑偶尔会关背光，`resume.sh` 会补 `blgon`。
- 原厂 AP 的付费限制只影响上网；本地通信可用，但不要改/杀 hostapd。
- `/tmp` 非持久；阶段 5 需安装到 `/mnt/userdata`，评估 jffs2 执行、内存、开机自启。
- 私有固件/镜像/转储在 `work/`（gitignore），不得提交。

## 下一步

1. 阶段 4：先解决 Gradle APK 打包环境冲突，再安装 Android App 做离线假模型、设备 Wi‑Fi 状态/动作、蜂窝分流、Key 存储与预算拒绝路径实机验收；之后才考虑真实 API 请求。
2. 阶段 4 后续：完善记忆与会话体验，确认 DeepSeek 输出兼容性；严格保留用户确认动作和 app 内费用 gate。
3. 阶段 5：常驻安装、开机自启、UI 线程挂起自动化、外壳、USB‑C 供电与移动电源实测。
4. 产品决策：BLE 是否硬性要求；若必须，需要重新确认硬件或接受外接方案。

## 入口文档

- `HANDOFF.md`：当前状态、交接快照、新对话启动包、关键坑、构建/部署/运行。
- `petcard/README.md`：PetCore 使用与 API。
- `docs/stage0-capability-report.md`：能力报告。
- `docs/known-issues.md`、`docs/function-matrix.md`、`docs/adr-0001-local-link-and-ui-hold.md`：已知问题、功能矩阵、关键决策。
- 硬件/备份交接在 Codex 私有会话目录，不在本仓库。

## 交接

- Git：`main` 代码基线 `15a4e29`；远端 `origin https://github.com/hayden726020/wifi_Pet.git`。
- 下一对话：读 `HANDOFF.md` 顶部“交接快照”和文末“新对话启动包”；设备当前为原厂 UI、AP 正常、PetCore 已停止。
