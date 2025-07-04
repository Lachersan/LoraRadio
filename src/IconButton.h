#pragma once
#include <QPushButton>
#include <QColor>

class IconButton : public QPushButton {
    Q_OBJECT
public:
    IconButton(const QString& glyphUtf8,
               int pixelSize,
               const QColor& color,
               const QString& tooltip = QString(),
               QWidget* parent = nullptr);

    void setGlyph(const QString& glyphUtf8);
    void setColor(const QColor& color);
    void setPixelSize(int pixelSize);

private:
    void updateIcon();

    QString m_glyph;
    int     m_pixelSize;
    QColor  m_color;
};
