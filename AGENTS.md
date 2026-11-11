# AGENTS.md · CUTEE 项目导览

面向在本仓库工作的开发者与 AI 代理：说明**每个文件负责什么**、改动时必须遵守的约定，以及常见需求的落点。

- 产品：Windows 单机离线、只维护**一位学生**的成绩管理工具。
- 目标平台：**仅 Windows 10/11 x64**。短期内不考虑 Linux/macOS 移植，不要为此引入跨平台抽象层或条件编译；
  涉及平台相关的改动（字体、路径、打包、CI）一律按 Windows 处理。
- 技术栈：Qt 6.5+ Widgets（Fusion + 全局 QSS）、C++17、CMake、SQLite。
- 不包含：多学生、在线作答、自动判分、代码测评、云同步、QML/WebEngine。

---

## 1. 构建 · 运行 · 测试

```powershell
# MSVC（首选）
$env:QTDIR = 'D:\Qt\6.11.1\msvc2022_64'
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --test-dir build --output-on-failure

# MinGW（验证构建；ctest 需要 Qt 在 PATH 中）
$env:QTDIR = 'D:\Qt\6.11.1\mingw_64'
$env:MINGW_BIN = 'D:\Qt\Tools\mingw1310_64\bin'
$env:NINJA_DIR = 'D:\msys2\ucrt64\bin'
$env:PATH = "$env:QTDIR\bin;$env:MINGW_BIN;$env:PATH"
cmake --preset windows-mingw
cmake --build --preset windows-mingw
ctest --test-dir build-mingw --output-on-failure
```

- 便携/测试数据目录：设置环境变量 `CUTEE_DATA_DIR`。
- 无 `QTDIR` 时 MSVC 预设无法配置；MinGW 预设还依赖 `MINGW_BIN`、`NINJA_DIR`。
- **构建工具统一来自 MSYS2**：`cmake` / `ctest` / `cpack` / `ninja` 都取 `D:\MSYS2\ucrt64\bin`
  （`pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja`），不再使用 Python 目录下的 pip 版 cmake。
  `ninja` 依赖同目录的 `libgcc_s_seh-1.dll` / `libstdc++-6.dll`，因此以**绝对路径**引用（`CMAKE_MAKE_PROGRAM`）。
- **编译器仍用 Qt 自带的 MinGW 13.1**（`D:\Qt\Tools\mingw1310_64`）。Qt 6.11 的 `mingw_64` 二进制链接
  `msvcrt.dll`，而 MSYS2 UCRT64 的 gcc 16 走 UCRT，两者混用会跨 CRT 传 `std::string` / `FILE*`，禁止用它链接 Qt。
- **运行时 DLL 顺序**：`D:\MSYS2\ucrt64\bin` 在 `PATH` 上，若它排在 `D:\Qt\6.11.1\mingw_64\bin` 之前，
  `CUTEE.exe` / 测试会加载 UCRT 版 `libstdc++` 配 msvcrt 版 Qt。`ctest --preset local-mingw` 已在 test preset 里
  前置 Qt 的 `bin`；手动运行 exe 时同样保证 Qt 的 `bin` 在前（或使用 `windeployqt` 产出的目录）。
- 本机固定路径可写进 `CMakeUserPresets.json`（已 gitignore），例如继承 `windows-mingw` 的 `local-mingw` 预设，
  之后无需任何环境变量即可 `cmake --preset local-mingw` / `--build` / `ctest --preset local-mingw`。
- 提交前必须保证：可构建（无警告）、`ctest` 全绿。

---

## 2. 分层与硬性约定

| 层 | 目录 | 允许依赖 |
| --- | --- | --- |
| 界面 | `src/ui`, `src/app` | core、service、storage（只通过其公开 API） |
| 服务 | `src/service` | core、storage |
| 存储 | `src/storage` | core |
| 基础 | `src/core` | Qt Core/Gui（不含 Widgets/Sql，`README` 中 `cutee_core` 仅链接 Core+Sql） |

硬性规则：

