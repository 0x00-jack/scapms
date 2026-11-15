#include "model/ClinicDatabase.h"

#include <utility>

namespace clinic::model {

void ClinicDatabase::insert(std::size_t index, std::unique_ptr<PatientRecord> patient) {
    patients_.insert(patients_.begin() + index, std::move(patient));
}

void ClinicDatabase::replace(std::size_t index, std::unique_ptr<PatientRecord> patient) {
    auto& previous = patients_.at(index);
    for (auto& entry : recent_) {
        if (entry == previous.get()) {
            entry = patient.get();
            break;
        }
    }
    previous = std::move(patient);
}

void ClinicDatabase::erase(std::size_t index) {
    const auto* removed = patients_.at(index).get();
    for (std::size_t position = 0; position < recent_.size(); ++position) {
        if (recent_[position] == removed) {
            for (auto next = position + 1; next < recent_.size(); ++next) {
                recent_[next - 1] = recent_[next];
            }
            recent_.back() = nullptr;
            break;
        }
    }
    patients_.erase(patients_.begin() + index);
}

const PatientRecord* ClinicDatabase::cached(int id) const noexcept {
    for (const auto* patient : recent_) {
        if (patient && patient->info().id == id) {
            return patient;
        }
    }
    return nullptr;
}

void ClinicDatabase::remember(const PatientRecord& patient) noexcept {
    // Move an existing entry to the front, or discard the oldest entry.
    std::size_t position = 0;
    while (position + 1 < recent_.size() && recent_[position] != &patient) {
        ++position;
    }
    while (position > 0) {
        recent_[position] = recent_[position - 1];
        --position;
    }
    recent_.front() = &patient;
}

PatientView ClinicDatabase::recentPatients() const {
    PatientView view;
    for (const auto* patient : recent_) {
        if (patient) {
            view.push_back(patient);
        }
    }
    return view;
}

} // namespace clinic::model
