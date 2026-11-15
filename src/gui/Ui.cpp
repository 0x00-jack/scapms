#include "gui/Ui.h"

#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>

namespace clinic::gui {

using namespace model;

RecordTable::RecordTable(const QStringList& columns) {
    setColumnCount(columns.size());
    setHorizontalHeaderLabels(columns);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setAlternatingRowColors(true);
    setShowGrid(false);
    verticalHeader()->hide();
    verticalHeader()->setDefaultSectionSize(33);
    horizontalHeader()->setDefaultSectionSize(116);
    horizontalHeader()->setStretchLastSection(true);
    setTextElideMode(Qt::ElideRight);
}

void RecordTable::showRows(const Rows& rows) {
    const int previousId = selectedId();
    const QSignalBlocker block(this);
    setUpdatesEnabled(false);
    clearContents();
    clearSelection();
    setRowCount(static_cast<int>(rows.size()));
    int row = 0;
    for (const auto& [id, cells] : rows) {
        for (int column = 0; column < cells.size(); ++column) {
            const auto& text = cells[column];
            auto* item = new QTableWidgetItem(text);
            item->setToolTip("<span>" + text.toHtmlEscaped() + "</span>");
            if (column == 0) {
                item->setData(Qt::UserRole, id);
            }
            const auto header = horizontalHeaderItem(column)->text();
            if (header == "Status" || header == "Category") {
                QFont font = item->font();
                font.setBold(true);
                item->setFont(font);
                if (text == "Emergency" || text == "Cancelled") {
                    item->setForeground(QColor("#a12e41"));
                } else if (text == "Pending") {
                    item->setForeground(QColor("#8b5609"));
                } else if (text == "Follow-up") {
                    item->setForeground(QColor("#4f559d"));
                } else {
                    item->setForeground(QColor("#116456"));
                }
            }
            setItem(row, column, item);
        }
        if (previousId > 0 && previousId == id) {
            selectRow(row);
        }
        ++row;
    }
    setUpdatesEnabled(true);
}

int RecordTable::selectedId() const {
    const auto selected = selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return 0;
    }
    const auto* cell = item(selected.first().row(), 0);
    return cell ? cell->data(Qt::UserRole).toInt() : 0;
}

Rows patientRows(const PatientView& patients) {
    Rows rows;
    for (const auto* patient : patients) {
        const auto& info = patient->info();
        rows.push_back({info.id, {QString::number(info.id), info.name, QString::number(info.age),
                                  label(patient->category()), info.contact, info.detail}});
    }
    return rows;
}

QString dateTimeLabel(const QDateTime& dateTime) {
    return dateTime.toLocalTime().toString("dd MMM yyyy  HH:mm");
}

QPushButton* button(const QString& text, const QString& tooltip, bool primary) {
    auto* control = new QPushButton(text);
    control->setToolTip(tooltip);
    control->setProperty("primary", primary);
    return control;
}

QLabel* heading(const QString& text, const char* role) {
    auto* label = new QLabel(text);
    label->setTextFormat(Qt::PlainText);
    label->setProperty("role", role);
    return label;
}

bool protect(QWidget* parent, const std::function<void()>& action) {
    try {
        action();
        return true;
    } catch (const std::exception& error) {
        QMessageBox message(QMessageBox::Warning, "Clinic workstation",
                            QString::fromUtf8(error.what()), QMessageBox::Ok, parent);
        message.setTextFormat(Qt::PlainText);
        message.exec();
        return false;
    }
}

QString styleSheet() {
    return QStringLiteral(R"CSS(
        QMainWindow, QDialog, QWidget#workspace { background: #f3f7f7; }
        QWidget { color: #162f35; }
        QFrame#banner { background: #153d43; border-radius: 12px; }
        QLabel[role="brand"] { color: #a2ded1; font-size: 11px; font-weight: 700; }
        QLabel[role="hero"] { color: white; font-size: 25px; font-weight: 700; }
        QLabel[role="bannerText"] { color: #d5ede9; font-size: 12px; }
        QLabel[role="title"] { font-size: 21px; font-weight: 700; }
        QLabel[role="section"] { font-size: 14px; font-weight: 700; }
        QLabel[role="muted"] { color: #4f686a; }
        QFrame[role="tile"] { background: white; border: 1px solid #dce7e6; border-radius: 10px; }
        QLabel[role="metric"] { color: #175e56; font-size: 27px; font-weight: 700; }
        QTabWidget::pane { border: 1px solid #dce7e6; background: #fcfefe; border-radius: 8px; }
        QTabBar::tab { background: #e9f0ef; border: 1px solid #dce7e6; padding: 10px 15px;
                       margin-right: 3px; border-top-left-radius: 7px; border-top-right-radius: 7px; }
        QTabBar::tab:selected { color: #075d53; background: white; border-top: 3px solid #1d8678; }
        QPushButton { background: white; border: 1px solid #c9d9d7; border-radius: 6px;
                      padding: 7px 11px; min-height: 17px; font-weight: 600; }
        QPushButton:hover { background: #e5f3ef; border-color: #87b9ad; }
        QPushButton:focus { border-color: #247c70; }
        QPushButton[primary="true"] { background: #117368; color: white; border-color: #117368; }
        QPushButton[primary="true"]:hover { background: #0a5a51; }
        QPushButton:disabled { color: #6c7d7e; background: #ebf1f0; border-color: #dce7e6; }
        QLineEdit, QComboBox, QSpinBox, QDateTimeEdit, QTextEdit {
            background: white; border: 1px solid #c9d9d7; border-radius: 5px;
            padding: 5px 7px; min-height: 18px;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDateTimeEdit:focus, QTextEdit:focus {
            border-color: #247c70;
        }
        QLineEdit:read-only { background: #edf3f2; }
        QTableWidget { background: #ffffff; alternate-background-color: #f2f8f7;
                       border: 1px solid #dce7e6; border-radius: 6px;
                       selection-background-color: #cce9e1; selection-color: #143b37; }
        QTableWidget::item { padding-left: 6px; }
        QHeaderView::section { background: #e6efee; border: 0; padding: 8px 7px;
                               font-weight: 700; color: #2d4d4d; }
        QMenuBar, QStatusBar { background: #f3f7f7; }
        QMenu { background: white; border: 1px solid #dce7e6; }
        QMenu::item:selected, QMenuBar::item:selected { background: #d8ebe6; }
        QToolTip { color: #163a3e; background: #fffef5; border: 1px solid #bbcfc9; }
    )CSS");
}

} // namespace clinic::gui