1. **UI 不拼 SQL**：所有查询走 `service/ScoreService` 或 `storage/*Repository`。
2. **不直接改 schema 语义**：改表结构必须新增迁移条目，并在 `tests/tst_database.cpp` 覆盖。
3. **所有页面用布局管理器**，禁止 `setGeometry` 固定坐标。
4. **样式集中**：颜色/边框/圆角只在 `resources/styles/app.qss` 与 `src/ui/Palette.h`；
   页面里用 `ui::applyRole(widget, "primary")` 之类的动态属性，禁止散落 `setStyleSheet`。
5. **表格用 Model/View**：大数据量列表不得用 `QTableWidget` 逐格 `setItem`。
6. **耗时操作离开 UI 线程**：用 `QtConcurrent` + `QFutureWatcher`，工作线程自建 SQLite 连接。
7. **破坏性操作二次确认**：统一用 `ui::confirmDanger(...)`（中文动作按钮，默认焦点在“取消”）；
   整库级操作（导入、重置）必须再升一级，用 `ui::confirmTyped(...)` 让用户手打关键字。
8. **仅浅色主题**：`QPalette` 显式锁定浅色，避免系统深色模式造成白底白字。

---

## 3. 文件职责清单

### 3.1 根目录

| 文件 | 职责 |
| --- | --- |
| `CMakeLists.txt` | 定义 `cutee_core`（基础层静态库）、`cutee_models`（表格模型，链 Gui）、`CUTEE` 可执行文件（`resources/cutee.qrc` + `AUTORCC ON`，链接 `Qt6::Charts`）、测试开关；MSVC 用 `/W4`，GCC 用 `-Wall -Wextra -Wpedantic`。 |
| `CMakePresets.json` | `windows-msvc`（VS 2022，输出 `build/`）与 `windows-mingw`（Ninja，输出 `build-mingw/`）配置/构建/测试预设。 |
| `README.md` | 面向用户的说明：功能、构建、数据目录、数据库与迁移、界面结构与配色、交互约定、手工验收清单、性能数据、已知限制、测试覆盖。 |
| `AGENTS.md` | 本文件：文件职责索引 + 开发约定。 |
| `.gitignore` | 忽略构建树（`build/`、`build-*/`）、IDE 状态、`*.log`、`CMakeUserPresets.json`。 |
| `.gitattributes` | 统一换行符处理。 |

### 3.2 `resources/`

| 文件 | 职责 |
| --- | --- |
| `resources/cutee.qrc` | Qt 资源清单，把样式表与图标编进可执行文件（需要目标开启 `AUTORCC`）。 |
| `resources/styles/app.qss` | **唯一的全局样式表**：基础控件、**浅色侧栏**（`#sidebar` / `#navTree` 及其 hover/selected 态、分组分支箭头、学生信息块 `#studentName` 粗体二级标题与 `#studentGrade` / `#studentGoal` 灰色小字）、页面标题、按钮角色、输入框、表格/表头/选中态、标签页（`QTabWidget`/`QTabBar`）、滚动条、分组框、状态栏、菜单、工具提示。文件头注释列出五色配色。 |
| `resources/icons/chevron-down.svg` | 下拉框/日期框箭头、降序排序指示。描边 `#6A6573`。 |
| `resources/icons/chevron-up.svg` | 微调框上箭头、升序排序指示。 |
| `resources/icons/chevron-down-disabled.svg` | 禁用态下拉箭头（低对比描边）。 |
| `resources/icons/chevron-right.svg` | 侧栏分组折叠时的右向箭头。 |

### 3.3 `src/app/` — 进程入口与主窗口

| 文件 | 职责 |
| --- | --- |
| `main.cpp` | 设置高 DPI 策略（构造 `QApplication` 之前）→ 应用主题 → 初始化日志 → 打开数据库 → 执行迁移 → 读取日志级别 → 创建并显示 `MainWindow`；启动失败用 `QMessageBox` 提示并返回非零码。 |
| `Theme.h` / `Theme.cpp` | `applyHighDpiPolicy()` 与 `apply(QApplication&)`：Fusion 风格、显式浅色 `QPalette`（颜色取自 `ui::palette`）、微软雅黑字体、加载 `:/styles/app.qss`、安装 `qtbase_zh_CN` 翻译（让标准对话框显示中文）。 |
| `MainWindow.h` / `MainWindow.cpp` | 导航壳：左侧浅色 `QTreeWidget` 侧栏（objectName `sidebar` / `navTree`，标题 `appTitle`、副标题 `appSubtitle`、学生信息块 `studentName` / `studentGrade` / `studentGoal`、版本 `sidebarMeta`）——顶层类别「档案」「考试」「设置」，类别行加粗且不可选中，点击展开/收起；子页面挂到 `QStackedWidget`；`Ctrl+1…6` 页面快捷键；状态栏显示 schema 版本与数据库路径；保存/恢复窗口几何与最后页面；首次启动提示补全档案。`refreshStudentHeader()` 从档案行读取姓名与目标院校（显示为“目标：…”）并用 `resolveGrade()` 推算年级，建库、保存档案与窗口显示时都会刷新；目标为空时隐藏 `studentGoal` 行。 |

