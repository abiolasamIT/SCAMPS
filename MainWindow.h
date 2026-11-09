#pragma once

#include "../controller/AppointmentController.h"

#include <QMainWindow>

class QLabel;
class QString;
class QWidget;
class QCloseEvent;

namespace scapms::gui {

class PatientPanel;
class AppointmentPanel;
class QueuePanel;
class SearchPanel;
class ReportsPanel;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void newFile();
    void saveFile();
    void loadFile();
    void importPatients();
    void exportPatients();
    void exitApplication();
    void showAbout();
    void undoLastOperation();
    void focusSearch();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    controller::ClinicManager manager_;
    controller::AppointmentController appointmentController_;
    PatientPanel* patientPanel_ = nullptr;
    AppointmentPanel* appointmentPanel_ = nullptr;
    QueuePanel* queuePanel_ = nullptr;
    SearchPanel* searchPanel_ = nullptr;
    ReportsPanel* reportsPanel_ = nullptr;
    QLabel* patientCountLabel_ = nullptr;
    QLabel* queueLengthLabel_ = nullptr;
    QLabel* lastActionLabel_ = nullptr;
    bool dirty_ = false;

    QWidget* createPlaceholderPanel(const QString& panelName);
    void buildMenus();
    void buildTabs();
    void buildStatusBar();
    void refreshStatus(const QString& action);
    void refreshAllPanels();
    bool saveToChosenFolder();
    void markDirty(const QString& action);
};

} // namespace scapms::gui
