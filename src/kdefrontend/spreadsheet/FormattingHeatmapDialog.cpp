/***************************************************************************
    File                 : FormattingHeatmapDialog.h
    Project              : LabPlot
    Description          : Dialog for the conditional formatting according to a heatmap
    --------------------------------------------------------------------
    Copyright            : (C) 2021 by Alexander Semke (alexander.semke@web.de)

 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor,                    *
 *   Boston, MA  02110-1301  USA                                           *
 *                                                                         *
 ***************************************************************************/
#include "FormattingHeatmapDialog.h"
#include "backend/core/column/Column.h"
// #include "backend/lib/macros.h"
#include "backend/spreadsheet/Spreadsheet.h"
#include "kdefrontend/colormaps/ColorMapsDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QPushButton>
#include <QWindow>

#include <KLocalizedString>
#include <KSharedConfig>
#include <KWindowConfig>

/*!
	\class FormattingHeatmapDialog
	\brief Dialog for generating non-uniform random numbers.

	\ingroup kdefrontend
 */

FormattingHeatmapDialog::FormattingHeatmapDialog(Spreadsheet* s, QWidget* parent) : QDialog(parent), m_spreadsheet(s) {
	setWindowTitle(i18nc("@title:window", "Conditional Formatting - Heatmap"));

	QWidget* mainWidget = new QWidget(this);
	ui.setupUi(mainWidget);
	auto* layout = new QVBoxLayout(this);

	auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	m_okButton = buttonBox->button(QDialogButtonBox::Ok);
	m_okButton->setDefault(true);
	m_okButton->setToolTip(i18n("Format the cells according the level and color settings"));
	m_okButton->setText(i18n("&Format"));

	connect(buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &FormattingHeatmapDialog::close);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &FormattingHeatmapDialog::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &FormattingHeatmapDialog::reject);

	layout->addWidget(mainWidget);
	layout->addWidget(buttonBox);
	setLayout(layout);
	setAttribute(Qt::WA_DeleteOnClose);

	ui.bColorMap->setIcon(QIcon::fromTheme(QLatin1String("color-management")));
	ui.lColorMapPreview->setMaximumHeight(ui.bColorMap->height());

	ui.cbLevelsType->addItem(i18n("Count"));
	ui.cbLevelsType->addItem(i18n("Increment"));

	ui.leMinimum->setValidator(new QDoubleValidator(ui.leMinimum));
	ui.leMaximum->setValidator(new QDoubleValidator(ui.leMaximum));
	ui.leLevels->setValidator(new QDoubleValidator(ui.leLevels));

	connect(ui.chkAutoRange, &QCheckBox::stateChanged, this, &FormattingHeatmapDialog::autoRangeChanged);
	connect(ui.leMaximum, &QLineEdit::textChanged, this, &FormattingHeatmapDialog::checkValues);
	connect(ui.leMinimum, &QLineEdit::textChanged, this, &FormattingHeatmapDialog::checkValues);
	connect(ui.bColorMap, &QPushButton::clicked, this, &FormattingHeatmapDialog::selectColorMap);

	//restore saved settings if available
	create(); // ensure there's a window created
	const KConfigGroup conf(KSharedConfig::openConfig(), "FormattingHeatmapDialog");
	if (conf.exists()) {
		auto state = (Qt::CheckState)(conf.readEntry("AutoRange", (int)Qt::Checked));
		ui.chkAutoRange->setCheckState(state);

		KWindowConfig::restoreWindowSize(windowHandle(), conf);
		resize(windowHandle()->size()); // workaround for QTBUG-40584
	} else {
		resize( QSize(400, 0).expandedTo(minimumSize()) );
	}
}

FormattingHeatmapDialog::~FormattingHeatmapDialog() {
	//save current settings
	KConfigGroup conf(KSharedConfig::openConfig(), "FormattingHeatmapDialog");
	conf.writeEntry("AutoRange", (int)ui.chkAutoRange->checkState());
	KWindowConfig::saveWindowSize(windowHandle(), conf);
}

void FormattingHeatmapDialog::setColumns(const QVector<Column*>& columns) {
	m_columns = columns;

	double min = INFINITY;
	double max = -INFINITY;
	for (auto* col : m_columns) {
		if (col->minimum() < min)
			min = col->minimum();
		if (col->maximum() > max)
			max = col->maximum();
	}

	if (min != INFINITY)
		ui.leMinimum->setText(QString::number(min));
	else
		ui.leMinimum->setText(QString());

	if (max != -INFINITY)
		ui.leMaximum->setText(QString::number(max));
	else
		ui.leMaximum->setText(QString());

}

void FormattingHeatmapDialog::autoRangeChanged(int index) {
	bool autoRange = (index == Qt::Checked);
	ui.leMinimum->setEnabled(!autoRange);
	ui.leMaximum->setEnabled(!autoRange);

	checkValues();
}

void FormattingHeatmapDialog::selectColorMap() {
	auto* dlg = new ColorMapsDialog(this);
	if (dlg->exec() == QDialog::Accepted) {
		ui.lColorMapPreview->setPixmap(dlg->previewPixmap());
		ui.lColorMapPreview->setFocus();
	}
}

void FormattingHeatmapDialog::checkValues() {
	if (ui.chkAutoRange->checkState() != Qt::Checked
		&& (ui.leMaximum->text().isEmpty() || ui.leMinimum->text().isEmpty())) {
		m_okButton->setEnabled(false);
		return;
	}


	m_okButton->setEnabled(true);
}
