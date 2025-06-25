#pragma once
#include <QPushButton>
#include "IconProvider.h"
#include <QtAwesome.h>


class IconButton : public QPushButton {
  Q_OBJECT
public:
  // iconName — что угодно: "fa-solid fa-coffee", "coffee", "solid coffee"
  IconButton(const QString& iconName,
             const QSize& iconSize,
             const QVariantMap& opts = {},
             const QString& tooltip = {},
             QWidget* parent = nullptr)
    : QPushButton(parent)
  {
    QIcon ic = A().icon(iconName, opts);
    setIcon(ic);
    setIconSize(iconSize);
    setText({});
    if (!tooltip.isEmpty()) setToolTip(tooltip);
  }
};

