# STATE

## 当前状态（2026-11-12 阶段5启动，阶段4收尾完成，Android 0.3.0）

- 收尾项已完成：release 签名（`work/android-signing`，gitignore）、release-only FLAG_SECURE、edge-to-edge insets、预算拒绝真机注入、Key 保存按钮非破坏校验、provider 错误码映射单测。Key 删除按用户选择未实测；真实 401/402/429/5xx 未触发。
- 构建与测试：debug APK 32,078 B / MD5 `fdaa8ed72d59a2e5c6c4f05e774f1869`；release APK 27,828 B / MD5 `e715fc3d9fd46c5e6df74b9199c4e0db`（生产签名）；主机单测 25 项全过（SuggestionParser 13、DeepSeekProvider 8、DailyBudget 4）。
- 实机证据：0.3.0 debug 安装后标题不再被状态栏遮挡；Key 短输入保存返回“保存失败：API key length is invalid”，`secure_provider_settings.xml` MD5 未变；注入今日已用 ¥5.00 后联网聊天返回“今日 App 预算上限 ¥5.00 已用完”，预算仍显示 ¥5.0000，未发真实请求；release 包（debug 签名重签覆盖安装）截屏全黑，uiautomator 仍可读 UI，随后已装回 debug。
- 手机现场已恢复：预算 ¥0.0017、Key 未动、记忆为空、代理/DNS 设置已清空。release 包证书与 debug 不同，切 release 需卸载迁移（会丢数据），待用户决策。
- 未验证/待办：Key 删除按钮、真实 provider 401/402/429/5xx；阶段5供电/外壳/长稳未开始。

## 上一状态（2026-11-12 阶段4实机验收通过，阶段5待开始）

- 阶段4收尾版已装到 Android 16 手机（v0.2.0）：`/state` 经 PetCard 热点读取、确认后 `/action` 执行、真实 DeepSeek 请求经蜂窝成功、AI 建议未改设备数值、记忆真实候选保存后重启仍在并可删除/清空、两个隐私勾选重启后恢复默认关闭。
- 主机单元测试 23 项全过：`SuggestionParser` 13、`DailyBudget` 4、`DeepSeekProvider` 消息构建 6。最终 clean APK 31,138 B，MD5 `4f0b29b9a4e11318f7d2e5f769764b75`，versionCode 2 / versionName 0.2.0。
- 提交前独立只读复核发现 4 个 P2，已修复：Wi‑Fi 选择不再回退到任意网段（只认持 `192.168.0.x` 且非 VPN 的 Wi‑Fi）；宠物状态注入前去换行/截断并标注只读；`usage` 缺失或无效时保留预算预留；预算日界改为本地日期。另收紧 JSON 字符串类型、输出上限 800、补 3 项单测。真实请求在硬化前构建 `35e3833…` 上完成；最终构建 `4f0b29b…` 已重装并复验 `/state` 与预算，未再次发起计费请求。
- 实机证据（2026-11-12）：手机连 `adol-2D5025`（192.168.0.100）时读取设备状态 `KID · EXP 58 · 互动 34`；假模型 `feed` 确认后变为 `EXP 60 · 互动 35`；真实请求一次返回英文回复，明确提到“hungry/sleepy”，与设备 `hunger=26/100、energy=12~13/100` 对应，预算结算 `reserved_cny=0.00165`（显示 ¥0.0017）；随后刷新状态仍为 `EXP 60 · 互动 35`，证明模型只给建议未写状态。
- 真实请求返回 `mood=tired`、`action=feed`、两条记忆候选；保存其中一条后强制停止/重启 App 仍显示，随后已清空测试记忆，当前 `local_pet_memory.xml` 为空。
- 系统行为：PetCard 热点无互联网，手机曾自动切回「311」；重新连接后在“无法访问互联网”对话框勾选“对于此网络不再询问”并选“是”，随后保持连接。该行为属手机系统网络选择，不是 App 缺陷；已在 known-issues 记录。
- 未验证/待办：Keystore Key 的保存/删除按钮本轮未重新操作（保留用户原有 Key，仅确认 `iv` + `ciphertext` 字段存在）；预算拒绝路径未做真机注入测试（避免触发额外计费）；DeepSeek 401/402/429/5xx 实际错误路径未触发（仅单测/代码映射）。
- 构建环境：本机全局 `~/.gradle/init.d/aliyun-mirror.gradle` 与项目 `RepositoriesMode.FAIL_ON_PROJECT_REPOS` 冲突；用 `work/gradle-local-mirror.init.gradle`（settings 级镜像，gitignore）并临时移开全局脚本完成构建，随后已恢复全局脚本。APK 增量打包曾出现 53 KB 体积膨胀，clean build 后恢复正常。
- 设备侧已恢复：D623+ 接充电头供电，`hostapd`/`udhcpd`/`dnsmasq` 正常，`br0=192.168.0.1/24`，`ui_hold` 挂起 2 个 MMI 线程，PetCard 刷新约 21.2 ms。阶段4改动已提交 `0002bb9` 和 `609527e`；本交接提交后推送 `origin/main`。下一步按 HANDOFF 顶部“阶段5启动包”执行。

