#pragma once

#include "../controller/ClinicManager.h"
#include "../utils/ClinicException.h"

#include <QWidget>

#include <memory>
#include <string>

class QComboBox;
class QFormLayout;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QString;
class QTableWidget;

namespace scapms::gui {

class PatientPanel final : public QWidget {
    Q_OBJECT

public:
    explicit PatientPanel(controller::ClinicManager& manager, QWidget* parent = nullptr);

    void refresh();

signals:
    void patientsChanged();

private slots:
    void onCategoryChanged(const QString& category);
    void onCurrentCellChanged(int currentRow, int currentColumn,
                              int previousRow, int previousColumn);
    void addPatient();
    void editPatient();
    void deletePatient();
    void undoLastOperation();
    void clearForm();

private:
    controller::ClinicManager& manager_;
    QTableWidget* table_ = nullptr;
    QFormLayout* formLayout_ = nullptr;
    QLineEdit* idEdit_ = nullptr;
    QLineEdit* nameEdit_ = nullptr;
    QLineEdit* ageEdit_ = nullptr;
    QLineEdit* contactEdit_ = nullptr;
    QComboBox* categoryCombo_ = nullptr;
    QLineEdit* generalReasonEdit_ = nullptr;
    QSpinBox* severitySpin_ = nullptr;
    QLineEdit* emergencyDetailsEdit_ = nullptr;
    QLineEdit* previousVisitEdit_ = nullptr;
    QLineEdit* followUpReasonEdit_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* editButton_ = nullptr;
    QPushButton* deleteButton_ = nullptr;
    QPushButton* undoButton_ = nullptr;
    QPushButton* clearButton_ = nullptr;
    std::string selectedPatientId_;

    void buildUi();
    std::unique_ptr<model::PatientRecord> makePatient(const std::string& id) const;
    void populateForm(const model::PatientRecord& patient);
    void setRowCategoryColor(int row, const QString& category);
    void showException(const model::ValidationException& error);
};

} // namespace scapms::gui
