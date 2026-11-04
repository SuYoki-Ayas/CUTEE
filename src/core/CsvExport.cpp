#include "core/CsvExport.h"

#include <QSaveFile>

namespace cutee::csvutil {

QString escapeField(const QString &value)
{
    const bool needsQuotes = value.contains(QLatin1Char(',')) || value.contains(QLatin1Char('"'))
        || value.contains(QLatin1Char('\n')) || value.contains(QLatin1Char('\r'));
    if (!needsQuotes)
        return value;

    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

QString toCsv(const QVector<QStringList> &rows)
{
    QString content;
    for (const QStringList &row : rows) {
        QStringList fields;
        fields.reserve(row.size());
        for (const QString &field : row)
            fields.append(escapeField(field));
        content += fields.join(QLatin1Char(',')) + QStringLiteral("\r\n");
    }
    return content;
}

Result<void> writeFile(const QString &path, const QString &content)
{
    if (path.isEmpty())
        return Error(QStringLiteral("未指定导出路径"), QStringLiteral("csvutil::writeFile"));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return Error(file.errorString(), QStringLiteral("csvutil::writeFile"));

    // UTF-8 BOM keeps Excel from mis-detecting GBK.
    const QByteArray bom("\xEF\xBB\xBF", 3);
    const QByteArray payload = content.toUtf8();
    if (file.write(bom) != bom.size() || file.write(payload) != payload.size()) {
        const QString message = file.errorString();
        file.cancelWriting();
        return Error(message, QStringLiteral("csvutil::writeFile"));
    }
    if (!file.commit())
        return Error(file.errorString(), QStringLiteral("csvutil::writeFile"));
    return {};
}

} // namespace cutee::csvutil
