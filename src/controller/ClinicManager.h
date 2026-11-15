#pragma once

#include "controller/SearchEngine.h"

#include <functional>
#include <optional>
#include <stack>

namespace clinic::controller {

struct AppointmentRow {
    const model::PatientRecord* patient;
    const model::Appointment* appointment;
};

struct AttendanceRow {
    int patientId;
    QString name;
    int completed;
};

struct Reports {
    std::array<int, 3> categories{};
    std::array<int, 3> daily{};
    std::vector<AttendanceRow> attendance;
    std::vector<AppointmentRow> pending;
};

class ClinicManager {
public:
    int suggestPatientId() const;
    const model::PatientRecord& patient(int id) const;
    model::PatientView patients() const;
    model::PatientView recentPatients() const;
    void markViewed(int id);

    void addPatient(model::PatientInfo info);
    void editPatient(model::PatientInfo info);
    void deletePatient(int id);

    std::vector<AppointmentRow> appointments() const;
    AppointmentRow appointment(int id) const;
    void schedule(int patientId, QDateTime start, model::Service service, QString clinician);
    void cancel(int id);
    void complete(int id, const QString& note);
    void removeAppointment(int id);

    std::vector<model::QueueEntry> waiting() const;
    void enqueue(int patientId);
    QString callNext();

    model::PatientView search(SearchField field, SearchAlgorithm algorithm,
                              const QString& query) const;
    Reports reports(QDate date) const;

    QString undoLabel() const;
    QString undo();
    void reset();
    void save(const QString& path) const;
    void load(const QString& path);

private:
    struct UndoEntry {
        QString label;
        int patientId;
        std::size_t index;
        std::optional<model::PatientSnapshot> before;
        std::optional<model::WaitingQueue> waitingBefore;
    };

    std::size_t indexOf(int id) const;
    void apply(const QString& label, int patientId, bool trackWaiting,
               const std::function<void()>& change);
    const model::PatientView& orderedById() const;

    model::ClinicState state_;
    std::stack<UndoEntry> history_;
    mutable std::optional<model::PatientView> idIndex_;
    qint64 nextAppointmentId_ = 5001;
};

} // namespace clinic::controller
