# D623+ / PetCard V1 交接

## 当前补充（2026-11-12，新版本已部署并通过普通重启验证）

- 已把工作区新版本安装到 `/mnt/userdata/petcard`：`bin/petcard`、`bin/ui_hold`、`autostart.sh`。旧文件保留为同目录 `.old`；未改 `/etc/rc`，未写 MTD/NV/bootloader。部署前核对设备 `.new` 与本地构建产物 MD5 完全一致，激活后再核对一次。首次部署时 MD5：petcard `115c31b0dba699efdf34ded3b4eea7e9`、ui_hold `cfdf8a8edf9b1c93b53d64b6852d04a2`、autostart.sh `fa50f4df3d64e1a46908583c3b78c64f`；`bin/petcard.old` 为首次部署版 `6251f17…`。
- 主机回归全通过：原模拟器 39 项、关机 20 项、管理器替身 2 项、ui_hold 真实 ptrace 3 项（迟建 UI 约 102–105 ms 接管）。
- 独立只读复核发现两个 P1，已修复：关机等待期间未保活面板会让关机画面黑屏；`/sbin/poweroff` 返回 0 但平台未实际断电时会永久卡在关机页。现关机等待每 2 秒 `lcd_wake`；超过 30 秒记录 `timeout`、清理标记并恢复宠物界面；新增超时/边界断言，`test_shutdown.sh` 同时要求输出 `RESULT PASS`，防止实现退回 `execl` 时测试进程被替换而虚假通过。重新构建后只替换了 `bin/petcard`，管理器未重启，PetCard 由管理器重拉，HTTP 正常。
- 首次不重启激活（杀旧管理器/PetCard/ui_hold 后手动拉起新管理器）出现一次 PetCard 重启：ADB shell 退出时的挂断波及会话内子进程；新管理器本身存活并自动重拉。该现象只影响手动激活，不代表开机路径。
- 随后执行普通重启验证。`adb reboot` 返回 0 但设备不重启，必须 `adb shell 'sync; /sbin/reboot'`。重启后 boot_id=`7284efd0-b87e-4a76-a2a0-f205a57662c4`；管理器在 uptime 3.70 s 启动并记录 boot_id，PetCard 首帧 ready、`ui_hold` 随后报告 `HOLDING`；root JFFS2 保持只读，hostapd/DHCP/DNS 正常。
- 开机时 `ui_hold` 只挂起 2 个线程（`mmi`、`mmihandlemsg`）：它在 MMI 创建 `mmi_lcd_main`/`mmihandlekey` 之前就停住了 `mmi`，因此这些线程尚未出现；手动重启旧进程时则为 8–9 个。此差异对原厂 Power On 是否可见的影响仍待用户实机确认。
- 补丁版再次普通重启后，ADB 在 uptime 约 65 秒时掉线且未重新枚举；同一时间 RNDIS `192.168.0.1` 可 ping，PetCard `/state`、主页和 PNG 均 200，说明系统与 PetCard 已启动。恢复 ADB 需拔插 USB-C（已知链路问题），之后再取进程/日志快照。
- 开机后 HTTP `/state` 返回 JSON、主页与 `/assets/happy_1.png` 均 200；fb 回读为 PetCard 页面（仅软件侧证据）。`poweroff.log` 尚无记录，关机流程仍未在实机触发。
- **2026-11-12 启动画面诊断（fb 时间线）**：`prepare_boot.sh` 在 uptime 3.20 s 抓到的 fb 全黑；4.56 s 已是 PetCard 宠物页，3.2 s 之后的时间线没有出现任何原厂帧。`/etc_ro/mmi` 的 26 个启动素材 bind 挂载正常，`image_poweron_0.png` 与 PetCard 资源 MD5 相同。结论：用户看到的原厂 Power On 在 Linux 用户态之前显示（bootloader/内核）或由面板保留到 PetCard 首次写屏；应用层无法从根上移除，只能在更早时刻写入 PetCard 画面来缩短可见时间；bootloader/内核 logo 修改超出当前“不写 MTD/NV/bootloader”的安全边界。
- **2026-11-12 关机诊断**：用户实机关机后，`poweroff.log` 记录 `POWEROFF requested pid=671 uptime=948.10 status=0`，约 7 秒后 boot_id 从 `64e554ea…` 变为 `a547867d…`，没有 `command-exited` 记录；说明 `/sbin/poweroff` 触发的是整机重启（或断电后立即被 USB 供电唤醒），不是保持关机。设备无电池、必须 USB-C 供电，真实关机能否保持需硬件/供电层面确认；应用层可改为“屏幕熄灭的软关机”避免重启，或尝试 `halt`。
- 诊断用的 fb 时间线抓取已从 `prepare_boot.sh` 移除并重新部署干净版（MD5 `5a3344f…`）；证据留在 gitignore 的 `work/boot-timeline/`。
- 用户已选择：关机采用软关机（熄屏、不重启、按键唤醒），启动采用提前写 PetCard 帧缩短原厂 Power On；两者已实现并部署，实机操作验证待完成。
- 历史采样：上一轮 boot_id=`c4c78590-20bb-4f50-8d96-61d00e912a73`，`boot_reason=0`，外接 USB 供电且 BAT_EXIST=0；不能仅由供电信息推断自动复位原因。
- 已复现并修复的缺陷：PetCard 用 exec 替换自身请求关机导致进程被替换、管理器无关机抑制；ui_hold 首次挂住部分 UI 后每 5 秒才发现新线程的空窗。修复保留关机父进程和画面、临时关机标记阻止重拉起、明确失败时恢复、持久关机日志；UI 初期扫描改为 100ms。


