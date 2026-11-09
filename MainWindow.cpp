#include "MainWindow.h"

#include "AppointmentPanel.h"
#include "PatientPanel.h"
#include "QueuePanel.h"
#include "SearchPanel.h"
#include "ReportsPanel.h"
#include "../utils/ClinicException.h"

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace scapms::gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), manager_(), appointmentController_(manager_)
{
    setWindowTitle(tr("Smart Clinic Appointment & Patient Management System[*]"));
    resize(1050, 700);
    buildMenus();
    buildTabs();
    buildStatusBar();
    refreshStatus(tr("Ready"));
}

QWidget* MainWindow::createPlaceholderPanel(const QString& panelName)
{
    auto* panel = new QWidget(this);
    auto* layout = new QVBoxLayout(panel);
    auto* placeholder = new QLabel(tr("%1 panel will be added in a later phase.").arg(panelName), panel);
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);
    return panel;
}

void MainWindow::buildMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));

    QAction* newAction = fileMenu->addAction(tr("&New"));
    newAction->setShortcut(QKeySequence::New);
    newAction->setToolTip(tr("Start a new clinic data set."));
    connect(newAction, &QAction::triggered, this, &MainWindow::newFile);

    QAction* saveAction = fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+S")));
    saveAction->setToolTip(tr("Save patients and appointments to CSV files."));
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);

    QAction* loadAction = fileMenu->addAction(tr("&Load"));
    loadAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    loadAction->setToolTip(tr("Load patients and appointments from CSV files."));
    connect(loadAction, &QAction::triggered, this, &MainWindow::loadFile);

    QAction* importAction = fileMenu->addAction(tr("Import Patients from CSV…"));
    importAction->setToolTip(tr("Add patient records from a patients CSV file."));
    connect(importAction, &QAction::triggered, this, &MainWindow::importPatients);
    QAction* exportAction = fileMenu->addAction(tr("Export Patients to CSV…"));
    exportAction->setToolTip(tr("Write patient records to a CSV file."));
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportPatients);

    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));
    exitAction->setToolTip(tr("Exit SCAPMS."));
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitApplication);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* undoAction = editMenu->addAction(tr("Undo Last Administrative Operation"));
    undoAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Z")));
    undoAction->setShortcutContext(Qt::ApplicationShortcut);
    undoAction->setToolTip(tr("Undo the last patient edit, deletion, or appointment removal."));
    connect(undoAction, &QAction::triggered, this, &MainWindow::undoLastOperation);

    QAction* findAction = editMenu->addAction(tr("Focus Search"));
    findAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+F")));
    findAction->setShortcutContext(Qt::ApplicationShortcut);
    findAction->setToolTip(tr("Open Search & Sort and focus the search field."));
    connect(findAction, &QAction::triggered, this, &MainWindow::focusSearch);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::buildTabs()
{
    auto* tabs = new QTabWidget(this);
    patientPanel_ = new PatientPanel(manager_, tabs);
    connect(patientPanel_, &PatientPanel::patientsChanged, this, [this]() {
        appointmentPanel_->refresh();
        queuePanel_->refresh();
        searchPanel_->refresh();
        reportsPanel_->refresh();
        markDirty(tr("Patient records updated"));
    });
    tabs->addTab(patientPanel_, tr("Patient Records"));
    appointmentPanel_ = new AppointmentPanel(manager_, appointmentController_, tabs);
    connect(appointmentPanel_, &AppointmentPanel::appointmentsChanged, this, [this]() {
        searchPanel_->refresh();
        reportsPanel_->refresh();
        markDirty(tr("Appointment updated"));
    });
    tabs->addTab(appointmentPanel_, tr("Appointments"));
    queuePanel_ = new QueuePanel(manager_, tabs);
    connect(queuePanel_, &QueuePanel::queueChanged, this, [this]() {
        reportsPanel_->refresh();
        refreshStatus(tr("Waiting queue updated"));
    });
    connect(queuePanel_, &QueuePanel::statusMessageRequested, this,
            [this](const QString& message) { statusBar()->showMessage(message, 15000); });
    tabs->addTab(queuePanel_, tr("Waiting Queue"));
    searchPanel_ = new SearchPanel(manager_, tabs);
    tabs->addTab(searchPanel_, tr("Search & Sort"));
    reportsPanel_ = new ReportsPanel(manager_, tabs);
    tabs->addTab(reportsPanel_, tr("Reports"));
    setCentralWidget(tabs);
}

void MainWindow::buildStatusBar()
{
    patientCountLabel_ = new QLabel(this);
    queueLengthLabel_ = new QLabel(this);
    lastActionLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(patientCountLabel_);
    statusBar()->addPermanentWidget(queueLengthLabel_);
    statusBar()->addPermanentWidget(lastActionLabel_, 1);
}

void MainWindow::refreshStatus(const QString& action)
{
    patientCountLabel_->setText(tr("Patients: %1").arg(
        static_cast<qulonglong>(manager_.database().patientCount())));
    queueLengthLabel_->setText(tr("Queue: %1").arg(
        static_cast<qulonglong>(manager_.queueSnapshot().size())));
    lastActionLabel_->setText(tr("Last action: %1").arg(action));
}

