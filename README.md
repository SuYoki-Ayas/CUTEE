# CUTEE · 单学生成绩管理

CUTEE 是一个 **Windows 单机离线** 的 Qt Widgets 桌面工具，用于记录与分析 **一位学生** 的考试成绩。

> **平台支持**：仅 Windows 10/11 x64，短期内不考虑 Linux/macOS 移植。

## 功能

- **学生档案**：全库唯一一条档案（姓名、学段、入学年份、目标院校）；年级由「入学年份 + 系统日期」自动推算（每年 9 月升一级，初中 3 年级升为高中 1 年级）。
- **科目管理**：OI 竞赛 / 文化课两类科目的增删改、排序、启用停用；被成绩引用的科目只能停用，不能删除。
- **成绩管理**：增删改、按类型/科目/日期/关键字筛选、自动计算得分率，默认按考试日期倒序；列可显示或隐藏并持久保存。
- **成绩汇总**：按科目与类型统计次数、平均分、平均得分率、最高/最低分、最近一次成绩；支持全部时间/本学期/自定义范围，含「汇总表」「成绩趋势」两个标签页。
- **CSV 导出**：当前筛选的明细或当前时间范围的汇总（UTF-8 BOM，Excel 可直接打开）。
- **备份与恢复**：导出/导入单个 `.db` 文件，或重置数据；导入与重置需在二级验证窗口手动输入「导入」/「重置」，执行前自动备份到 `backups/`。
- **设置 / 日志**：数据目录、日志级别、schema 版本、日志查看与自动刷新。
- **界面**：左侧分组侧栏（档案 / 考试 / 设置）显示姓名、推算年级与目标院校；`Ctrl+1…6` 切换页面。

不包含：多学生、考生名单、在线作答、自动评分、代码测评、账号、云同步、QML。

## 构建

要求：Windows 10/11 x64、Qt 6.5+（Widgets / Sql / Concurrent / Test / **Charts**）、CMake 3.21+、MSVC 2022 或 MinGW 13。

> 成绩趋势依赖 Qt Charts（GPLv3 / 商业双许可），安装 Qt 时需勾选该模块。

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

> 编译器必须用 Qt 自带的 MinGW 13.1：Qt 6.11 的 MinGW 构建基于 `msvcrt.dll`，与 UCRT64 的 gcc 混用会跨 CRT 传递 C++ 对象。运行时保证 `Qt\bin` 排在 `D:\MSYS2\ucrt64\bin` 之前。也可把上面几个变量写进 `CMakeUserPresets.json`（已 gitignore）做成 `local-mingw` 预设，之后直接 `cmake --preset local-mingw`、`cmake --build --preset local-mingw`、`ctest --preset local-mingw`。

## 数据目录

默认 Qt `AppDataLocation`（`%APPDATA%\CUTEE\CUTEE`），可用环境变量 `CUTEE_DATA_DIR` 覆盖：

| 文件 | 说明 |
| --- | --- |
| `cutee.db` | SQLite 数据库（WAL 模式） |
| `logs/cutee.log` | 运行日志 |
| `backups/` | 「导入」「重置」前自动生成的快照，以及「导出数据库」的默认保存位置 |

- **导出**：用 `VACUUM INTO` 生成含 WAL 未合并改动的一致性快照，得到可独立打开的 `.db`。
- **导入**：先校验文件（是否 CUTEE 库、schema 是否过新），二级验证确认后自动备份 → 关闭连接 → 替换文件 → 重开并迁移 → 刷新所有页面；失败自动还原。
- **重置**：二级验证后先备份，再清空 `student_profile`、`subjects`、`exam_records`，保留 `settings` 与 schema。

## 数据库与迁移

`schema_version` 记录版本，迁移在事务中执行，失败回滚并写日志。当前为 v3：

- **v2**：`student_profile`（固定单行 `id = 1`）、`subjects`（`type` 为 `OI`/`ACADEMIC`，`(type, name)` 大小写不敏感唯一）、`exam_records`（`subject_id` 外键 `ON DELETE RESTRICT`，`rank`/`participants` 可空，`detail_json` 为 OI 多题明细预留）、`settings`（键值对）。
- **v3**：`student_profile` 增加 `stage`（`PRIMARY`/`JUNIOR`/`SENIOR`）与 `enrollment_year`（`0` 表示未填写），用于推算年级；目标院校存放在 `note` 列。

