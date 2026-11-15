#include "model/PatientRecord.h"

#include <utility>

namespace clinic::model {

QString label(Category category) {
    switch (category) {
    case Category::General: return "General";
    case Category::Emergency: return "Emergency";
    case Category::FollowUp: return "Follow-up";
    }
    throw ClinicError("Unknown patient category.");
}

QString label(Service service) {
    switch (service) {
    case Service::Consultation: return "General consultation";
    case Service::Nursing: return "Nursing care";
    case Service::Immunisation: return "Immunisation";
    case Service::UrgentAssessment: return "Urgent assessment";
    case Service::FollowUpReview: return "Follow-up review";
    }
    throw ClinicError("Unknown medical service.");
}

QString label(Status status) {
    switch (status) {
    case Status::Pending: return "Pending";
    case Status::Completed: return "Completed";
    case Status::Cancelled: return "Cancelled";
    }
    throw ClinicError("Unknown appointment status.");
}

QString detailLabel(Category category) {
    switch (category) {
    case Category::General: return "Visit reason";
    case Category::Emergency: return "Triage note";
    case Category::FollowUp: return "Review focus";
    }
    throw ClinicError("Unknown patient category.");
}

std::vector<Category> categories() {
    return {Category::General, Category::Emergency, Category::FollowUp};
}

Category parseCategory(const QString& text) {
    for (const auto value : categories()) {
        if (label(value).compare(text, Qt::CaseInsensitive) == 0) {
            return value;
        }
    }
    throw ClinicError("Choose General, Emergency, or Follow-up.");
}

Service parseService(const QString& text) {
    const Service services[] = {Service::Consultation, Service::Nursing, Service::Immunisation,
                                Service::UrgentAssessment, Service::FollowUpReview};
    for (const auto value : services) {
        if (label(value) == text) {
            return value;
        }
    }
    throw ClinicError("Unknown medical service: " + text);
}

Status parseStatus(const QString& text) {
    for (const auto value : {Status::Pending, Status::Completed, Status::Cancelled}) {
        if (label(value) == text) {
            return value;
        }
    }
    throw ClinicError("Unknown appointment status: " + text);
}

ClinicError::ClinicError(const QString& message)
    : std::runtime_error(message.toUtf8().constData()) {}

QDateTime minuteSlot(QDateTime dateTime) {
    const auto time = dateTime.time();
    return dateTime.addMSecs(-1000 * time.second() - time.msec());
}

void validateAppointment(const Appointment& appointment) {
    if (appointment.id <= 0 || !appointment.start.isValid()) {
        throw ClinicError("An appointment needs a positive ID and a valid date and time.");
    }
    if (appointment.clinician.trimmed().isEmpty()) {
        throw ClinicError("Enter the clinician or staff member for this appointment.");
    }
}

PatientRecord::PatientRecord(PatientSnapshot snapshot)
    : info_(std::move(snapshot.info)), appointments_(std::move(snapshot.appointments)) {
    info_.name = info_.name.simplified();
    info_.contact = info_.contact.trimmed();
    info_.detail = info_.detail.trimmed();
    if (info_.id <= 0) {
        throw ClinicError("Patient ID must be a positive whole number.");
    }
    if (info_.name.isEmpty() || info_.contact.isEmpty()) {
        throw ClinicError("Enter both the patient's name and contact information.");
    }
    if (info_.age < 0 || info_.age > 120) {
        throw ClinicError("Patient age must be between 0 and 120.");
    }
    for (const auto& appointment : appointments_) {
        validateAppointment(appointment);
    }
}

PatientSnapshot PatientRecord::snapshot() const {
    return {info_, appointments_};
}

bool PatientRecord::supports(Service service) const {
    for (const auto available : services()) {
        if (available == service) {
            return true;
        }
    }
    return false;
}

void PatientRecord::scheduleAppointment(Appointment appointment) {
    validateAppointment(appointment);
    if (appointment.status != Status::Pending || !supports(appointment.service)) {
        throw ClinicError("Choose an available service for this patient category.");
    }
    appointments_.push_back(std::move(appointment));
}

Appointment& PatientRecord::pendingAppointment(int id) {
    for (auto& appointment : appointments_) {
        if (appointment.id == id && appointment.status == Status::Pending) {
            return appointment;
        }
    }
    throw ClinicError("Select an appointment with Pending status.");
}

void PatientRecord::cancelAppointment(int id) {
    pendingAppointment(id).status = Status::Cancelled;
}

void PatientRecord::completeAppointment(int id, QString note) {
    auto& appointment = pendingAppointment(id);
    appointment.note = note.trimmed();
    appointment.status = Status::Completed;
}

void PatientRecord::removeAppointment(int id) {
    for (auto current = appointments_.begin(); current != appointments_.end(); ++current) {
        if (current->id == id) {
            appointments_.erase(current);
            return;
        }
    }
    throw ClinicError("The selected appointment no longer exists.");
}

GeneralPatient::GeneralPatient(PatientSnapshot snapshot) : PatientRecord(std::move(snapshot)) {}
Category GeneralPatient::category() const { return Category::General; }
std::vector<Service> GeneralPatient::services() const {
    return {Service::Consultation, Service::Nursing, Service::Immunisation};
}

EmergencyPatient::EmergencyPatient(PatientSnapshot snapshot)
    : PatientRecord(std::move(snapshot)) {}
Category EmergencyPatient::category() const { return Category::Emergency; }
std::vector<Service> EmergencyPatient::services() const {
    return {Service::UrgentAssessment};
}

FollowUpPatient::FollowUpPatient(PatientSnapshot snapshot)
    : PatientRecord(std::move(snapshot)) {}
Category FollowUpPatient::category() const { return Category::FollowUp; }
std::vector<Service> FollowUpPatient::services() const {
    return {Service::FollowUpReview, Service::Nursing};
}

std::unique_ptr<PatientRecord> makePatient(PatientSnapshot snapshot) {
    std::unique_ptr<PatientRecord> record;
    switch (snapshot.info.category) {
    case Category::General:
        record = std::make_unique<GeneralPatient>(std::move(snapshot));
        break;
    case Category::Emergency:
        record = std::make_unique<EmergencyPatient>(std::move(snapshot));
        break;
    case Category::FollowUp:
        record = std::make_unique<FollowUpPatient>(std::move(snapshot));
        break;
    default:
        throw ClinicError("Choose a valid patient category.");
    }
    for (const auto& appointment : record->appointments()) {
        if (appointment.status == Status::Pending && !record->supports(appointment.service)) {
            throw ClinicError("A pending appointment uses a service unavailable to this category. "
                              "Complete or cancel it before changing the category.");
        }
    }
    return record;
}

} // namespace clinic::model
