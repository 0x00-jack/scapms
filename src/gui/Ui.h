#pragma once

#include "model/ClinicDatabase.h"

#include <QTableWidget>
#include <functional>
#include <utility>

class QLabel;
class QPushButton;

namespace clinic::gui {

using Rows = std::vector<std::pair<int, QStringList>>;

class RecordTable final : public QTableWidget {
public:
    explicit RecordTable(const QStringList& columns);
    void showRows(const Rows& rows);
    int selectedId() const;
};

Rows patientRows(const model::PatientView& patients);
QString dateTimeLabel(const QDateTime& dateTime);
QPushButton* button(const QString& text, const QString& tooltip, bool primary = false);
QLabel* heading(const QString& text, const char* role);
bool protect(QWidget* parent, const std::function<void()>& action);
QString styleSheet();

} // namespace clinic::gui
