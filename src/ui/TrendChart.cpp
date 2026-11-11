#include "ui/TrendChart.h"

#include "core/ScoreSummary.h"
#include "core/ScoreValue.h"
#include "ui/Palette.h"
#include "ui/UiKit.h"

#include <QComboBox>
#include <QDateTime>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QStackedWidget>
#include <QTime>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>

#include <QtGraphs/QDateTimeAxis>
#include <QtGraphs/QLineSeries>
#include <QtGraphs/QScatterSeries>
#include <QtGraphs/QValueAxis>
#include <QtGraphs/QXYSeries>

#include <algorithm>

namespace cutee {
namespace {

// Qt Graphs ships no C++ widget for 2D charts (only private QQuickItem based
// GraphsView), so the structure lives in QML while the data and axis ranges are
// pushed from C++ to keep the page free of QML property plumbing.
const char *const kTrendChartQml = R"QML(
import QtQuick
import QtGraphs

GraphsView {
    id: view
    anchors.fill: parent
    marginTop: 14
    marginBottom: 6
    marginLeft: 6
    marginRight: 14

    theme: GraphsTheme {
        colorScheme: GraphsTheme.ColorScheme.Light
        theme: GraphsTheme.Theme.UserDefined
        backgroundColor: chartColors.surface
        backgroundVisible: true
        plotAreaBackgroundVisible: false
        labelTextColor: chartColors.text
        labelBackgroundVisible: false
        labelBorderVisible: false
        grid.mainColor: chartColors.grid
        grid.subColor: "transparent"
        axisX.mainColor: chartColors.border
        axisY.mainColor: chartColors.border
    }

    axisX: DateTimeAxis {
        objectName: "trendAxisX"
        labelFormat: "MM-dd"
    }

    axisY: ValueAxis {
        objectName: "trendAxisY"
        min: 0
        max: 100
        tickInterval: 20
        labelDecimals: 0
    }

    LineSeries {
        objectName: "trendLine"
        width: 2
        axisX: view.axisX
        axisY: view.axisY
    }

    ScatterSeries {
        objectName: "trendDots"
        axisX: view.axisX
        axisY: view.axisY
        pointDelegate: Rectangle {
            width: 9
            height: 9
            radius: 4.5
            color: chartColors.point
            border.width: 1
            border.color: chartColors.line
        }
    }
}
)QML";

} // namespace

TrendChart::TrendChart(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *bar = new QHBoxLayout();
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(8);
    bar->addWidget(new QLabel(QStringLiteral("科目"), this));

    m_subject = new QComboBox(this);
    m_subject->setObjectName(QStringLiteral("trendSubject"));
    m_subject->setMinimumWidth(200);
    bar->addWidget(m_subject);
    bar->addStretch(1);
    bar->addWidget(ui::makeHintLabel(
        QStringLiteral("横轴为考试日期、纵轴为得分率。"), this));
    layout->addLayout(bar);

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("trendStack"));

    m_chartHost = new QWidget(m_stack);
    auto *chartLayout = new QVBoxLayout(m_chartHost);
    chartLayout->setContentsMargins(0, 0, 0, 0);
    chartLayout->setSpacing(6);

    m_chartTitle = new QLabel(m_chartHost);
    m_chartTitle->setObjectName(QStringLiteral("trendTitle"));
    QFont titleFont = font();
    titleFont.setBold(true);
    m_chartTitle->setFont(titleFont);
    chartLayout->addWidget(m_chartTitle);

    m_chartArea = new QWidget(m_chartHost);
    m_chartArea->setObjectName(QStringLiteral("trendChartArea"));
    m_chartArea->setMinimumHeight(260);
    auto *chartAreaLayout = new QVBoxLayout(m_chartArea);
    chartAreaLayout->setContentsMargins(0, 0, 0, 0);
    chartLayout->addWidget(m_chartArea, 1);

    m_stack->addWidget(m_chartHost);

    auto *empty = new QWidget(m_stack);
    empty->setObjectName(QStringLiteral("emptyState"));
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(32, 32, 32, 32);
    emptyLayout->addStretch(1);
    m_emptyHint = ui::makePageDescription(QString(), empty);
    m_emptyHint->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyHint);
    emptyLayout->addStretch(1);
    m_stack->addWidget(empty);

    layout->addWidget(m_stack, 1);

    connect(m_subject, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this] { updateChart(); });

    clear();
}

void TrendChart::clear()
{
    m_records.clear();
    m_subjects.clear();
    m_points.clear();
    m_title.clear();
    m_subject->blockSignals(true);
    m_subject->clear();
    m_subject->blockSignals(false);
    m_chartTitle->clear();
    if (m_view)
        applyToChart();
    m_emptyHint->setText(QStringLiteral("当前时间范围内没有可绘制的成绩。"));
    m_stack->setCurrentIndex(1);
}

void TrendChart::setData(const QVector<ExamRecord> &records,
                         const QVector<SubjectAggregate> &subjects)
{
    m_records = records;
    m_subjects = subjects;
    updateSubjectChoices();
    updateChart();
}

void TrendChart::showEvent(QShowEvent *event)
{
    ensureChartLoaded();
    QWidget::showEvent(event);
}

void TrendChart::updateSubjectChoices()
{
    const qint64 currentId =
        m_subject->currentData().isValid() ? m_subject->currentData().toLongLong() : 0;

    m_subject->blockSignals(true);
    m_subject->clear();
    for (const SubjectAggregate &subject : m_subjects)
        m_subject->addItem(subject.subjectName, subject.subjectId);
    const int index = m_subject->findData(currentId);
    m_subject->setCurrentIndex(index >= 0 ? index : (m_subjects.isEmpty() ? -1 : 0));
    m_subject->blockSignals(false);
}