## 当前状态（2026-11-12，新版本已部署并完成普通重启验证）

- 新版本已从工作区安装到设备 `/mnt/userdata/petcard`：`bin/petcard`、`bin/ui_hold`、`autostart.sh`；旧文件保留为 `.old`，可回滚。未改 `/etc/rc`，未写 MTD/NV/bootloader。
- 首次部署时 MD5 一致（本地构建产物 = 设备激活文件）：petcard `115c31b0dba699efdf34ded3b4eea7e9`、ui_hold `cfdf8a8edf9b1c93b53d64b6852d04a2`、autostart.sh `fa50f4df3d64e1a46908583c3b78c64f`；`bin/petcard.old` 为首次部署版 `6251f17…`。
- 主机回归全通过：模拟器 39 项、关机 20 项、管理器替身 2 项、ui_hold ptrace 3 项（共 64 项）。工作区文档与源码仍未提交。
- 独立复核后补了两个 P1：关机等待期间每 2 秒 `lcd_wake` 保持关机画面；`/sbin/poweroff` 返回 0 但平台未断电时，30 秒超时记录 `timeout`、清理标记并恢复宠物界面。新增超时/边界断言；`test_shutdown.sh` 要求输出 `RESULT PASS`，防止 `execl` 回归导致测试进程被替换而虚假通过。补丁后只替换 `bin/petcard`，管理器未重启。
- 普通重启验证：`adb reboot` 被设备忽略（返回 0 但不重启），改用 `adb shell 'sync; /sbin/reboot'`。新 boot_id `7284efd0-b87e-4a76-a2a0-f205a57662c4`；管理器 uptime 3.70 s 启动并记录 boot_id，PetCard 首帧 ready，`ui_hold` 报告 `HOLDING 2 threads`（`mmi`、`mmihandlemsg`；因在 MMI 创建 LCD/按键线程前停住 `mmi`）。
- 开机后 HTTP `/state`、主页、`/assets/happy_1.png` 均正常；fb 回读为 PetCard 页面。root JFFS2 只读，hostapd/DHCP/DNS 正常，`poweroff.log` 尚无记录。
- 补丁版再次普通重启后，ADB 在 uptime 约 65 秒时掉线且未重新枚举；同一时间 RNDIS `192.168.0.1` 可 ping，PetCard `/state`、主页、PNG 均 200，证明系统与 PetCard 已启动。恢复 ADB 需拔插 USB-C。
- 用户已确认：启动仍能看到原厂 Power On；关机后设备会回到 PetCard 开机页。
- 启动画面 fb 时间线（uptime 3.20–14.69 s）：3.20–4.05 s fb 全黑，4.56 s 起为 PetCard 宠物页；3.2 s 之后没有任何原厂帧。`/etc_ro/mmi` 26 个 bind 挂载正常，`image_poweron_0.png` 与 PetCard 资源 MD5 相同。结论：原厂 Power On 在 Linux 用户态之前显示或由面板保留到 PetCard 首次写屏；应用层只能提前写 PetCard 帧来缩短，不能从根上移除。
- 关机证据：`poweroff.log` 记录 `POWEROFF requested pid=671 uptime=948.10 status=0`，约 7 秒后 boot_id 从 `64e554ea…` 变为 `a547867d…`，无 `command-exited`；说明是整机重启或断电后被 USB 供电立即唤醒，不是保持关机。
- 诊断用 fb 时间线抓取已移除；证据在 gitignore 的 `work/boot-timeline/`。
- 用户已选择软关机 + 提前写 PetCard 启动帧，并已实现部署：默认关机显示 GOOD NIGHT 后 `sleepin`/`blgoff` 熄屏，不调用 `/sbin/poweroff`、不建关机标记，PetCard/管理器保持运行，按电源键唤醒；`--hard-poweroff` 保留真实断电路径。debugfs 实测 `sleepin`/`blgoff` 后 `sleep mode=sleep、backlight=off`，`sleepout`/`blgon` 恢复 `normal/on`。`prepare_boot.sh` 在挂载素材后用 `petcard --restore boot-frame.raw` 写 128×128 RGB565 启动图，设备日志记录 `PetCard early frame 3.73`；`prepare_boot.sh` 当前 MD5 `5ee065f…`。
- 当前激活 MD5：petcard `11e0f661…`、ui_hold `cfdf8a8…`、autostart.sh `fa50f4d…`、prepare_boot.sh `5ee065f…`、boot-frame.raw `829aa02…`；`bin/petcard.old` 为 `1ed912b8…`。主机回归：模拟器 39 项、关机 20 项、管理器 2 项、ui_hold 3 项，共 64 项通过。
- 待用户实机确认：关机页后是否熄屏且不重启、电源键是否唤醒；启动时 PetCard 启动帧是否出现、原厂 Power On 是否缩短。
- 手动激活（不重启）时曾因 ADB shell 退出挂断导致 PetCard 被重拉一次；开机路径未复现。

