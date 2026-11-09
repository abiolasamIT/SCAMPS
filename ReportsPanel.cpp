#include "ReportsPanel.h"

#include "../controller/Reports.h"
#include "../utils/ClinicException.h"

#include <QDate>
#include <QDateEdit>
#include <QAbstractItemView>
#include <QColor>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

namespace scapms::gui {
namespace {

QWidget* makeSummaryCard(const QString& title, QLabel*& value, QWidget* parent)
{
    auto* card = new QGroupBox(parent);
    auto* layout = new QVBoxLayout(card);
    auto* titleLabel = new QLabel(title, card);
    value = new QLabel(QStringLiteral("0"), card);
    value->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: bold;"));
    value->setToolTip(title);
    layout->addWidget(titleLabel);
    layout->addWidget(value);
    return card;
}

void setRow(QTableWidget* table, int row, const QStringList& cells)
{
    table->insertRow(row);
    for (int column = 0; column < cells.size(); ++column) {
        table->setItem(row, column, new QTableWidgetItem(cells[column]));
    }
}

void setRowColor(QTableWidget* table, int row, const QColor& color)
{
    if (!color.isValid()) return;
    for (int column = 0; column < table->columnCount(); ++column) {
        if (auto* item = table->item(row, column)) item->setBackground(color);
    }
}

QColor patientCategoryColor(const std::string& category)
{
    if (category == "Emergency") return QColor(QStringLiteral("#4A2A2A"));
    if (category == "Follow-up") return QColor(QStringLiteral("#2a3a4a"));
    return {};
}

} // namespace

ReportsPanel::ReportsPanel(controller::ClinicManager& manager, QWidget* parent)
    : QWidget(parent), manager_(manager)
{
    buildUi();
    refresh();
    timer_->start(30000);
}

void ReportsPanel::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    auto* actions = new QHBoxLayout();
    refreshButton_ = new QPushButton(tr("Refresh"), this);
    exportButton_ = new QPushButton(tr("Export Report"), this);
    refreshButton_->setToolTip(tr("Refresh summary cards and all report tables."));
    exportButton_->setToolTip(tr("Save the current dashboard report as a text file."));
    actions->addStretch();
    actions->addWidget(refreshButton_);
    actions->addWidget(exportButton_);
    outer->addLayout(actions);

    auto* content = new QWidget(this);
    auto* contentLayout = new QVBoxLayout(content);
    auto* cards = new QHBoxLayout();
    cards->addWidget(makeSummaryCard(tr("Total patients"), patientCountValue_, content));
    cards->addWidget(makeSummaryCard(tr("Total appointments"), appointmentCountValue_, content));
    cards->addWidget(makeSummaryCard(tr("Waiting queue"), queueLengthValue_, content));
    contentLayout->addLayout(cards);

    auto* reportsGrid = new QGridLayout();
    auto* frequentGroup = new QGroupBox(tr("Most frequently attended patients"), content);
    auto* frequentLayout = new QVBoxLayout(frequentGroup);
    frequentTable_ = makeTable(frequentGroup, {tr("Patient ID"), tr("Name"), tr("Completed visits")});
    frequentTable_->setToolTip(tr("Patients ranked by completed appointment count."));
    frequentLayout->addWidget(frequentTable_);

    auto* pendingGroup = new QGroupBox(tr("Pending appointments"), content);
    auto* pendingLayout = new QVBoxLayout(pendingGroup);
    pendingTable_ = makeTable(pendingGroup, {tr("Appointment ID"), tr("Patient"), tr("Date"), tr("Time")});
    pendingTable_->setToolTip(tr("Appointments that are still pending."));
    pendingLayout->addWidget(pendingTable_);

    auto* categoryGroup = new QGroupBox(tr("Patient category distribution"), content);
    auto* categoryLayout = new QVBoxLayout(categoryGroup);
    categoryTable_ = makeTable(categoryGroup, {tr("Category"), tr("Patient count")});
    categoryTable_->setToolTip(tr("Patient counts grouped by category."));
    categoryLayout->addWidget(categoryTable_);

