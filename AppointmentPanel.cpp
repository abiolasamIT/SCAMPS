#include "AppointmentPanel.h"

#include "../utils/ClinicException.h"

#include <QColor>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTime>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace scapms::gui {

AppointmentPanel::AppointmentPanel(controller::ClinicManager& manager,
                                   controller::AppointmentController& appointmentController,
                                   QWidget* parent)
    : QWidget(parent), manager_(manager), appointmentController_(appointmentController)
{
    buildUi();
    refresh();
}

void AppointmentPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    patientCombo_ = new QComboBox(this);
    patientCombo_->setToolTip(tr("Choose the patient for this appointment."));
    dateEdit_ = new QDateEdit(QDate::currentDate().addDays(1), this);
    dateEdit_->setCalendarPopup(true);
    dateEdit_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    dateEdit_->setToolTip(tr("Choose the appointment date."));
    timeEdit_ = new QTimeEdit(QTime::currentTime(), this);
    timeEdit_->setDisplayFormat(QStringLiteral("HH:mm"));
    timeEdit_->setToolTip(tr("Choose the appointment time."));
    notesEdit_ = new QLineEdit(this);
    notesEdit_->setPlaceholderText(tr("Optional appointment notes"));
    notesEdit_->setToolTip(tr("Add optional notes for the appointment."));
    form->addRow(tr("Patient:"), patientCombo_);
    form->addRow(tr("Date:"), dateEdit_);
    form->addRow(tr("Time:"), timeEdit_);
    form->addRow(tr("Notes:"), notesEdit_);
    root->addLayout(form);

    auto* buttons = new QHBoxLayout();
    scheduleButton_ = new QPushButton(tr("Schedule"), this);
    cancelButton_ = new QPushButton(tr("Cancel"), this);
    completedButton_ = new QPushButton(tr("Mark Completed"), this);
    scheduleButton_->setToolTip(tr("Schedule an appointment for the selected patient."));
    cancelButton_->setToolTip(tr("Cancel the selected pending appointment."));
    completedButton_->setToolTip(tr("Mark the selected pending appointment as completed."));
    buttons->addWidget(scheduleButton_);
    buttons->addWidget(cancelButton_);
    buttons->addWidget(completedButton_);
    buttons->addStretch();
    buttons->addWidget(new QLabel(tr("Status filter:"), this));
    filterCombo_ = new QComboBox(this);
    filterCombo_->addItems({tr("All"), tr("Pending"), tr("Completed"), tr("Cancelled")});
    filterCombo_->setToolTip(tr("Show all appointments or filter by status."));
    buttons->addWidget(filterCombo_);
    root->addLayout(buttons);

    table_ = new QTableWidget(this);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({tr("ID"), tr("Patient"), tr("Date"),
                                       tr("Time"), tr("Status")});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setToolTip(tr("Select a row to cancel or complete a pending appointment."));
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    root->addWidget(table_, 1);
    root->addWidget(new QLabel(
        tr("Legend: Pending — amber; Completed — green; Cancelled — grey."), this));

    connect(scheduleButton_, &QPushButton::clicked, this, &AppointmentPanel::scheduleAppointment);
    connect(cancelButton_, &QPushButton::clicked, this, &AppointmentPanel::cancelAppointment);
    connect(completedButton_, &QPushButton::clicked, this, &AppointmentPanel::markAppointmentCompleted);
    connect(filterCombo_, &QComboBox::currentTextChanged, this, &AppointmentPanel::onFilterChanged);
    connect(table_, &QTableWidget::currentCellChanged,
            this, &AppointmentPanel::onCurrentCellChanged);
}

