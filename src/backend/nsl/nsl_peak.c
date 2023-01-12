/*
	File                 : nsl_peak.c
	Project              : LabPlot
	Description          : NSL peak detection and releated methods
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2023 Stefan Gerlach <stefan.gerlach@uni.kn>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "nsl_peak.h"

#include <math.h>
#include <stdio.h>

/* simple peak detection
 */
/* default options for nsl_peak_detect() */
nsl_peak_detect_options nsl_peak_detect_default_options() {
	nsl_peak_detect_options options;

	options.height = -INFINITY;
	options.distance = 0;

	return options;
}
size_t* nsl_peak_detect(double* data, size_t n, size_t* np) {
	return nsl_peak_detect_opt(data, n, np, nsl_peak_detect_default_options());
}
size_t* nsl_peak_detect_opt(double* data, size_t n, size_t* np, nsl_peak_detect_options options) {
	printf("options: h=%g d=%lu\n", options.height, options.distance);
	if (n <= 1) // nothing to do
		return NULL;

	size_t* peaks = (size_t*)malloc(n * sizeof(size_t));
	if (!peaks) {
		fprintf(stderr, "ERROR allocating memory for peak detection\n");
		return NULL;
	}

	// find peaks
	*np = 0;
	for (size_t i = 0; i < n; i++) {
		if (i == 0 && n > 1 && data[0] > data[1]) { // start
			peaks[(*np)++] = i;
			continue;
		}
		if (i == n - 1 && n > 1 && data[n - 1] > data[n - 2]) { // end
			peaks[(*np)++] = i;
			continue;
		}

		if (data[i - 1] < data[i] && data[i] > data[i + 1])
			peaks[(*np)++] = i;
	}

	if (!realloc(peaks, *np * sizeof(size_t))) { // should never happen since *np <= n
		fprintf(stderr, "ERROR reallocating memory for peak detection\n");
		free(peaks);
		return NULL;
	}

	return peaks;
}
