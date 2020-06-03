/***************************************************************************
    File                 : ColumnDock.cpp
    Project              : LabPlot
    --------------------------------------------------------------------
    Copyright            : (C) 2011-2019 by Alexander Semke (alexander.semke@web.de)
    Copyright            : (C) 2013-2017 by Stefan Gerlach (stefan.gerlach@uni.kn)
    Description          : widget for column properties

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

#include "ColumnDock.h"

#include "backend/core/AbstractFilter.h"
#include "backend/core/datatypes/SimpleCopyThroughFilter.h"
#include "backend/core/datatypes/Double2StringFilter.h"
#include "backend/core/datatypes/String2DoubleFilter.h"
#include "backend/core/datatypes/DateTime2StringFilter.h"
#include "backend/core/datatypes/String2DateTimeFilter.h"
#include "backend/spreadsheet/Spreadsheet.h"

#include <KLocalizedString>

/*!
  \class ColumnDock
  \brief Provides a widget for editing the properties of the spreadsheet columns currently selected in the project explorer.

  \ingroup kdefrontend
*/

ColumnDock::ColumnDock(QWidget* parent) : BaseDock(parent) {
	ui.setupUi(this);
	m_leName = ui.leName;
	m_leComment = ui.leComment;

	connect(ui.leName, &QLineEdit::textChanged, this, &ColumnDock::nameChanged);
	connect(ui.leComment, &QLineEdit::textChanged, this, &ColumnDock::commentChanged);
	connect(ui.cbType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColumnDock::typeChanged);
	connect(ui.cbFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColumnDock::formatChanged);
	connect(ui.sbPrecision, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColumnDock::precisionChanged);
	connect(ui.cbPlotDesignation, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ColumnDock::plotDesignationChanged);

	retranslateUi();
}

void ColumnDock::setColumns(QList<Column*> list) {
	m_initializing = true;
	m_columnsList = list;
	m_column = list.first();
	m_aspect = list.first();

	//check whether we have non-editable columns:
	//1. columns in a LiveDataSource
	//2. columns in the spreadsheet of a datapicker curve
	//3. columns for residuals calculated in XYFitCurve)
	bool nonEditable = false;
	for (auto* col : m_columnsList) {
		auto* s = dynamic_cast<Spreadsheet*>(col->parentAspect());
		if (s) {
			if (s->type() == AspectType::LiveDataSource || s->parentAspect()->type() == AspectType::DatapickerCurve) {
				nonEditable = true;
				break;
			}
		} else {
			nonEditable = true;
			break;
		}
	}

	if (list.size() == 1) {
		//names and comments of non-editable columns in a file data source can be changed.
		if (!nonEditable && m_column->parentAspect()->type() == AspectType::LiveDataSource) {
			ui.leName->setEnabled(false);
			ui.leComment->setEnabled(false);
		} else {
			ui.leName->setEnabled(true);
			ui.leComment->setEnabled(true);
		}

		ui.leName->setText(m_column->name());
		ui.leComment->setText(m_column->comment());
	} else {
		ui.leName->setEnabled(false);
		ui.leComment->setEnabled(false);

		ui.leName->setText(QString());
		ui.leComment->setText(QString());
	}
	ui.leName->setStyleSheet("");
	ui.leName->setToolTip("");

	//show the properties of the first column
	updateTypeWidgets(m_column->columnMode());
	ui.cbPlotDesignation->setCurrentIndex( int(m_column->plotDesignation()) );

	// slots
	connect(m_column, &AbstractColumn::aspectDescriptionChanged, this, &ColumnDock::columnDescriptionChanged);
	connect(m_column, &AbstractColumn::modeChanged, this, &ColumnDock::columnModeChanged);
	connect(m_column->outputFilter(), &AbstractSimpleFilter::formatChanged, this, &ColumnDock::columnFormatChanged);
	connect(m_column->outputFilter(), &AbstractSimpleFilter::digitsChanged, this, &ColumnDock::columnPrecisionChanged);
	connect(m_column, &AbstractColumn::plotDesignationChanged, this, &ColumnDock::columnPlotDesignationChanged);

	//don't allow to change the column type at least one non-editable column
	ui.cbType->setEnabled(!nonEditable);

	m_initializing = false;
}

