#pragma once
#include <QIcon>
#include <QPainter>
#include <QFontMetrics>

// glyph – это ваша строка, например fluent_icons::ic_fluent_add_24_filled
inline QIcon fluentIcon(const QString &fontFamily,
                        const QString &glyphUtf8,
                        int pixelSize,
                        const QColor &color = Qt::black)
{
    // конвертируем UTF-8 строку ("\uf10a") в QString-глиф
    QString glyph = QString::fromUtf8(glyphUtf8.toUtf8());

    QFont f(fontFamily);
    f.setPixelSize(pixelSize);

    // делаем немного больше, чтобы ничего не обрезало
    QSize sz = QSize(pixelSize, pixelSize) + QSize(4,4);
    QPixmap pm(sz);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setFont(f);
    p.setPen(color);
    p.drawText(pm.rect(), Qt::AlignCenter, glyph);
    return QIcon(pm);
}

