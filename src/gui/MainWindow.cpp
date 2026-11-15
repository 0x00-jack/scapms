#include "gui/MainWindow.h"
#include "gui/Dialogs.h"
#include "gui/SearchPanel.h"
#include "gui/Ui.h"

#include <QAction>
#include <QCloseEvent>
#include <QDateEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <numeric>

namespace clinic::gui {

using namespace model;
using controller::AppointmentRow;

namespace {

QString number(std::size_t count) {
    return QString::number(static_cast<qulonglong>(count));
}

QVBoxLayout* pageLayout(QWidget* page) {
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(22, 19, 22, 18);
    layout->setSpacing(12);
    return layout;
}

QHBoxLayout* pageHeader(QVBoxLayout* page, const QString& title, const QString& subtitle) {
    auto* line = new QHBoxLayout;
    auto* labels = new QVBoxLayout;
    labels->setSpacing(3);
    labels->addWidget(heading(title, "title"));
    labels->addWidget(heading(subtitle, "muted"));
    line->addLayout(labels);
    line->addStretch();
    page->addLayout(line);
    return line;
}

QVBoxLayout* tableSection(const QString& title, RecordTable* table) {
    auto* section = new QVBoxLayout;
    section->addWidget(heading(title, "section"));
    section->addWidget(table, 1);
    return section;
}

int selected(RecordTable* table, const QString& prompt) {
    const int id = table->selectedId();
    if (id <= 0) {
        throw ClinicError(prompt);
    }
    return id;
}

bool confirm(QWidget* parent, const QString& title, const QString& prompt) {
    QMessageBox message(QMessageBox::Question, title, prompt, QMessageBox::Yes | QMessageBox::No,
                        parent);
    message.setTextFormat(Qt::PlainText);
    message.setDefaultButton(QMessageBox::No);
    return message.exec() == QMessageBox::Yes;
}

QString patientCaption(const PatientRecord& record) {
    return QString("%1 · %2").arg(record.info().name).arg(record.info().id);
}

QString waitLabel(qint64 minutes) {
    if (minutes < 1) {
        return "< 1 min";
    }
    if (minutes < 60) {
        return QString("%1 min").arg(minutes);
    }
    return QString("%1 hr %2 min").arg(minutes / 60).arg(minutes % 60);
}

} // namespace

MainWindow::MainWindow(const QString& initialPath) {
    setMinimumSize(920, 650);
    resize(1160, 780);
    auto* workspace = new QWidget;
    workspace->setObjectName("workspace");
    setCentralWidget(workspace);
    auto* layout = new QVBoxLayout(workspace);
    layout->setContentsMargins(20, 10, 20, 12);
    layout->setSpacing(14);

    auto* banner = new QFrame;
    banner->setObjectName("banner");
    auto* bannerLayout = new QHBoxLayout(banner);
    bannerLayout->setContentsMargins(20, 12, 20, 12);
    auto* identity = new QVBoxLayout;
    identity->setSpacing(1);
    identity->addWidget(heading("UNIVERSITY CLINIC  /  SCAPMS", "brand"));
    identity->addWidget(heading("Clinic workstation", "hero"));
    bannerLayout->addLayout(identity);
    bannerLayout->addStretch();
    clock_ = heading({}, "bannerText");
    bannerLayout->addWidget(clock_);
    layout->addWidget(banner);

    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    tabs_->setObjectName("clinicTabs");
    tabs_->addTab(makeDashboard(), "Overview");
    tabs_->addTab(makePatients(), "Patient Records");
    tabs_->addTab(makeAppointments(), "Appointments");
    tabs_->addTab(makeQueue(), "Waiting Queue");
    search_ = new SearchPanel(manager_, [this](int id) { openPatient(id); });
    tabs_->addTab(search_, "Search && Sort");
    tabs_->addTab(makeReports(), "Reports");
    layout->addWidget(tabs_, 1);

    fileStatus_ = heading({}, "muted");
    statusBar()->addPermanentWidget(fileStatus_);
    makeMenus();
    refresh();
    statusBar()->showMessage("Ready");
    if (!initialPath.isEmpty()) {
        protect(this, [&] { load(initialPath); });
    }

    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        refreshDashboard();
        refreshQueue();
    });
    timer->start(30000);
}