void ColumnDock::updateTypeWidgets(AbstractColumn::ColumnMode mode) {
	const Lock lock(m_initializing);
	ui.cbType->setCurrentIndex(ui.cbType->findData((int)mode));
	switch (mode) {
	case AbstractColumn::ColumnMode::Numeric: {
			auto* filter = static_cast<Double2StringFilter*>(m_column->outputFilter());
			ui.cbFormat->setCurrentIndex(ui.cbFormat->findData(filter->numericFormat()));
			ui.sbPrecision->setValue(filter->numDigits());
			break;
		}
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
	case AbstractColumn::ColumnMode::DateTime: {
			auto* filter = static_cast<DateTime2StringFilter*>(m_column->outputFilter());
// 			DEBUG("	set column format: " << STDSTRING(filter->format()));
			ui.cbFormat->setCurrentIndex(ui.cbFormat->findData(filter->format()));
			break;
		}
	case AbstractColumn::ColumnMode::Integer:	// nothing to set
	case AbstractColumn::ColumnMode::BigInt:
	case AbstractColumn::ColumnMode::Text:
		break;
	}

	this->updateFormatWidgets(mode);
}

/*!
  depending on the currently selected column type (column mode) updates the widgets for the column format,
  shows/hides the allowed widgets, fills the corresponding combobox with the possible entries.
  Called when the type (column mode) is changed.
*/
void ColumnDock::updateFormatWidgets(AbstractColumn::ColumnMode mode) {
	ui.cbFormat->clear();

	switch (mode) {
	case AbstractColumn::ColumnMode::Numeric:
		ui.cbFormat->addItem(i18n("Decimal"), QVariant('f'));
		ui.cbFormat->addItem(i18n("Scientific (e)"), QVariant('e'));
		ui.cbFormat->addItem(i18n("Scientific (E)"), QVariant('E'));
		ui.cbFormat->addItem(i18n("Automatic (g)"), QVariant('g'));
		ui.cbFormat->addItem(i18n("Automatic (G)"), QVariant('G'));
		break;
	case AbstractColumn::ColumnMode::Month:
		ui.cbFormat->addItem(i18n("Number without Leading Zero"), QVariant("M"));
		ui.cbFormat->addItem(i18n("Number with Leading Zero"), QVariant("MM"));
		ui.cbFormat->addItem(i18n("Abbreviated Month Name"), QVariant("MMM"));
		ui.cbFormat->addItem(i18n("Full Month Name"), QVariant("MMMM"));
		break;
	case AbstractColumn::ColumnMode::Day:
		ui.cbFormat->addItem(i18n("Number without Leading Zero"), QVariant("d"));
		ui.cbFormat->addItem(i18n("Number with Leading Zero"), QVariant("dd"));
		ui.cbFormat->addItem(i18n("Abbreviated Day Name"), QVariant("ddd"));
		ui.cbFormat->addItem(i18n("Full Day Name"), QVariant("dddd"));
		break;
	case AbstractColumn::ColumnMode::DateTime:
		for (const auto& s : AbstractColumn::dateTimeFormats())
			ui.cbFormat->addItem(s, QVariant(s));
		break;
	case AbstractColumn::ColumnMode::Integer:
	case AbstractColumn::ColumnMode::BigInt:
	case AbstractColumn::ColumnMode::Text:
		break;
	}

	if (mode == AbstractColumn::ColumnMode::Numeric) {
		ui.lPrecision->show();
		ui.sbPrecision->show();
	} else {
		ui.lPrecision->hide();
		ui.sbPrecision->hide();
	}

	if (mode == AbstractColumn::ColumnMode::Text || mode == AbstractColumn::ColumnMode::Integer || mode == AbstractColumn::ColumnMode::BigInt) {
		ui.lFormat->hide();
		ui.cbFormat->hide();
	} else {
		ui.lFormat->show();
		ui.cbFormat->show();
	}

	if (mode == AbstractColumn::ColumnMode::DateTime) {
		ui.cbFormat->setEditable(true);
		ui.cbFormat->setCurrentItem("yyyy-MM-dd hh:mm:ss.zzz");
	} else {
		ui.cbFormat->setEditable(false);
		ui.cbFormat->setCurrentIndex(0);
	}
}

