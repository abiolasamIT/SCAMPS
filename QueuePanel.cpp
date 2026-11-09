#include "QueuePanel.h"

#include "../utils/ClinicException.h"

#include <QColor>
#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace scapms::gui {

QueuePanel::QueuePanel(controller::ClinicManager& manager, QWidget* parent)
    : QWidget(parent), manager_(manager)
{
    buildUi();
    refresh();
    timer_->start(30000);
}

void QueuePanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout();
    patientCombo_ = new QComboBox(this);
    patientCombo_->setToolTip(tr("Choose a registered patient to add to the waiting queue."));
    addButton_ = new QPushButton(tr("Add Selected Patient to Queue"), this);
    callNextButton_ = new QPushButton(tr("Call Next Patient"), this);
    addButton_->setToolTip(tr("Place the selected patient at the end of the waiting queue."));
    callNextButton_->setToolTip(tr("Call the patient who has waited longest."));
    controls->addWidget(patientCombo_, 1);
    controls->addWidget(addButton_);
    controls->addWidget(callNextButton_);
    root->addLayout(controls);

    servingLabel_ = new QLabel(tr("Currently being served: None"), this);
    servingLabel_->setToolTip(tr("The most recently called patient remains here until another is called."));
    root->addWidget(servingLabel_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({tr("Position"), tr("Patient"), tr("Waiting time")});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setToolTip(tr("Patients are listed in the order they joined the queue."));
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    root->addWidget(table_, 1);
    root->addWidget(new QLabel(
        tr("Legend: Emergency — light red; Follow-up — light blue."), this));

    timer_ = new QTimer(this);
    timer_->setInterval(30000);
    connect(timer_, &QTimer::timeout, this, &QueuePanel::updateWaitingTimes);
    connect(addButton_, &QPushButton::clicked, this, &QueuePanel::addSelectedPatient);
    connect(callNextButton_, &QPushButton::clicked, this, &QueuePanel::callNextPatient);
}

void QueuePanel::refresh()
{
    const QString selectedId = patientCombo_->currentData().toString();
    {
        const QSignalBlocker blocker(patientCombo_);
        patientCombo_->clear();
        for (const auto* patient : manager_.getPatients()) {
            const QString id = QString::fromStdString(patient->getId());
            const QString label = QString::fromStdString(patient->getName()) +
                                  QStringLiteral(" (") + id + QStringLiteral(")");
            patientCombo_->addItem(label, id);
        }
        const int index = patientCombo_->findData(selectedId);
        if (index >= 0) patientCombo_->setCurrentIndex(index);
    }

    const auto queueItems = manager_.queueDetailsSnapshot();
    std::set<std::string> activeIds;
    const QSignalBlocker tableBlocker(table_);
    table_->setRowCount(0);
    int position = 1;
    for (const auto& entry : queueItems) {
        activeIds.insert(entry.patientId);
        const int row = table_->rowCount();
        table_->insertRow(row);
        table_->setItem(row, 0, new QTableWidgetItem(QString::number(position++)));
        table_->setItem(row, 1, new QTableWidgetItem(
            QString::fromStdString(entry.patientName) + QStringLiteral(" (") +
            QString::fromStdString(entry.patientId) + QStringLiteral(")")));
        table_->setItem(row, 2, new QTableWidgetItem(
            formatWaitingTime(entry.waitingTime.count())));
        if (entry.category == "Emergency" || entry.category == "Follow-up") {
            const QColor highlight(entry.category == "Emergency"
                ? QStringLiteral("#ffe0e0") : QStringLiteral("#e0f0ff"));
            for (int column = 0; column < table_->columnCount(); ++column) {
                table_->item(row, column)->setBackground(highlight);
            }
        }
        if (entry.waitingTime.count() > static_cast<long long>(alertAfterMinutes_) * 60 &&
            alertedPatientIds_.insert(entry.patientId).second) {
            emit statusMessageRequested(
                tr("Warning: %1 has waited over %2 minutes in the queue.")
                    .arg(QString::fromStdString(entry.patientName))
                    .arg(alertAfterMinutes_));
        }
    }
    for (auto it = alertedPatientIds_.begin(); it != alertedPatientIds_.end();) {
        if (activeIds.find(*it) == activeIds.end()) it = alertedPatientIds_.erase(it);
        else ++it;
    }

    const std::string servingId = manager_.currentlyBeingServed();
    if (servingId.empty()) {
        servingLabel_->setText(tr("Currently being served: None"));
    } else {
        const auto* patient = manager_.getPatient(servingId);
        const QString display = patient
            ? QString::fromStdString(patient->getName()) + QStringLiteral(" (") +
              QString::fromStdString(servingId) + QStringLiteral(")")
            : QString::fromStdString(servingId);
        servingLabel_->setText(tr("Currently being served: %1").arg(display));
    }
}

QString QueuePanel::formatWaitingTime(long long seconds)
{
    const long long hours = seconds / 3600;
    const long long minutes = (seconds % 3600) / 60;
    const long long remainingSeconds = seconds % 60;
    if (hours > 0) {
        return tr("%1h %2m %3s").arg(hours).arg(minutes).arg(remainingSeconds);
    }
    return tr("%1m %2s").arg(minutes).arg(remainingSeconds);
}

void QueuePanel::addSelectedPatient()
{
    if (patientCombo_->currentIndex() < 0) {
        QMessageBox::warning(this, tr("Waiting queue"), tr("Add or select a patient first."));
        return;
    }
    const std::string patientId = patientCombo_->currentData().toString().toStdString();
    const auto existing = manager_.queueSnapshot();
    for (const auto& queuedId : existing) {
        if (queuedId == patientId) {
            QMessageBox::warning(this, tr("Waiting queue"), tr("That patient is already waiting."));
            return;
        }
    }
    try {
        manager_.addToQueue(patientId);
        refresh();
        emit queueChanged();
    } catch (const model::ValidationException& error) {
        QMessageBox::warning(this, tr("Waiting queue"),
                             tr("Error: %1").arg(QString::fromUtf8(error.what())));
    }
}

void QueuePanel::callNextPatient()
{
    const std::string patientId = manager_.callNextPatient();
    if (patientId.empty()) {
        QMessageBox::information(this, tr("Waiting queue"), tr("The waiting queue is empty."));
        return;
    }
    refresh();
    emit queueChanged();
}

void QueuePanel::updateWaitingTimes()
{
    refresh();
}

} // namespace scapms::gui
