# 已知问题与风险（PetCard V1）

更新：2026-11-12。按严重度排序；P0/P1 未解决不得交付。

## P1（高优先级，需持续观察）

1. **原厂 UI 线程挂起依赖线程名与 ptrace**
   - 现象/影响：`ui_hold` 通过 `mmi_lcd_main`、`mmihandlekey`、`mmi`、`mmihandlemsg` 线程名挂起；原厂固件若重新拉起 UI 线程，可能重新绘制并覆盖宠物画面。
   - 当前缓解：`ui_hold` 启动前 10 秒每 100 ms 重扫、之后每秒重扫并挂起新出现的同名线程；2026-11-12 开机早期只出现 `mmi`、`mmihandlemsg` 两个线程（LCD/按键线程尚未创建），手动启动旧进程时为 8–9 个，均属预期范围。
   - 待办：更长时间（小时级）观察；必要时改成在 `petcard` 内部托管并监控线程变化。
2. **看门狗**
   - 整进程 `SIGSTOP zte_mifi` 约 10 分钟会重启设备（两次复现）。禁止使用整进程暂停。
   - 当前保留 `zte_watchdog`、主线程、AP 线程；如原厂固件策略变化，可能仍需重新设计常驻方案。
3. **BLE 缺失**
   - 未发现 `/sys/class/bluetooth`、HCI、BT 固件。V1 手机控制只能走 Wi‑Fi 本地 HTTP；若 BLE 是硬性要求，需要硬件/需求重新评估。
4. **本地 HTTP 无认证**
   - 任何连上原厂热点且知道 Wi‑Fi 密码的客户端都能调用 `/state` 和 `/action`。V1 是个人设备、本地网；后续手机 App 可加 token/配对。
5. **常驻安装已落盘，关机/断电与长期稳定性未验**
   - 程序、`ui_hold`、管理器和素材已安装到 `/mnt/userdata/petcard`，普通重启后由 `/etc/rc` 钩子自动拉起；旧文件保留 `.old` 可回滚。
   - 待办：实机关机确认页/关机页与“按住 5 秒→松开→短按”流程验证；关机后自动开机根因未确认；断电重启和小时级稳定性需观察；`/mnt/userdata` 仅约 1.2 MB 可用。
   - 已缓解：关机等待期间每 2 秒唤醒面板；`/sbin/poweroff` 返回 0 但 30 秒内未断电时记录 `timeout`、清理标记并恢复宠物界面，避免永久卡在关机页。
   - 2026-11-12 起默认改为**软关机**：关机页后 `sleepin`/`blgoff` 熄屏，不调用 `/sbin/poweroff`，设备不重启；按电源键唤醒。`--hard-poweroff` 保留真实断电路径。debugfs 实测 `sleepin`/`blgoff` 后 `sleep mode=sleep`、`backlight=off`，`sleepout`/`blgon` 恢复 `normal/on`。
   - 实机证据：真实 poweroff 请求后约 7 秒 boot_id 变化，无 `command-exited`；USB 供电下真实断电无法保持关机。软关机的实机熄屏/唤醒仍待用户验证。

## P2（中优先级）

6. **软关机期间本地 HTTP 不应答**
   - 2026-11-12 实测：软关机后 PetCard 仍有监听 socket，但 `run_loop` 在 `g_soft_off` 分支不调用 `http_poll()`，ADB 转发的 `/state` 连接建立后无响应；Companion 会显示 timeout。
   - 当前规避：按电源键唤醒宠物后再使用手机控制。
   - 待决策：是否允许手机通过 `/action?do=wake` 远程唤醒；若需要，软关机循环要保留 HTTP 轮询并仅允许 wake/state。
7. **面板刷新 22 ms/帧**
   - 整帧 32 KiB 写入约 21.6–22.0 ms；当前动画 4–8 fps。更高帧率需要减少写入/局部刷新，但驱动目前只验证了整帧写。
8. **背光可能被原厂休眠逻辑关闭**
   - 停止宠物后偶发 `g_sLcd_Backlight off`；`resume.sh`/包装脚本已补 `blgon`。
9. **存档频率与掉电**
   - 每 120 秒或退出保存；掉电最多丢约 2 分钟状态（离线结算可按 `last_seen` 补回时间，但未保存的交互会丢）。
10. **时间回退/无网络校时**
   - 离线结算依赖 RTC；若时间回退，按 0 处理；跨重启的时间正确性依赖设备 RTC。
11. **原厂 AP 付费边界**
    - 只用本地通信，不启用 NAT/转发；不要误连到需要付费的上网服务。保留原厂 `hostapd`，不要杀/重配。
12. **手机无互联网提示与自动切网**
    - 手机连原厂 AP 时 Android 会提示“无互联网”；2026-11-12 实测手机在真实 API 请求后自动切回有网 Wi‑Fi「311」。
    - 规避：在该提示框勾选“对于此网络不再询问”并选“是/保持连接”；之后手机保持 `adol-2D5025`，Companion 的 `/state` 与 `/action` 正常。
    - App 只选择持有 `192.168.0.x` 地址且非 VPN 的 Wi‑Fi 走本地，排除 VPN 的蜂窝走云端；不再回退到其他网段。系统层网络选择仍由手机决定，必要时可临时忘记其他 Wi‑Fi。
13. **字体/界面**
    - 设备端只有 3×5 ASCII 字体；中文只在手机网页/未来 App 展示。情绪/动作目前是程序化绘制，不是逐帧精灵图。
14. **原厂 Power On 无法在应用层移除**
    - 2026-11-12 fb 时间线：uptime 3.20–4.05 s fb 全黑，4.56 s 起为 PetCard 宠物页，3.2 s 后无原厂帧；`/etc_ro/mmi` 启动素材 bind 挂载正常。
    - 结论：原厂画面在 Linux 用户态之前显示或由面板保留到 PetCard 首次写屏；`prepare_boot.sh` 已在 userdata 可用的最早时刻（约 3.73 s）写入 PetCard 启动帧，只能缩短可见时间，不能从根上移除。bootloader/内核 logo 修改超出当前“不写 MTD/NV/bootloader”边界。

## P3（低优先级）

15. 存储/内存：`/mnt/userdata` 可用约 1.2 MB；静态 petcard 约 604 KB、ui_hold 约 506 KB，另保留 `.old` 备份。PetCore VmRSS 约 396 KB。不要同时保留多份大二进制。
16. 面板 `update` 是驱动彩条测试图，禁止用于刷新；文档已标注。
17. 私有固件/镜像/转储在 `work/`（gitignore），不得提交/上传。
18. 电池焊盘未修；USB‑C 外接供电和移动电源低负载断电仍需实测。
19. `adb reboot` 在这台设备上返回 0 但不重启；普通重启需 `adb shell 'sync; /sbin/reboot'`。
20. Android Companion 当前为 debug 包（`debuggable=true`），未加 `FLAG_SECURE`；正式发布前需 release 签名与截屏/备份策略评估。
21. Companion 使用单线程 worker：云端请求进行中刷新/动作会排队；联网按钮已禁用防重复计费，后续可拆分本地与云端执行器。
