#pragma once

#include "controller/ClinicManager.h"

class QWidget;

namespace clinic::gui {

bool patientDialog(QWidget* parent, controller::ClinicManager& manager, int id = 0);
bool scheduleDialog(QWidget* parent, controller::ClinicManager& manager);
bool enqueueDialog(QWidget* parent, controller::ClinicManager& manager);
bool completeDialog(QWidget* parent, controller::ClinicManager& manager, int appointmentId);

} // namespace clinic::gui
