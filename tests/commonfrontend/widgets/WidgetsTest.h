/*
	File                 : WidgetsTest.h
	Project              : LabPlot
	Description          : Tests for widgets
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2022 Martin Marmsoler <martin.marmsoler@gmail.com>

	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef WIDGETSTEST_H
#define WIDGETSTEST_H

#include "tests/CommonTest.h"

class WidgetsTest : public CommonTest {
	Q_OBJECT

private Q_SLOTS:
	void numberSpinBoxTest1();
	void numberSpinBoxProperties();
	void numberSpinBoxCreateStringNumber();
	void numberSpinBoxChangingValueKeyPress();
};
#endif // WIDGETSTEST_H
