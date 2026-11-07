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
resources/
  cutee.qrc                     Qt 资源清单（样式表与图标）
  styles/app.qss                唯一的全局样式表（浅色主题）
  icons/*.svg                   表头排序箭头等小图标
src/
  app/       main.cpp、MainWindow（导航壳）、Theme（Fusion + 调色板 + QSS + 高 DPI）
  core/      模型、时间/分数工具、汇总计算、CSV
  storage/   Database、迁移、各 Repository、设置
  service/   ScoreService（校验/汇总/CSV）、LegacyDataService
  ui/        UiKit（页面标题/按钮/筛选栏等公共构件）、ScoreTableModel（表格模型 + 过滤代理）、
             学生档案、科目、成绩、汇总、设置、日志页面
tests/       tst_database、tst_scores、tst_summary、tst_score_model
```

UI 不直接拼 SQL；校验与汇总逻辑集中在 `ScoreService` 与 `core/ScoreSummary`，便于单元测试。

## 界面结构与样式

- **主窗口**：左侧 `QListWidget` 导航 + `QStackedWidget` 页面区，默认尺寸 1240×800，最小尺寸 1024×680；窗口几何与最后停留页面保存在 `settings` 表。
- **主题**：`main()` 依次调用 `cutee::theme::applyHighDpiPolicy()`（在构造 `QApplication` 之前）与 `cutee::theme::apply(app)`。后者设置 Fusion 风格、显式浅色 `QPalette`、微软雅黑字体，并加载 `:/styles/app.qss`。**当前只提供浅色主题**，不随系统深色模式切换，从而避免白底白字。
- **样式集中管理**：所有颜色、圆角、表头、选中态、禁用态、滚动条都在 `resources/styles/app.qss`。页面不再散落 `setStyleSheet`，控件通过动态属性 `uiRole`（`primary`、`danger`、`hint`、`status`、`error`、`success`、`warning`、`banner`、`invalid`、`section`）选择变体，改动样式只需编辑 qss。
- **公共构件**：`src/ui/UiKit.h` 提供 `makePageLayout`、`makePageTitle`、`makePageDescription`、`makeSectionTitle`、`makeHintLabel`、`makeStatusLabel`、`makeErrorLabel`、`makeFilterBar` 以及带角色的按钮工厂，保证各页面边距（24px）与控件间距一致。
- **成绩表**：`QTableView` + `ScoreTableModel`（`src/ui/ScoreTableModel.*`）+ `ScoreFilterProxyModel`。筛选（类型/科目/日期/关键字）与排序全部在 proxy 内完成，**不会重新查询数据库，也不会重建控件**；关键字输入有 180ms 防抖。得分率低于 60% 的单元格以警示色显示，得分高于满分的单元格以错误色显示。
- **汇总页**：统计在 `QtConcurrent` 工作线程执行，工作线程使用独立 SQLite 连接（WAL 支持并发读）；结果通过 `QFutureWatcher` 回到 UI 线程。平均得分率用轻量自绘条形展示，未引入 Qt Charts。
- **空状态**：成绩页区分「还没有成绩记录」与「没有符合筛选条件的成绩」，后者提供“清除筛选条件”；汇总页区分「还没有成绩记录」与「所选时间范围内没有成绩」。

## 交互约定

- 删除成绩、删除科目、停用科目、清理旧数据都会二次确认；确认框使用中文动作按钮（删除/停用/继续），默认焦点在“取消”。被成绩引用的科目删除按钮直接禁用并给出提示，只能停用。
- 成绩编辑对话框采用就近校验：必填项/分数不合法时在对话框内显示红色错误与红框，不再弹出消息框；仅“得分高于满分”在保存时弹一次确认。
- 学生档案保存成功在按钮旁显示绿色“已保存”；读取/保存失败使用就近错误提示。
- 标准对话框（文件选择、文本输入）通过 Qt 自带的 `qtbase_zh_CN` 翻译显示中文。
- 所有页面使用布局管理器，无 `setGeometry` 绝对定位。

## 手工验收清单

1. 启动应用：左侧导航为深色、当前页高亮，页面背景浅灰、表格白底，字体统一（微软雅黑）。
2. 在 100% 与 150% 缩放（或 `QT_SCALE_FACTOR=1.5`）下检查：窗口可缩放、无控件重叠或截断，表格与表单自适应；最小尺寸 1024×680 下筛选栏不重叠。
3. 成绩管理：录入 1000+ 条数据后滚动、点击表头排序、切换类型/科目/日期/关键字筛选，均应即时响应且不重建页面；得分率 <60% 显示警示色。
4. 成绩新增/编辑：清空考试名称或把满分改为 0，应立即出现红框与错误文字；保存“得分高于满分”时弹一次确认。
5. 删除成绩：弹出二次确认，动作为“删除/取消”，默认焦点在“取消”。
6. 科目管理：类型以颜色区分；选中被引用科目时“删除”为禁用并提示“只能停用”；停用后新成绩不再可选，但历史成绩仍可编辑。
7. 成绩汇总：切换全部时间/本学期/自定义，统计随之刷新；平均得分率显示条形；无数据时显示空状态而不是空表。
8. 导出：成绩明细导出当前筛选结果，汇总导出当前时间范围；UTF-8 BOM，Excel 可直接打开。
9. 设置/日志：窗口默认浅色，日志为等宽字体；切换日志级别并保存后，按钮旁显示“已保存”。
10. 重启应用：窗口尺寸、最后页面保持不变。

## 性能设计

- 成绩表筛选/排序不触碰数据库：`reload()` 仅在进入页面、显式刷新或 CRUD 后重新查询一次，其余交互只更新 proxy。
- 关键指标（本机 MinGW Debug 构建，1500 条成绩）：查询 + 模型重置 + 视图刷新约 **12ms**；启动到主窗口可见约 **0.4s**；表头排序与筛选无可感知延迟。
- 汇总统计一律异步执行（即使耗时很短），避免数据量增大后阻塞界面；工作线程每次统计使用独立连接，结束后关闭。
- 科目页的“成绩引用”改为单条 `GROUP BY` 查询，不再按行 N+1 查询。

## 已知限制

- 仅维护一位学生，无法新增第二位；如需更换学生请直接编辑档案。
- 科目类型创建后不可修改（避免与已有成绩的 `exam_type` 冲突），请新建科目。
- 科目只有“停用”而没有级联删除：删除被引用的科目会被拒绝。
- 成绩导入（CSV）尚未实现，仅支持导出；`detail_json` 尚未提供 UI。
- 汇总中的“平均得分”“最高/最低分”按原始分数计算；同一科目满分不同的考试请结合“平均得分率”判断。
- 旧版考试数据不会自动转换，也未提供图形化迁移工具。
- 无网络、账号、云同步功能。
- 界面仅提供浅色主题；深色主题、自定义图表、列显隐配置属于后续可选项。

## 测试

```powershell
ctest --test-dir build-mingw --output-on-failure
```

覆盖内容：

- 迁移建表、幂等性、外键、旧学生档案采用、旧数据清理不影响新数据。
- 科目 CRUD、同类型重名保护、类型不可变、引用保护、排序与启用/停用。
- 成绩校验（满分为 0、负分、类型不一致、停用科目、排名/人数、得分高于满分的警告）、CRUD 与筛选。
- 分数解析/格式化、按科目/类型/时间汇总、本学期边界、CSV 字段与转义。
- 成绩表模型：行列与表头、展示/对齐/前景色（<60% 警示、得分超满分报错）、数值排序角色、proxy 的类型/科目/关键字/日期筛选与排序、模型重置。
