#pragma once

#include "model/PatientRecord.h"

#include <array>
#include <queue>

namespace clinic::model {

using PatientList = std::vector<std::unique_ptr<PatientRecord>>;
using PatientView = std::vector<const PatientRecord*>;

struct QueueEntry {
    int patientId;
    QDateTime arrived;
};
using WaitingQueue = std::queue<QueueEntry>;

class ClinicDatabase {
public:
    const PatientList& records() const noexcept { return patients_; }
    PatientRecord& at(std::size_t index) { return *patients_.at(index); }

    void insert(std::size_t index, std::unique_ptr<PatientRecord> patient);
    void replace(std::size_t index, std::unique_ptr<PatientRecord> patient);
    void erase(std::size_t index);

    const PatientRecord* cached(int id) const noexcept;
    void remember(const PatientRecord& patient) noexcept;
    PatientView recentPatients() const;

private:
    PatientList patients_;
    std::array<const PatientRecord*, 10> recent_{};
};

struct ClinicState {
    ClinicDatabase database;
    WaitingQueue waiting;
};

} // namespace clinic::model
