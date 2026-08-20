#include "Theme.h"

#include <QApplication>
#include <QPalette>

namespace jupitr::Theme {

QColor darkBackground() { return QColor(QStringLiteral("#1A0F0A")); }
QColor cardBackground() { return QColor(QStringLiteral("#2D1F16")); }
QColor alternateCardBackground() { return QColor(QStringLiteral("#37271C")); }
QColor hoverBackground() { return QColor(QStringLiteral("#463223")); }
QColor orange() { return QColor(QStringLiteral("#FF6B35")); }
QColor orangeLight() { return QColor(QStringLiteral("#FF8C42")); }
QColor yellow() { return QColor(QStringLiteral("#FFD23F")); }
QColor yellowLight() { return QColor(QStringLiteral("#FFE678")); }
QColor cream() { return QColor(QStringLiteral("#FFF8E7")); }
QColor muted() { return QColor(QStringLiteral("#B8A99A")); }

QIcon trayIcon()
{
    return QIcon(QStringLiteral(":/assets/jupitr.svg"));
}

void apply(QApplication &application)
{
    QPalette palette;
    palette.setColor(QPalette::Window, darkBackground());
    palette.setColor(QPalette::WindowText, cream());
    palette.setColor(QPalette::Base, cardBackground());
    palette.setColor(QPalette::AlternateBase, alternateCardBackground());
    palette.setColor(QPalette::Text, cream());
    palette.setColor(QPalette::Button, cardBackground());
    palette.setColor(QPalette::ButtonText, cream());
    palette.setColor(QPalette::Highlight, hoverBackground());
    palette.setColor(QPalette::HighlightedText, cream());
    application.setPalette(palette);
    application.setStyleSheet(QStringLiteral(R"(
        QWidget { color: #FFF8E7; font-family: "Noto Sans"; }
        QToolTip { background: #2D1F16; color: #FFF8E7; border: 1px solid #463223; }
        QLineEdit, QComboBox { background: #2D1F16; color: #FFF8E7; border: 1px solid #463223; border-radius: 4px; padding: 4px; }
        QLineEdit:focus, QComboBox:focus { border: 1px solid #FF8C42; }
        QPushButton { background: #FF6B35; color: #1A0F0A; border: none; border-radius: 5px; padding: 8px 18px; font-weight: 600; }
        QPushButton:hover { background: #FF8C42; }
        QScrollBar:vertical { background: #1A0F0A; width: 8px; }
        QScrollBar::handle:vertical { background: #463223; border-radius: 4px; min-height: 20px; }
    )"));
}

} // namespace jupitr::Theme