void AppointmentPanel::refresh()
{
    const QString selectedPatient = patientCombo_->currentData().toString();
    {
        const QSignalBlocker blocker(patientCombo_);
        patientCombo_->clear();
        for (const auto* patient : manager_.getPatients()) {
            const QString id = QString::fromStdString(patient->getId());
            const QString label = QString::fromStdString(patient->getName()) +
                                  QStringLiteral(" (") + id + QStringLiteral(")");
            patientCombo_->addItem(label, id);
        }
        const int index = patientCombo_->findData(selectedPatient);
        if (index >= 0) patientCombo_->setCurrentIndex(index);
    }

    const QString filter = filterCombo_->currentText();
    const QSignalBlocker tableBlocker(table_);
    table_->setRowCount(0);
    int selectedRow = -1;
    for (const auto& appointment : manager_.getAppointments()) {
        const QString status = QString::fromStdString(
            model::Appointment::statusToString(appointment.getStatus()));
        if (filter != tr("All") && filter != status) continue;

        const int row = table_->rowCount();
        table_->insertRow(row);
        const QString id = QString::fromStdString(appointment.getId());
        const auto* patient = manager_.getPatient(appointment.getPatientId());
        const QString patientName = patient
            ? QString::fromStdString(patient->getName()) : QString::fromStdString(appointment.getPatientId());
        const QString values[] = {
            id,
            patientName + QStringLiteral(" (") + QString::fromStdString(appointment.getPatientId()) + QStringLiteral(")"),
            QString::fromStdString(appointment.getDate()),
            QString::fromStdString(appointment.getTime()),
            status
        };
        for (int column = 0; column < 5; ++column) {
            table_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
        setRowStatusColor(row, status);
        if (appointment.getId() == selectedAppointmentId_) selectedRow = row;
    }
    if (selectedRow >= 0) {
        table_->setCurrentCell(selectedRow, 0);
    } else {
        selectedAppointmentId_.clear();
    }
}

void AppointmentPanel::onFilterChanged(const QString&)
{
    refresh();
}

void AppointmentPanel::onCurrentCellChanged(int currentRow, int, int, int)
{
    if (currentRow < 0) {
        selectedAppointmentId_.clear();
        return;
    }
    if (const auto* item = table_->item(currentRow, 0)) {
        selectedAppointmentId_ = item->text().toStdString();
    }
}

void AppointmentPanel::setRowStatusColor(int row, const QString& status)
{
    QColor color;
    if (status == tr("Pending")) color = QColor(QStringLiteral("#2a3a4a"));
    else if (status == tr("Completed")) color = QColor(QStringLiteral("#4a2a2a"));
    else if (status == tr("Cancelled")) color = QColor(QStringLiteral("#4a4020"));
    if (!color.isValid()) return;
    for (int column = 0; column < table_->columnCount(); ++column) {
        table_->item(row, column)->setBackground(color);
    }
}

void AppointmentPanel::showValidationError(const model::ValidationException& error)
{
    QMessageBox::warning(this, tr("Appointment error"),
                         tr("Error: %1").arg(QString::fromUtf8(error.what())));
}

void AppointmentPanel::scheduleAppointment()
{
    if (patientCombo_->currentIndex() < 0) {
        QMessageBox::warning(this, tr("Schedule appointment"), tr("Add or select a patient first."));
        return;
    }
    try {
        appointmentController_.schedule(
            patientCombo_->currentData().toString().toStdString(),
            dateEdit_->date().toString(Qt::ISODate).toStdString(),
            timeEdit_->time().toString(QStringLiteral("HH:mm")).toStdString(),
            notesEdit_->text().trimmed().toStdString());
        refresh();
        emit appointmentsChanged();
    } catch (const model::DuplicateIdException&) {
        QMessageBox::warning(this, tr("Appointment error"), tr("Error: Appointment ID already exists."));
    } catch (const model::ValidationException& error) {
        showValidationError(error);
    }
}

void AppointmentPanel::cancelAppointment()
{
    if (selectedAppointmentId_.empty()) {
        QMessageBox::warning(this, tr("Cancel appointment"), tr("Select an appointment row first."));
        return;
    }
    try {
        if (!appointmentController_.cancel(selectedAppointmentId_)) {
            QMessageBox::warning(this, tr("Cancel appointment"), tr("Appointment was not found."));
            return;
        }
        refresh();
        emit appointmentsChanged();
    } catch (const model::DuplicateIdException&) {
        QMessageBox::warning(this, tr("Appointment error"), tr("Error: Appointment ID already exists."));
    } catch (const model::ValidationException& error) {
        showValidationError(error);
    }
}

void AppointmentPanel::markAppointmentCompleted()
{
    if (selectedAppointmentId_.empty()) {
        QMessageBox::warning(this, tr("Complete appointment"), tr("Select an appointment row first."));
        return;
    }
    try {
        if (!appointmentController_.markCompleted(selectedAppointmentId_)) {
            QMessageBox::warning(this, tr("Complete appointment"), tr("Appointment was not found."));
            return;
        }
        refresh();
        emit appointmentsChanged();
    } catch (const model::DuplicateIdException&) {
        QMessageBox::warning(this, tr("Appointment error"), tr("Error: Appointment ID already exists."));
    } catch (const model::ValidationException& error) {
        showValidationError(error);
    }
}

} // namespace scapms::gui