### 3.4 `src/core/` — 与界面无关的基础能力

| 文件 | 职责 |
| --- | --- |
| `Result.h` | `Error`（消息 + 上下文）与 `Result<T>` / `Result<void>`（C++17 版 expected），所有可失败 API 的统一返回类型。 |
| `Models.h` / `Models.cpp` | 领域模型：`ExamType`（OI/文化课）及存储字符串互转、`StudentProfile`（姓名、`stage`、`enrollmentYear`、`targetSchool` 目标院校——存于遗留 `note` 列；**年级不入库**）、`Subject`、`ExamRecord`（分数以**百分之一分**的整数保存）。 |
| `SchoolStage.h` / `SchoolStage.cpp` | 学段与年级推算：`SchoolStage`（`Primary`/`Junior`/`Senior`）与存储串互转、`schoolYearStart()`（9 月 1 日换学年）、`resolveGrade(entryStage, enrollmentYear, today)` 返回 `GradeInfo{known, graduated, stage, grade}` 与展示文案 `初中·2年级` / `已毕业` / `未填写学段`。纯函数，可独立单测。 |
| `ScoreValue.h` / `ScoreValue.cpp` | 分数格式化与解析：`formatPoints`、`parsePoints`（最多两位小数）、`formatRate`、`rateOf`，界面与 CSV 共用。 |
| `TimeUtils.h` / `TimeUtils.cpp` | 时间工具：UTC ISO-8601 ↔ `QDateTime`、本地时间格式化、当前时间。 |
| `ScoreSummary.h` / `ScoreSummary.cpp` | 纯聚合逻辑：`DateRange`、`resolveTimeRange`（含“本学期”跨年规则）、`AggregateBucket`，`buildScoreSummary()` 按科目与类型分别统计（OI 与文化课不混算）。可独立单测。 |
| `CsvExport.h` / `CsvExport.cpp` | RFC 4180 转义、行拼接，写文件时加 UTF-8 BOM（Excel 直接打开中文不乱码）。 |
| `AppPaths.h` / `AppPaths.cpp` | 数据目录解析（支持 `CUTEE_DATA_DIR` 覆盖）、`cutee.db`、`logs/cutee.log`、`backups/` 路径。 |
| `Logging.h` / `Logging.cpp` | 安装 Qt 消息处理器、写滚动日志文件、日志级别读写、读取日志尾部（日志页使用）。 |

### 3.5 `src/storage/` — SQLite 与迁移

