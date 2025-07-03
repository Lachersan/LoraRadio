#include "IconButton.h"
#include "FontLoader.h"
#include <QFontDatabase>
#include <QPainter>
#include <QDebug>
#include <QMessageBox>

IconButton::IconButton(const QString& glyphUtf8,
                       int pixelSize,
                       const QColor& color,
                       const QString& tooltip,
                       QWidget* parent)
  : QPushButton(parent)
{
    setFlat(true);



    static QString fluentFamily = loadFluentIconsFont();

    QFont f(fluentFamily);
    f.setPixelSize(pixelSize);
    QSize sz(pixelSize + 4, pixelSize + 4);
    QPixmap pm(sz);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setFont(f);
    p.setPen(color);
    QString glyph = QString::fromUtf8(glyphUtf8.toUtf8());
    p.drawText(pm.rect(), Qt::AlignCenter, glyph);

    setIcon(QIcon(pm));
    setIconSize({pixelSize, pixelSize});
    if (!tooltip.isEmpty()) setToolTip(tooltip);
}