- 2026-11-12 按用户选择改为**默认软关机**：确认关机后显示 GOOD NIGHT、保存状态、`sleepin`/`blgoff` 熄屏，不调用 `/sbin/poweroff`、不创建关机标记，PetCard 与管理器保持运行；按电源键唤醒并恢复宠物界面。`--hard-poweroff` 保留真实断电路径和原关机回归。debugfs 实测 `sleepin`/`blgoff` 后 `sleep mode=sleep、backlight=off`，`sleepout`/`blgon` 恢复 `normal/on`。新增软关机帧、500 ms 唤醒防抖和默认关机决策测试；当前模拟器 39 项、关机 20 项、管理器 2 项、ui_hold 3 项，共 64 项通过。
- 2026-11-12 新增**启动早期 PetCard 帧**：`prepare_boot.sh` 在挂载素材后用 `petcard --restore "$BASE/boot-frame.raw"` 尽早写 128×128 RGB565 高字节优先启动图。`boot-frame.raw` 由 `assets/boot/image_poweron_0.png` 生成；设备实测 `--restore` 显示正确，启动日志记录 `PetCard early frame 3.73`。该帧只能缩短、不能消除 Linux 之前的原厂 Power On。
- 当前激活文件 MD5：petcard `11e0f66166318a017c59178f95ae8330`、ui_hold `cfdf8a8…`、autostart.sh `fa50f4d…`、prepare_boot.sh `5ee065f…`、boot-frame.raw `829aa02…`；`bin/petcard.old` 为上一版 `1ed912b8…`。
- 待用户实机确认：按住 5 秒→松开→短按后是否正常熄屏且不重启；按电源键是否唤醒；启动时 PetCard 启动帧是否出现、原厂 Power On 是否缩短。

## 最新状态：2026-11-12 阶段 3 开关机画面排查与交接