| 文件 | 职责 |
| --- | --- |
| `Database.h` / `Database.cpp` | 拥有一个命名连接：打开、PRAGMA（外键/WAL/同步/超时）、按版本顺序执行迁移（事务内，失败回滚）、查询当前 schema 版本。工作线程可创建自己的 `Database` 实例。 |
| `DataMaintenance.h` / `DataMaintenance.cpp` | 数据维护：`exportSnapshot()`（`VACUUM INTO` 一致性快照）、`createBackup()`（写到 `backups/`）、`inspectDatabaseFile()`（校验是否 CUTEE 库并返回 schema 版本）、`importDatabaseFile()`（备份 → 关闭连接 → 替换文件 → 重开 → 迁移，失败自动还原）、`resetAllData()`（清空三张数据表并重置自增，保留 `settings` 与 schema）。**界面绝不直接操作 SQLite 文件**，一律走这里。 |
| `Migrations.h` / `Migrations.cpp` | 迁移列表（v2 建表：`student_profile`、`subjects`、`exam_records`、`settings` 及索引；v3 给 `student_profile` 追加 `stage` / `enrollment_year`）与 `targetSchemaVersion()`。**旧版多学生表与旧数据迁移已移除**，新建库不会创建旧表。`student_no` / `school_class` 已停止读写，仅作为历史列保留。 |
| `DbUtils.h` / `DbUtils.cpp` | `db::execute()`：把驱动错误转成 `Error`；错误文本小工具。 |
| `SettingsRepository.h` / `SettingsRepository.cpp` | `settings` 键值表读写；`settings::kLogLevel`、`kLastPage`、`kWindowGeometry`、`kWindowState` 等键名常量。 |
| `StudentProfileRepository.h` / `StudentProfileRepository.cpp` | 单行学生档案（`id = 1`）的读取、存在性判断、首次插入 / 后续更新（不提供新增第二个学生的 API）。 |
| `SubjectRepository.h` / `SubjectRepository.cpp` | 科目 CRUD、按类型/启用状态列出、类型不可变校验、同类型重名保护、排序移动、启用/停用、单条与批量（`referenceCounts()`，`GROUP BY`）成绩引用统计、删除时引用保护。 |
| `ExamRecordRepository.h` / `ExamRecordRepository.cpp` | 成绩 CRUD 与筛选查询（类型/科目/日期区间/关键字，默认按日期倒序），`listInRange()` 供汇总使用，JOIN 出科目名。 |

### 3.6 `src/service/` — 业务规则

| 文件 | 职责 |
| --- | --- |
| `ScoreService.h` / `ScoreService.cpp` | 成绩的校验（满分为正、分数范围、类型与科目一致、停用科目不可新增、排名/人数约束、得分超满分给出警告）、增删改、列表查询、科目下拉数据、汇总（`summarize`）、明细与汇总的 CSV 生成。**界面唯一的成绩入口**。 |

### 3.7 `src/ui/` — 页面与可复用构件

