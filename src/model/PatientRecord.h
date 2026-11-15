#pragma once

#include <QDateTime>
#include <QString>
#include <memory>
#include <stdexcept>
#include <vector>

namespace clinic::model {

enum class Category { General, Emergency, FollowUp };
enum class Service { Consultation, Nursing, Immunisation, UrgentAssessment, FollowUpReview };
enum class Status { Pending, Completed, Cancelled };

QString label(Category category);
QString label(Service service);
QString label(Status status);
QString detailLabel(Category category);
std::vector<Category> categories();
Category parseCategory(const QString& text);
Service parseService(const QString& text);
Status parseStatus(const QString& text);

class ClinicError : public std::runtime_error {
public:
    explicit ClinicError(const QString& message);
};

struct PatientInfo {
    int id;
    QString name;
    int age;
    QString contact;
    Category category;
    QString detail;
};

struct Appointment {
    int id;
    QDateTime start;
    Service service;
    QString clinician;
    Status status = Status::Pending;
    QString note;
};

struct PatientSnapshot {
    PatientInfo info;
    std::vector<Appointment> appointments;
};

class Appointable {
public:
    virtual ~Appointable() = default;
    virtual void scheduleAppointment(Appointment appointment) = 0;
    virtual void cancelAppointment(int id) = 0;
};

class PatientRecord : public Appointable {
public:
    virtual Category category() const = 0;
    virtual std::vector<Service> services() const = 0;

    const PatientInfo& info() const noexcept { return info_; }
    const std::vector<Appointment>& appointments() const noexcept { return appointments_; }
    PatientSnapshot snapshot() const;

    bool supports(Service service) const;
    void scheduleAppointment(Appointment appointment) override;
    void cancelAppointment(int id) override;
    void completeAppointment(int id, QString note);
    void removeAppointment(int id);

protected:
    explicit PatientRecord(PatientSnapshot snapshot);

private:
    Appointment& pendingAppointment(int id);

    PatientInfo info_;
    std::vector<Appointment> appointments_;
};

class GeneralPatient final : public PatientRecord {
public:
    explicit GeneralPatient(PatientSnapshot snapshot);
    Category category() const override;
    std::vector<Service> services() const override;
};

class EmergencyPatient final : public PatientRecord {
public:
    explicit EmergencyPatient(PatientSnapshot snapshot);
    Category category() const override;
    std::vector<Service> services() const override;
};

class FollowUpPatient final : public PatientRecord {
public:
    explicit FollowUpPatient(PatientSnapshot snapshot);
    Category category() const override;
    std::vector<Service> services() const override;
};

std::unique_ptr<PatientRecord> makePatient(PatientSnapshot snapshot);
void validateAppointment(const Appointment& appointment);
QDateTime minuteSlot(QDateTime dateTime);

} // namespace clinic::model
