#pragma once
#include <QPushButton>

class IconButton : public QPushButton {
  Q_OBJECT
public:
  IconButton(const QString& glyphUtf8,
             int pixelSize,
             const QColor& color = Qt::black,
             const QString& tooltip = {},
             QWidget* parent = nullptr);
};
