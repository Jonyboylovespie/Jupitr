#pragma once

#include <QColor>
#include <QIcon>

class QApplication;

namespace jupitr::Theme {

QColor darkBackground();
QColor cardBackground();
QColor alternateCardBackground();
QColor hoverBackground();
QColor orange();
QColor orangeLight();
QColor yellow();
QColor yellowLight();
QColor cream();
QColor muted();
QIcon trayIcon();
void apply(QApplication &application);

} // namespace jupitr::Theme