| 文件 | 职责 |
| --- | --- |
| `UiKit.h` / `UiKit.cpp` | 公共构件：`makePageLayout`（统一 24px 边距）、`makePageTitle` / `makePageDescription` / `makeSectionTitle` / `makeHintLabel` / `makeStatusLabel` / `makeErrorLabel`、`makeFilterBar`、`makeButton` / `makePrimaryButton` / `makeDangerButton`、`applyRole` / `clearRole`（切换 QSS 角色属性）、`confirmDanger`（中文二次确认）。 |
| `DangerConfirmDialog.h` / `DangerConfirmDialog.cpp` | 破坏性操作的**二级验证窗口**：`DangerPrompt{title, heading, bullets, keyword, acceptText, hint}` + `DangerConfirmDialog`（确认按钮在输入精确关键字前始终禁用；未匹配时 Enter 落在「取消」）+ 便捷函数 `confirmTyped()`。导入与重置必须用它。 |
| `Palette.h` | 五色配色与文本/表面色的**唯一代码来源**（`#C4F2C8` 薄荷绿、`#E6DAF7` 淡紫、`#E7EBC3` 黄绿、`#D8DDF0` 蓝紫、`#EDD1D1` 淡粉，以及对应的深色文本、中性表面与浅色侧栏色 `kSidebar*`）；图表色 `kChartLine` / `kChartPoint` / `kChartTrack` / `kChartGrid` 也在这里。必须与 `app.qss` 保持一致。 |
| `TagDelegate.h` | `QStyledItemDelegate`：把单元格文本画成圆角标签（背景取 `Qt::BackgroundRole`，文字取 `Qt::ForegroundRole`），用于科目的“类型/状态”列。 |
| `ScoreTableModel.h` / `ScoreTableModel.cpp` | `ScoreTableModel`（10 列只读模型：展示、对齐、前景色、排序角色、搜索串、记录 id）+ `ScoreFilterProxyModel`（类型/科目/日期区间/关键字筛选，数值排序）。位于 `cutee_models` 静态库，可被测试直接链接。 |
| `ScorePage.h` / `ScorePage.cpp` | 成绩管理页：`QTableView` + model/proxy；类型/科目/日期/关键字筛选（关键字 180ms 防抖，只更新 proxy，不查库、不重建页面）；增删改、CSV 导出、两种空状态、行内得分率高亮；整表查询在 `QtConcurrent` 工作线程执行；「列」菜单显示/隐藏列并持久化到 `settings`。 |
| `ScoreDialog.h` / `ScoreDialog.cpp` | 新增/编辑成绩对话框：科目随类型联动、停用科目仅对历史记录可见、字段就近校验（红框 + 内联错误）、得分高于满分时保存前确认。 |
| `SubjectPage.h` / `SubjectPage.cpp` | 科目管理页：列表（彩色标签）、类型/停用筛选、增删改、排序移动、启用/停用、引用数统计、被引用科目禁用删除并提示。 |
| `SubjectDialog.h` / `SubjectDialog.cpp` | 新增/编辑科目对话框：名称、类型（仅新建可选）、排序、启用、备注；类型创建后不可改的提示。 |
| `SummaryPage.h` / `SummaryPage.cpp` | 成绩汇总页：时间范围筛选（全部/本学期/自定义）；页内 `QTabWidget` 分「汇总表」「成绩趋势」；统计在 `QtConcurrent` 工作线程执行（独立数据库连接），`QFutureWatcher` 回主线程；按科目与按类型两张表；平均得分率自绘条形（`RateBarDelegate`）；区分“无任何成绩”和“所选范围无成绩”；导出汇总 CSV。 |
| `TrendChart.h` / `TrendChart.cpp` | Qt Charts 成绩趋势：科目下拉 + `QChartView`（`QLineSeries` + `QScatterSeries`，横轴日期、纵轴 0–100% 得分率）；数据由汇总线程一次取回，切换科目不查库；无数据时显示空状态。 |
| `ProfilePage.h` / `ProfilePage.cpp` | 学生档案页：唯一档案的表单（姓名必填、所在学段 `profileStage`、入学年份 `profileEnrollmentYear` 微调框、目标院校 `profileGoal` 单行输入）；表单下方 `profileGradeHint` 用 `resolveGrade()` 实时显示推算年级；三态操作按钮——内容与库一致时显示灰色 `已保存`（禁用、隐藏撤销）、有改动时显示浅绿 `保存修改` + `撤销更改`、刚保存后显示灰色 `已保存` + `撤销修改`（回填保存前的值，需再次保存才落库）；首次引导横幅、就地校验、创建/更新时间提示。用 `textChanged` / `currentIndexChanged` / `valueChanged` + `m_updatingForm` 判断脏状态，保证中文输入法提交也能触发。 |
| `SettingsPage.h` / `SettingsPage.cpp` | 设置页：数据目录/数据库/日志文件路径与打开按钮、日志级别保存（复用档案页的保存按钮三态约定）、**数据备份与恢复**（导出/导入 `.db`、重置数据，导入与重置走 `DangerConfirmDialog` 二级验证并自动备份）、关于与备份说明（含 schema 版本）。数据变更后发 `dataReplaced()`，由 `MainWindow` 刷新所有页面。 |
| `LogPage.h` / `LogPage.cpp` | 日志页：只读等宽文本、手动刷新、可选 2 秒自动刷新（离开页面即停止）、打开日志目录。 |

### 3.8 `tests/`