//*************************************************************
//******** SLOTs for changes triggered in ColumnDock **********
//*************************************************************
void ColumnDock::retranslateUi() {
	m_initializing = true;

	ui.cbType->clear();
	ui.cbType->addItem(i18n("Numeric"), QVariant(static_cast<int>(AbstractColumn::ColumnMode::Numeric)));
	ui.cbType->addItem(i18n("Integer"), QVariant(static_cast<int>(AbstractColumn::ColumnMode::Integer)));
	ui.cbType->addItem(i18n("Big Integer"), QVariant(static_cast<int>(AbstractColumn::ColumnMode::BigInt)));
	ui.cbType->addItem(i18n("Text"), QVariant(static_cast<int>(AbstractColumn::ColumnMode::Text)));
	ui.cbType->addItem(i18n("Month Names"), QVariant(static_cast<int>(AbstractColumn::ColumnMode::Month)));
	ui.cbType->addItem(i18n("Day Names"), QVariant(static_cast<int>(AbstractColumn::ColumnMode::Day)));
	ui.cbType->addItem(i18n("Date and Time"), QVariant(static_cast<int>(AbstractColumn::ColumnMode::DateTime)));

	ui.cbPlotDesignation->clear();
	ui.cbPlotDesignation->addItem(i18n("None"));
	ui.cbPlotDesignation->addItem(i18n("X"));
	ui.cbPlotDesignation->addItem(i18n("Y"));
	ui.cbPlotDesignation->addItem(i18n("Z"));
	ui.cbPlotDesignation->addItem(i18n("X-error"));
	ui.cbPlotDesignation->addItem(i18n("X-error -"));
	ui.cbPlotDesignation->addItem(i18n("X-error +"));
	ui.cbPlotDesignation->addItem(i18n("Y-error"));
	ui.cbPlotDesignation->addItem(i18n("Y-error -"));
	ui.cbPlotDesignation->addItem(i18n("Y-error +"));

	m_initializing = false;
}

/*!
  called when the type (column mode - numeric, text etc.) of the column was changed.
*/
void ColumnDock::typeChanged(int index) {
	DEBUG("ColumnDock::typeChanged()");
	if (m_initializing)
		return;

	auto columnMode = (AbstractColumn::ColumnMode)ui.cbType->itemData(index).toInt();

	switch (columnMode) {
	case AbstractColumn::ColumnMode::Numeric: {
			int digits = ui.sbPrecision->value();
			for (auto* col : m_columnsList) {
				col->beginMacro(i18n("%1: change column type", col->name()));
				col->setColumnMode(columnMode);
				auto* filter = static_cast<Double2StringFilter*>(col->outputFilter());

				//TODO: using
				//char format = ui.cbFormat->itemData(ui.cbFormat->currentIndex()).toChar().toLatin1();
				//outside of the for-loop and
				//filter->setNumericFormat(format);
				//inseide the loop leads to wrong results when converting from integer to numeric -> 'f' is set instead of 'e'
				filter->setNumericFormat(ui.cbFormat->itemData(ui.cbFormat->currentIndex()).toChar().toLatin1());
				filter->setNumDigits(digits);
				col->endMacro();
			}
			break;
		}
	case AbstractColumn::ColumnMode::Integer:
	case AbstractColumn::ColumnMode::BigInt:
	case AbstractColumn::ColumnMode::Text:
		for (auto* col : m_columnsList)
			col->setColumnMode(columnMode);
		break;
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
		for (auto* col : m_columnsList) {
			col->beginMacro(i18n("%1: change column type", col->name()));
			// the format is saved as item data
			QString format = ui.cbFormat->itemData(ui.cbFormat->currentIndex()).toString();
			col->setColumnMode(columnMode);
			auto* filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
			filter->setFormat(format);
			col->endMacro();
		}
		break;
	case AbstractColumn::ColumnMode::DateTime:
		for (auto* col : m_columnsList) {
			col->beginMacro(i18n("%1: change column type", col->name()));
			// the format is the current text
			QString format = ui.cbFormat->currentText();
			col->setColumnMode(columnMode);
			auto* filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
			filter->setFormat(format);
			col->endMacro();
		}
		break;
	}

	updateTypeWidgets(columnMode);

	DEBUG("ColumnDock::typeChanged() DONE");
}

