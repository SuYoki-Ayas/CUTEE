# CUTEE · 单学生成绩管理

CUTEE 是一个 **Windows 单机离线** 的 Qt Widgets 桌面工具，用于记录与分析 **一位学生** 的考试成绩。

- 全库只维护一个学生档案，年级由「入学年份 + 系统日期」自动推算，不需要手工维护。
- 科目分 **OI 竞赛** 与 **文化课** 两类，支持排序与启用/停用；被成绩引用的科目只能停用。
- 成绩支持增删改、按类型/科目/日期/关键字筛选，自动计算得分率；汇总提供「汇总表」与「成绩趋势」两个视图（趋势图基于 Qt Graphs）。
- 支持导出成绩明细/汇总 CSV、导出与导入单文件数据库、重置数据（导入与重置需手动输入关键字二次确认）。
- 不含：多学生、在线作答、自动评分、代码测评、账号、云同步。

> **平台支持**：仅 Windows 10/11 x64，短期内不考虑 Linux/macOS 移植。

**可靠性验证完成的操作系统：**
  - Windows11 24H2

如果您在您的操作系统上验证了，请在 [**2**](https://github.com/SuYoki-Ayas/CUTEE/issues/2) 反馈。

开发约定、文件职责与架构说明见 [AGENTS.md](AGENTS.md)。

**最新版本：** Release V1.0.0

## 构建

要求：

- Windows 10/11 x64
- Qt 6.5+（Widgets / Sql / Concurrent / Test / **Graphs** / QuickWidgets）——成绩趋势使用 Qt Graphs
- CMake 3.21+ 与 Ninja
- MSVC 2022+（MSYS2 的 `cmake`/`ninja` + MSVC 的 `cl.exe`，生成器为 Ninja）

```powershell
# 机器路径（Qt 前缀、cl.exe、PATH/INCLUDE/LIB）写在 gitignore 的 CMakeUserPresets.json 里
cmake --preset local-msvc
cmake --build --preset local-msvc
ctest --preset local-msvc
```

通用预设 `windows-msvc` 只假设 `PATH` / `INCLUDE` / `LIB` 已指向 MSVC 与 Windows SDK，`QTDIR` 指向 Qt 的 MSVC kit：

```powershell
$env:QTDIR = 'D:\Qt\6.11.3\msvc2022_64'
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

发布包：

```powershell
powershell -ExecutionPolicy Bypass -File scripts\package.ps1
```

脚本构建 Release、用 `windeployqt` 收集 Qt 运行时（含 Qt Graphs 的 QML 模块与中文翻译），
输出 `dist\CUTEE-<版本>-win64.zip`，解压即可运行，无需安装 Qt。

## 数据目录

默认 Qt `AppDataLocation`（`%APPDATA%\CUTEE\CUTEE`），可用环境变量 `CUTEE_DATA_DIR` 覆盖：

| 文件 | 说明 |
| --- | --- |
| `cutee.db` | SQLite 数据库（WAL 模式） |
| `logs/cutee.log` | 运行日志 |
| `backups/` | 「导入」「重置」前自动生成的快照，以及「导出数据库」的默认保存位置 |

## 测试

```powershell
ctest --preset local-msvc
```

覆盖迁移与 schema 兼容、年级推算、科目/成绩校验与 CRUD、汇总与时间范围、表格模型与筛选代理、数据导入导出/重置、二级验证窗口、页面保存三态、趋势图数据契约。

## 许可证

本项目以 **GNU General Public License v3.0（GPLv3）** 发布，完整条款见 [LICENSE](LICENSE)。

第三方许可：

- **Qt 6**（Widgets / Sql / Concurrent / Gui / Core）采用  GPLv3 许可。
- **Qt Graphs** 与 **Qt Quick / QML**（成绩趋势图使用）采用 GPLv3 许可。
- **SQLite** 为 Public Domain。