    auto* dailyGroup = new QGroupBox(tr("Daily appointment summary"), content);
    auto* dailyLayout = new QVBoxLayout(dailyGroup);
    auto* dateRow = new QHBoxLayout();
    dateEdit_ = new QDateEdit(QDate::currentDate(), dailyGroup);
    dateEdit_->setCalendarPopup(true);
    dateEdit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    dateEdit_->setToolTip(tr("Choose the date for the daily appointment totals."));
    dateRow->addWidget(new QLabel(tr("Date:"), dailyGroup));
    dateRow->addWidget(dateEdit_);
    dateRow->addStretch();
    dailyLayout->addLayout(dateRow);
    dailyTable_ = makeTable(dailyGroup, {tr("Date"), tr("Total"), tr("Completed"),
                                         tr("Pending"), tr("Cancelled")});
    dailyTable_->setToolTip(tr("Appointment totals for the selected date."));
    dailyLayout->addWidget(dailyTable_);

    reportsGrid->addWidget(frequentGroup, 0, 0);
    reportsGrid->addWidget(pendingGroup, 0, 1);
    reportsGrid->addWidget(categoryGroup, 1, 0);
    reportsGrid->addWidget(dailyGroup, 1, 1);
    contentLayout->addLayout(reportsGrid);
    contentLayout->addWidget(new QLabel(
        tr("Legend: Emergency — light red; Follow-up — light blue; Pending — amber; "
           "Completed — green; Cancelled — grey."), content));

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    timer_ = new QTimer(this);
    timer_->setInterval(30000);
    connect(timer_, &QTimer::timeout, this, &ReportsPanel::refresh);
    connect(refreshButton_, &QPushButton::clicked, this, &ReportsPanel::refresh);
    connect(exportButton_, &QPushButton::clicked, this, &ReportsPanel::exportReport);
    connect(dateEdit_, &QDateEdit::dateChanged, this, [this](const QDate&) {
        refreshDailySummary();
    });
}

QTableWidget* ReportsPanel::makeTable(QWidget* parent, const QStringList& headers)
{
    auto* table = new QTableWidget(parent);
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setMinimumHeight(125);
    return table;
}

void ReportsPanel::refresh()
{
    const auto& database = manager_.database();
    patientCountValue_->setText(QString::number(
        static_cast<qulonglong>(manager_.totalPatientCount())));
    appointmentCountValue_->setText(QString::number(
        static_cast<qulonglong>(manager_.totalAppointmentCount())));
    queueLengthValue_->setText(QString::number(
        static_cast<qulonglong>(manager_.queueSnapshot().size())));

    const auto frequent = controller::Reports::mostFrequentPatients(database);
    frequentTable_->setRowCount(0);
    for (const auto& row : frequent) {
        const int index = frequentTable_->rowCount();
        setRow(frequentTable_, index, {
            QString::fromStdString(row.patientId), QString::fromStdString(row.patientName),
            QString::number(static_cast<qulonglong>(row.completedAppointments))});
        if (const auto* patient = manager_.getPatient(row.patientId)) {
            setRowColor(frequentTable_, index, patientCategoryColor(patient->getCategory()));
        }
    }

    const auto pending = controller::Reports::pendingAppointments(database);
    pendingTable_->setRowCount(0);
    for (const auto& row : pending) {
        const int index = pendingTable_->rowCount();
        setRow(pendingTable_, index, {
            QString::fromStdString(row.appointmentId),
            QString::fromStdString(row.patientName) + QStringLiteral(" (") +
                QString::fromStdString(row.patientId) + QStringLiteral(")"),
            QString::fromStdString(row.date), QString::fromStdString(row.time)});
        setRowColor(pendingTable_, index, QColor(QStringLiteral("#4A2A2A")));
    }

    const auto distribution = controller::Reports::categoryDistribution(database);
    categoryTable_->setRowCount(0);
    for (const auto& row : distribution) {
        const int index = categoryTable_->rowCount();
        setRow(categoryTable_, index, {
            QString::fromStdString(row.category),
            QString::number(static_cast<qulonglong>(row.count))});
        setRowColor(categoryTable_, index, patientCategoryColor(row.category));
    }
    refreshDailySummary();
}

