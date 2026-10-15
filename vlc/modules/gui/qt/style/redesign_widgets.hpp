// SPDX-License-Identifier: GPL-2.0-or-later
// Apply the reference's surface, typography and accent to native VLC dialogs.
#pragma once
#include <QApplication>
#include <QStyleHints>
#include <QPalette>
#include <cmath>

inline void applyRedesignWidgetStyle(MainCtx *ctx)
{
    auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app) return;
    const auto scheme = ctx->getColorScheme()->currentScheme();
    bool systemDark = false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    systemDark = app->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#endif
    const bool dark = scheme == ColorSchemeModel::Night || (scheme == ColorSchemeModel::System && systemDark);
    const QString paper = dark ? QStringLiteral("#0e0e0e") : QStringLiteral("#f8f8f8");
    const QString panel = dark ? QStringLiteral("#191919") : QStringLiteral("#eaeaea");
    const QString field = dark ? QStringLiteral("#090909") : QStringLiteral("#ffffff");
    const QString ink = dark ? QStringLiteral("#8f8f8f") : QStringLiteral("#333333");
    const QString title = dark ? QStringLiteral("#c6c6c6") : QStringLiteral("#242424");
    const QString edge = dark ? QStringLiteral("#373737") : QStringLiteral("#cccccc");
    const QString muted = dark ? QStringLiteral("#6b6b6b") : QStringLiteral("#777777");
    QColor accent(ctx->redesignConfigValue(QStringLiteral("qt-accent-color")).toString());
    if (!accent.isValid()) accent = QColor(QStringLiteral("#d37918"));
    app->setStyleSheet(QString::fromLatin1(R"CSS(
QDialog, QMenu, QToolTip { background: %1; color: %4; font-family: Inter; }
QDialog QWidget { color: %4; font-family: Inter; font-size: 14px; }
QDialog QLabel { background: transparent; }
QDialog QGroupBox { color: %5; border: 1px solid %6; border-radius: 6px; margin-top: 18px; padding: 12px; }
QDialog QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 6px; }
QDialog QLineEdit, QDialog QSpinBox, QDialog QDoubleSpinBox, QDialog QComboBox, QDialog QTextEdit {
 background: %3; color: %4; border: 1px solid %6; border-radius: 6px; min-height: 28px; padding: 2px 8px; }
QDialog QComboBox { background: %2; border-color: %3; }
QDialog QLineEdit:focus, QDialog QSpinBox:focus, QDialog QDoubleSpinBox:focus, QDialog QComboBox:focus { border-color: %8; }
QDialog QPushButton, QDialog QToolButton { background: %2; color: %4; border: 1px solid transparent; border-radius: 6px; padding: 6px 12px; }
QDialog QPushButton:hover, QDialog QToolButton:hover { border-color: %6; }
QDialog QPushButton:focus, QDialog QToolButton:focus { border-color: %8; }
QDialog QPushButton:default, QDialog QPushButton:checked, QDialog QToolButton:checked { background: %8; color: #090909; }
QDialog QAbstractItemView { background: %3; alternate-background-color: %2; color: %4; border: 1px solid %6; border-radius: 6px; selection-background-color: %2; selection-color: %8; }
QDialog QHeaderView::section { background: %2; color: %5; border: 0; border-bottom: 1px solid %6; padding: 6px; }
QDialog QTabWidget::pane { background: %1; border: 1px solid %6; border-radius: 6px; }
QDialog QTabBar::tab { background: %1; color: %4; padding: 8px 10px; border-bottom: 2px solid transparent; }
QDialog QTabBar::tab:selected { color: %5; border-bottom-color: %8; }
QDialog QTabBar::scroller { width: 48px; }
QDialog QTabBar QToolButton { background: %1; border: none; border-radius: 0; padding: 0; }
QDialog QWidget:disabled { color: %7; }
QMenu { border: 1px solid %6; padding: 6px; }
QMenu::item { padding: 8px 26px; border-radius: 4px; }
QMenu::item:selected { background: %2; color: %8; }
QMenu::separator { height: 1px; background: %6; margin: 4px 8px; }
QToolTip { border: 1px solid %6; border-radius: 4px; padding: 6px; }
QDialog QFrame#footer { background: %2; }
)CSS").arg(paper, panel, field, ink, title, edge, muted, accent.name()));
    // list selections and links in native dialogs follow the accent, not system blue
    const auto linear = [](double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); };
    const double luminance = 0.2126 * linear(accent.redF()) + 0.7152 * linear(accent.greenF()) + 0.0722 * linear(accent.blueF());
    QPalette palette = app->palette();
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, (luminance + 0.05) / 0.0528 >= 1.05 / (luminance + 0.05) ? QColor(0x09, 0x09, 0x09) : QColor(Qt::white));
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::LinkVisited, accent.darker(115));
    app->setPalette(palette);
}