- 用户确认目标：启动进入 PetCard、隐藏原厂 UI，同时保留联网/看门狗/USB ADB 救援；启动与关机页采用 PetCard 画面。关机操作确认：按住电源键 5 秒显示「是否关机？」；松开后再短按执行；确认页再次长按或 15 秒超时取消。
- 由于 `zte_mifi` 是 MMI、联网与看门狗共用的单体进程，不能整体替换或冻结。当前通过 `ui_hold` 挂起 MMI/按键 UI 线程，并保留原厂主进程和网络服务。固件内原厂 UI 组件仍存在，当前方案隐藏它们，不是从固件中物理删除。
- 用户最初反馈关机/开机时原厂 UI 叠加乱码；之后确认乱码已消失。用户最新确认的顺序是：PetCard 关机页 → PetCard 开机页 → 原厂 Power On → PetCard 开机页，提示关机后可能发生两次启动/复位；先前认为只是单段早期 Power On 的判断不充分。
- 第一版把 UI 接管钩子放在 `zte_mifi` 启动行之后，约 10 秒启动 PetCard，但仍留下原厂 Power On 画面。
- `zte_mifi` 同时承载 MMI、联网和看门狗，因此只挂起 UI 线程并保留主进程。原始 `rc`、`inittab` 备份及 `rollback_startup.sh` 位于 `/mnt/userdata/petcard/rollback/`。未写 MTD/NV/bootloader。
- 用户已物理触发关机路径，设备随后自动开机；根因未确认。USB-C 外接供电时 sysfs 报 charger online=1、status=Discharging，但该值无法判定是供电唤醒还是平台 poweroff 行为。本轮普通重启只验证提前接管和服务存活，没有再次触发关机。
- 用户确认设备必须连接 USB-C 供电，且关机后自动启动时线仍连接；外接供电触发启动是可能原因，但尚未证明。本轮没有重复关机。
- autostart 直接执行 userdata 中的 `ui_hold`/PetCard 二进制，页面和素材通过软链接读取。因前一版后台 manager 仍晚于 `zte_mifi`，最新版把启动钩子移到 `fs_check` 后，并等待 PetCard 首个完整 framebuffer 帧写成功后再继续启动；等待最多 2 秒，超时保留原回退路径。
- 只读核对确认 `/etc/rc` 在 `zte_mifi` 之前执行启动画面资源挂载；设备日志显示挂载成功，`image_poweron_0.png` 与 PetCard 资源 MD5 相同，MMI 有 `mmi_showPowerOnFrame`、`mmi_startFastPowerOnFrame` 路径。`resource` 分区本地镜像 262144 B 全为 `0xFF`。此前记录的两轮启动中第二轮与 USB 重新插拔时序重叠，无法归因到原关机。设备无 dmesg/logread/last_kmsg/pstore 历史记录，`boot_reason=0` 无法说明复位原因。
- 最新普通重启日志确认 PetCard PID 668 先于 `zte_mifi` PID 725，首帧握手生效；`ui_hold` 于 08:49:26 挂起 MMI；约 111 秒时服务正常。但用户随后确认原厂 Power On 仍会显示约 5–7 秒，因此首帧握手方案没有解决可见现象。现有证据不能区分是 MMI 启动绘制到线程挂起之间的间隙，还是更早阶段的显示。当前工作区 ADB daemon 启动卡住，无法关联新一轮实时日志。本轮无关机测试或 MTD/NV/bootloader 写入。
- **当前本地代码基线**：`e42d5dd Checkpoint PetCard startup and shutdown UI`，已提交到本地 `main`，未推送。它包含 5 秒关机确认、PetCard 启/关机 MMI 素材、`/etc/rc` 早期启动与首帧握手、回滚脚本及相关文档。私有固件转储位于 gitignore 的 `work/`，没有纳入提交。
- `origin/main` 仍为阶段 4 提交 `0959d72 Add Phase 4 Android AI vertical slice`；本地阶段 3 提交尚未推送。
- 下一轮先恢复电脑与设备的 ADB 数据连接，确认 `adb devices -l` 可返回设备；再采集持久日志 `/mnt/userdata/petcard/autostart.log`、`boot-assets.log`、`install.log`，以及 `/proc/uptime`、`/proc/cmdline`、`/proc/mounts`、`ps`、`/tmp/ui_hold.log` 和 MMI 线程状态。当前工作区 ADB daemon 启动卡住，本轮没有读到新设备快照。
- 后续验证重点是把用户看到的 5–7 秒 OEM Power On 与 PetCard 首帧、`zte_mifi` 启动、MMI UI 线程挂起三个时间点对应起来。现有日志只证明软件写过首帧及 MMI 后续被挂起，不证明面板当时实际显示的内容。MMI 绘制间隙和更早启动阶段画面都只是待验证假设。
- 不要对 `zte_mifi` 整进程 SIGSTOP，不要杀 `hostapd`，不要写 MTD/NV/bootloader。下一轮无需再触发关机；先用只读检查和普通启动日志确认问题来源。

## 最新状态：2026-11-11 阶段 4 Android 首个竖切

