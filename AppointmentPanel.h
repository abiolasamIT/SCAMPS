#pragma once

#include "../controller/AppointmentController.h"
#include "../utils/ClinicException.h"

#include <QWidget>

#include <string>

class QComboBox;
class QDateEdit;
class QLineEdit;
class QPushButton;
class QString;
class QTableWidget;
class QTimeEdit;

namespace scapms::gui {

class AppointmentPanel final : public QWidget {
    Q_OBJECT

public:
    AppointmentPanel(controller::ClinicManager& manager,
                     controller::AppointmentController& appointmentController,
                     QWidget* parent = nullptr);

    void refresh();

signals:
    void appointmentsChanged();

private slots:
    void scheduleAppointment();
    void cancelAppointment();
    void markAppointmentCompleted();
    void onFilterChanged(const QString& filter);
    void onCurrentCellChanged(int currentRow, int currentColumn,
                              int previousRow, int previousColumn);

private:
    controller::ClinicManager& manager_;
    controller::AppointmentController& appointmentController_;
    QComboBox* patientCombo_ = nullptr;
    QDateEdit* dateEdit_ = nullptr;
    QTimeEdit* timeEdit_ = nullptr;
    QLineEdit* notesEdit_ = nullptr;
    QComboBox* filterCombo_ = nullptr;
    QTableWidget* table_ = nullptr;
    QPushButton* scheduleButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
    QPushButton* completedButton_ = nullptr;
    std::string selectedAppointmentId_;

    void buildUi();
    void setRowStatusColor(int row, const QString& status);
    void showValidationError(const model::ValidationException& error);
};

} // namespace scapms::gui
