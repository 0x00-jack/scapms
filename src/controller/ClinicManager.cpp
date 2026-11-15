#include "controller/ClinicManager.h"
#include "io/FileHandler.h"

#include <limits>
#include <map>
#include <utility>

namespace clinic::controller {

using namespace model;

std::size_t ClinicManager::indexOf(int id) const {
    const auto& records = state_.database.records();
    const auto index = SearchEngine::linearId(records, id);
    if (index == records.size()) {
        throw ClinicError("The selected patient no longer exists.");
    }
    return index;
}

const PatientView& ClinicManager::orderedById() const {
    if (!idIndex_) {
        auto view = patients();
        SearchEngine::sort(view, SortField::Id, SortAlgorithm::Merge);
        idIndex_ = std::move(view);
    }
    return *idIndex_;
}

int ClinicManager::suggestPatientId() const {
    const auto& ordered = orderedById();
    const int highest = ordered.empty() ? 1000 : ordered.back()->info().id;
    if (highest < std::numeric_limits<int>::max()) {
        return highest < 1000 ? 1001 : highest + 1;
    }
    int firstFree = 1;
    for (const auto* record : ordered) {
        if (record->info().id != firstFree) {
            return firstFree;
        }
        if (firstFree == std::numeric_limits<int>::max()) {
            break;
        }
        ++firstFree;
    }
    throw ClinicError("All patient IDs have been used.");
}

const PatientRecord& ClinicManager::patient(int id) const {
    if (const auto* record = state_.database.cached(id)) {
        return *record;
    }
    return *state_.database.records()[indexOf(id)];
}

PatientView ClinicManager::patients() const {
    return SearchEngine::viewOf(state_.database.records());
}

PatientView ClinicManager::recentPatients() const {
    return state_.database.recentPatients();
}

void ClinicManager::markViewed(int id) {
    state_.database.remember(patient(id));
}

void ClinicManager::apply(const QString& label, int patientId, bool trackWaiting,
                          const std::function<void()>& change) {
    const auto& records = state_.database.records();
    const auto index = SearchEngine::linearId(records, patientId);
    UndoEntry entry{label, patientId, index, std::nullopt, std::nullopt};
    if (index < records.size()) {
        entry.before = records[index]->snapshot();
    }
    if (trackWaiting) {
        entry.waitingBefore = state_.waiting;
    }
    history_.push(std::move(entry));
    try {
        change();
    } catch (...) {
        history_.pop();
        throw;
    }
    idIndex_.reset();
}

void ClinicManager::addPatient(PatientInfo info) {
    const auto& records = state_.database.records();
    const auto index = SearchEngine::linearId(records, info.id);
    if (index < records.size()) {
        throw ClinicError("That patient ID is already in use.");
    }
    auto record = makePatient({std::move(info), {}});
    apply("register patient", record->info().id, false, [&] {
        state_.database.insert(index, std::move(record));
    });
}

void ClinicManager::editPatient(PatientInfo info) {
    const auto index = indexOf(info.id);
    auto record = makePatient({std::move(info), state_.database.at(index).appointments()});
    apply("edit patient", record->info().id, false, [&] {
        state_.database.replace(index, std::move(record));
    });
}

void ClinicManager::deletePatient(int id) {
    const auto index = indexOf(id);
    WaitingQueue remaining;
    for (const auto& entry : waiting()) {
        if (entry.patientId != id) {
            remaining.push(entry);
        }
    }
    apply("delete patient", id, true, [&] {
        state_.database.erase(index);
        state_.waiting.swap(remaining);
    });
}

std::vector<AppointmentRow> ClinicManager::appointments() const {
    std::vector<AppointmentRow> rows;
    for (const auto& record : state_.database.records()) {
        for (const auto& appointment : record->appointments()) {
            rows.push_back({record.get(), &appointment});
        }
    }
    return rows;
}

AppointmentRow ClinicManager::appointment(int id) const {
    for (const auto& record : state_.database.records()) {
        for (const auto& item : record->appointments()) {
            if (item.id == id) {
                return {record.get(), &item};
            }
        }
    }
    throw ClinicError("The selected appointment no longer exists.");
}

void ClinicManager::schedule(int patientId, QDateTime start, Service service, QString clinician) {
    const auto index = indexOf(patientId);
    start = minuteSlot(start).toLocalTime();
    if (!start.isValid() || start <= minuteSlot(QDateTime::currentDateTime())) {
        throw ClinicError("Choose an appointment time at least one minute in the future.");
    }
    if (nextAppointmentId_ > std::numeric_limits<int>::max()) {
        throw ClinicError("All appointment IDs have been used.");
    }
    Appointment item{static_cast<int>(nextAppointmentId_), start, service,
                     clinician.simplified(), Status::Pending, {}};
    validateAppointment(item);
    for (const auto& row : appointments()) {
        if (row.appointment->status != Status::Pending || row.appointment->start != item.start) {
            continue;
        }
        if (row.patient->info().id == patientId) {
            throw ClinicError("This patient already has an appointment at that time.");
        }
        if (row.appointment->clinician.compare(item.clinician, Qt::CaseInsensitive) == 0) {
            throw ClinicError("This clinician already has an appointment at that time.");
        }
    }
    Appointable& record = state_.database.at(index);
    apply("schedule appointment", patientId, false, [&] {
        record.scheduleAppointment(std::move(item));
    });
    ++nextAppointmentId_;
}

void ClinicManager::cancel(int id) {
    const int patientId = appointment(id).patient->info().id;
    Appointable& record = state_.database.at(indexOf(patientId));
    apply("cancel appointment", patientId, false, [&] { record.cancelAppointment(id); });
}

void ClinicManager::complete(int id, const QString& note) {
    const int patientId = appointment(id).patient->info().id;
    auto& record = state_.database.at(indexOf(patientId));
    apply("complete appointment", patientId, false, [&] { record.completeAppointment(id, note); });
}

void ClinicManager::removeAppointment(int id) {
    const int patientId = appointment(id).patient->info().id;
    auto& record = state_.database.at(indexOf(patientId));
    apply("remove appointment", patientId, false, [&] { record.removeAppointment(id); });
}

std::vector<QueueEntry> ClinicManager::waiting() const {
    std::vector<QueueEntry> entries;
    auto remaining = state_.waiting;
    while (!remaining.empty()) {
        entries.push_back(remaining.front());
        remaining.pop();
    }
    return entries;
}

void ClinicManager::enqueue(int patientId) {
    indexOf(patientId);
    for (const auto& entry : waiting()) {
        if (entry.patientId == patientId) {
            throw ClinicError("This patient is already waiting.");
        }
    }
    const QueueEntry entry{patientId, QDateTime::currentDateTime()};
    apply("add to queue", 0, true, [&] { state_.waiting.push(entry); });
}

QString ClinicManager::callNext() {
    if (state_.waiting.empty()) {
        throw ClinicError("The waiting queue is empty.");
    }
    const auto name = patient(state_.waiting.front().patientId).info().name;
    apply("call next patient", 0, true, [&] { state_.waiting.pop(); });
    return name;
}

PatientView ClinicManager::search(SearchField field, SearchAlgorithm algorithm,
                                  const QString& query) const {
    const auto term = query.trimmed();
    if (term.isEmpty()) {
        throw ClinicError("Enter a value to search for.");
    }
    if (algorithm == SearchAlgorithm::Binary && field != SearchField::Id) {
        throw ClinicError("Binary search requires a patient ID.");
    }
    if (algorithm == SearchAlgorithm::Recursive && field != SearchField::Name) {
        throw ClinicError("Recursive search requires a patient name.");
    }
    if (field == SearchField::Id) {
        bool valid = false;
        const int id = term.toInt(&valid);
        if (!valid || id <= 0) {
            throw ClinicError("Enter a positive whole number for the patient ID.");
        }
        if (algorithm == SearchAlgorithm::Binary) {
            const auto* record = SearchEngine::binaryId(orderedById(), id);
            return record ? PatientView{record} : PatientView{};
        }
        const auto& records = state_.database.records();
        const auto index = SearchEngine::linearId(records, id);
        return index < records.size() ? PatientView{records[index].get()} : PatientView{};
    }
    if (field == SearchField::Category) {
        parseCategory(term);
    }
    if (algorithm == SearchAlgorithm::Recursive) {
        return SearchEngine::recursiveName(patients(), term);
    }
    return SearchEngine::linearMatches(patients(), field, term);
}

Reports ClinicManager::reports(QDate date) const {
    Reports result;
    std::map<int, std::vector<AttendanceRow>, std::greater<int>> attendanceByCount;
    std::multimap<QDateTime, AppointmentRow> pendingByTime;
    for (const auto& record : state_.database.records()) {
        ++result.categories[static_cast<std::size_t>(record->category())];
        int completed = 0;
        for (const auto& appointment : record->appointments()) {
            if (appointment.start.date() == date) {
                ++result.daily[static_cast<std::size_t>(appointment.status)];
            }
            if (appointment.status == Status::Completed) {
                ++completed;
            } else if (appointment.status == Status::Pending) {
                pendingByTime.emplace(appointment.start, AppointmentRow{record.get(), &appointment});
            }
        }
        if (completed > 0) {
            const auto& info = record->info();
            attendanceByCount[completed].push_back({info.id, info.name, completed});
        }
    }
    for (const auto& [count, rows] : attendanceByCount) {
        (void)count;
        result.attendance.insert(result.attendance.end(), rows.begin(), rows.end());
    }
    for (const auto& [time, row] : pendingByTime) {
        (void)time;
        result.pending.push_back(row);
    }
    return result;
}

QString ClinicManager::undoLabel() const {
    return history_.empty() ? QString{} : history_.top().label;
}

QString ClinicManager::undo() {
    if (history_.empty()) {
        throw ClinicError("There is no operation to undo.");
    }
    auto& entry = history_.top();
    const auto label = entry.label;
    if (entry.patientId > 0) {
        const auto& records = state_.database.records();
        const auto index = SearchEngine::linearId(records, entry.patientId);
        if (entry.before) {
            auto record = makePatient(*entry.before);
            if (index == records.size()) {
                state_.database.insert(entry.index, std::move(record));
            } else {
                state_.database.replace(index, std::move(record));
            }
        } else {
            state_.database.erase(indexOf(entry.patientId));
        }
    }
    if (entry.waitingBefore) {
        state_.waiting.swap(*entry.waitingBefore);
    }
    history_.pop();
    idIndex_.reset();
    return label;
}

void ClinicManager::reset() {
    state_ = ClinicState{};
    history_ = {};
    idIndex_.reset();
    nextAppointmentId_ = 5001;
}

void ClinicManager::save(const QString& path) const {
    io::FileHandler::save(path, state_);
}

void ClinicManager::load(const QString& path) {
    auto parsed = io::FileHandler::load(path);
    qint64 next = 5001;
    for (const auto& record : parsed.database.records()) {
        for (const auto& appointment : record->appointments()) {
            if (appointment.id >= next) {
                next = static_cast<qint64>(appointment.id) + 1;
            }
        }
    }
    state_ = std::move(parsed);
    history_ = {};
    idIndex_.reset();
    nextAppointmentId_ = next;
}

} // namespace clinic::controller
