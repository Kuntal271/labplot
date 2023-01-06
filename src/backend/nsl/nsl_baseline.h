/*
	File                 : nsl_baseline.h
	Project              : LabPlot
	Description          : NSL baseline detection and subtraction methods
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2016-2023 Stefan Gerlach <stefan.gerlach@uni.kn>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef NSL_BASELINE_H
#define NSL_BASELINE_H

#include <stdlib.h>

/* remove mimimum base line from data */
void nsl_baseline_remove_minimum(double *data, size_t n);

#endif
