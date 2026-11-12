# 功能完成矩阵（A–F）

更新：2026-11-12。A=已实现且有实机证据；B=已实现但未充分验证；C=部分实现；D=未开发；E=有缺陷待修；F=架构方向待评估。

| 功能 | 状态 | 证据 | 风险/下一步 |
|---|---|---|---|
| PetCard 开机接管 / 原厂 UI 隐藏 | B | 2026-11-12 新版本已落盘 `/mnt/userdata/petcard` 并通过普通重启验证：boot_id `7284efd0…`，管理器 uptime 3.70 s 启动，PetCard 首帧 ready，`ui_hold` 报告 `HOLDING 2 threads`；HTTP、hostapd、DHCP/DNS 正常，根分区只读。`prepare_boot.sh` 在 3.73 s 用 `petcard --restore` 写入 PetCard 启动帧 | 用户确认仍见原厂 Power On；fb 时间线显示 3.2 s 后无原厂帧，画面来自 Linux 用户态之前或面板保留到 PetCard 首次写屏；早期帧只能缩短，不能从根上移除；bootloader/内核 logo 修改超出当前边界 |
| PetCard 启动 / 关机视觉 | B | PetCard 启动与关机图已覆盖 MMI 动画路径；设备日志确认挂载成功，启动图 MD5 与 PetCard 资源相同；资源分区镜像为全擦除态 | 文件覆盖和首帧先行均未消除用户看到的 Power On；MMI 含 PowerOn 帧显示逻辑，是否另有更早阶段显示仍未确认 |
| 5 秒关机确认 | B | 默认软关机已部署：关机页后熄屏、不重启、按键唤醒；`--hard-poweroff` 保留真实断电与 30 秒超时恢复；debugfs 实测 `sleepin`/`blgoff` 生效、`sleepout`/`blgon` 恢复；主机关机回归 20 项、管理器替身 2 项、模拟器软关机/防抖/唤醒 3 项通过 | 实机软关机熄屏与按键唤醒尚待用户验证；真实 poweroff 在 USB 供电下会重启（已实测），默认不再使用 |
| 启动钩子回滚 | B | 原 `/etc/rc`、`inittab` 有 userdata 备份；`rollback_startup.sh` 已部署 | 尚未在本次部署中实际回滚（回滚会恢复原厂 UI 启动） |
| 128×128 宠物显示/动画 | A | 用户实机确认；清理后的透明素材已叠加到设备 fb mmap 画布，32768 B fb 回读可见新图且彩边已清理；整帧约 22 ms | 更高帧率需优化 |
| 单键短按/长按/双击 | A | 用户实机确认三种操作；长按约 1 秒不关机 | 长按阈值/误触可再调 |
| 离线养成（饥饿/心情/精力/亲密/经验/阶段） | A | 主机 16 项自测通过；实机状态与存档延续 | 数值平衡待产品调 |
| 本地存档/离线结算 | A | `/mnt/userdata/petcard/state.bin` + `.bak`；重启后 age/exp 延续 | 掉电最多丢约 2 分钟交互 |
| 手机本地 HTTP 控制页 | A | 用户确认页面/按钮；`/state`、`/action` 实测 | 无认证；长连接未做 |
| 手机 Companion 情绪动画页 | A | 2026-11-11 用户在手机连接设备 AP 后确认页面及喂食/玩耍/摸摸/睡觉按钮正常；设备日志记录操作成功，`interactions` 从 17 增至 21；2026-11-12 重启后 USB ADB 转发验证 `/state`、主页和 PNG 路由为 200 | 页面素材受开机自启管理器软链接保护；仍需小时级常驻与断电重启稳定性观察 |
| 退出恢复原厂 UI | A | 包装脚本停止后 `zte_mifi` S、线程释放、快照恢复；fb/用户确认 | 背光偶发关闭，已补 blgon |
| 常驻/看门狗兼容 | B | 只挂 UI 线程约 11.4 分钟无重启 | 小时级稳定性待观察 |
| Android Companion App | C | Android 16 手机已通过 ADB 安装并启动 APK；离线假模型实际返回默认回复、happy、none；Gradle `assembleDebug` 成功，APK 26,694 B | 手机未连 PetCard AP，设备状态 timeout；本地 Wi‑Fi/蜂窝路由、Keystore 尚待验收 |
| 云端 AI/长期记忆 | C | 离线假模型实际运行；DeepSeek adapter、严格建议 schema、Keystore key、本机记忆审批/清空、动作确认及 ¥5/日预算 gate 已实现；预算仍为 ¥0.0000，本轮未调用 API | 真实网络、计费及分流需验收；¥5 为本 App 本机估算 gate，不约束同一 key 的其他用量或本机日期/数据清理 |
| 开机自启/安装到 userdata | B | 程序、ui_hold、管理器、素材已安装到 `/mnt/userdata/petcard`；普通重启后由 `/etc/rc` 钩子自动拉起并完成首帧握手，旧文件保留 `.old` | 断电重启、关机后自动开机与小时级稳定性待验；`/mnt/userdata` 可用空间约 1.2 MB |
| 外壳/移动电源/供电 | D | — | 阶段 5 |
| 手机语音/设备语音 | D | — | V1.1 之后 |
| BLE | F | 未发现 HCI/BT 固件 | 若硬性要求需重评硬件 |
| STA 连手机热点 | F | 无 `wpa_supplicant` 二进制 | 需自编译或改用 AP 本地链路 |
| 私有恢复/回滚脚本 | A | `resume.sh`、快照写回、锁清理 | 异常 SIGKILL 仍需人工恢复 |

## 阶段结论

- 阶段 0：能力确认完成（见 `docs/stage0-capability-report.md`）。
- 阶段 1：显示/按键原型完成并实机验收。
- 阶段 2：离线养成完成并实机验收。
- 阶段 3：本地 HTTP 协议与 Companion 页面部署完成；2026-11-11 手机视觉及喂食/玩耍/摸摸/睡觉触控验收通过。Android App 与长连接未开始，按后续产品需求决定。
- 阶段 3 UI 接管：2026-11-12 新版本已部署并通过普通重启验证，管理器提前启动、PetCard 首帧 ready、`ui_hold` 在启动早期停住 `mmi`/`mmihandlemsg`，服务与网络正常。用户是否仍看到约 5–7 秒原厂 Power On、以及关机/开机画面的实机确认仍待完成。
- 阶段 4：安全契约、路线图和首个 Android 竖切已开始；Java 源编译及 Gradle APK 打包通过，手机安装/运行验收待办。DeepSeek `deepseek-flash` 与 key/数据/预算边界已由用户确认；本轮未发起真实 API 请求。阶段 5：常驻安装与开机自启已完成并重启验证；外壳、移动电源和供电测试未开始。
