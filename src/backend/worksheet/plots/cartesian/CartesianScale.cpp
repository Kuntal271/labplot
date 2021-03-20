
/***************************************************************************
    File                 : CartesianScale.cpp
    Project              : LabPlot
    Description          : Cartesian coordinate system for plots.
    --------------------------------------------------------------------
    Copyright            : (C) 2012-2016 by Alexander Semke (alexander.semke@web.de)
    Copyright            : (C) 2020-2021 by Stefan Gerlach (stefan.gerlach@uni.kn)

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

#include "CartesianScale.h"

/**
 * \class CartesianScale
 * \brief Base class for cartesian coordinate system scales.
 */
CartesianScale::CartesianScale(Type type, const Range<double> &range, double a, double b, double c)
	: m_type(type), m_range(range), m_a(a), m_b(b), m_c(c) {
}

CartesianScale::~CartesianScale() = default;

void CartesianScale::getProperties(Type *type, Range<double> *range,
		double *a, double *b, double *c) const {
	if (type)
		*type = m_type;
	if (range)
		*range = m_range;
	if (a)
		*a = m_a;
	if (b)
		*b = m_b;
	if (c)
		*c = m_c;
}

/**
 * \class CartesianCoordinateSystem::LinearScale
 * \brief implementation of a linear scale for cartesian coordinate systems
 * y =  b * x + a. a - offset, b - gradient
 */
class LinearScale : public CartesianScale {
public:
	LinearScale(const Range<double> &range, double offset, double gradient)
		: CartesianScale(Type::Linear, range, offset, gradient, 0) {
			Q_ASSERT(gradient != 0.0);
		}

	~LinearScale() override = default;

	bool map(double *value) const override {
		*value = *value * m_b + m_a;
		return true;
	}

	bool inverseMap(double *value) const override {
		CHECK(m_b != 0.0)
		*value = (*value - m_a) / m_b;
		return true;
	}

	int direction() const override {
		return m_b < 0 ? -1 : 1;
	}
};

/**
 * \class CartesianCoordinateSystem::LogScale
 * \brief implementation of a logarithmic scale for cartesian coordinate systems
 * y = TODO. a - offset, b - scaleFactor, c - base
 */
class LogScale : public CartesianScale {
public:
	LogScale(const Range<double> &range, double offset, double scaleFactor, double base)
		: CartesianScale(Type::Log, range, offset, scaleFactor, base) {
			Q_ASSERT(scaleFactor != 0.0);
			Q_ASSERT(base > 0.0);
	}

	~LogScale() override = default;

	bool map(double *value) const override {
		CHECK(m_c > 0)
		CHECK(*value != 0)

		*value = log(qAbs(*value))/log(m_c) * m_b + m_a;

		return true;
	}

	bool inverseMap(double *value) const override {
		CHECK(m_c > 0)

		*value = pow(m_c, (*value - m_a) / m_b);
		return true;
	}
	int direction() const override {
		return m_b < 0 ? -1 : 1;
	}
};

/**
 * \class CartesianCoordinateSystem::SqrtScale
 * \brief implementation of a square-root scale for cartesian coordinate systems
 * y = TODO. a - offset, b - scaleFactor
 */
class SqrtScale : public CartesianScale {
public:
	SqrtScale(const Range<double> &range, double offset, double scaleFactor)
		: CartesianScale(Type::Sqrt, range, offset, scaleFactor, 0) {
			Q_ASSERT(scaleFactor != 0.0);
	}

	~SqrtScale() override = default;

	bool map(double *value) const override {
		*value = sqrt(qAbs(*value)) * m_b + m_a;
		return true;
	}

	bool inverseMap(double *value) const override {
		CHECK(m_b != 0)

		*value = pow((*value - m_a) / m_b, 2);
		return true;
	}
	int direction() const override {
		return m_b < 0 ? -1 : 1;
	}
};


/***************************************************************/

CartesianScale* CartesianScale::createLinearScale(const Range<double> &range,
		const Range<double> &sceneRange, const Range<double> &logicalRange) {

	if (logicalRange.size() == 0.0)
		return nullptr;

	double b = sceneRange.size() / logicalRange.size();
	double a = sceneRange.start() - b * logicalRange.start();

	return new LinearScale(range, a, b);
}

CartesianScale* CartesianScale::createLogScale(const Range<double> &range,
		const Range<double> &sceneRange, const Range<double> &logicalRange, RangeT::Scale scale) {

	if (logicalRange.start() <= 0.0 || logicalRange.end() <= 0.0 || logicalRange.isZero()) {
		DEBUG(Q_FUNC_INFO << ", WARNING: invalid range for log scale : " << logicalRange.toStdString())
		return nullptr;
	}

	double base;
	if (scale == RangeT::Scale::Log10)
		base = 10.0;
	else if (scale == RangeT::Scale::Log2)
		base = 2.0;
	else	// RangeT::Scale::Ln
		base = M_E;

	const double lDiff = (log(logicalRange.end()) - log(logicalRange.start())) / log(base);
	double b = sceneRange.size() / lDiff;
	double a = sceneRange.start() - b * log(logicalRange.start()) / log(base);

	return new LogScale(range, a, b, base);
}

CartesianScale* CartesianScale::createSqrtScale(const Range<double> &range,
		const Range<double> &sceneRange, const Range<double> &logicalRange) {

	if (logicalRange.start() < 0.0 || logicalRange.end() < 0.0 || logicalRange.isZero()) {
		DEBUG(Q_FUNC_INFO << ", WARNING: invalid range for sqrt scale : " << logicalRange.toStdString())
		return nullptr;
	}

	//TODO: check
	const double lDiff = sqrt(logicalRange.end()) - sqrt(logicalRange.start());
	double b = sceneRange.size() / lDiff;
	double a = sceneRange.start() - b * sqrt(logicalRange.start());

	return new SqrtScale(range, a, b);
}
