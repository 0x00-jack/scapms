#include "io/FileHandler.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <set>
#include <utility>

namespace clinic::io {

using namespace model;

namespace {

QJsonObject object(const QJsonValue& value, const QString& name) {
    if (!value.isObject()) {
        throw ClinicError("Expected a JSON object for " + name + ".");
    }
    return value.toObject();
}

QJsonArray array(const QJsonObject& record, const QString& key) {
    if (!record.value(key).isArray()) {
        throw ClinicError("Expected a JSON array for " + key + ".");
    }
    return record.value(key).toArray();
}

QString text(const QJsonObject& record, const QString& key) {
    if (!record.value(key).isString()) {
        throw ClinicError("Expected text for " + key + ".");
    }
    return record.value(key).toString();
}

QDateTime readTime(const QJsonObject& record, const QString& key) {
    const auto value = QDateTime::fromString(text(record, key), Qt::ISODate);
    if (!value.isValid()) {
        throw ClinicError("Invalid date/time in " + key + ".");
    }
    return value.toLocalTime();
}

QString writeTime(const QDateTime& dateTime) {
    return dateTime.toUTC().toString(Qt::ISODate);
}

} // namespace

void FileHandler::save(const QString& path, const ClinicState& state) {
    QJsonArray patients;
    for (const auto& patient : state.database.records()) {
        QJsonArray appointments;
        for (const auto& item : patient->appointments()) {
            appointments.append(QJsonObject{
                {"id", item.id}, {"start", writeTime(item.start)},
                {"service", label(item.service)}, {"clinician", item.clinician},
                {"status", label(item.status)}, {"note", item.note}
            });
        }
        const auto& info = patient->info();
        patients.append(QJsonObject{
            {"id", info.id}, {"name", info.name}, {"age", info.age},
            {"contact", info.contact}, {"category", label(patient->category())},
            {"detail", info.detail}, {"appointments", appointments}
        });
    }
    QJsonArray waiting;
    auto queue = state.waiting;
    while (!queue.empty()) {
        const auto& entry = queue.front();
        waiting.append(QJsonObject{
            {"patientId", entry.patientId}, {"arrived", writeTime(entry.arrived)}
        });
        queue.pop();
    }
    const QJsonObject root{{"version", 1}, {"patients", patients}, {"waiting", waiting}};
    const auto bytes = QJsonDocument(root).toJson();

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        throw ClinicError("Could not save the clinic file. " + file.errorString());
    }
}

ClinicState FileHandler::load(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw ClinicError("Could not open the clinic file. " + file.errorString());
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (file.error() != QFileDevice::NoError) {
        throw ClinicError("Could not read the clinic file. " + file.errorString());
    }
    if (error.error != QJsonParseError::NoError) {
        throw ClinicError("Invalid clinic JSON. " + error.errorString());
    }
    const auto root = document.object();
    if (root.value("version").toInt() != 1) {
        throw ClinicError("This file is not a supported clinic document (version 1).");
    }

    ClinicState state;
    std::set<int> patientIds;
    std::set<int> appointmentIds;
    std::set<std::pair<qint64, QString>> clinicianSlots;
    for (const auto entry : array(root, "patients")) {
        const auto item = object(entry, "patient");
        PatientInfo info{item.value("id").toInt(), text(item, "name"), item.value("age").toInt(-1),
                         text(item, "contact"), parseCategory(text(item, "category")),
                         text(item, "detail")};
        if (!patientIds.insert(info.id).second) {
            throw ClinicError("Duplicate patient ID in the clinic file.");
        }
        PatientSnapshot snapshot{std::move(info), {}};
        std::set<qint64> patientSlots;
        for (const auto appointmentValue : array(item, "appointments")) {
            const auto appt = object(appointmentValue, "appointment");
            Appointment appointment{
                appt.value("id").toInt(), minuteSlot(readTime(appt, "start")),
                parseService(text(appt, "service")), text(appt, "clinician").simplified(),
                parseStatus(text(appt, "status")), text(appt, "note")
            };
            if (!appointmentIds.insert(appointment.id).second) {
                throw ClinicError("Duplicate appointment ID in the clinic file.");
            }
            if (appointment.status == Status::Pending) {
                const auto slot = appointment.start.toMSecsSinceEpoch();
                if (!patientSlots.insert(slot).second
                    || !clinicianSlots.emplace(slot, appointment.clinician.toCaseFolded()).second) {
                    throw ClinicError("Two pending appointments in the clinic file share a slot.");
                }
            }
            snapshot.appointments.push_back(std::move(appointment));
        }
        auto record = makePatient(std::move(snapshot));
        state.database.insert(state.database.records().size(), std::move(record));
    }

    std::set<int> queuedIds;
    for (const auto entry : array(root, "waiting")) {
        const auto item = object(entry, "waiting entry");
        const int id = item.value("patientId").toInt();
        if (patientIds.count(id) == 0 || !queuedIds.insert(id).second) {
            throw ClinicError("The waiting queue refers to a missing or duplicate patient.");
        }
        state.waiting.push({id, readTime(item, "arrived")});
    }
    return state;
}

} // namespace clinic::io
