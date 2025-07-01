#pragma once
#include <QIcon>
#include <QPainter>
#include <QFontMetrics>

inline QIcon fluentIcon(const QString &fontFamily,
                        const QString &glyphUtf8,
                        int pixelSize,
                        const QColor &color = Qt::black)
{
    QString glyph = QString::fromUtf8(glyphUtf8.toUtf8());

    QFont f(fontFamily);
    f.setPixelSize(pixelSize);

    QSize sz = QSize(pixelSize, pixelSize) + QSize(4,4);
    QPixmap pm(sz);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setFont(f);
    p.setPen(color);
    p.drawText(pm.rect(), Qt::AlignCenter, glyph);
    return QIcon(pm);
}

