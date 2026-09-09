# Alpha 5 实际操作验收清单

版本已选定：`v0.1.0-alpha.5`。此文档是待执行清单，未记录任何 PASS 或批准。
正式候选、七张新截图及全新状态已经生成；直接使用现有候选，本清单不重复构建。

## 先确认测试的是候选

- [ ] 从 `build/release/v0.1.0-alpha.5/attestation.txt` 读取 `source_commit`、`source_tag`、`artifact_filename`、`artifact_sha256`；将真实候选身份记入测试记录。
- [ ] 使用该 ZIP 解出的 `Tanks3D.app`，不使用日常开发的 `build/Tanks3D.app`；核对 ZIP 校验和与程序内嵌身份。
- [ ] 记录实际测试者、机器型号/macOS、开始结束时间、候选身份；保存真实录像/观察笔记，由另一位实际人员复核。
- [ ] `build/release-evidence/v0.1.0-alpha.5/interactive/observation-plan.json` 包含固定 v2 发布规范的 70 项观察；此简表不替代该矩阵。新增 Pixel Style 和敌军国家规则还须完成下列补充检查，旧规范没有单独列出这两项。

## 蓝牙手柄、菜单和相机

- [ ] 实际蓝牙连接两只手柄，记录型号及连接方式；比较方向键、摇杆轻推/急转/松开与开火，记录延迟或粘键现象。
- [ ] 菜单分别使用方向键、WASD、手柄方向键/摇杆；分别验证 Enter、Space、底部面键确认、返回及顶部面键重置。
- [ ] Advanced Settings：HP 1–6；三项敌方参数 -30% 至 +30%、步长 5%；逐项验证端点、Reset、Esc 保留及开始/重开生效。
- [ ] Pixel Style 默认 OFF，左右和确认可切换，Reset 恢复 OFF；检查清晰度、HUD和返回行在小窗口可见。
- [ ] 相机水平 -45° 至 +45°、俯角 40° 至 70°，各步长 5°；默认 0°/50°，开始、重开、返回设置均保留选择。
- [ ] 检查全部角度、横屏/方屏/竖屏：摇杆跟随画面道路，键盘和方向键仍对应世界四方向；玩家、阴影、地形不被错误裁掉。
- [ ] 底部/左侧面键及右肩/右扳机开火，+/Start 暂停，-/Back 返回；ABXY 不会退出战斗。
- [ ] 战斗中断开/重连 P1，再测试 P2：无粘键，仍连接的玩家不换槽，重新加入控制正确玩家。
- [ ] 持续推摇杆返回菜单：先不碰方向键，再测试方向键交叠；均须归中后才重新导航，无意外跳行。

## 单人、双人和实际战斗

- [ ] P1：四方向及 Space/右 Option/右 Control 分别开火；P2：WASD 及 F/左 Option/左 Control 分别开火。
- [ ] 两人同时移动、射击、暂停/恢复；逐项测试 Enter、Esc、R、N/B、F8、F11 和设置菜单的 Q/Esc。
- [ ] F8 的画面变化及日志中的 `high-quality` 2048×2048 / `balanced` 1024×1024 均录入相应场景记录。
- [ ] 双人分居地图两端、交叉、死亡及重生：相机保持两人可见，控制与计分不串位。
- [ ] 单人不受隐藏 P2 国家选项影响；同国双人敌军轮换另外两国，不同国双人敌军仅第三国；一方阵亡不改变敌军阵营。
- [ ] 实际验证三国基地、砖/钢破坏、水与船、冰面、树林、九种道具、雷达、HP/绷带条件和炮弹抵消。
- [ ] 实际通关、失败和结算，检查分类击杀/得分、下一关及重开；预览 showcase 不作为这些操作的验收证据。

## 30 分钟候选性能（仅键盘单人）

候选与初始文档完成后，在干净工作树、无其他 GPU 测试的机器上，从仓库根目录运行：

