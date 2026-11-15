#pragma once

#include "controller/ClinicManager.h"
#include "gui/Ui.h"

#include <QWidget>

class QComboBox;
class QLineEdit;

namespace clinic::gui {

class SearchPanel final : public QWidget {
public:
    SearchPanel(controller::ClinicManager& manager, std::function<void(int)> openPatient);
    void refresh();
    void focusQuery();

private:
    struct Query {
        controller::SearchField field;
        controller::SearchAlgorithm algorithm;
        QString text;
    };

    void updateSearchOptions();
    void applySearch();
    void showResults(model::PatientView patients);

    controller::ClinicManager& manager_;
    std::optional<Query> query_;
    std::optional<std::pair<controller::SortField, controller::SortAlgorithm>> sort_;
    QComboBox* field_;
    QComboBox* algorithm_;
    QLineEdit* text_;
    QComboBox* category_;
    QComboBox* sortField_;
    QComboBox* sortAlgorithm_;
    QLabel* result_;
    RecordTable* table_;
};

} // namespace clinic::gui
