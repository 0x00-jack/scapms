#include "controller/SearchEngine.h"

namespace clinic::controller {

using namespace model;

namespace {

bool less(const PatientRecord* left, const PatientRecord* right, SortField field) {
    switch (field) {
    case SortField::Id: return left->info().id < right->info().id;
    case SortField::Name:
        return left->info().name.compare(right->info().name, Qt::CaseInsensitive) < 0;
    case SortField::Age: return left->info().age < right->info().age;
    }
    return false;
}

void insertionSort(PatientView& patients, SortField field) {
    for (std::size_t index = 1; index < patients.size(); ++index) {
        const auto* current = patients[index];
        std::size_t position = index;
        while (position > 0 && less(current, patients[position - 1], field)) {
            patients[position] = patients[position - 1];
            --position;
        }
        patients[position] = current;
    }
}

void mergeSort(PatientView& patients, PatientView& scratch, SortField field,
               std::size_t begin, std::size_t end) {
    if (end - begin < 2) {
        return;
    }
    const auto middle = begin + (end - begin) / 2;
    mergeSort(patients, scratch, field, begin, middle);
    mergeSort(patients, scratch, field, middle, end);

    auto left = begin;
    auto right = middle;
    for (auto out = begin; out < end; ++out) {
        // Choose the left entry on ties so the algorithm is stable.
        if (right == end || (left < middle && !less(patients[right], patients[left], field))) {
            scratch[out] = patients[left++];
        } else {
            scratch[out] = patients[right++];
        }
    }
    for (auto out = begin; out < end; ++out) {
        patients[out] = scratch[out];
    }
}

void collectNames(const PatientView& patients, const QString& query,
                  std::size_t begin, std::size_t end, PatientView& results) {
    if (begin == end) {
        return;
    }
    if (end - begin == 1) {
        if (patients[begin]->info().name.contains(query, Qt::CaseInsensitive)) {
            results.push_back(patients[begin]);
        }
        return;
    }
    const auto middle = begin + (end - begin) / 2;
    collectNames(patients, query, begin, middle, results);
    collectNames(patients, query, middle, end, results);
}

} // namespace

PatientView SearchEngine::viewOf(const PatientList& patients) {
    PatientView view;
    view.reserve(patients.size());
    for (const auto& patient : patients) {
        view.push_back(patient.get());
    }
    return view;
}

std::size_t SearchEngine::linearId(const PatientList& patients, int id) {
    for (std::size_t index = 0; index < patients.size(); ++index) {
        if (patients[index]->info().id == id) {
            return index;
        }
    }
    return patients.size();
}

const PatientRecord* SearchEngine::binaryId(const PatientView& ordered, int id) {
    std::size_t begin = 0;
    auto end = ordered.size();
    while (begin < end) {
        const auto middle = begin + (end - begin) / 2;
        const auto* candidate = ordered[middle];
        if (candidate->info().id == id) {
            return candidate;
        }
        if (candidate->info().id < id) {
            begin = middle + 1;
        } else {
            end = middle;
        }
    }
    return nullptr;
}

PatientView SearchEngine::linearMatches(const PatientView& patients,
                                       SearchField field, const QString& query) {
    PatientView results;
    for (const auto* patient : patients) {
        const bool matches = field == SearchField::Name
            ? patient->info().name.contains(query, Qt::CaseInsensitive)
            : label(patient->category()).compare(query, Qt::CaseInsensitive) == 0;
        if (matches) {
            results.push_back(patient);
        }
    }
    return results;
}

PatientView SearchEngine::recursiveName(const PatientView& patients, const QString& query) {
    PatientView results;
    collectNames(patients, query, 0, patients.size(), results);
    return results;
}

void SearchEngine::sort(PatientView& patients, SortField field, SortAlgorithm algorithm) {
    if (algorithm == SortAlgorithm::Insertion) {
        insertionSort(patients, field);
    } else {
        PatientView scratch(patients.size());
        mergeSort(patients, scratch, field, 0, patients.size());
    }
}

} // namespace clinic::controller
