#include "SearchPanel.h"

#include "../utils/ClinicException.h"

#include <QCheckBox>
#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace scapms::gui {

SearchPanel::SearchPanel(controller::ClinicManager& manager, QWidget* parent)
    : QWidget(parent), manager_(manager)
{
    buildUi();
    refresh();
}

void SearchPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);

    auto* searchRow = new QHBoxLayout();
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText(tr("Type an ID, name, or category"));
    searchEdit_->setToolTip(tr("Search updates as you type; press Enter to search again."));
    searchEdit_->setClearButtonEnabled(true);
    searchByCombo_ = new QComboBox(this);
    searchByCombo_->addItems({tr("ID"), tr("Name"), tr("Category")});
    searchByCombo_->setToolTip(tr("Choose which patient field to search."));
    techniqueCombo_ = new QComboBox(this);
    techniqueCombo_->addItems({tr("Auto"), tr("Linear"), tr("Binary"), tr("Recursive")});
    techniqueCombo_->setToolTip(tr("Choose automatic, linear, binary, or recursive search."));
    searchRow->addWidget(searchEdit_, 1);
    searchRow->addWidget(searchByCombo_);
    searchRow->addWidget(techniqueCombo_);
    root->addLayout(searchRow);

    auto* sortRow = new QHBoxLayout();
    sortAlgorithmCombo_ = new QComboBox(this);
    sortAlgorithmCombo_->addItems({tr("Insertion Sort"), tr("Merge Sort")});
    sortByCombo_ = new QComboBox(this);
    sortByCombo_->addItems({tr("Name"), tr("ID"), tr("Age"), tr("Appointment Date")});
    ascendingCheck_ = new QCheckBox(tr("Ascending"), this);
    ascendingCheck_->setChecked(true);
    sortButton_ = new QPushButton(tr("Sort"), this);
    sortAlgorithmCombo_->setToolTip(tr("Choose a hand-written sorting algorithm."));
    sortByCombo_->setToolTip(tr("Choose the patient field to sort by."));
    ascendingCheck_->setToolTip(tr("Sort from lowest to highest or A to Z."));
    sortButton_->setToolTip(tr("Sort the current patient view."));
    sortRow->addWidget(new QLabel(tr("Algorithm:"), this));
    sortRow->addWidget(sortAlgorithmCombo_);
    sortRow->addWidget(new QLabel(tr("Sort by:"), this));
    sortRow->addWidget(sortByCombo_);
    sortRow->addWidget(ascendingCheck_);
    sortRow->addWidget(sortButton_);
    sortRow->addStretch();
    root->addLayout(sortRow);

    techniqueLabel_ = new QLabel(tr("Enter a search term to begin."), this);
    sortExplanationLabel_ = new QLabel(this);
    sortExplanationLabel_->setWordWrap(true);
    root->addWidget(techniqueLabel_);
    root->addWidget(sortExplanationLabel_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(6);
    table_->setHorizontalHeaderLabels({tr("ID"), tr("Name"), tr("Age"), tr("Contact"),
                                       tr("Category"), tr("Details")});
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setToolTip(tr("Search matches or the sorted non-owning patient view appear here."));
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    root->addWidget(table_, 1);
    root->addWidget(new QLabel(
        tr("Legend: Emergency — light red; Follow-up — light blue."), this));

    connect(searchEdit_, &QLineEdit::textChanged, this, &SearchPanel::searchNow);
    connect(searchEdit_, &QLineEdit::returnPressed, this, &SearchPanel::searchNow);
    connect(searchByCombo_, &QComboBox::currentTextChanged, this, &SearchPanel::searchNow);
    connect(techniqueCombo_, &QComboBox::currentTextChanged, this, &SearchPanel::searchNow);
    connect(sortAlgorithmCombo_, &QComboBox::currentTextChanged,
            this, &SearchPanel::updateSortExplanation);
    connect(sortButton_, &QPushButton::clicked, this, &SearchPanel::sortPatients);
    updateSortExplanation();
}

void SearchPanel::refresh()
{
    (void)manager_.getPatients(); // Refreshes the manager's non-owning search view.
    searchNow();
}

void SearchPanel::focusSearchField()
{
    searchEdit_->setFocus();
    searchEdit_->selectAll();
}

utils::SortField SearchPanel::selectedSortField() const
{
    const QString field = sortByCombo_->currentText();
    if (field == tr("ID")) return utils::SortField::Id;
    if (field == tr("Age")) return utils::SortField::Age;
    if (field == tr("Appointment Date")) return utils::SortField::AppointmentDate;
    return utils::SortField::Name;
}

