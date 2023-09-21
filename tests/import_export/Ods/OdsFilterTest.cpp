/*
	File                 : OdsFilterTest.cpp
	Project              : LabPlot
	Description          : Tests for the Ods filter
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2023 Stefan Gerlach <stefan.gerlach@uni.kn>

	SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "OdsFilterTest.h"
#include "backend/datasources/filters/OdsFilter.h"
#include "backend/spreadsheet/Spreadsheet.h"

void OdsFilterTest::importFile3SheetsRangesFormula() {
	const QString& fileName = QFINDTESTDATA(QLatin1String("data/ranges-formula.ods"));

	Spreadsheet spreadsheet(QStringLiteral("test"), false);
	OdsFilter filter;

	// sheet 1
	filter.setSelectedSheetNames(QStringList() << QStringLiteral("Sheet1"));
	filter.readDataFromFile(fileName, &spreadsheet);

	QCOMPARE(spreadsheet.columnCount(), 2);
	QCOMPARE(spreadsheet.rowCount(), 5);

	QCOMPARE(spreadsheet.column(0)->columnMode(), AbstractColumn::ColumnMode::Double);
	QCOMPARE(spreadsheet.column(1)->columnMode(), AbstractColumn::ColumnMode::Double);

	QCOMPARE(spreadsheet.column(0)->valueAt(0), 1);
	QCOMPARE(spreadsheet.column(0)->valueAt(1), 2);
	QCOMPARE(spreadsheet.column(0)->valueAt(2), 3);
	QCOMPARE(spreadsheet.column(0)->valueAt(3), 4);
	QCOMPARE(spreadsheet.column(0)->valueAt(4), 5);
	QCOMPARE(spreadsheet.column(1)->valueAt(0), 1);
	QCOMPARE(spreadsheet.column(1)->valueAt(1), 2.1);
	QCOMPARE(spreadsheet.column(1)->valueAt(2), 3.21);
	QCOMPARE(spreadsheet.column(1)->valueAt(3), 4.321);
	QCOMPARE(spreadsheet.column(1)->valueAt(4), 5.4321);

	// sheet 2
	filter.setSelectedSheetNames(QStringList() << QStringLiteral("Sheet2"));
	filter.readDataFromFile(fileName, &spreadsheet);

	QCOMPARE(spreadsheet.columnCount(), 4);
	QCOMPARE(spreadsheet.rowCount(), 2);

	QCOMPARE(spreadsheet.column(0)->columnMode(), AbstractColumn::ColumnMode::Double);
	QCOMPARE(spreadsheet.column(1)->columnMode(), AbstractColumn::ColumnMode::Double);
	QCOMPARE(spreadsheet.column(2)->columnMode(), AbstractColumn::ColumnMode::Double);
	QCOMPARE(spreadsheet.column(3)->columnMode(), AbstractColumn::ColumnMode::Double);

	QCOMPARE(spreadsheet.column(0)->valueAt(0), 1);
	QCOMPARE(spreadsheet.column(0)->valueAt(1), 3);
	QCOMPARE(spreadsheet.column(1)->valueAt(0), 2.2);
	QCOMPARE(spreadsheet.column(1)->valueAt(1), 2.1);
	QCOMPARE(spreadsheet.column(2)->valueAt(0), 3);
	QCOMPARE(spreadsheet.column(2)->valueAt(1), 1);
	QCOMPARE(spreadsheet.column(3)->valueAt(0), qQNaN());
	QCOMPARE(spreadsheet.column(3)->valueAt(1), 12.3);

	// sheet 3
	filter.setSelectedSheetNames(QStringList() << QStringLiteral("Sheet3"));
	filter.readDataFromFile(fileName, &spreadsheet);

	QCOMPARE(spreadsheet.columnCount(), 3);
	QCOMPARE(spreadsheet.rowCount(), 4);

	QCOMPARE(spreadsheet.column(0)->columnMode(), AbstractColumn::ColumnMode::Text);
	QCOMPARE(spreadsheet.column(1)->columnMode(), AbstractColumn::ColumnMode::Double);
	QCOMPARE(spreadsheet.column(2)->columnMode(), AbstractColumn::ColumnMode::Double);

	QCOMPARE(spreadsheet.column(0)->textAt(0), QLatin1String("A"));
	QCOMPARE(spreadsheet.column(0)->textAt(1), QLatin1String("B"));
	QCOMPARE(spreadsheet.column(0)->textAt(2), QLatin1String("C"));
	QCOMPARE(spreadsheet.column(0)->textAt(3), QLatin1String("0")); // formula
	QCOMPARE(spreadsheet.column(1)->valueAt(0), 2.2);
	QCOMPARE(spreadsheet.column(1)->valueAt(1), 1.1);
	QCOMPARE(spreadsheet.column(1)->valueAt(2), 3.3);
	QCOMPARE(spreadsheet.column(1)->valueAt(3), 6.6); // formula
	QCOMPARE(spreadsheet.column(2)->valueAt(0), 42);
	QCOMPARE(spreadsheet.column(2)->valueAt(1), 23);
	QCOMPARE(spreadsheet.column(2)->valueAt(2), 5);
	QCOMPARE(spreadsheet.column(2)->valueAt(3), 70); // formula
}

QTEST_MAIN(OdsFilterTest)