QWidget* MainWindow::makeDashboard() {
    auto* page = new QWidget;
    auto* layout = pageLayout(page);
    auto* header = pageHeader(layout, "Clinic overview", "Patient, appointment and queue activity.");
    undoButton_ = button("Undo", "Reverse the last administrative operation.");
    auto* add = button("Register patient", "Open the patient registration form.", true);
    header->addWidget(undoButton_);
    header->addWidget(add);
    connect(add, &QPushButton::clicked, this, [this] { openPatient(0); });
    connect(undoButton_, &QPushButton::clicked, this, [this] { undo(); });

    auto* tiles = new QHBoxLayout;
    const QStringList titles{"Registered patients", "Waiting now", "Appointments today", "Completed today"};
    for (std::size_t index = 0; index < metrics_.size(); ++index) {
        auto* tile = new QFrame;
        tile->setProperty("role", "tile");
        auto* content = new QVBoxLayout(tile);
        content->setContentsMargins(13, 10, 13, 10);
        content->setSpacing(2);
        content->addWidget(heading(titles[static_cast<int>(index)], "muted"));
        metrics_[index] = heading("0", "metric");
        content->addWidget(metrics_[index]);
        tiles->addWidget(tile, 1);
    }
    layout->addLayout(tiles);
    reminders_ = new RecordTable({"Patient", "Scheduled time", "Reminder"});
    reminders_->setColumnWidth(0, 195);
    reminders_->setColumnWidth(1, 174);
    recent_ = new RecordTable({"ID", "Patient", "Category"});
    recent_->setColumnWidth(0, 62);
    recent_->setColumnWidth(1, 170);
    auto* tables = new QHBoxLayout;
    tables->addLayout(tableSection("Pending appointment reminders", reminders_), 3);
    tables->addLayout(tableSection("Recently accessed patients", recent_), 2);
    layout->addLayout(tables, 1);
    connect(recent_, &RecordTable::cellDoubleClicked, this,
            [this] { openPatient(recent_->selectedId()); });
    return page;
}

QWidget* MainWindow::makePatients() {
    auto* page = new QWidget;
    auto* layout = pageLayout(page);
    auto* header = pageHeader(layout, "Patient records", "Register, review and maintain patient information.");
    auto* add = button("Register patient", "Create a patient record.", true);
    auto* edit = button("Edit", "Open the selected patient record.");
    auto* remove = button("Delete", "Delete the patient and associated appointments.");
    add->setObjectName("registerPatient");
    edit->setObjectName("editPatient");
    remove->setObjectName("deletePatient");
    header->addWidget(add);
    header->addWidget(edit);
    header->addWidget(remove);
    patients_ = new RecordTable({"ID", "Patient name", "Age", "Category", "Contact", "Visit detail"});
    patients_->setObjectName("patientTable");
    patients_->setColumnWidth(0, 68);
    patients_->setColumnWidth(1, 188);
    patients_->setColumnWidth(2, 56);
    patients_->setColumnWidth(4, 190);
    layout->addWidget(patients_, 1);
    connect(add, &QPushButton::clicked, this, [this] { openPatient(0); });
    const auto editSelected = [this] {
        protect(this, [this] { openPatient(selected(patients_, "Select a patient to edit.")); });
    };
    connect(edit, &QPushButton::clicked, this, editSelected);
    connect(patients_, &RecordTable::cellDoubleClicked, this, editSelected);
    connect(remove, &QPushButton::clicked, this, [this] {
        protect(this, [this] {
            const int id = selected(patients_, "Select a patient to delete.");
            const auto name = manager_.patient(id).info().name;
            const auto prompt = QString("Delete %1 and all their appointments and queue entries?").arg(name);
            if (confirm(this, "Delete patient", prompt)) {
                manager_.deletePatient(id);
                changed("Patient deleted. Use Undo to restore the record.");
            }
        });
    });
    return page;
}

