#pragma once

#include "model/ClinicDatabase.h"

namespace clinic::io {

class FileHandler {
public:
    static void save(const QString& path, const model::ClinicState& state);
    static model::ClinicState load(const QString& path);
};

} // namespace clinic::io