```sh
make run-alpha-performance-qa DIST_CHANNEL=alpha.5 \
  ALPHA_CANDIDATE_DIR="$PWD/build/release/v0.1.0-alpha.5" \
  RELEASE_PERFORMANCE_QA_OUTPUT_DIR="$PWD/build/release-evidence/v0.1.0-alpha.5/performance"
```

- [ ] 输出目录必须不存在或为空；保留旧失败记录，不覆盖。runner 自行校验、解包并启动确切候选，不启动开发程序。
- [ ] 真实键盘游玩约 1801 秒并完成至少一关，及时确认结算继续游戏；保持窗口焦点，不同时截图/编译或切到其他 GPU 应用。
- [ ] 此模式关闭手柄、暂停、重开、手动切关、F8/F11；Esc、关闭窗口或失败后退回菜单会中止有效记录。
- [ ] 验收阈值：≥30 分钟、真实通关≥1、gameplay≥80%、focus≥95%、均帧≥50、1% low≥30、内存增长≤256 MiB；`mode_mix` 为 `one-player`。
- [ ] 保存四份原始输出：`performance-log-v2.json`、`performance-qa-receipt.json`、`performance-stdout.log`、空的 `performance-stderr.log`。
- [ ] 实际观察温度/风扇、音频、画面异常、崩溃/卡死，写明方法和事实；receipt 成功不等于全部通过。
- [ ] runner 自动采集、不自动通关；外部真实键盘自动操作须标为自动压力测试，不能充当人工/蓝牙验收。

## 干净 Mac 首次下载与启动

- [ ] 当前还没有 staging URL。进行此项测试前，须准备与候选字节完全相同的 HTTPS ZIP；URL 可记录且不包含令牌，staging 不等于正式发布。
- [ ] 准备另一台符合候选最低系统版本的 Apple Silicon Mac 或合格干净账户：无本项目源码、旧 app/旧批准，不安装 Homebrew/raylib/Python 来启动游戏。
- [ ] 获得实际 URL 后，用 `make prepare-alpha-v2-clean-mac-qa-kit DIST_CHANNEL=alpha.5 CLEAN_MAC_DOWNLOAD_URL="实际URL"` 准备新的四文件 kit；完整转交。
- [ ] 干净 Mac 开始连续录像，再运行 kit 的 `START_HERE.command`，参数为父目录已存在、最终目录不存在的新 intake 路径。
- [ ] 人工在 Safari 下载、Finder 解压并首次启动；记录真实 Gatekeeper 对话框及实际达到主菜单的路径。
- [ ] 若系统提供 Open Anyway，由本人按发布说明操作；不自动批准、不改 quarantine、不重新签名。保留实际 `spctl` 结果。
- [ ] 交回原 ZIP、完整原始 intake/COMPLETE 和连续 MOV/MP4/M4V；录像 64 KiB–95 MB，静态 PNG 不足以证明启动路径。
- [ ] 另一位复核者检查录像及发布说明后，填写真实复核身份/时间/意见，再编译新证据包。COMPLETE 不是通过或批准。

## 音频与最后复核

- [ ] 实际听完候选的 22 个声音，检查开场、失败、复活、暂停、结算、双人重叠及音量；记录缺失、失真、截断或异常静音。
- [ ] 音频 owner 阅读 `ASSET_LICENSES.md`、`THIRD_PARTY_NOTICES.md` 及候选随附许可证：20 个声音来自 krystiankaluzny/Tanks，两段开场/失败曲来自 JustoSenka/BattleCity。
- [ ] 两仓库的 MIT 声明不等于独立原音乐版权确认；owner 需作真实、明确的风险接受决定。本清单不预填 ACCEPT 或任何签名。
- [ ] 未完成或失败项保持未完成/失败；将经复核的候选证据存入正式版本目录并校验，不能把历史截图、脚本模拟或本清单改写成真实观察。
- [ ] QA lead 与 release owner 分别完成实际最终复核；`check-alpha-release-evidence` 允许 BLOCKED，其成功不能代替严格 `verify-alpha-release-ready`。

完整命令及必填字段见同目录 `QA_PLAN.md`；正式观察细目见仓库 `docs/ALPHA_QA_REPORT_TEMPLATE.md`。