void ReportsPanel::refreshDailySummary()
{
    try {
        const auto summary = controller::Reports::dailySummary(
            manager_.database(), dateEdit_->date().toString(Qt::ISODate).toStdString());
        refreshDailySummary(summary);
    } catch (const model::ValidationException& error) {
        QMessageBox::warning(this, tr("Daily report error"),
                             tr("Error: %1").arg(QString::fromUtf8(error.what())));
    }
}

void ReportsPanel::refreshDailySummary(const controller::DailySummary& summary)
{
    dailyTable_->setRowCount(0);
    setRow(dailyTable_, 0, {
        QString::fromStdString(summary.date),
        QString::number(static_cast<qulonglong>(summary.total)),
        QString::number(static_cast<qulonglong>(summary.completed)),
        QString::number(static_cast<qulonglong>(summary.pending)),
        QString::number(static_cast<qulonglong>(summary.cancelled))});
    dailyTable_->item(0, 2)->setBackground(QColor(QStringLiteral("#2A3A4A")));
    dailyTable_->item(0, 3)->setBackground(QColor(QStringLiteral("#4A4020")));
    dailyTable_->item(0, 4)->setBackground(QColor(QStringLiteral("#4A2A2A")));
}

void ReportsPanel::exportReport()
{
    const QString fileName = QFileDialog::getSaveFileName(
        this, tr("Export clinic report"), QStringLiteral("SCAPMS-report.txt"),
        tr("Text files (*.txt)"));
    if (fileName.isEmpty()) return;

    const auto& database = manager_.database();
    const auto frequent = controller::Reports::mostFrequentPatients(database);
    const auto pending = controller::Reports::pendingAppointments(database);
    const auto distribution = controller::Reports::categoryDistribution(database);
    const auto daily = controller::Reports::dailySummary(
        database, dateEdit_->date().toString(Qt::ISODate).toStdString());

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Export failed"), file.errorString());
        return;
    }
    QTextStream out(&file);
    out << "SCAPMS Dashboard Report\n"
        << "Total patients: " << static_cast<qulonglong>(manager_.totalPatientCount()) << '\n'
        << "Total appointments: " << static_cast<qulonglong>(manager_.totalAppointmentCount()) << '\n'
        << "Waiting queue length: " << static_cast<qulonglong>(manager_.queueSnapshot().size()) << "\n\n"
        << "Most frequently attended patients\n";
    for (const auto& row : frequent) {
        out << QString::fromStdString(row.patientId) << " | "
            << QString::fromStdString(row.patientName) << " | completed: "
            << static_cast<qulonglong>(row.completedAppointments) << '\n';
    }
    out << "\nPending appointments\n";
    for (const auto& row : pending) {
        out << QString::fromStdString(row.appointmentId) << " | "
            << QString::fromStdString(row.patientName) << " | "
            << QString::fromStdString(row.date) << ' '
            << QString::fromStdString(row.time) << '\n';
    }
    out << "\nPatient category distribution\n";
    for (const auto& row : distribution) {
        out << QString::fromStdString(row.category) << " | "
            << static_cast<qulonglong>(row.count) << '\n';
    }
    out << "\nDaily appointment summary for " << QString::fromStdString(daily.date) << '\n'
        << "Total: " << static_cast<qulonglong>(daily.total)
        << ", completed: " << static_cast<qulonglong>(daily.completed)
        << ", pending: " << static_cast<qulonglong>(daily.pending)
        << ", cancelled: " << static_cast<qulonglong>(daily.cancelled) << '\n';
    file.close();
    QMessageBox::information(this, tr("Report exported"),
                             tr("Report saved to %1").arg(fileName));
}

} // namespace scapms::gui