/*!
  called when the format for the current type (column mode) was changed.
*/
void ColumnDock::formatChanged(int index) {
	DEBUG("ColumnDock::formatChanged()");
	if (m_initializing)
		return;

	auto mode = (AbstractColumn::ColumnMode)ui.cbType->itemData(ui.cbType->currentIndex()).toInt();

	switch (mode) {
	case AbstractColumn::ColumnMode::Numeric: {
			char format = ui.cbFormat->itemData(index).toChar().toLatin1();
			for (auto* col : m_columnsList) {
				auto* filter = static_cast<Double2StringFilter*>(col->outputFilter());
				filter->setNumericFormat(format);
			}
			break;
		}
	case AbstractColumn::ColumnMode::Integer:
	case AbstractColumn::ColumnMode::BigInt:
	case AbstractColumn::ColumnMode::Text:
		break;
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
	case AbstractColumn::ColumnMode::DateTime: {
			QString format = ui.cbFormat->itemData(index).toString();
			for (auto* col : m_columnsList) {
				auto* filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
				filter->setFormat(format);
			}
			break;
		}
	}
	DEBUG("ColumnDock::formatChanged() DONE");
}

void ColumnDock::precisionChanged(int digits) {
	if (m_initializing)
		return;

	for (auto* col : m_columnsList) {
		auto* filter = static_cast<Double2StringFilter*>(col->outputFilter());
		filter->setNumDigits(digits);
	}
}

void ColumnDock::plotDesignationChanged(int index) {
	if (m_initializing)
		return;

	auto pd = AbstractColumn::PlotDesignation(index);
	for (auto* col : m_columnsList)
		col->setPlotDesignation(pd);
}

//*************************************************************
//********* SLOTs for changes triggered in Column *************
//*************************************************************
void ColumnDock::columnDescriptionChanged(const AbstractAspect* aspect) {
	if (m_column != aspect)
		return;

	m_initializing = true;
	if (aspect->name() != ui.leName->text())
		ui.leName->setText(aspect->name());
	else if (aspect->comment() != ui.leComment->text())
		ui.leComment->setText(aspect->comment());
	m_initializing = false;
}

void ColumnDock::columnModeChanged(const AbstractAspect* aspect) {
	if (m_column != aspect)
		return;

	updateTypeWidgets(m_column->columnMode());
}

void ColumnDock::columnFormatChanged() {
	DEBUG("ColumnDock::columnFormatChanged()");
	const Lock lock(m_initializing);
	auto columnMode = m_column->columnMode();
	switch (columnMode) {
	case AbstractColumn::ColumnMode::Numeric: {
			auto* filter = static_cast<Double2StringFilter*>(m_column->outputFilter());
			ui.cbFormat->setCurrentIndex(ui.cbFormat->findData(filter->numericFormat()));
			break;
		}
	case AbstractColumn::ColumnMode::Integer:
	case AbstractColumn::ColumnMode::BigInt:
	case AbstractColumn::ColumnMode::Text:
		break;
	case AbstractColumn::ColumnMode::Month:
	case AbstractColumn::ColumnMode::Day:
	case AbstractColumn::ColumnMode::DateTime: {
			auto* filter = static_cast<DateTime2StringFilter*>(m_column->outputFilter());
			ui.cbFormat->setCurrentIndex(ui.cbFormat->findData(filter->format()));
			break;
		}
	}
}

void ColumnDock::columnPrecisionChanged() {
	const Lock lock(m_initializing);
	auto* filter = static_cast<Double2StringFilter*>(m_column->outputFilter());
	ui.sbPrecision->setValue(filter->numDigits());
}

void ColumnDock::columnPlotDesignationChanged(const AbstractColumn* col) {
	const Lock lock(m_initializing);
	ui.cbPlotDesignation->setCurrentIndex(int(col->plotDesignation()));
}
