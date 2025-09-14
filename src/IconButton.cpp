#include "IconButton.h"
#include "../include/FontLoader.h"
#include <QPainter>
#include <QPixmap>
#include <QFont>

IconButton::IconButton(const QString& glyphUtf8,
                       int pixelSize,
                       const QColor& color,
                       const QString& tooltip,
                       QWidget* parent)
  : QPushButton(parent)
  , m_glyph    (glyphUtf8)
  , m_pixelSize(pixelSize)
  , m_color    (color)
{
    setFlat(true);
    if (!tooltip.isEmpty())
        setToolTip(tooltip);
    updateIcon();
}

void IconButton::setGlyph(const QString& glyphUtf8)
{
    if (this->parent() == nullptr && !this->window()) {
        qWarning() << "[IconButton] called on destroyed or orphaned object";
        return;
    }
    if (glyphUtf8 == m_glyph) return;
    m_glyph = glyphUtf8;
    updateIcon();
}

void IconButton::setColor(const QColor& color)
{
    if (color == m_color) return;
    m_color = color;
    updateIcon();
}

void IconButton::setPixelSize(int pixelSize)
{
    if (pixelSize == m_pixelSize) return;
    m_pixelSize = pixelSize;
    updateIcon();
}

void IconButton::updateIcon()
{
    QSize canvas(m_pixelSize + 4, m_pixelSize + 4);
    QPixmap pm(canvas);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    QFont font(loadFluentIconsFont());
    font.setPixelSize(m_pixelSize);
    p.setFont(font);
    p.setPen(m_color);
    p.drawText(pm.rect(), Qt::AlignCenter, m_glyph);

    QPushButton::setIcon(QIcon(pm));
    setIconSize(QSize(m_pixelSize, m_pixelSize));
}