void TrendChart::updateChart()
{
    m_points.clear();
    m_title.clear();

    if (m_subject->currentIndex() < 0) {
        m_chartTitle->clear();
        if (m_view)
            applyToChart();
        m_emptyHint->setText(QStringLiteral("当前时间范围内没有可绘制的成绩。"));
        m_stack->setCurrentIndex(1);
        return;
    }

    const qint64 subjectId = m_subject->currentData().toLongLong();
    QVector<ExamRecord> records;
    records.reserve(m_records.size());
    for (const ExamRecord &record : m_records) {
        if (record.subjectId == subjectId)
            records.append(record);
    }

    if (records.isEmpty()) {
        m_chartTitle->clear();
        if (m_view)
            applyToChart();
        m_emptyHint->setText(QStringLiteral("这个科目在当前时间范围内没有成绩记录。"));
        m_stack->setCurrentIndex(1);
        return;
    }

    std::sort(records.begin(), records.end(), [](const ExamRecord &a, const ExamRecord &b) {
        if (a.examDate != b.examDate)
            return a.examDate < b.examDate;
        return a.id < b.id;
    });

    m_points.reserve(records.size());
    for (const ExamRecord &record : records) {
        TrendPoint point;
        point.date = record.examDate;
        point.ratePercent =
            scoreutil::rateOf(record.scoreHundredths, record.fullScoreHundredths) * 100.0;
        m_points.append(point);
    }

    const SubjectAggregate *selected = nullptr;
    for (const SubjectAggregate &subject : m_subjects) {
        if (subject.subjectId == subjectId) {
            selected = &subject;
            break;
        }
    }
    const QString subjectName = m_subject->currentText();
    const QString typeName = selected ? examTypeDisplayName(selected->type) : QString();
    m_title = typeName.isEmpty() ? subjectName
                                 : QStringLiteral("%1 · %2").arg(subjectName, typeName);
    m_chartTitle->setText(m_title);

    if (m_view)
        applyToChart();
    m_stack->setCurrentIndex(0);
}

void TrendChart::ensureChartLoaded()
{
    if (m_view || !m_chartArea)
        return;

    m_view = new QQuickWidget(m_chartArea);
    m_view->setObjectName(QStringLiteral("trendGraphsView"));
    m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_view->setClearColor(palette::color(palette::kSurface));
    m_chartArea->layout()->addWidget(m_view);

    QVariantMap colors;
    colors.insert(QStringLiteral("surface"), palette::color(palette::kSurface));
    colors.insert(QStringLiteral("text"), palette::color(palette::kTextSecondary));
    colors.insert(QStringLiteral("grid"), palette::color(palette::kChartGrid));
    colors.insert(QStringLiteral("border"), palette::color(palette::kBorder));
    colors.insert(QStringLiteral("line"), palette::color(palette::kChartLine));
    colors.insert(QStringLiteral("point"), palette::color(palette::kChartPoint));
    m_view->rootContext()->setContextProperty(QStringLiteral("chartColors"), colors);

    QQmlComponent *component = new QQmlComponent(m_view->engine(), m_view);
    component->setData(QByteArray(kTrendChartQml), QUrl());
    QObject *rootObject = component->isReady() ? component->create(m_view->rootContext()) : nullptr;
    if (!rootObject) {
        qWarning("TrendChart: Qt Graphs view could not be created: %s%s",
                 qPrintable(component->errorString()),
                 qPrintable(component->errors().isEmpty()
                                ? QString()
                                : QStringLiteral(" / ")
                                      + component->errors().first().toString()));
        delete component;
        delete m_view;
        m_view = nullptr;
        return;
    }
    m_component = component;
    m_view->setContent(QUrl(), m_component, rootObject);
    applyToChart();
}

void TrendChart::applyToChart()
{
    if (!m_view)
        return;
    QObject *root = static_cast<QObject *>(m_view->rootObject());
    if (!root)
        return;

    auto *axisX = root->findChild<QDateTimeAxis *>(QStringLiteral("trendAxisX"));
    auto *axisY = root->findChild<QValueAxis *>(QStringLiteral("trendAxisY"));
    auto *line = root->findChild<QLineSeries *>(QStringLiteral("trendLine"));
    auto *dots = root->findChild<QScatterSeries *>(QStringLiteral("trendDots"));

    if (line)
        line->setColor(palette::color(palette::kChartLine));
    if (dots)
        dots->setColor(palette::color(palette::kChartPoint));

    if (m_points.isEmpty()) {
        if (line)
            line->clear();
        if (dots)
            dots->clear();
        return;
    }

    QList<QPointF> plot;
    plot.reserve(m_points.size());
    for (const TrendPoint &point : m_points) {
        const qreal x = QDateTime(point.date, QTime(12, 0)).toMSecsSinceEpoch();
        plot.append(QPointF(x, point.ratePercent));
    }

    if (line)
        line->replace(plot);
    if (dots)
        dots->replace(plot);

    const QDate firstDate = m_points.first().date;
    const QDate lastDate = m_points.last().date;
    if (axisX) {
        axisX->setMin(QDateTime(firstDate, QTime(0, 0)));
        axisX->setMax(QDateTime(lastDate.addDays(1), QTime(0, 0)));
    }
    if (axisY) {
        axisY->setMin(0.0);
        axisY->setMax(100.0);
    }
}

} // namespace cutee
