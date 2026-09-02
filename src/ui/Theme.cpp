#include "Theme.h"

QString Theme::styleSheet()
{
    return QStringLiteral(R"(
QWidget {
    background-color: %1;
    color: %5;
    font-family: "Segoe UI", "Microsoft YaHei", sans-serif;
    font-size: 13px;
}
QMainWindow, QDialog { background-color: %1; }
QMenuBar { background-color: %2; border-bottom: 1px solid %4; }
QMenuBar::item { padding: 4px 10px; }
QMenuBar::item:selected { background-color: %3; border-radius: 3px; }
QMenu { background-color: %2; border: 1px solid %4; padding: 4px; }
QMenu::item { padding: 5px 24px; border-radius: 3px; }
QMenu::item:selected { background-color: %3; }
QMenu::separator { height: 1px; background-color: %4; margin: 4px 8px; }
QToolBar { background-color: %2; border: none; border-bottom: 1px solid %4; padding: 3px; spacing: 3px; }
QToolButton {
    background-color: transparent; border: 1px solid transparent;
    border-radius: 4px; padding: 5px 9px; color: %5;
}
QToolButton:hover { background-color: %3; border-color: %4; }
QToolButton:checked { background-color: %3; border-color: %7; color: %8; }
QSplitter::handle { background-color: %4; width: 2px; }
QGroupBox {
    border: 1px solid %4; border-radius: 6px; margin-top: 10px;
    padding: 8px 6px 6px 6px; font-weight: 600;
}
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: %6; }
QPushButton {
    background-color: %3; border: 1px solid %4; border-radius: 4px;
    padding: 6px 14px; color: %5;
}
QPushButton:hover { background-color: %2; border-color: %7; }
QPushButton:pressed { background-color: %4; }
QPushButton:disabled { color: %6; border-color: %4; }
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background-color: %3; border: 1px solid %4; border-radius: 4px;
    padding: 4px 8px; color: %5; selection-background-color: %7;
}
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus { border-color: %7; }
QComboBox::drop-down { border: none; width: 22px; }
QComboBox QAbstractItemView {
    background-color: %2; border: 1px solid %4; selection-background-color: %3;
}
QSlider::groove:horizontal { height: 4px; background-color: %4; border-radius: 2px; }
QSlider::sub-page:horizontal { background-color: %7; border-radius: 2px; }
QSlider::handle:horizontal {
    width: 14px; height: 14px; margin: -5px 0; border-radius: 7px;
    background-color: %5; border: 1px solid %4;
}
QSlider::handle:horizontal:hover { border-color: %7; }
QTableWidget, QTableView, QTreeView {
    background-color: %2; alternate-background-color: %3;
    border: 1px solid %4; gridline-color: %4; selection-background-color: %7;
    selection-color: #ffffff;
}
QHeaderView::section {
    background-color: %3; border: none; border-right: 1px solid %4;
    border-bottom: 1px solid %4; padding: 5px 8px; color: %6; font-weight: 600;
}
QProgressBar { background-color: %3; border: 1px solid %4; border-radius: 4px; text-align: center; }
QProgressBar::chunk { background-color: %7; border-radius: 3px; }
QLabel { color: %5; background-color: transparent; }
QStatusBar { background-color: %2; border-top: 1px solid %4; color: %6; }
QScrollBar:vertical { background-color: %1; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background-color: %4; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background-color: %7; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar:horizontal { background-color: %1; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background-color: %4; border-radius: 5px; min-width: 30px; }
QCheckBox::indicator, QRadioButton::indicator { width: 15px; height: 15px; }
QCheckBox::indicator:unchecked { background-color: %3; border: 1px solid %4; border-radius: 3px; }
QCheckBox::indicator:checked { background-color: %7; border: 1px solid %7; border-radius: 3px; }
QTabWidget::pane { border: 1px solid %4; border-radius: 4px; }
QTabBar::tab {
    background-color: %3; border: 1px solid %4; padding: 6px 16px;
    border-top-left-radius: 4px; border-top-right-radius: 4px;
}
QTabBar::tab:selected { background-color: %2; border-bottom-color: %7; }
)")
    .arg(QLatin1String(bgColor()), QLatin1String(panelColor()),
         QLatin1String(panelAltColor()), QLatin1String(borderColor()),
         QLatin1String(textColor()), QLatin1String(textDimColor()),
         QLatin1String(accentColor()), QLatin1String(accentHoverColor()));
}
