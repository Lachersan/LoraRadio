#pragma once
#include <QPushButton>
#include "IconProvider.h"

class IconButton : public QPushButton {
  Q_OBJECT
public:
  // FontAwesome ctor (оставляем без тела)
  IconButton(const QString& iconName,
             const QSize& iconSize,
             const QVariantMap& opts = {},
             const QString& tooltip = {},
             QWidget* parent = nullptr);

  // Fluent ctor (только объявляем)
  IconButton(const QString& glyphUtf8,
             int pixelSize,
             const QColor& color = Qt::black,
             const QString& tooltip = {},
             QWidget* parent = nullptr);
};