QString SearchPanel::techniqueText(controller::SearchEngine::Technique technique)
{
    switch (technique) {
    case controller::SearchEngine::Technique::Linear: return tr("Linear search");
    case controller::SearchEngine::Technique::Binary: return tr("Binary search");
    case controller::SearchEngine::Technique::Recursive: return tr("Recursive search");
    }
    return tr("Unknown search");
}

void SearchPanel::searchNow()
{
    const QString query = searchEdit_->text().trimmed();
    table_->setRowCount(0);
    if (query.isEmpty()) {
        techniqueLabel_->setText(tr("Enter a search term; an empty search is not run."));
        return;
    }

    try {
        const QString searchBy = searchByCombo_->currentText();
        const QString requested = techniqueCombo_->currentText();
        if (searchBy == tr("Category")) {
            if (requested == tr("Binary") || requested == tr("Recursive")) {
                techniqueLabel_->setText(tr("Binary and recursive search are available for ID or Name only."));
                return;
            }
            const auto matches = manager_.searchByCategory(query.toStdString());
            showPatients(matches);
            techniqueLabel_->setText(tr("Technique used: Linear category scan (%1 result(s)).")
                                         .arg(static_cast<qulonglong>(matches.size())));
            return;
        }

        const utils::SortField field = searchBy == tr("ID")
            ? utils::SortField::Id : utils::SortField::Name;
        controller::SearchEngine::SearchResult result;
        if (requested == tr("Auto")) {
            result = manager_.search(field, query.toStdString());
        } else {
            const auto technique = requested == tr("Binary")
                ? controller::SearchEngine::Technique::Binary
                : requested == tr("Recursive")
                    ? controller::SearchEngine::Technique::Recursive
                    : controller::SearchEngine::Technique::Linear;
            result = manager_.searchWithTechnique(field, query.toStdString(), technique);
        }

        const auto view = manager_.searchResultsView();
        const model::PatientRecord* match = result.index >= 0 &&
            static_cast<std::size_t>(result.index) < view.size()
            ? view[static_cast<std::size_t>(result.index)] : nullptr;
        showOnePatient(match);
        techniqueLabel_->setText(tr("Technique used: %1. %2")
            .arg(techniqueText(result.technique))
            .arg(match ? tr("Match found.") : tr("No matching patient found.")));
    } catch (const model::ValidationException& error) {
        techniqueLabel_->setText(tr("Error: %1").arg(QString::fromUtf8(error.what())));
    }
}

void SearchPanel::sortPatients()
{
    (void)manager_.getPatients();
    const bool ascending = ascendingCheck_->isChecked();
    if (sortAlgorithmCombo_->currentText() == tr("Insertion Sort")) {
        manager_.insertionSortPatients(selectedSortField(), ascending);
    } else {
        manager_.mergeSortPatients(selectedSortField(), ascending);
    }
    const QString query = searchEdit_->text().trimmed();
    if (query.isEmpty()) {
        showPatients(manager_.searchResultsView());
        techniqueLabel_->setText(tr("Patient view sorted by %1.").arg(sortByCombo_->currentText()));
    } else {
        searchNow();
    }
}

void SearchPanel::updateSortExplanation()
{
    if (sortAlgorithmCombo_->currentText() == tr("Insertion Sort")) {
        sortExplanationLabel_->setText(
            tr("Insertion sort: efficient for small or nearly sorted lists."));
    } else {
        sortExplanationLabel_->setText(
            tr("Merge sort: stable and O(n log n) for larger lists."));
    }
}

void SearchPanel::showPatients(const std::vector<const model::PatientRecord*>& patients)
{
    table_->setRowCount(0);
    for (const auto* patient : patients) {
        const int row = table_->rowCount();
        table_->insertRow(row);
        const QString values[] = {
            QString::fromStdString(patient->getId()),
            QString::fromStdString(patient->getName()),
            QString::number(patient->getAge()),
            QString::fromStdString(patient->getContact()),
            QString::fromStdString(patient->getCategory()),
            QString::fromStdString(patient->describe())
        };
        for (int column = 0; column < 6; ++column) {
            table_->setItem(row, column, new QTableWidgetItem(values[column]));
        }
        const QColor color = values[4] == tr("Emergency")
            ? QColor(QStringLiteral("#ffe0e0"))
            : values[4] == tr("Follow-up") ? QColor(QStringLiteral("#e0f0ff")) : QColor();
        if (color.isValid()) {
            for (int column = 0; column < 6; ++column) table_->item(row, column)->setBackground(color);
        }
    }
}

void SearchPanel::showOnePatient(const model::PatientRecord* patient)
{
    if (!patient) {
        table_->setRowCount(0);
        return;
    }
    showPatients({patient});
}

} // namespace scapms::gui
