#pragma once

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>

namespace cutee::ui {

/// Draws short cell text as a rounded pill filled with the item's
/// BackgroundRole colour, so categorical columns (exam type, enabled state)
/// read as tags instead of fully filled cells. ForegroundRole still drives the
/// text colour, which keeps the contrast decision in the model/page.
class TagDelegate : public QStyledItemDelegate
{
public:
    explicit TagDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        const QString text = opt.text;
        const QVariant background = index.data(Qt::BackgroundRole);

        // Draw the row/selection chrome only: the pastel is painted as a pill
        // below instead of a fully filled cell.
        QStyleOptionViewItem base(opt);
        base.text.clear();
        base.backgroundBrush = QBrush();
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &base, painter, opt.widget);

        if (text.isEmpty() || !background.isValid())
            return;

        const QFontMetrics metrics(opt.font);
        const int width = qMin(opt.rect.width() - 12, metrics.horizontalAdvance(text) + 18);
        const int height = qMin(18, opt.rect.height() - 8);
        const QRect pill(opt.rect.left() + 6, opt.rect.center().y() - height / 2, width, height);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(background.value<QBrush>());
        painter->drawRoundedRect(pill, height / 2.0, height / 2.0);
        painter->setPen(opt.palette.color(QPalette::Text));
        painter->drawText(pill, Qt::AlignCenter, text);
        painter->restore();
    }
};

} // namespace cutee::ui
