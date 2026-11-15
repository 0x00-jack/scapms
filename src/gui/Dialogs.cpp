#include "gui/Dialogs.h"
#include "gui/Ui.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <limits>

namespace clinic::gui {

using namespace model;
using controller::ClinicManager;

namespace {

QString patientChoice(const PatientRecord& record) {
    return QString("%1  ·  ID %2").arg(record.info().name).arg(record.info().id);
}

QFormLayout* makeForm(QDialog& dialog, const QString& title, const QString& description,
                      const QString& actionLabel, const std::function<void()>& action) {
    dialog.setWindowTitle(title + " | SCAPMS");
    dialog.setMinimumWidth(490);
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(13);
    layout->addWidget(heading(title, "title"));
    auto* instructions = heading(description, "muted");
    instructions->setWordWrap(true);
    layout->addWidget(instructions);

    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setSpacing(10);
    layout->addLayout(form);

    auto* controls = new QDialogButtonBox(QDialogButtonBox::Cancel);
    auto* accept = controls->addButton(actionLabel, QDialogButtonBox::AcceptRole);
    accept->setProperty("primary", true);
    accept->setDefault(true);
    layout->addWidget(controls);
    QObject::connect(controls, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(controls, &QDialogButtonBox::accepted, &dialog, [&dialog, action] {
        if (protect(&dialog, action)) {
            dialog.accept();
        }
    });
    return form;
}

} // namespace

bool patientDialog(QWidget* parent, ClinicManager& manager, int id) {
    const bool editing = id > 0;
    const auto info = editing
        ? manager.patient(id).info()
        : PatientInfo{manager.suggestPatientId(), {}, 18, {}, Category::General, {}};
    if (editing) {
        manager.markViewed(id);
    }

    QDialog dialog(parent);
    auto* identifier = new QSpinBox(&dialog);
    identifier->setObjectName("patientId");
    identifier->setRange(1, std::numeric_limits<int>::max());
    identifier->setValue(info.id);
    identifier->setReadOnly(editing);
    identifier->setButtonSymbols(QAbstractSpinBox::NoButtons);

    auto* name = new QLineEdit(info.name, &dialog);
    name->setObjectName("patientName");
    name->setPlaceholderText("Full name");
    auto* age = new QSpinBox(&dialog);
    age->setObjectName("patientAge");
    age->setRange(0, 120);
    age->setValue(info.age);
    auto* contact = new QLineEdit(info.contact, &dialog);
    contact->setObjectName("patientContact");
    contact->setPlaceholderText("Phone number or email");
    auto* category = new QComboBox(&dialog);
    category->setObjectName("patientCategory");
    for (const auto value : categories()) {
        category->addItem(label(value), static_cast<int>(value));
    }
    category->setCurrentIndex(category->findData(static_cast<int>(info.category)));
    auto* detail = new QLineEdit(info.detail, &dialog);
    detail->setObjectName("patientDetail");
    auto* detailTitle = new QLabel(&dialog);
    detailTitle->setBuddy(detail);
    const auto updateDetail = [=] {
        const auto value = static_cast<Category>(category->currentData().toInt());
        detailTitle->setText(detailLabel(value));
        detail->setPlaceholderText(detailLabel(value) + " (optional)");
    };
    QObject::connect(category, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, updateDetail);
    updateDetail();

    auto* form = makeForm(dialog, editing ? "Edit patient" : "Register patient",
                          "Enter the patient's details and choose the category for this visit.",
                          editing ? "Save changes" : "Register patient", [&] {
        PatientInfo revised{identifier->value(), name->text(), age->value(), contact->text(),
                            static_cast<Category>(category->currentData().toInt()), detail->text()};
        if (editing) {
            manager.editPatient(std::move(revised));
        } else {
            manager.addPatient(std::move(revised));
        }
        manager.markViewed(identifier->value());
    });
    form->addRow("Patient &ID", identifier);
    form->addRow("Full &name", name);
    form->addRow("&Age", age);
    form->addRow("&Contact", contact);
    form->addRow("C&ategory", category);
    form->addRow(detailTitle, detail);
    name->setFocus();
    return dialog.exec() == QDialog::Accepted;
}

bool scheduleDialog(QWidget* parent, ClinicManager& manager) {
    const auto patients = manager.patients();
    if (patients.empty()) {
        throw ClinicError("Register a patient before scheduling an appointment.");
    }
    QDialog dialog(parent);
    auto* patient = new QComboBox(&dialog);
    patient->setObjectName("schedulePatient");
    for (const auto* record : patients) {
        patient->addItem(patientChoice(*record), record->info().id);
    }
    auto* service = new QComboBox(&dialog);
    service->setObjectName("scheduleService");
    const auto updateServices = [&] {
        service->clear();
        for (const auto value : manager.patient(patient->currentData().toInt()).services()) {
            service->addItem(label(value), static_cast<int>(value));
        }
    };
    QObject::connect(patient, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, updateServices);
    updateServices();

    auto* dateTime = new QDateTimeEdit(minuteSlot(QDateTime::currentDateTime()).addSecs(900), &dialog);
    dateTime->setObjectName("scheduleTime");
    dateTime->setCalendarPopup(true);
    dateTime->setDisplayFormat("dd MMM yyyy  HH:mm");
    auto* clinician = new QLineEdit(&dialog);
    clinician->setObjectName("scheduleClinician");
    clinician->setPlaceholderText("e.g. Dr Sam Lee");

    auto* form = makeForm(dialog, "Schedule appointment",
                          "Choose a service and a future time in the clinic's local time zone.",
                          "Schedule appointment", [&] {
        const int id = patient->currentData().toInt();
        manager.schedule(id, dateTime->dateTime(), static_cast<Service>(service->currentData().toInt()),
                         clinician->text());
        manager.markViewed(id);
    });
    form->addRow("&Patient", patient);
    form->addRow("&Service", service);
    form->addRow("&Date and time", dateTime);
    form->addRow("&Clinician", clinician);
    clinician->setFocus();
    return dialog.exec() == QDialog::Accepted;
}

bool enqueueDialog(QWidget* parent, ClinicManager& manager) {
    const auto patients = manager.patients();
    if (patients.empty()) {
        throw ClinicError("Register a patient before adding them to the waiting queue.");
    }
    QStringList options;
    for (const auto* record : patients) {
        options.append(patientChoice(*record));
    }
    bool accepted = false;
    const auto selected = QInputDialog::getItem(parent, "Add to waiting queue", "Patient",
                                                options, 0, false, &accepted);
    if (accepted) {
        manager.enqueue(patients.at(options.indexOf(selected))->info().id);
    }
    return accepted;
}

bool completeDialog(QWidget* parent, ClinicManager& manager, int appointmentId) {
    bool accepted = false;
    const auto note = QInputDialog::getMultiLineText(parent, "Complete appointment",
                                                   "Consultation note (optional)", {}, &accepted);
    if (accepted) {
        manager.complete(appointmentId, note);
    }
    return accepted;
}

} // namespace clinic::gui