| 文件 | 职责 |
| --- | --- |
| `tests/CMakeLists.txt` | `cutee_add_test()` 帮助函数：`qt_add_executable` + 链接 `cutee_core`、`cutee_models`、`Qt6::Test`，设置 `QT_QPA_PLATFORM=offscreen` 与独立 `CUTEE_DATA_DIR`。 |
| `tst_database.cpp` | 迁移建表与幂等性（断言旧表不再被创建）、v3 新列存在、旧版本号 v1 库升级到当前 schema、v3 之前的档案行仍可读取（学年份为 0）、`settings` 往返、档案单行 upsert、外键约束。 |
| `tst_school_stage.cpp` | 年级推算：学段存储串往返、学年起点（9 月 1 日）、学段内升级、跨学段升学（初中 3 → 高中 1、小学 6 → 初中 1）、满 12 年毕业、未填入学年份与未来年份的降级处理。 |
| `tst_scores.cpp` | 科目 CRUD/重名保护/类型不可变/引用保护/排序/启用停用；成绩校验（满分 0、负分、类型不一致、停用科目、排名人数、超满分警告）、CRUD 与筛选；分数解析与 CSV 转义。 |
| `tst_summary.cpp` | 按科目/类型/时间汇总、本学期边界、最近一次成绩选择。 |
| `tst_score_model.cpp` | 表格模型行列/表头、展示与对齐、前景色（<60% 警示、超满分错误色）、数值排序角色、proxy 筛选（类型/科目/关键字/日期）与排序、模型重置。 |
| `tst_profile_page.cpp` | 档案页三态按钮：首次启动的建档按钮、无改动时灰色 `已保存` 且隐藏撤销、改动学段后浅绿 `保存修改` + `撤销更改`、保存后 `撤销修改` 回填保存前的值且不落库、撤销更改不写库、年级提示随学段变化、目标院校往返一致。注意 `QTest::keyClicks` 只支持 ASCII，中文输入用 `setText()`（同样触发 `textChanged`）。 |
| `tst_settings_page.cpp` | 设置页三态按钮：默认灰色 `已保存` 且隐藏撤销、切换日志级别后浅绿 `保存设置` + `撤销更改`、保存后写库并切到 `撤销修改`、撤销修改回填上一级别但库中仍是新值、撤销更改不写库。 |
| `tst_trend_chart.cpp` | 趋势图数据契约：每条记录生成一个折线/散点数据点、无数据时切到空状态并清空 series、切换数据后重建坐标轴。 |
| `tst_data_maintenance.cpp` | 数据维护：导出快照可独立打开且内容一致（并允许覆盖已有导出）、拒绝非 SQLite / 非 CUTEE / schema 过新的导入文件、导入后活动库为新数据且留下导入前备份、不残留 `cutee-import-*` 临时文件、拒绝导入正在使用的库、重置清空三表但保留 `settings` 与 schema。 |
| `tst_danger_dialog.cpp` | 二级验证窗口：确认按钮初始禁用、输入精确关键字后启用、首尾空格被忽略、错误关键字保持禁用、`isDefault` 随匹配状态在「取消」与确认按钮之间切换。 |

---

## 4. 配色与样式约定

| 颜色 | 色值 | 用途 |
| --- | --- | --- |
| 薄荷绿 | `#C4F2C8` | 成功、启用、空状态底色、`保存修改` 按钮 |
| 淡紫 | `#E6DAF7` | 主操作按钮、表格与导航选中态 |
| 黄绿 | `#E7EBC3` | 警示、低得分率、文化课标签 |
| 蓝紫 | `#D8DDF0` | 表头、信息提示块 |
| 淡粉 | `#EDD1D1` | 危险操作、错误、得分超满分 |

按钮角色补充：`save`（浅绿 `#C4F2C8`，用于“保存修改”）与 `saved`（中性灰 `#D5D5D5`，用于“已保存”状态胶囊）。

配套深色文本：`#2F6B3D`（绿）、`#4A3A6B`（紫）、`#6E6420`（黄绿）、`#3D4876`（蓝紫）、`#8B3E3E`（粉），
中性色 `#2F2C36` / `#6A6573` / `#9A94A3`，画布 `#F7F6FA`，边框 `#DCD7E6`。

**侧栏同样是浅色**，与主界面统一：

| 角色 | 色值 | 说明 |
| --- | --- | --- |
| 侧栏底色 | `#EDE9F7` | 比画布深一档的淡紫，靠右侧 `1px #DFD8EE` 分隔线与内容区区分 |
| 侧栏悬停 | `#E6E0F3` | 导航项悬停 |
| 侧栏选中 | `#E6DAF7` | 导航项当前页，配深紫文字 `#4A3A6B` 并加粗 |
| 侧栏标题 | `#2F2C36` / `#6A6573` / `#9A94A3` | 应用名 / 副标题 / 版本号，objectName 分别为 `appTitle`、`appSubtitle`、`sidebarMeta` |

深色只保留在提示气泡等反色表面（`Palette::kInk` = `#2E2A3A`），界面主体不再出现深色面板。

改配色的正确做法：

1. 改 `src/ui/Palette.h`（模型/委托里用到的颜色）。
2. 改 `resources/styles/app.qss` 顶部注释与对应规则。
3. 若涉及图标，同步 `resources/icons/*.svg` 的 `stroke`。
4. 更新 `tests/tst_score_model.cpp` 中的期望色值，运行 `ctest`。

---

## 5. 保存类按钮的三态约定

任何"编辑表单 + 保存"的页面（当前为 `ProfilePage`、`SettingsPage`）都必须服从同一套按钮状态机，
不要各自发明保存按钮的文案与配色。

