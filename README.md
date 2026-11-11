# CUTEE · 单学生成绩管理

CUTEE 是一个 **Windows 单机离线** 的 Qt Widgets 桌面工具，用于记录与分析 **一位学生** 的考试成绩。

- 全库只维护一个学生档案，年级由「入学年份 + 系统日期」自动推算，不需要手工维护。
- 科目分 **OI 竞赛** 与 **文化课** 两类，支持排序与启用/停用；被成绩引用的科目只能停用。
- 成绩支持增删改、按类型/科目/日期/关键字筛选，自动计算得分率；汇总提供「汇总表」与「成绩趋势」两个视图。
- 支持导出成绩明细/汇总 CSV、导出与导入单文件数据库、重置数据（导入与重置需手动输入关键字二次确认）。
- 不含：多学生、在线作答、自动评分、代码测评、账号、云同步。

> **平台支持**：仅 Windows 10/11 x64，短期内不考虑 Linux/macOS 移植。

开发约定、文件职责与架构说明见 [AGENTS.md](AGENTS.md)。

## 构建

要求：Windows 10/11 x64、Qt 6.5+（Widgets / Sql / Concurrent / Test / **Charts**）、CMake 3.21+、MSVC 2022 或 MinGW 13。

> 成绩趋势依赖 Qt Charts，安装 Qt 时需在 Maintenance Tool 中勾选该模块。

MSVC：

```powershell
$env:QTDIR = 'D:\Qt\6.11.1\msvc2022_64'
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --test-dir build --output-on-failure
```

MinGW（构建工具取自 MSYS2 UCRT64：`pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja`）：

```powershell
$env:QTDIR = 'D:\Qt\6.11.1\mingw_64'
$env:MINGW_BIN = 'D:\Qt\Tools\mingw1310_64\bin'
$env:NINJA_DIR = 'D:\msys2\ucrt64\bin'
$env:PATH = "$env:QTDIR\bin;$env:MINGW_BIN;$env:PATH"
cmake --preset windows-mingw
cmake --build --preset windows-mingw
ctest --test-dir build-mingw --output-on-failure
```

> 编译器必须用 Qt 自带的 MinGW 13.1（Qt 6.11 的 MinGW 构建基于 `msvcrt.dll`）；运行时 `Qt\bin` 要排在 `D:\MSYS2\ucrt64\bin` 之前。可把这些变量写进 `CMakeUserPresets.json`（已 gitignore）做成 `local-mingw` 预设，之后直接用 `cmake --preset local-mingw` / `--build` / `ctest --preset local-mingw`。

## 数据目录

默认 Qt `AppDataLocation`（`%APPDATA%\CUTEE\CUTEE`），可用环境变量 `CUTEE_DATA_DIR` 覆盖：

| 文件 | 说明 |
| --- | --- |
| `cutee.db` | SQLite 数据库（WAL 模式） |
| `logs/cutee.log` | 运行日志 |
| `backups/` | 「导入」「重置」前自动生成的快照，以及「导出数据库」的默认保存位置 |

## 测试

```powershell
ctest --test-dir build-mingw --output-on-failure
```

覆盖迁移与 schema 兼容、年级推算、科目/成绩校验与 CRUD、汇总与时间范围、表格模型与筛选代理、数据导入导出/重置、二级验证窗口、页面保存三态、趋势图数据契约。

## 许可证

本项目以 **GNU General Public License v3.0（GPLv3）** 发布。