分数以**百分之一分的整数**保存（`152.5` 存为 `15250`），避免浮点累加误差；界面与 CSV 按两位小数格式化。考试日期存 `yyyy-MM-dd`，创建/更新时间存 UTC ISO-8601，界面按本地时间展示。「本学期」为 `2 月 1 日 – 8 月 31 日` 或 `9 月 1 日 – 次年 1 月 31 日`。

## 项目结构

```
resources/
  cutee.qrc                     Qt 资源清单（样式表与图标）
  styles/app.qss                唯一的全局样式表（浅色主题）
  icons/*.svg                   表头排序箭头等小图标
src/
  app/       main.cpp、MainWindow（导航壳）、Theme（Fusion + 调色板 + QSS + 高 DPI）
  core/      模型、时间/分数工具、汇总计算、CSV
  storage/   Database、迁移、数据导入导出/备份/重置、各 Repository、设置
  service/   ScoreService（校验/汇总/CSV）
  ui/        UiKit / Palette / TagDelegate / DangerConfirmDialog 公共构件、
             ScoreTableModel（表格模型 + 过滤代理）、TrendChart（Qt Charts 成绩趋势）、
             学生档案、科目、成绩、汇总、设置、日志页面
tests/       tst_database、tst_scores、tst_summary、tst_score_model、tst_school_stage、
             tst_data_maintenance、tst_danger_dialog、tst_profile_page、tst_settings_page、
             tst_trend_chart
```

UI 不直接拼 SQL；校验与汇总逻辑集中在 `ScoreService` 与 `core/ScoreSummary`，便于单元测试。逐文件职责见 [AGENTS.md](AGENTS.md)。

## 界面与交互

- **布局**：左侧 `QTreeWidget` 分组导航 + `QStackedWidget` 页面区，默认 1240×800、最小 1024×680；窗口几何与最后页面存入 `settings`。类别「档案」「考试」「设置」加粗并可展开收起，标题下方显示姓名、推算年级与目标院校（未建档时隐藏）。
- **主题**：Fusion + 显式浅色 `QPalette` + 微软雅黑 + `:/styles/app.qss`，只提供浅色主题，不随系统深色模式切换。颜色、圆角、表头、选中与禁用态集中在 `app.qss`，页面通过 `uiRole` 动态属性选择样式，不写 `setStyleSheet`。
- **配色**（`src/ui/Palette.h` 与 `app.qss` 保持一致）：

| 颜色 | 色值 | 用途 |
| --- | --- | --- |
| 薄荷绿 | `#C4F2C8` | 成功、启用、空状态底色 |
| 淡紫 | `#E6DAF7` | 主操作按钮、表格与导航选中态 |
| 黄绿 | `#E7EBC3` | 警示、低得分率、文化课标签 |
| 蓝紫 | `#D8DDF0` | 表头、信息提示块 |
| 淡粉 | `#EDD1D1` | 危险操作、错误、得分超满分 |

侧栏同为浅色：底色 `#EDE9F7`、悬停 `#E6E0F3`、选中 `#E6DAF7`（深紫文字 `#4A3A6B`），右侧 1px `#DFD8EE` 分隔线。

- **成绩表**：`QTableView` + `ScoreTableModel` + `ScoreFilterProxyModel`；筛选与排序全在 proxy 内完成，不查库、不重建控件，关键字输入 180ms 防抖；得分率 <60% 显示警示色，得分超满分显示错误色；整表查询在 `QtConcurrent` 工作线程执行。
- **汇总页**：统计异步执行（独立 SQLite 连接），`QFutureWatcher` 回主线程；平均得分率用自绘条形展示；趋势图为 `QChartView`（折线 + 散点），横轴日期、纵轴 0–100%。
- **保存按钮三态**（档案页与设置页一致）：无改动时灰色 `已保存`（禁用、隐藏撤销）；有改动时浅绿 `保存修改` / `保存设置` + `撤销更改`；保存成功后回到 `已保存` 并出现 `撤销修改`（回填保存前内容，需再次保存才落库）。
- **破坏性操作**：删除成绩/科目、停用科目二次确认（中文动作按钮，默认焦点在「取消」），被引用科目禁用删除；导入与重置使用 `DangerConfirmDialog`，必须手动输入关键字，匹配前确认按钮禁用，Enter 落在「取消」。
- **表单校验**：对话框内就近提示（红框 + 错误文字），不弹消息框；仅「得分高于满分」在保存时确认一次。
- **快捷键**：`Ctrl+1…6` 切换页面；成绩页 `Ctrl+N` 新增、`Ctrl+E` 导出、`Ctrl+F` 搜索、`F2` 编辑、`Delete` 删除；科目页 `Ctrl+N`/`F2`/`Delete`；档案与设置页 `Ctrl+S`；汇总页 `Ctrl+E`；日志页 `F5`。均为页内作用域，互不冲突。
- 所有页面使用布局管理器，无 `setGeometry` 绝对定位。

