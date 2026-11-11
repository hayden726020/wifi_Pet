# STATE

## 最新状态（2026-11-11 手机页面故障排查，覆盖下文历史状态）

用户确认手机连接设备热点 `adol-2D5025`。设备之前重启过（`/proc/uptime` 曾为 64.91 秒，`/tmp` 已清空），原因未知；之后按授权重新部署程序、脚本、页面和 44 个素材。设备没有 `nohup`/`setsid`，后台启动收到 Hangup 后会按设计清理；目前通过保持 ADB shell 前台连接成功运行。当前 `ui_hold` 为 `HOLDING 8 threads`，PetCore 监听 `0.0.0.0:8420`，`zte_mifi` 和 `hostapd` 正常；USB ADB 转发实测 `/state` 返回 JSON、主页和动画 PNG 均 HTTP 200，临时转发已移除。用户手机报告页面和所有操作正常；日志显示 feed/play/pet/sleep 请求成功，`interactions` 从 17 增至 21。设备 uptime 从约 325 秒增长到 906 秒，无再次重启。阶段 3 手机视觉/触控验收通过。启动用 ADB shell 必须保持打开，关闭后包装脚本会收到 Hangup 并恢复 UI。ADB 路径为 `C:\Users\SPcuteAG\AppData\Local\Android\Sdk\platform-tools\adb.exe`。

此前用户确认固定帧下 RGB565 高字节优先模式已消除实物屏色块，程序曾恢复动画运行。该 PID 快照已过时，不代表当前运行状态。详见 HANDOFF 中“此前状态”与当前排查记录。

- 更新：2026-11-11
- 阶段：PetCard V1 阶段 1/2 完成实机验收；阶段 3 Companion 页面视觉和四种触控操作由用户手机确认正常，设备日志与状态计数吻合。当前 PetCard 正运行，需保持 ADB 前台会话。Android App/长连接尚未开始；阶段 4 与阶段 5 未开始。
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

1. 阶段 3：用手机打开已部署页面，确认视觉布局和触控喂食/玩耍/摸摸/睡觉；设备 LCD 新图已通过 framebuffer 回读确认，主机已验证 `/state`、无状态变更的 `wake` 与 20 张 PNG。再决定是否包装为 Android App，以及是否需要长连接/WebSocket。
2. 阶段 4：云端 AI 代理、人格、长期记忆；AI 只提供情绪/动作建议。
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