| 状态 | 判定 | 主按钮 | 撤销按钮 |
| --- | --- | --- | --- |
| 干净（未修改） | 表单值与库中已加载值相同 | 灰色 `已保存`：`uiRole="saved"`、`setEnabled(false)`、tooltip 说明内容已一致 | 隐藏；**仅当**本次会话刚保存过改动时显示 `撤销修改` |
| 脏（有改动） | 表单值与已加载值不同 | 浅绿 `保存修改`（档案页）/ `保存设置`（设置页）：`uiRole="save"`、可点击 | 显示 `撤销更改`，回填已加载值，不写库 |
| 刚保存后 | 保存成功且值确实变化 | 回到灰色 `已保存` | 显示 `撤销修改`，回填本次保存前的快照，不写库 |

实现要点（两个页面结构一致，改一处记得同步另一处）：

- 成员：`m_loaded`（或 `m_loadedLevel`）存库中的值，`m_previous`（或 `m_previousLevel`）存保存前快照，
  `m_canUndoLastSave` 只在"原本已有内容 → 修改 → 保存成功"时为 true（首次创建不可撤销）。
- 单一入口 `updateActionState()`：所有状态变更（表单编辑、保存、撤销、重新加载）都只调它，
  它负责文案、`uiRole`、enable/disable 与撤销按钮显隐；不要在别处零散改按钮。
- 脏检测必须用 `QLineEdit::textChanged` / `QPlainTextEdit::textChanged` / `QComboBox::currentIndexChanged`
  **加上** `m_updatingForm` 守卫，不能用 `textEdited`（中文输入法提交不触发）。
- "撤销更改"与"撤销修改"共用一个按钮，点击时按当前状态分流：
  脏 → 回填已加载值；干净且 `m_canUndoLastSave` → 回填保存前快照。
- 撤销只改界面，不写库；必须再点一次保存才会持久化。
- 不额外加"已保存"文字提示，状态由按钮胶囊本身表达，反馈标签只用于错误。

---

## 6. 常见需求的落点

| 需求 | 需要改的文件 |
| --- | --- |
| 新增一个页面 | `src/ui/XxxPage.{h,cpp}`（用 `ui::makePageLayout`）→ `CMakeLists.txt` → `MainWindow::buildUi()` 的 `addPage(id, page)` → 在 `buildSidebar()` 的对应分组下 `makeLeaf(...)` 注册同一个 id |
| 新增一个页面的单元测试 | 参考 `tests/tst_profile_page.cpp`：页面源码直接编进测试目标（见 `tests/CMakeLists.txt`），用 objectName 查控件 |
| 按钮出现新状态 | 先在 `app.qss` 加 `QPushButton[uiRole="xxx"]` 变体（含 `:hover`、`:disabled`），再用 `ui::applyRole()` 切换 |
| 新增"表单+保存"页面 | 复制 `SettingsPage` 的 `m_loaded` / `m_previous` / `m_canUndoLastSave` / `updateActionState()` 结构（见第 5 节），并补一个页面级测试 |
| 成绩表新增一列 | `ScoreTableModel`（`Column` 枚举 + `data` + `headerData` + `SortValueRole`）→ `ScorePage` 列宽/委托 → `tests/tst_score_model.cpp`；列显隐菜单会自动多出一项 |
| 新增筛选条件 | `ScoreFilterProxyModel`（setter + `filterAcceptsRow`）→ `ScorePage::buildFilterBar/applyFilters` → 模型测试 |
| 新增页面级快捷键 | `new QShortcut(seq, this)` 后**必须** `setContext(Qt::WidgetWithChildrenShortcut)`；表格内快捷键把 parent 设为表格并使用 `Qt::WidgetShortcut` |
| 修改趋势图 | `TrendChart::updateChart()`（series/坐标轴在此重建）+ `tst_trend_chart.cpp`；颜色取 `palette::kChart*` |
| 改表结构 | 在 `Migrations.cpp` **追加**新版本条目（不要改已有条目）→ 相关 Repository → `tst_database.cpp` |
| 调整年级推算或档案字段 | `core/SchoolStage.{h,cpp}`（纯逻辑 + 单测）→ 如新增字段则追加迁移 → `StudentProfileRepository` → `ProfilePage` 表单 + `MainWindow::refreshStudentHeader()` |
| 新增破坏性数据操作 | 用 `ui::confirmTyped()`（`DangerConfirmDialog`）做二级验证 → `storage/DataMaintenance` 里加纯逻辑函数 → `tst_data_maintenance.cpp`；界面只允许调用 maintenance，不直接动文件 |
| 新增业务规则 | `ScoreService::validate` / 对应 Repository，测试写在 `tst_scores.cpp` |
| 调整视觉 | `app.qss` + `Palette.h`（+ `TagDelegate` 若是标签） |
| 增加耗时统计 | 参考 `SummaryPage` / `ScorePage`：`QtConcurrent::run` + 工作线程自建 `Database` + `QFutureWatcher` |

