#include "ui/TrendChart.h"

#include "core/ScoreSummary.h"
#include "core/ScoreValue.h"
#include "ui/Palette.h"
#include "ui/UiKit.h"

#include <QComboBox>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineSeries>
#include <QPainter>
#include <QPen>
#include <QScatterSeries>
#include <QStackedWidget>
#include <QTime>
#include <QVBoxLayout>
#include <QValueAxis>

#include <QtCharts/QAbstractAxis>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>

#include <algorithm>

namespace cutee {
namespace {

QColor withAlpha(const QColor &color, int alpha)
{
    QColor copy = color;
    copy.setAlpha(alpha);
    return copy;
}

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
    m_subject->setMinimumWidth(200);
    bar->addWidget(m_subject);
    bar->addStretch(1);
    bar->addWidget(ui::makeHintLabel(
        QStringLiteral("横轴为考试日期、纵轴为得分率；悬停数据点可查看分数。"), this));
    layout->addLayout(bar);

    m_stack = new QStackedWidget(this);

    m_chart = new QChart();
    m_chart->setTheme(QChart::ChartThemeLight);
    m_chart->setBackgroundRoundness(8);
    m_chart->setAnimationOptions(QChart::NoAnimation);
    m_chart->setMargins(QMargins(8, 8, 8, 8));
    m_chart->legend()->hide();
    m_chart->setBackgroundBrush(QBrush(palette::color(palette::kSurface)));
    m_chart->setPlotAreaBackgroundVisible(false);

    m_view = new QChartView(m_chart, m_stack);
    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setMinimumHeight(260);
    m_stack->addWidget(m_view);

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
    m_subject->blockSignals(true);
    m_subject->clear();
    m_subject->blockSignals(false);
    m_chart->removeAllSeries();
    const auto axes = m_chart->axes();
    for (QAbstractAxis *axis : axes) {
        m_chart->removeAxis(axis);
        delete axis;
    }
    m_chart->setTitle(QString());
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
    m_chart->removeAllSeries();
    const auto axes = m_chart->axes();
    for (QAbstractAxis *axis : axes) {
        m_chart->removeAxis(axis);
        delete axis;
    }

    if (m_subject->currentIndex() < 0) {
        m_emptyHint->setText(QStringLiteral("当前时间范围内没有可绘制的成绩。"));
        m_stack->setCurrentIndex(1);
        return;
    }

    const qint64 subjectId = m_subject->currentData().toLongLong();
    QVector<ExamRecord> points;
    points.reserve(m_records.size());
    for (const ExamRecord &record : m_records) {
        if (record.subjectId == subjectId)
            points.append(record);
    }

    if (points.isEmpty()) {
        m_emptyHint->setText(QStringLiteral("这个科目在当前时间范围内没有成绩记录。"));
        m_stack->setCurrentIndex(1);
        return;
    }

    std::sort(points.begin(), points.end(), [](const ExamRecord &a, const ExamRecord &b) {
        if (a.examDate != b.examDate)
            return a.examDate < b.examDate;
        return a.id < b.id;
    });

    auto *line = new QLineSeries();
    QPen linePen(palette::color(palette::kChartLine));
    linePen.setWidth(2);
    line->setPen(linePen);

    auto *dots = new QScatterSeries();
    dots->setMarkerSize(9.0);
    dots->setColor(palette::color(palette::kChartPoint));
    dots->setBorderColor(palette::color(palette::kChartLine));

    QDate firstDate = points.first().examDate;
    QDate lastDate = points.last().examDate;

    for (const ExamRecord &record : points) {
        const double percent = scoreutil::rateOf(record.scoreHundredths,
                                                 record.fullScoreHundredths) *
            100.0;
        const qreal x = QDateTime(record.examDate, QTime(12, 0)).toMSecsSinceEpoch();
        line->append(x, percent);
        dots->append(x, percent);
    }

    auto *axisX = new QDateTimeAxis();
    axisX->setFormat(QStringLiteral("MM-dd"));
    axisX->setTickCount(5);
    axisX->setLabelsColor(palette::color(palette::kTextSecondary));
    axisX->setGridLineColor(withAlpha(palette::color(palette::kChartGrid), 180));
    axisX->setLinePenColor(palette::color(palette::kBorder));
    axisX->setRange(QDateTime(firstDate.addDays(-1), QTime(0, 0)),
                    QDateTime(lastDate.addDays(1), QTime(23, 59)));

    auto *axisY = new QValueAxis();
    axisY->setRange(0.0, 100.0);
    axisY->setTickCount(6);
    axisY->setLabelFormat(QStringLiteral("%d%%"));
    axisY->setLabelsColor(palette::color(palette::kTextSecondary));
    axisY->setGridLineColor(withAlpha(palette::color(palette::kChartGrid), 180));
    axisY->setLinePenColor(palette::color(palette::kBorder));

    m_chart->addSeries(line);
    m_chart->addSeries(dots);
    m_chart->addAxis(axisX, Qt::AlignBottom);
    m_chart->addAxis(axisY, Qt::AlignLeft);
    line->attachAxis(axisX);
    line->attachAxis(axisY);
    dots->attachAxis(axisX);
    dots->attachAxis(axisY);

    const SubjectAggregate *selected = nullptr;
    for (const SubjectAggregate &subject : m_subjects) {
        if (subject.subjectId == subjectId) {
            selected = &subject;
            break;
        }
    }
    const QString subjectName = m_subject->currentText();
    const QString typeName = selected ? examTypeDisplayName(selected->type) : QString();
    m_chart->setTitle(typeName.isEmpty() ? subjectName
                                         : QStringLiteral("%1 · %2").arg(subjectName, typeName));
    m_chart->setTitleBrush(QBrush(palette::color(palette::kTextPrimary)));
    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() + 1.0);
    m_chart->setTitleFont(titleFont);

    m_stack->setCurrentIndex(0);
}

} // namespace cutee