QWidget* MainWindow::makeAppointments() {
    auto* page = new QWidget;
    auto* layout = pageLayout(page);
    auto* header = pageHeader(layout, "Appointments", "Book services and record consultation outcomes.");
    auto* add = button("Schedule", "Book a medical service for a patient.", true);
    auto* complete = button("Complete", "Mark the selected booking completed and record a note.");
    auto* cancel = button("Cancel", "Change a pending booking to Cancelled.");
    auto* remove = button("Remove", "Delete the selected appointment from the file.");
    add->setObjectName("scheduleAppointment");
    complete->setObjectName("completeAppointment");
    cancel->setObjectName("cancelAppointment");
    header->addWidget(add);
    header->addWidget(complete);
    header->addWidget(cancel);
    header->addWidget(remove);
    appointments_ = new RecordTable({"ID", "Patient", "Date and time", "Service", "Clinician",
                                      "Status", "Consultation note"});
    appointments_->setObjectName("appointmentTable");
    appointments_->setColumnWidth(0, 67);
    appointments_->setColumnWidth(1, 159);
    appointments_->setColumnWidth(2, 163);
    appointments_->setColumnWidth(3, 153);
    appointments_->setColumnWidth(5, 99);
    layout->addWidget(appointments_, 1);

    connect(add, &QPushButton::clicked, this, [this] { scheduleAppointment(); });
    connect(complete, &QPushButton::clicked, this, [this] {
        protect(this, [this] {
            const int id = selected(appointments_, "Select an appointment to complete.");
            if (manager_.appointment(id).appointment->status != Status::Pending) {
                throw ClinicError("Select an appointment with Pending status.");
            }
            if (completeDialog(this, manager_, id)) {
                changed("Consultation recorded.");
            }
        });
    });
    connect(cancel, &QPushButton::clicked, this, [this] {
        protect(this, [this] {
            const int id = selected(appointments_, "Select an appointment to cancel.");
            if (confirm(this, "Cancel appointment", "Mark the selected appointment as Cancelled?")) {
                manager_.cancel(id);
                changed("Appointment cancelled.");
            }
        });
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        protect(this, [this] {
            const int id = selected(appointments_, "Select an appointment to remove.");
            if (confirm(this, "Remove appointment", "Delete the selected appointment from the record?")) {
                manager_.removeAppointment(id);
                changed("Appointment removed. Use Undo to restore it.");
            }
        });
    });
    return page;
}

QWidget* MainWindow::makeQueue() {
    auto* page = new QWidget;
    auto* layout = pageLayout(page);
    auto* header = pageHeader(layout, "Waiting queue", "Call patients in their order of arrival.");
    auto* add = button("Add to queue", "Check in an existing patient.");
    auto* next = button("Call next patient", "Remove and display the first waiting patient.", true);
    add->setObjectName("addToQueue");
    next->setObjectName("callNextPatient");
    header->addWidget(add);
    header->addWidget(next);
    waiting_ = new RecordTable({"Position", "Patient", "Category", "Arrived", "Waiting time"});
    waiting_->setObjectName("queueTable");
    waiting_->setColumnWidth(0, 84);
    waiting_->setColumnWidth(1, 230);
    waiting_->setColumnWidth(3, 174);
    layout->addWidget(waiting_, 1);

    connect(add, &QPushButton::clicked, this, [this] {
        protect(this, [this] {
            if (enqueueDialog(this, manager_)) {
                changed("Patient added to the waiting queue.");
            }
        });
    });
    connect(next, &QPushButton::clicked, this, [this] {
        protect(this, [this] {
            const auto name = manager_.callNext();
            changed("Called " + name + ".");
            QMessageBox notice(QMessageBox::Information, "Next patient",
                               name + " is next for consultation.", QMessageBox::Ok, this);
            notice.setTextFormat(Qt::PlainText);
            notice.exec();
        });
    });
    return page;
}

QWidget* MainWindow::makeReports() {
    auto* page = new QWidget;
    auto* layout = pageLayout(page);
    auto* header = pageHeader(layout, "Clinic reports", "Track attendance, bookings and patient categories.");
    auto* dateLabel = new QLabel("&Report date");
    reportDate_ = new QDateEdit(QDate::currentDate());
    reportDate_->setObjectName("reportDate");
    reportDate_->setCalendarPopup(true);
    reportDate_->setDisplayFormat("dd MMM yyyy");
    dateLabel->setBuddy(reportDate_);
    header->addWidget(dateLabel);
    header->addWidget(reportDate_);

    attendance_ = new RecordTable({"ID", "Patient name", "Completed"});
    attendance_->setObjectName("attendanceReport");
    attendance_->setColumnWidth(0, 64);
    attendance_->setColumnWidth(1, 185);
    pending_ = new RecordTable({"Patient", "Date and time", "Service"});
    pending_->setObjectName("pendingReport");
    pending_->setColumnWidth(0, 170);
    pending_->setColumnWidth(1, 158);
    categories_ = new RecordTable({"Category", "Patients"});
    categories_->setColumnWidth(0, 162);
    daily_ = new RecordTable({"Status", "Appointments"});
    daily_->setObjectName("dailyReport");
    daily_->setColumnWidth(0, 161);
    auto* grid = new QGridLayout;
    grid->setSpacing(13);
    grid->addLayout(tableSection("Most frequently attended patients", attendance_), 0, 0);
    grid->addLayout(tableSection("Pending appointments", pending_), 0, 1);
    grid->addLayout(tableSection("Patient category distribution", categories_), 1, 0);
    grid->addLayout(tableSection("Daily appointment summary", daily_), 1, 1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);
    layout->addLayout(grid, 1);
    connect(reportDate_, &QDateEdit::dateChanged, this, [this] { refreshReports(); });
    return page;
}

