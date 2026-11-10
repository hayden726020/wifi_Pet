# STATE

- 更新：2026-11-10
- 阶段：显示/按键链路已实机打通；单键贪吃蛇基线完成（本次提交）；PetCard V1 待开发。
- 已完成：
  - ADB root、唯一按键 KEY_POWER(116)、fb0 128×128 16bpp 已确认。
  - 面板 sleep/wake 控制（debugfs `lcd_lead`: `sleepout`/`blgon`）已实机验证。
  - 整帧刷新 `write(fb_fd, fb, 32768)` + B-R-G 颜色字段转换已实机验证。
  - 单键贪吃蛇（READY/PLAY/OVER、恢复原厂 UI）已部署运行；P1 恢复问题已修复并实机测试。
- 验证证据：用户确认 TEST 四角颜色正确；`FBWRITE n=32768`；`sleep mode normal`、背光 on；按键 code 116；wrapper SIGTERM / `resume.sh` / 强制 `sleepin` 恢复测试通过。
- 当前设备状态：本次基线提交时游戏已停止，原厂 `zte_mifi` 为 `S`，面板 normal，锁已清理。
- 阻塞：无（显示/按键已通）。
- 风险：面板自动 sleep（需保活）；暂停 `zte_mifi` 的异常恢复；`/tmp` 重启清空；私有固件/镜像不得入库。
- 下一步：
  1. 按 `PetCard_V1_项目实施方案.md` 做 PetCore 显示与状态引擎，复用已验证显示路径。
  2. 评估常驻安装、与 `zte_mifi` 共存、Wi-Fi STA/BLE。
- 入口文档：`HANDOFF.md`；硬件/备份交接在 Codex 私有仓库，不在本仓库。