- **阶段 4 基线已提交并推送**：`0959d72 Add Phase 4 Android AI vertical slice`。后续阶段 3 本地基线为 `e42d5dd Checkpoint PetCard startup and shutdown UI`，尚未推送到 `origin/main`。
- 用户配置：DeepSeek `https://api.deepseek.com` / `deepseek-flash`；Key 在 Android Keystore 保护的本机存储；聊天内容允许发给 provider，但 App 内默认需勾选确认；每天最多 ¥5 的 App 内预算 gate；长期记忆默认只存在手机。
- 新增 `android/` 原生 App 骨架和首个竖切：离线假模型、provider 接口、DeepSeek Chat Completions adapter、严格建议 JSON parser、AES/GCM + Android Keystore KeyStore、请求前预算预留/返回 usage 后结算、本机记忆人工批准/清空、宠物 `/state` 和确认后 `/action` 走设备 Wi‑Fi Network、云 API 走已验证蜂窝 Network。
- 建议 schema 在 `docs/stage4-ai-suggestion.schema.json`。模型只返回回复、有限情绪/动作枚举和记忆候选，不产生宠物数值；动作执行前二次确认，记忆只在用户勾选后本机保存。
- **验证**：Android Java 源码 `javac -encoding UTF-8` 对 Android 36 `android.jar` 编译通过；Gradle 8.13 `:app:assembleDebug` 成功，生成 [debug APK](/D:/Pet_wifi/android/app/build/outputs/apk/debug/app-debug.apk)（26,694 B）。构建提示 SDK XML 文件版本 4 而当前工具支持到 3，以及 Gradle 9.0 弃用警告。已通过 ADB 安装并启动到 Android 16 手机；离线假模型实际返回默认回复、`happy`、`none`，预算为 ¥0.0000。手机未连 PetCard 热点，设备状态显示 timeout；Wi‑Fi/蜂窝分流、Key 存储和真实 API 尚未验收。
- **没有真实 API 请求**，没有输入/写入 Key，没有产生费用。预算是本 App 的本地估算 gate，不能覆盖同一 DeepSeek Key 在其他 App/设备上的用量，也不是 provider 账单硬上限。
- 下一步：手机连接 PetCard 热点后验收设备 Wi‑Fi 状态/动作；再验收蜂窝流量分流、Key 保存/删除、本地记忆和预算 gate。真实 API 调用应在用户把 Key 输入 App 并主动点击联网聊天后进行。
- 详细路线图：`.planning/ROADMAP.md`；阶段状态：`.planning/STATE.md`；schema：`docs/stage4-ai-suggestion.schema.json`。

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

PetCore 本地电子宠物已完成阶段 1/2 实机验收：128×128 宠物显示、单键短按/长按/双击、离线养成、存档、退出恢复原厂 UI。清理后的 64×64 透明帧按五类情绪四帧播放并随成长放大；面板现用用户实机确认的 RGB565 高字节优先格式，经 fb mmap 写入。阶段 3 Companion 页面及手机 Wi‑Fi 视觉/喂食/玩耍/摸摸/睡觉触控验收已通过，USB ADB 也验证 `/state`、主页和 PNG 路由。PetCard 当前运行；启动用 ADB shell 必须保持打开，否则设备会向包装脚本发送 Hangup 并恢复原厂 UI。阶段 4 Android 首个竖切已在工作区：离线假模型、DeepSeek adapter、Keystore key、¥5/日 App 预算 gate、Wi‑Fi/蜂窝分流、动作确认和本机记忆审批/清空；Java 源编译及 Gradle APK 构建通过，已装到 Android 16 手机并运行离线假模型初验。手机未连 PetCard AP，网络分流待验。用户允许聊天发送，App 内仍要求勾选；本轮没有 API 请求。未使用付费上网或运营商流量；未发现 BLE 硬件。阶段 5 常驻安装与外壳仍待做。

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

11. `adb reboot` 在这台设备上返回 0 但不会重启；普通重启用 `adb shell 'sync; /sbin/reboot'`。开机早期 `ui_hold` 可能只挂起 `mmi`/`mmihandlemsg` 两个线程，因为 MMI 的 LCD/按键线程尚未创建；不要据此判定接管失败。

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

`petcard.sh` 会先快照原厂 fb，启动 `ui_hold` 并确认 `HOLDING`，再启动 PetCore；退出时杀 PetCore/`ui_hold`、写回快照、唤醒面板。`ui_hold` 启动前 10 秒每 100 ms 重扫线程、之后每秒重扫，原厂 UI 线程若被重新拉起会再次挂起。

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

1. 用户实机验证默认软关机：按住电源键 5 秒 → 松开 → 短按，确认 GOOD NIGHT 后熄屏且不重启；再按电源键应唤醒。若失败，先读 `/tmp/petcard.log`、`/tmp/ui_hold.log`、`/sys/kernel/debug/lcd_zx29/lcd_lead` 和 `/proc/uptime`，再调 `g_soft_ignore_until` 或 `sleepin`/`blgoff` 顺序。
2. 用户确认启动时 PetCard 早期帧是否出现、原厂 Power On 是否缩短。fb 时间线已证明原厂画面在 Linux 用户态之前或由面板保留，应用层不能根除；bootloader/内核 logo 修改超出当前安全边界。
3. 软关机通过后，继续阶段 4 Android App 的 Wi‑Fi/蜂窝分流、Key 存储和预算拒绝路径实机验收。真实 API 调用仍需用户主动输入 Key 并点击。
4. 阶段 5 外壳、移动电源和供电测试尚未开始；BLE 硬件此前未发现。

