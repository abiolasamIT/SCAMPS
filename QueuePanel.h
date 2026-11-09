#pragma once

#include "../controller/ClinicManager.h"

#include <QWidget>

#include <set>
#include <string>

class QComboBox;
class QLabel;
class QPushButton;
class QString;
class QTableWidget;
class QTimer;

namespace scapms::gui {

class QueuePanel final : public QWidget {
    Q_OBJECT

public:
    explicit QueuePanel(controller::ClinicManager& manager, QWidget* parent = nullptr);

    void refresh();

signals:
    void queueChanged();
    void statusMessageRequested(const QString& message);

private slots:
    void addSelectedPatient();
    void callNextPatient();
    void updateWaitingTimes();

private:
    static constexpr int alertAfterMinutes_ = 15;

    controller::ClinicManager& manager_;
    QComboBox* patientCombo_ = nullptr;
    QTableWidget* table_ = nullptr;
    QLabel* servingLabel_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* callNextButton_ = nullptr;
    QTimer* timer_ = nullptr;
    std::set<std::string> alertedPatientIds_;

    void buildUi();
    static QString formatWaitingTime(long long seconds);
};

} // namespace scapms::gui