void MainWindow::makeMenus() {
    const auto add = [this](QMenu* menu, const QString& text, const QKeySequence& shortcut,
                            const std::function<void()>& action) {
        auto* entry = menu->addAction(text);
        entry->setShortcut(shortcut);
        connect(entry, &QAction::triggered, this, action);
        return entry;
    };

    auto* file = menuBar()->addMenu("&File");
    add(file, "&New", QKeySequence::New, [this] {
        if (confirmFileChange()) {
            manager_.reset();
            path_.clear();
            dirty_ = false;
            refresh();
            statusBar()->showMessage("New clinic file.", 5000);
        }
    });
    add(file, "&Load…", QKeySequence::Open, [this] {
        const auto path = QFileDialog::getOpenFileName(this, "Load clinic file", {},
                                                       "Clinic JSON files (*.json)");
        if (!path.isEmpty() && confirmFileChange()) {
            protect(this, [&] { load(path); });
        }
    });
    add(file, "&Save", QKeySequence::Save, [this] { save(false); });
    add(file, "Save &as…", QKeySequence::SaveAs, [this] { save(true); });
    file->addSeparator();
    add(file, "E&xit", QKeySequence("Ctrl+Q"), [this] { close(); });

    auto* edit = menuBar()->addMenu("&Edit");
    undoAction_ = add(edit, "&Undo", QKeySequence::Undo, [this] { undo(); });
    add(edit, "&Find patient", QKeySequence::Find, [this] {
        tabs_->setCurrentWidget(search_);
        search_->focusQuery();
    });
}

void MainWindow::refresh() {
    patients_->showRows(patientRows(manager_.patients()));
    Rows appointmentRows;
    for (const auto& [patient, item] : manager_.appointments()) {
        appointmentRows.push_back({item->id, {QString::number(item->id), patientCaption(*patient),
            dateTimeLabel(item->start), label(item->service), item->clinician,
            label(item->status), item->note}});
    }
    appointments_->showRows(appointmentRows);
    refreshDashboard();
    refreshQueue();
    search_->refresh();
    refreshReports();
    updateFileStatus();
}

void MainWindow::refreshDashboard() {
    const auto now = QDateTime::currentDateTime();
    const auto report = manager_.reports(now.date());
    clock_->setText(now.toString("ddd, dd MMM yyyy  ·  HH:mm"));
    metrics_[0]->setText(number(manager_.patients().size()));
    metrics_[1]->setText(number(manager_.waiting().size()));
    metrics_[2]->setText(QString::number(std::accumulate(report.daily.begin(), report.daily.end(), 0)));
    metrics_[3]->setText(QString::number(report.daily[static_cast<int>(Status::Completed)]));

    Rows reminders;
    for (const auto& [patient, item] : report.pending) {
        const auto seconds = now.secsTo(item->start);
        QString timing = "Upcoming";
        if (seconds < -60) {
            timing = "Overdue";
        } else if (seconds <= 60) {
            timing = "Due now";
        } else if (seconds <= 1800) {
            timing = QString("Due in %1 min").arg((seconds + 59) / 60);
        }
        reminders.push_back({item->id, {patient->info().name, dateTimeLabel(item->start), timing}});
        if (reminders.size() == 5) {
            break;
        }
    }
    reminders_->showRows(reminders);
    Rows recent;
    for (const auto* patient : manager_.recentPatients()) {
        const auto& info = patient->info();
        recent.push_back({info.id, {QString::number(info.id), info.name, label(patient->category())}});
    }
    recent_->showRows(recent);
}