---

## 7. 不要做的事

- 不要在控件上写 `setStyleSheet`（全局 QSS + `uiRole` 已覆盖需求）。
- 不要在 UI 里拼 SQL，也不要绕过 `ScoreService` 直接改 `exam_records`。
- 不要用 `QTableWidget` 承载可能增长到千行的数据。
- 不要修改已发布的迁移条目；只追加新版本。
- 不要在 UI 线程做批量导入、复杂汇总等耗时工作。
- 不要引入 QML/WebEngine/Electron 或大型第三方 UI 库。
- 不要在页面级 `QShortcut` 使用默认的 `Qt::WindowShortcut`，否则多个页面同名快捷键会冲突。
- 不要在没有二次确认的情况下执行删除、停用等破坏性操作。
- 不要在界面里用 `QFile::copy` / `QFile::remove` 直接替换 `cutee.db`，也不要自己拼 `VACUUM INTO`；导入、导出、重置一律走 `storage/DataMaintenance`。
- 不要在"导入/重置"这类整库级破坏性操作上用普通 `QMessageBox` 确认，必须用 `ui::confirmTyped()` 输入关键字。
- 不要为 Linux/macOS 移植添加条件编译、跨平台抽象层或平台预设——短期只支持 Windows。

---

## 8. 导航、列配置与图表约定

- **侧栏**：`QTreeWidget#navTree`。类别行（`makeGroup`）加粗、`Qt::ItemIsSelectable` 被移除，点击只切换展开状态；叶子行（`makeLeaf`）用 `Qt::UserRole` 存页面 id，`QStackedWidget` 索引由 `addPage()` 注册。新增页面时两处 id 必须一致。
- **快捷键**：窗口级 `Ctrl+1…6` 在 `MainWindow::installShortcuts()` 注册；页面级快捷键见第 6 节。页面快捷键不要与窗口级冲突。
- **列配置**：键名为 `settings::kScoreHiddenColumns`，值为隐藏列的索引（逗号分隔）。读取时只影响 `QHeaderView`，不要动模型数据；改动后立即写 `settings`。
- **图表**：使用 Qt Charts（`QChartView`），不引入 QML。数据由汇总工作线程返回的 `SummaryOutcome::records` 提供，`TrendChart::setData` 只在 UI 线程重建 series/坐标轴；颜色来自 `palette::kChart*`。Qt Charts 采用 GPLv3/商业许可，发行前需确认许可策略。
- **学生信息与年级**：`student_profile` 只存 `stage` 与 `enrollment_year`，**年级永不入库**。任何需要显示年级的地方都调用 `core::resolveGrade(stage, enrollmentYear, QDate::currentDate()).label()`；不要自己写“当前年份 - 入学年份”的算术，也不要在数据库里缓存年级。侧栏用 `MainWindow::refreshStudentHeader()`：建档、保存档案、窗口 `showEvent` 时刷新，未建档时整块隐藏；目标院校显示为灰色小字“目标：…”，为空时隐藏该行（存于 `note` 列，界面字段名为“目标院校”）。学年切换点为 9 月 1 日（`kSchoolYearStartMonth`）。
- **整库替换**：`DataMaintenance::importDatabaseFile()` 会 `close()` 再 `open()` 同一个 `Database` 对象，所以各页面持有的 `Database&` 引用依然有效；替换后必须由 `MainWindow::reloadAllPages()` 重新拉取数据（设置页发 `dataReplaced()`）。导入校验在**副本**上做，绝不迁移或改写用户选择的原始文件。
