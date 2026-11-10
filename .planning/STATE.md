# STATE

- 更新：2026-11-10 晚
- 阶段：PetCard V1 阶段 1（显示/按键）与阶段 2（离线养成）已完成实机验收；手机本地 HTTP 控制页可用。阶段 3（手机 App/长连接）、阶段 4（云端 AI）、阶段 5（常驻安装/外壳）未开始。
- 基线提交：`e082af1`（单键贪吃蛇 + 显示链路）；本轮新增 PetCore 与能力报告。

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
- 手机直连：保留原厂 AP（`adol-2D5025`，密码为原厂默认值，不写入仓库），手机访问 `192.168.0.1:8420` 正常；未缴费、无运营商流量，未启用 NAT/转发。
- 看门狗处理：整进程 SIGSTOP `zte_mifi` 约 10 分钟会重启设备（两次复现）；改为 `ui_hold` 只挂 UI/按键线程，保留 `zte_watchdog`/AP 线程。
- 验证证据：
  - 用户实机确认宠物画面、三种按键、手机页面均正常；
  - `FLUSH ... avg_us=21600~22040`（整帧约 21.6–22.0 ms）；
  - 存档文件存在且重启后加载（年龄/经验延续）；
  - `resume.sh` 退出恢复：`zte_mifi` 为 `S`、面板 `normal`、原厂 UI 恢复（用户确认）；
  - 长稳：`ui_hold` 只挂 UI 线程约 11.4 分钟、3510 帧无重启（超过整进程 SIGSTOP 约 10 分钟的重启窗口）；包装脚本启动/停止/快照恢复实测通过；更长时间仍需观察。

## 当前设备状态

- 本轮收尾时停止 PetCore，原厂 UI 恢复，`hostapd`/`udhcpd` 正常，存档保留。
- `/tmp` 中程序重启后需重新 push。

## 风险 / 阻塞

- 没有 BLE 硬件（无 HCI/BT 固件）；V1 手机控制走 Wi‑Fi 本地 HTTP。
- `ui_hold` 依赖原厂线程名与 ptrace；长期稳定性、线程被重新拉起、看门狗策略变化需持续观察。
- 原厂 UI 的背光休眠逻辑偶尔会关背光，`resume.sh` 会补 `blgon`。
- 原厂 AP 的付费限制只影响上网；本地通信可用，但不要改/杀 hostapd。
- `/tmp` 非持久；阶段 5 需安装到 `/mnt/userdata`，评估 jffs2 执行、内存、开机自启。
- 私有固件/镜像/转储在 `work/`（gitignore），不得提交。

## 下一步

1. 阶段 3：Android App 或本地网页 PWA；绑定 Wi‑Fi 本地网络 + 蜂窝云端；HTTP 长连接/WebSocket。
2. 阶段 4：云端 AI 代理、人格、长期记忆；AI 只提供情绪/动作建议。
3. 阶段 5：常驻安装、开机自启、UI 线程挂起自动化、外壳、USB‑C 供电与移动电源实测。
4. 产品决策：BLE 是否硬性要求；若必须，需要重新确认硬件或接受外接方案。

## 入口文档

- `HANDOFF.md`：当前状态、关键坑、构建/部署/运行。
- `petcard/README.md`：PetCore 使用与 API。
- `docs/stage0-capability-report.md`：能力报告。
- `docs/known-issues.md`、`docs/function-matrix.md`、`docs/adr-0001-local-link-and-ui-hold.md`：已知问题、功能矩阵、关键决策。
- 硬件/备份交接在 Codex 私有会话目录，不在本仓库。
