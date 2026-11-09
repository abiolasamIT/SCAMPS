#pragma once

#include "../controller/ClinicManager.h"
#include "../controller/Reports.h"

#include <QWidget>
#include <QStringList>

class QLabel;
class QDateEdit;
class QPushButton;
class QTableWidget;
class QTimer;

namespace scapms::gui {

class ReportsPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ReportsPanel(controller::ClinicManager& manager, QWidget* parent = nullptr);
    void refresh();

private slots:
    void refreshDailySummary();
    void exportReport();

private:
    controller::ClinicManager& manager_;
    QLabel* patientCountValue_ = nullptr;
    QLabel* appointmentCountValue_ = nullptr;
    QLabel* queueLengthValue_ = nullptr;
    QDateEdit* dateEdit_ = nullptr;
    QTableWidget* frequentTable_ = nullptr;
    QTableWidget* pendingTable_ = nullptr;
    QTableWidget* categoryTable_ = nullptr;
    QTableWidget* dailyTable_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QPushButton* exportButton_ = nullptr;
    QTimer* timer_ = nullptr;

    void buildUi();
    static QTableWidget* makeTable(QWidget* parent, const QStringList& headers);
    void refreshDailySummary(const controller::DailySummary& summary);
};

} // namespace scapms::gui