## 历史排查（2026-11-12，修复完成待部署时的记录）

- 用户再次报告关机后 PetCard → 原厂 Power On → PetCard。画面顺序本身不能证明发生了两次系统启动。
- 已恢复 ADB：Windows 能识别 WINUSB 调试接口，但 ADB 列表为空；重启电脑端 ADB 服务后恢复。当前采样 boot_id=`c4c78590-20bb-4f50-8d96-61d00e912a73`，`boot_reason=0`，USB online=1、BAT_EXIST=0，根 JFFS2 只读；这些信息仍不能确认自动复位原因。
- 已复现软件缺陷：PetCard 用 exec 替换自身请求关机，自启管理器没有关机抑制；另有 ui_hold 首次挂住部分 UI 后每 5 秒才发现新线程的空窗。两者不能直接等同于实机全部症状的根因。
- 本地修复已完成：保留关机父进程和画面、临时关机标记阻止重拉起、明确失败时恢复、持久关机日志；UI 初期扫描改为 100ms。主机关机测试 14 项、管理器替身测试 2 项、原模拟器 36 项通过；真实主机 ptrace 测试新版约 104ms 接管迟建 UI，非 UI 线程继续运行；ARM 构建成功，独立代码及安全复核通过。未部署本轮修复，未触发设备关机/重启，未写 MTD/NV/bootloader。


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

## 下一步（阶段5）

1. 供电与场景：USB-C 充电头/移动电源低负载断电与冷启动、上电自启、至少 3 次断电重启；记录可复现供电方案。
2. 外壳：结构验证件 → 屏幕/单键/USB-C/散热/走线 → 定稿；装壳后复测按键与屏幕。
3. 长稳：小时级 `ui_hold` + HTTP + 看门狗；多次重启；存储空间与磨损。
4. 阶段4剩余（可选）：Key 删除实测（需用户同意并重输 Key）、真实 provider 401/402/429/5xx；release 签名、`FLAG_SECURE`、edge-to-edge、预算拒绝注入已完成。
5. 产品决策：BLE 是否硬性、记忆是否云同步、软关机是否允许手机远程唤醒；release 迁移是否卸载重装。
6. 用户实机确认：软关机熄屏/按键唤醒；启动早期帧与原厂 Power On 可见时长。

## 入口文档

- `HANDOFF.md`：当前状态、交接快照、新对话启动包、关键坑、构建/部署/运行。
- `petcard/README.md`：PetCore 使用与 API。
- `docs/stage0-capability-report.md`：能力报告。
- `docs/known-issues.md`、`docs/function-matrix.md`、`docs/adr-0001-local-link-and-ui-hold.md`：已知问题、功能矩阵、关键决策。
- 硬件/备份交接在 Codex 私有会话目录，不在本仓库。

## 交接

- Git：`main` 代码基线 `fa965b5`；远端 `origin https://github.com/hayden726020/wifi_Pet.git`。
- 下一对话：读 `HANDOFF.md` 顶部“交接快照”和文末“新对话启动包”；设备当前为原厂 UI、AP 正常、PetCore 已停止。
