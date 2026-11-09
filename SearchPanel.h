#pragma once

#include "../controller/ClinicManager.h"

#include <QWidget>

#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QString;
class QTableWidget;

namespace scapms::gui {

class SearchPanel final : public QWidget {
    Q_OBJECT

public:
    explicit SearchPanel(controller::ClinicManager& manager, QWidget* parent = nullptr);
    void refresh();
    void focusSearchField();

private slots:
    void searchNow();
    void sortPatients();
    void updateSortExplanation();

private:
    controller::ClinicManager& manager_;
    QLineEdit* searchEdit_ = nullptr;
    QComboBox* searchByCombo_ = nullptr;
    QComboBox* techniqueCombo_ = nullptr;
    QComboBox* sortAlgorithmCombo_ = nullptr;
    QComboBox* sortByCombo_ = nullptr;
    QCheckBox* ascendingCheck_ = nullptr;
    QPushButton* sortButton_ = nullptr;
    QLabel* techniqueLabel_ = nullptr;
    QLabel* sortExplanationLabel_ = nullptr;
    QTableWidget* table_ = nullptr;

    void buildUi();
    void showPatients(const std::vector<const model::PatientRecord*>& patients);
    void showOnePatient(const model::PatientRecord* patient);
    utils::SortField selectedSortField() const;
    static QString techniqueText(controller::SearchEngine::Technique technique);
};

} // namespace scapms::gui