void MainWindow::refreshQueue() {
    Rows rows;
    const auto now = QDateTime::currentDateTime();
    for (const auto& entry : manager_.waiting()) {
        const auto& patient = manager_.patient(entry.patientId);
        rows.push_back({entry.patientId, {number(rows.size() + 1), patientCaption(patient),
            label(patient.category()), dateTimeLabel(entry.arrived), waitLabel(entry.arrived.secsTo(now) / 60)}});
    }
    waiting_->showRows(rows);
}

void MainWindow::refreshReports() {
    const auto report = manager_.reports(reportDate_->date());
    Rows attendance;
    for (const auto& row : report.attendance) {
        attendance.push_back({row.patientId, {QString::number(row.patientId), row.name,
                                               QString::number(row.completed)}});
    }
    attendance_->showRows(attendance);

    Rows pending;
    for (const auto& [patient, item] : report.pending) {
        pending.push_back({item->id, {patientCaption(*patient), dateTimeLabel(item->start),
                                      label(item->service)}});
    }
    pending_->showRows(pending);

    Rows categories;
    for (const auto category : model::categories()) {
        const auto count = report.categories[static_cast<int>(category)];
        categories.push_back({0, {label(category), QString::number(count)}});
    }
    categories_->showRows(categories);
    const auto total = std::accumulate(report.daily.begin(), report.daily.end(), 0);
    Rows daily{{0, {"Total", QString::number(total)}}};
    for (const auto status : {Status::Completed, Status::Pending, Status::Cancelled}) {
        daily.push_back({0, {label(status), QString::number(report.daily[static_cast<int>(status)])}});
    }
    daily_->showRows(daily);
}

void MainWindow::updateFileStatus() {
    const auto name = path_.isEmpty() ? QString("Untitled clinic file") : QFileInfo(path_).fileName();
    const auto caption = name + (dirty_ ? " · Unsaved changes" : QString{});
    fileStatus_->setText(caption);
    fileStatus_->setToolTip("<span>" + path_.toHtmlEscaped() + "</span>");
    setWindowTitle("SCAPMS | " + caption);
    const auto label = manager_.undoLabel();
    undoAction_->setEnabled(!label.isEmpty());
    undoAction_->setText(label.isEmpty() ? "&Undo" : "&Undo " + label);
    undoButton_->setEnabled(!label.isEmpty());
    undoButton_->setToolTip(label.isEmpty() ? "No operation to undo." : "Undo " + label);
}

void MainWindow::changed(const QString& message) {
    dirty_ = true;
    refresh();
    statusBar()->showMessage(message, 6000);
}

void MainWindow::openPatient(int id) {
    protect(this, [&] {
        if (patientDialog(this, manager_, id)) {
            changed(id ? "Patient updated." : "Patient registered.");
        } else {
            refreshDashboard();
        }
    });
}

void MainWindow::scheduleAppointment() {
    protect(this, [this] {
        if (scheduleDialog(this, manager_)) {
            changed("Appointment scheduled.");
        }
    });
}

void MainWindow::undo() {
    protect(this, [this] {
        const auto description = manager_.undo();
        changed("Undid " + description + ".");
    });
}

bool MainWindow::confirmFileChange() {
    if (!dirty_) {
        return true;
    }
    const auto result = QMessageBox::warning(this, "Unsaved changes",
        "Save changes to the current clinic file?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (result == QMessageBox::Save) {
        return save(false);
    }
    return result == QMessageBox::Discard;
}

bool MainWindow::save(bool saveAs) {
    auto target = path_;
    if (target.isEmpty() || saveAs) {
        target = QFileDialog::getSaveFileName(this, "Save clinic file",
            target.isEmpty() ? QString("clinic.json") : target, "Clinic JSON files (*.json)");
        if (target.isEmpty()) {
            return false;
        }
        if (QFileInfo(target).suffix().isEmpty()) {
            target += ".json";
        }
    }
    return protect(this, [&] {
        manager_.save(target);
        path_ = target;
        dirty_ = false;
        updateFileStatus();
        statusBar()->showMessage("Clinic file saved.", 5000);
    });
}

void MainWindow::load(const QString& path) {
    manager_.load(path);
    path_ = path;
    dirty_ = false;
    refresh();
    statusBar()->showMessage("Clinic file loaded.", 5000);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirmFileChange()) {
        event->accept();
    } else {
        event->ignore();
    }
}

} // namespace clinic::gui