## 本次会话证据与指标（2026-11-12）

- 主机回归 64 项全过：模拟器 39 项、关机 20 项、管理器替身 2 项、ui_hold 真实 ptrace 3 项（迟建 UI 约 102–105 ms 接管）。
- ARM 构建通过；设备部署后普通重启验证：boot_id `7284efd0…`，管理器 uptime 3.70 s 启动，PetCard 首帧 ready，`ui_hold` 报告 `HOLDING`；HTTP `/state`、主页、PNG 均 200。
- 设备实测 `sleepin`/`blgoff` 后 `sleep mode=sleep、backlight=off`，`sleepout`/`blgon` 恢复 `normal/on`。
- 启动早期帧：`prepare_boot.sh` 日志记录 `PetCard early frame 3.73`；设备 `petcard --restore boot-frame.raw` 显示正确。
- 真实 poweroff 实机证据：`poweroff.log` 记录 `requested uptime=948.10`，约 7 秒后 boot_id 变化，无 `command-exited`；USB 供电下真实断电会重启，故默认改为软关机。
- 未完成：软关机熄屏/按键唤醒的物理按钮验收；原厂 Power On 只能缩短不能移除。
- 本轮未写 MTD/NV/bootloader；诊断时间线证据在 gitignore 的 `work/boot-timeline/`。

## 新对话启动包（可直接粘贴）

```text
项目：`D:\Pet_wifi`（Adol D623+ / ZX297520V3，PetCard V1）。
Git：`main` 当前基线为本次提交 `Checkpoint PetCard soft shutdown and early boot frame`；SHA 以 `git log -1` 为准，`origin/main` 尚未包含阶段 3+ 改动；工作区提交后应干净。
先读：`HANDOFF.md` 顶部、`.planning/STATE.md` 顶部、`petcard/README.md` 的关机/开机接管章节、`docs/function-matrix.md`；然后 `git status`。

当前设备（2026-11-12）：PetCard 默认软关机；`/mnt/userdata/petcard` 激活 petcard `11e0f661…`、ui_hold `cfdf8a8…`、autostart.sh `fa50f4d…`、prepare_boot.sh `5ee065f…`、boot-frame.raw `829aa02…`；`bin/petcard.old` 为 `1ed912b8…`。管理器/ui_hold 由 `/etc/rc` 启动；ADB 掉线时拔插 USB-C。

当前未解决/待验收：
- 软关机物理按钮流程未验收（5 秒长按→松开→短按；GOOD NIGHT→熄屏不重启；电源键唤醒）。
- 原厂 Power On 仍可见；fb 时间线显示 3.2 s 后无原厂帧，来自 Linux 之前或面板保留；早期帧 3.73 s 只能缩短，不能移除；bootloader 修改超出边界。
- 真实 `/sbin/poweroff` 在 USB 供电下约 7 秒重启；默认已不用；`--hard-poweroff` 可复现。
- 阶段 4 Android 分流/Key/预算实机验收、阶段 5 外壳/供电未完成。

接手后：先 `adb devices -l`；若掉线拔插 USB-C。只读读取 `/mnt/userdata/petcard/autostart.log`、`boot-assets.log`、`poweroff.log`、`/tmp/petcard.log`、`/tmp/ui_hold.log`、`/proc/uptime`、`/proc/sys/kernel/random/boot_id`、`lcd_lead`、进程列表；让用户实机测试软关机/唤醒。不要主动触发真实关机；需要真实电源行为时先征得同意。

安全边界：不整进程 SIGSTOP `zte_mifi`，不杀 `hostapd`，不启用 NAT/转发或付费流量；显示通过 `/dev/fb0` mmap 写入；不写 MTD/NV/bootloader；`work/` 内固件和转储为私有资料，不提交或上传；真实 AI API 仅在用户主动输入 Key 并确认发送后调用。

回滚：设备启动钩子 `/mnt/userdata/petcard/rollback_startup.sh`；启动入口原件在 `/mnt/userdata/petcard/rollback/`；程序回滚可用 `bin/petcard.old`（先停 PetCard，再替换并让管理器重拉）。
```

## 历史基线

单键贪吃蛇、显示/按键/颜色校准的详细记录保留在提交 `e082af1` 的 `HANDOFF.md` 历史版本；本文件已更新为 PetCard 当前状态。硬件只读检测的完整数据见 `docs/stage0-capability-report.md` 与 `work/probe/`（work 已 gitignore）。