## 手工验收清单

1. 启动：侧栏浅紫、当前页高亮、画布浅灰、表格白底，字体统一，无深色面板。
2. 100% 与 150% 缩放（`QT_SCALE_FACTOR=1.5`）下无重叠或截断；最小尺寸 1024×680 时筛选栏不重叠。
3. 成绩页：1000+ 条数据下滚动、表头排序、切换各筛选均即时响应且不重建页面；得分率 <60% 显示警示色。
4. 新增/编辑成绩：清空考试名称或满分填 0 就地报错；得分高于满分时弹一次确认。
5. 科目页：类型/状态为彩色标签；被引用科目删除禁用；停用后新成绩不可选，历史成绩仍可编辑。
6. 汇总页：切换时间范围统计刷新、无数据时显示空状态；趋势图随科目与时间范围变化。
7. 导出明细/汇总为 UTF-8 BOM CSV，Excel 打开中文不乱码。
8. 档案/设置保存三态：灰 `已保存` → 浅绿 `保存修改` + `撤销更改` → `已保存` + `撤销修改`。
9. 重启后窗口尺寸、最后页面、成绩表列显隐保持；侧栏姓名、年级、目标院校正确，目标为空时该行隐藏。
10. 导出 `.db` 可独立打开；导入需输入「导入」，覆盖后所有页面刷新且 `backups/` 留下快照；重置后档案/科目/成绩清空而 `settings` 与 schema 保留。

## 性能

- 成绩表筛选/排序只更新 proxy；仅进入页面、显式刷新或 CRUD 后查库一次。
- 整表查询与汇总统计均在 `QtConcurrent` 工作线程执行（独立连接），`QFutureWatcher` 回主线程；刷新请求会合并，不堆积任务。
- 本机 MinGW Debug、1500 条成绩：查询 + 模型重置 + 视图刷新约 12ms，启动到主窗口可见约 0.4s。

## 已知限制

- 仅维护一位学生；更换学生请直接编辑档案。
- 科目类型创建后不可修改；科目只能停用，无级联删除。
- 仅支持 CSV 导出，导入未实现；`detail_json` 暂无 UI。
- 汇总的平均分/最高/最低按原始分计算，满分不同的考试请结合平均得分率判断。
- 无网络、账号、云同步。
- 仅 Windows，短期不考虑 Linux/macOS。
- 仅浅色主题，深色属后续可选项。
- 趋势图依赖 Qt Charts（GPLv3 / 商业双许可）；以 LGPL 方式使用 Qt 时需评估该模块的许可影响，或改用自绘图表。

## 测试

```powershell
ctest --test-dir build-mingw --output-on-failure
```

| 测试 | 覆盖 |
| --- | --- |
| `tst_database` | 迁移建表与幂等、v1 → 当前 schema、外键与档案单行约束、v3 列默认值 |
| `tst_school_stage` | 学年起点、学段内升级、跨学段升学、12 年毕业、未填入学年份 |
| `tst_scores` | 科目 CRUD/重名/类型不可变/引用保护/排序/启停；成绩校验、CRUD 与筛选；分数解析、CSV 转义 |
| `tst_summary` | 按科目/类型/时间汇总、本学期边界、最近一次成绩 |
| `tst_score_model` | 行列与表头、前景色、排序角色、proxy 筛选与排序、模型重置 |
| `tst_data_maintenance` | 导出快照、导入校验与还原、备份与临时文件、拒绝导入使用中的库、重置 |
| `tst_danger_dialog` | 关键字门控、首尾空格容忍、默认按钮切换 |
| `tst_profile_page` / `tst_settings_page` | 保存按钮三态、撤销语义、年级提示、目标院校往返 |
| `tst_trend_chart` | 数据点数量、空状态、切换数据后重建坐标轴 |