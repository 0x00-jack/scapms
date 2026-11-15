#include "gui/SearchPanel.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace clinic::gui {

using namespace controller;
using namespace model;

SearchPanel::SearchPanel(ClinicManager& manager, std::function<void(int)> openPatient)
    : manager_(manager), field_(new QComboBox), algorithm_(new QComboBox), text_(new QLineEdit),
      category_(new QComboBox), sortField_(new QComboBox), sortAlgorithm_(new QComboBox),
      result_(heading({}, "muted")),
      table_(new RecordTable({"ID", "Patient name", "Age", "Category", "Contact", "Visit detail"})) {
    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(22, 19, 22, 18);
    page->addWidget(heading("Search & sort", "title"));
    page->addWidget(heading("Find records and order the results using your chosen algorithm.", "muted"));

    field_->setObjectName("searchField");
    algorithm_->setObjectName("searchAlgorithm");
    text_->setObjectName("searchText");
    sortAlgorithm_->setObjectName("sortAlgorithm");
    field_->addItem("Patient ID", static_cast<int>(SearchField::Id));
    field_->addItem("Patient name", static_cast<int>(SearchField::Name));
    field_->addItem("Category", static_cast<int>(SearchField::Category));
    for (const auto category : categories()) {
        category_->addItem(label(category));
    }
    sortField_->addItem("Patient name", static_cast<int>(SortField::Name));
    sortField_->addItem("Patient ID", static_cast<int>(SortField::Id));
    sortField_->addItem("Age", static_cast<int>(SortField::Age));
    sortAlgorithm_->addItem("Insertion sort", static_cast<int>(SortAlgorithm::Insertion));
    sortAlgorithm_->addItem("Merge sort", static_cast<int>(SortAlgorithm::Merge));

    auto* search = button("Search", "Run the selected search algorithm.", true);
    auto* sort = button("Sort results", "Apply the selected sort to the current results.");
    auto* reset = button("Reset", "Clear the search and restore registration order.");
    auto* searchControls = new QGridLayout;
    searchControls->addWidget(new QLabel("Search by"), 0, 0);
    searchControls->addWidget(new QLabel("Search algorithm"), 0, 1);
    searchControls->addWidget(new QLabel("Search value"), 0, 2);
    searchControls->addWidget(field_, 1, 0);
    searchControls->addWidget(algorithm_, 1, 1);
    searchControls->addWidget(text_, 1, 2);
    searchControls->addWidget(category_, 1, 2);
    searchControls->addWidget(search, 1, 3);
    searchControls->setColumnStretch(2, 1);
    page->addLayout(searchControls);

    auto* sortControls = new QHBoxLayout;
    sortControls->addWidget(new QLabel("Sort by"));
    sortControls->addWidget(sortField_);
    sortControls->addWidget(sortAlgorithm_);
    sortControls->addWidget(sort);
    sortControls->addStretch();
    sortControls->addWidget(reset);
    page->addLayout(sortControls);
    page->addWidget(result_);
    page->addWidget(table_, 1);

    table_->setObjectName("searchResults");
    table_->setColumnWidth(0, 70);
    table_->setColumnWidth(1, 180);
    table_->setColumnWidth(2, 58);
    table_->setColumnWidth(4, 190);
    connect(field_, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this] { updateSearchOptions(); });
    connect(search, &QPushButton::clicked, this, [this] { applySearch(); });
    connect(text_, &QLineEdit::returnPressed, this, [this] { applySearch(); });
    connect(sort, &QPushButton::clicked, this, [this] {
        sort_ = {static_cast<SortField>(sortField_->currentData().toInt()),
                 static_cast<SortAlgorithm>(sortAlgorithm_->currentData().toInt())};
        refresh();
    });
    connect(reset, &QPushButton::clicked, this, [this] {
        query_.reset();
        sort_.reset();
        text_->clear();
        refresh();
    });
    connect(table_, &RecordTable::cellDoubleClicked, this,
            [this, openPatient] { openPatient(table_->selectedId()); });
    updateSearchOptions();
}

void SearchPanel::updateSearchOptions() {
    const auto field = static_cast<SearchField>(field_->currentData().toInt());
    algorithm_->clear();
    algorithm_->addItem("Linear search", static_cast<int>(SearchAlgorithm::Linear));
    if (field == SearchField::Id) {
        algorithm_->addItem("Binary search", static_cast<int>(SearchAlgorithm::Binary));
        text_->setPlaceholderText("e.g. 1001");
    } else if (field == SearchField::Name) {
        algorithm_->addItem("Recursive search", static_cast<int>(SearchAlgorithm::Recursive));
        text_->setPlaceholderText("Full or partial name");
    }
    algorithm_->setToolTip("Binary search uses an ID-ordered view. Name searches match partial text.");
    category_->setVisible(field == SearchField::Category);
    text_->setVisible(field != SearchField::Category);
}

void SearchPanel::applySearch() {
    protect(this, [this] {
        const auto field = static_cast<SearchField>(field_->currentData().toInt());
        Query next{field, static_cast<SearchAlgorithm>(algorithm_->currentData().toInt()),
                   field == SearchField::Category ? category_->currentText() : text_->text()};
        auto matches = manager_.search(next.field, next.algorithm, next.text);
        query_ = std::move(next);
        showResults(std::move(matches));
    });
}

void SearchPanel::refresh() {
    auto patients = query_ ? manager_.search(query_->field, query_->algorithm, query_->text)
                           : manager_.patients();
    showResults(std::move(patients));
}

void SearchPanel::showResults(PatientView patients) {
    if (sort_) {
        SearchEngine::sort(patients, sort_->first, sort_->second);
    }
    const auto count = static_cast<qulonglong>(patients.size());
    result_->setText(query_ ? QString("%1 matching patient record(s)").arg(count)
                           : QString("%1 patient record(s)").arg(count));
    table_->showRows(patientRows(patients));
}

void SearchPanel::focusQuery() {
    if (category_->isVisible()) {
        category_->setFocus();
    } else {
        text_->setFocus();
        text_->selectAll();
    }
}

} // namespace clinic::gui
