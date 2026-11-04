# CUTEE · 单学生成绩管理

CUTEE 是一个 **Windows 单机离线** 的 Qt Widgets 桌面工具，用于维护 **一位学生** 的考试成绩。

> 产品定位已从早期的“多学生考试工具箱”调整为“**单学生成绩管理**”。旧版本的多学生名单、在线作答、交卷与自动评分入口已全部移除；旧数据不会被静默删除，详见[旧数据保留策略](#旧数据保留策略)。

## 功能

- **学生档案**：首次启动引导填写姓名、学号/准考证号、学校/班级、备注；全库只允许存在一条档案，之后只能编辑。
- **科目管理**：科目 CRUD，分为 **OI 竞赛**（CSP-J、NOIP、模拟赛、算法专题等）与 **文化课**（语文、数学、英语、物理等）。支持排序、启用/停用；被成绩引用的科目不能删除，只能停用。
- **成绩管理**：新增、编辑、删除、按考试类型/科目/日期筛选、关键字搜索；自动计算得分率，列表默认按考试日期倒序。
- **成绩汇总**：按科目、按考试类型分别统计考试次数、平均得分、平均得分率、最高分、最低分、最近一次成绩；支持全部时间、本学期、自定义起止日期。
- **CSV 导出**：导出当前筛选下的成绩明细，或当前时间范围的汇总（UTF-8 BOM，Excel 可直接打开）。
- **设置 / 日志**：数据目录、日志级别、旧数据统计与显式清理。

本期不包含：多学生、考生名单、联考、在线作答、自动交卷/评分、代码测评、账号、云同步。

## 构建

要求：

- Windows 10/11 x64
- Qt 6.5 及以上（Widgets、Sql、Test）
- CMake 3.21+
- MSVC 2022（推荐）或 MinGW 13 验证构建

```powershell
$env:QTDIR = 'D:\Qt\6.11.1\msvc2022_64'
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --test-dir build --output-on-failure
```

仓库同时提供 `windows-mingw` 预设（Ninja + MinGW）。使用前设置 `QTDIR`、`MINGW_BIN`、`NINJA_DIR`，运行 ctest 时需把 Qt 的 `bin` 目录加入 `PATH`，否则会因找不到 Qt DLL 而失败：

```powershell
$env:QTDIR = 'D:\Qt\6.11.1\mingw_64'
$env:MINGW_BIN = 'D:\Qt\Tools\mingw1310_64\bin'
$env:NINJA_DIR = '<dir containing ninja.exe>'
$env:PATH = "$env:QTDIR\bin;$env:MINGW_BIN;$env:PATH"
cmake --preset windows-mingw
cmake --build --preset windows-mingw
ctest --test-dir build-mingw --output-on-failure
```

## 数据目录

默认使用 Qt 的 `AppDataLocation`（Windows 上通常为 `%APPDATA%\CUTEE\CUTEE`），包含：

| 文件 | 说明 |
| --- | --- |
| `cutee.db` | SQLite 数据库（WAL 模式） |
| `logs/cutee.log` | 运行日志 |

可通过环境变量 `CUTEE_DATA_DIR` 覆盖数据目录（测试与便携部署使用）。备份时复制整个数据目录即可。

## 数据库与迁移

`schema_version` 记录当前版本，迁移在事务中执行，失败会回滚并写入日志。

- **v1（历史）**：`students`、`questions`、`exams`、`exam_questions`、`exam_students`、`answers`、`submissions`、`scores`、`settings`。
- **v2（当前）**：
  - `student_profile`：固定单行（`id = 1`，`CHECK (id = 1)`）。迁移时若旧 `students` 表已有数据，会取 `created_at` 最早的一条作为当前学生，旧表原样保留。
  - `subjects`：`type` 为 `OI` 或 `ACADEMIC`，`(type, name)` 大小写不敏感唯一；`enabled` 控制是否可用于新成绩。
  - `exam_records`：`exam_type` 区分 OI/文化课，`subject_id` 外键 `ON DELETE RESTRICT`；`rank`、`participants` 可为空；`detail_json` 为后续 OI 多题明细预留（本期 UI 只录总分）。

分数一律以 **百分之一分的整数** 保存（`152.5` 存为 `15250`），避免浮点累加误差；界面与 CSV 按两位小数以内格式化。考试日期以 `yyyy-MM-dd` 保存；创建/更新时间以 UTC ISO-8601 保存，界面按本地时间展示。

“本学期”定义为：`2 月 1 日 – 8 月 31 日`，或 `9 月 1 日 – 次年 1 月 31 日`（跨年）。

### 旧数据保留策略

- 旧表与旧数据 **不会** 被自动删除或覆盖，新版本 UI 也不再读写它们。
- 旧 `scores` 不会自动迁移为 `exam_records`：旧成绩以“考试 + 学生”为键，缺少科目与满分语义，自动映射会制造错误数据。需要时可手工整理。
- “设置 → 旧版考试数据（只读保留）”会显示各旧表行数，并提供 **两次确认**（勾选确认 + 输入“清理”）的显式清理按钮。清理只删除旧表中的行，不影响学生档案、科目与成绩记录。
- 每次启动与清理都会写入日志。

## 项目结构

```
src/
  app/       main.cpp、MainWindow（导航壳）
  core/      模型、时间/分数工具、汇总计算、CSV
  storage/   Database、迁移、各 Repository、设置
  service/   ScoreService（校验/汇总/CSV）、LegacyDataService
  ui/        学生档案、科目、成绩、汇总、设置、日志页面
tests/       tst_database、tst_scores、tst_summary
```

UI 不直接拼 SQL；校验与汇总逻辑集中在 `ScoreService` 与 `core/ScoreSummary`，便于单元测试。

## 已知限制

- 仅维护一位学生，无法新增第二位；如需更换学生请直接编辑档案。
- 科目类型创建后不可修改（避免与已有成绩的 `exam_type` 冲突），请新建科目。
- 科目只有“停用”而没有级联删除：删除被引用的科目会被拒绝。
- 成绩导入（CSV）尚未实现，仅支持导出；`detail_json` 尚未提供 UI。
- 汇总中的“平均得分”“最高/最低分”按原始分数计算；同一科目满分不同的考试请结合“平均得分率”判断。
- 旧版考试数据不会自动转换，也未提供图形化迁移工具。
- 无网络、账号、云同步功能。

## 测试

```powershell
ctest --test-dir build-mingw --output-on-failure
```

覆盖内容：

- 迁移建表、幂等性、外键、旧学生档案采用、旧数据清理不影响新数据。
- 科目 CRUD、同类型重名保护、类型不可变、引用保护、排序与启用/停用。
- 成绩校验（满分为 0、负分、类型不一致、停用科目、排名/人数、得分高于满分的警告）、CRUD 与筛选。
- 分数解析/格式化、按科目/类型/时间汇总、本学期边界、CSV 字段与转义。
