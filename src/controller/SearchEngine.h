#pragma once

#include "model/ClinicDatabase.h"

namespace clinic::controller {

enum class SearchField { Id, Name, Category };
enum class SearchAlgorithm { Linear, Binary, Recursive };
enum class SortField { Id, Name, Age };
enum class SortAlgorithm { Insertion, Merge };

class SearchEngine {
public:
    static model::PatientView viewOf(const model::PatientList& patients);
    static std::size_t linearId(const model::PatientList& patients, int id);
    static const model::PatientRecord* binaryId(const model::PatientView& ordered, int id);
    static model::PatientView linearMatches(const model::PatientView& patients,
                                            SearchField field, const QString& query);
    static model::PatientView recursiveName(const model::PatientView& patients,
                                            const QString& query);
    static void sort(model::PatientView& patients, SortField field, SortAlgorithm algorithm);
};

} // namespace clinic::controller