void MainWindow::newFile()
{
    const auto answer = QMessageBox::question(
        this, tr("New clinic data"),
        tr("Clear all current patients, appointments, and waiting-queue entries?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;
    manager_.clearData();
    refreshAllPanels();
    markDirty(tr("New data set"));
}

void MainWindow::saveFile()
{
    (void)saveToChosenFolder();
}

bool MainWindow::saveToChosenFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Choose folder for clinic CSV files"));
    if (folder.isEmpty()) return false;
    try {
        manager_.saveData(folder.toStdString());
        dirty_ = false;
        setWindowModified(false);
        refreshStatus(tr("Saved clinic data"));
        return true;
    } catch (const model::FileException& error) {
        refreshStatus(tr("Save failed"));
        QMessageBox::critical(this, tr("Save failed"), QString::fromUtf8(error.what()));
    } catch (const std::exception& error) {
        refreshStatus(tr("Save failed"));
        QMessageBox::critical(this, tr("Save failed"), QString::fromUtf8(error.what()));
    }
    return false;
}

void MainWindow::loadFile()
{
    if (dirty_) {
        const auto answer = QMessageBox::question(
            this, tr("Load clinic data"),
            tr("Loading will replace the current unsaved changes. Continue?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
    }
    const QString folder = QFileDialog::getExistingDirectory(
        this, tr("Choose folder containing clinic CSV files"));
    if (folder.isEmpty()) return;
    try {
        manager_.loadData(folder.toStdString());
        dirty_ = false;
        setWindowModified(false);
        refreshAllPanels();
        refreshStatus(tr("Loaded clinic data"));
    } catch (const model::FileException& error) {
        refreshStatus(tr("Load failed"));
        QMessageBox::critical(this, tr("Load failed"), QString::fromUtf8(error.what()));
    } catch (const std::exception& error) {
        QMessageBox::critical(this, tr("Load failed"), QString::fromUtf8(error.what()));
    }
}

void MainWindow::importPatients()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Import patients from CSV"), QString(), tr("CSV files (*.csv)"));
    if (filePath.isEmpty()) return;
    try {
        manager_.importPatientsCsv(filePath.toStdString());
        refreshAllPanels();
        markDirty(tr("Patients imported"));
    } catch (const model::FileException& error) {
        QMessageBox::critical(this, tr("Patient import failed"), QString::fromUtf8(error.what()));
    } catch (const std::exception& error) {
        QMessageBox::critical(this, tr("Patient import failed"), QString::fromUtf8(error.what()));
    }
}

void MainWindow::exportPatients()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this, tr("Export patients to CSV"), QStringLiteral("patients.csv"), tr("CSV files (*.csv)"));
    if (filePath.isEmpty()) return;
    try {
        manager_.exportPatientsCsv(filePath.toStdString());
        refreshStatus(tr("Patients exported"));
    } catch (const model::FileException& error) {
        QMessageBox::critical(this, tr("Patient export failed"), QString::fromUtf8(error.what()));
    } catch (const std::exception& error) {
        QMessageBox::critical(this, tr("Patient export failed"), QString::fromUtf8(error.what()));
    }
}

void MainWindow::exitApplication()
{
    close();
}

void MainWindow::undoLastOperation()
{
    if (!manager_.undoLastOperation()) {
        QMessageBox::information(this, tr("Undo"), tr("There is no administrative operation to undo."));
        return;
    }
    refreshAllPanels();
    markDirty(tr("Last administrative operation undone"));
}

void MainWindow::focusSearch()
{
    auto* tabs = qobject_cast<QTabWidget*>(centralWidget());
    if (tabs) tabs->setCurrentWidget(searchPanel_);
    searchPanel_->focusSearchField();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!dirty_) {
        const auto answer = QMessageBox::question(
            this, tr("Exit SCAPMS"), tr("Are you sure you want to exit?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::Yes) event->accept();
        else event->ignore();
        return;
    }

    QMessageBox prompt(this);
    prompt.setIcon(QMessageBox::Warning);
    prompt.setWindowTitle(tr("Unsaved changes"));
    prompt.setText(tr("Save your changes before exiting?"));
    prompt.addButton(QMessageBox::Save);
    prompt.addButton(QMessageBox::Discard);
    prompt.addButton(QMessageBox::Cancel);
    prompt.setDefaultButton(QMessageBox::Save);
    prompt.exec();
    const auto clicked = prompt.standardButton(prompt.clickedButton());
    if (clicked == QMessageBox::Save) {
        if (saveToChosenFolder()) event->accept();
        else event->ignore();
    } else if (clicked == QMessageBox::Discard) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::refreshAllPanels()
{
    patientPanel_->refresh();
    appointmentPanel_->refresh();
    queuePanel_->refresh();
    searchPanel_->refresh();
    reportsPanel_->refresh();
}

void MainWindow::markDirty(const QString& action)
{
    dirty_ = true;
    refreshStatus(action);
    setWindowModified(true);
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About SCAPMS"),
                       tr("Smart Clinic Appointment & Patient Management System\n"
                          "COS202 Programming II lab assessment."));
}

} // namespace scapms::gui
