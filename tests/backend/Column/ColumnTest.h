/*
	File                 : ColumnTest.h
	Project              : LabPlot
	Description          : Tests for Column
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2021 Martin Marmsoler <martin.marmsoler@gmail.com>
	SPDX-FileCopyrightText: 2022 Stefan Gerlach <stefan.gerlach@uni.kn>
	SPDX-FileCopyrightText: 2022 Alexander Semke <alexander.semke@web.de>

	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COLUMNTEST_H
#define COLUMNTEST_H

#include "../../CommonTest.h"

class ColumnTest : public CommonTest {
	Q_OBJECT

private Q_SLOTS:
	// ranges
	void doubleMinimum();
	void doubleMaximum();
	void integerMinimum();
	void integerMaximum();
	void bigIntMinimum();
	void bigIntMaximum();

	// statistical properties for different column modes
	void statisticsDouble(); // only positive double values
	void statisticsDoubleNegative(); // contains negative values (> -100)
	void statisticsDoubleBigNegative(); // contains big negative values (<= -100)
	void statisticsDoubleZero(); // contains zero value
	void statisticsInt(); // only positive integer values
	void statisticsIntNegative(); // contains negative values (> -100)
	void statisticsIntBigNegative(); // contains big negative values (<= -100)
	void statisticsIntZero(); // contains zero value
	void statisticsIntOverflow(); // check overflow of integer
	void statisticsBigInt(); // big ints
	void statisticsText();

	// dictionary related tests for text columns
	void testDictionaryIndex();
	void testTextFrequencies();

	// performance of save and load
	void loadDoubleFromProject();
	void loadIntegerFromProject();
	void loadBigIntegerFromProject();
	void loadTextFromProject();
	void loadDateTimeFromProject();
	void saveLoadDateTime();

	void testIndexForValue();
	void testIndexForValueDoubleVector();

	void testInsertRow();
	void testRemoveRow();

	void testFormula();
	void testFormulaCell();
	void testFormulaCellInvalid();
	void testFormulaCellConstExpression();
	void testFormulaCellMulti();
	void testFormulaCellMultiSemikolon();
	void testFormulasmmin();
	void testFormulasmmax();
	void testFormulasma();

	void testFormulasmminSemikolon();
	void testFormulasmmaxSemikolon();
	void testFormulasmaSemikolon();
};

#endif // COLUMNTEST_H
