#pragma once

#include "controller/ClinicManager.h"

#include <QMainWindow>
#include <array>

class QAction;
class QCloseEvent;
class QDateEdit;
class QLabel;
class QPushButton;
class QTabWidget;

namespace clinic::gui {

class RecordTable;
class SearchPanel;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(const QString& initialPath = {});

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QWidget* makeDashboard();
    QWidget* makePatients();
    QWidget* makeAppointments();
    QWidget* makeQueue();
    QWidget* makeReports();
    void makeMenus();

    void refresh();
    void refreshDashboard();
    void refreshQueue();
    void refreshReports();
    void updateFileStatus();
    void changed(const QString& message);

    void openPatient(int id);
    void scheduleAppointment();
    void undo();
    bool confirmFileChange();
    bool save(bool saveAs);
    void load(const QString& path);

    controller::ClinicManager manager_;
    QString path_;
    bool dirty_ = false;

    QTabWidget* tabs_;
    SearchPanel* search_;
    QAction* undoAction_;
    QPushButton* undoButton_;
    QLabel* clock_;
    QLabel* fileStatus_;
    std::array<QLabel*, 4> metrics_{};
    RecordTable* reminders_;
    RecordTable* recent_;
    RecordTable* patients_;
    RecordTable* appointments_;
    RecordTable* waiting_;
    RecordTable* attendance_;
    RecordTable* pending_;
    RecordTable* categories_;
    RecordTable* daily_;
    QDateEdit* reportDate_;
};

} // namespace clinic::gui
