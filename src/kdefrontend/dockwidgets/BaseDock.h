/*
	File             : BaseDock.h
	Project          : LabPlot
	Description      : Base dock widget
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2019 Martin Marmsoler <martin.marmsoler@gmail.com>
	SPDX-FileCopyrightText: 2019-2020 Alexander Semke <alexander.semke@web.de>
	SPDX-FileCopyrightText: 2020-2021 Stefan Gerlach <stefan.gerlach@uni.kn>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef BASEDOCK
#define BASEDOCK

#include "backend/worksheet/Worksheet.h"

#include <QLineEdit>
#include <QWidget>

class AbstractAspect;
class ResizableTextEdit;
class QComboBox;
class QDoubleSpinBox;

class BaseDock : public QWidget {
	Q_OBJECT

public:
	explicit BaseDock(QWidget* parent);
	~BaseDock();

	enum class Units { Metric, Imperial };

	virtual void updateLocale(){};
	virtual void updateUnits(){};
	virtual void updatePlotRanges(){}; // used in worksheet element docks
	static void spinBoxCalculateMinMax(QDoubleSpinBox* spinbox, Range<double> range, double newValue = NAN);
	template<typename T>
	void setAspects(QList<T*> aspects) {
		if (m_aspect)
			disconnect(m_aspect, nullptr, this, nullptr);

		m_aspects.clear();
		if (aspects.length() == 0) {
			m_aspect = nullptr;
			return;
		}

		m_aspect = aspects.first();
		connect(m_aspect, &AbstractAspect::aspectAboutToBeRemoved, this, &BaseDock::disconnectAspect);
		for (auto* aspect : aspects) {
			if (aspect->inherits(AspectType::AbstractAspect))
				m_aspects.append(static_cast<AbstractAspect*>(aspect));
		}
	}

	void disconnectAspect(const AbstractAspect* a) {
		disconnect(a);
		m_aspect = nullptr;
	}

	const AbstractAspect* aspect() {
		return m_aspect;
	}

protected:
	bool m_initializing{false};
	QLineEdit* m_leName{nullptr};
	ResizableTextEdit* m_teComment{nullptr};
	Units m_units{Units::Metric};
	Worksheet::Unit m_worksheetUnit{Worksheet::Unit::Centimeter};
	void updatePlotRangeList(QComboBox*); // used in worksheet element docks
private:
	AbstractAspect* m_aspect{nullptr};
	QList<AbstractAspect*> m_aspects;

protected Q_SLOTS:
	void nameChanged();
	void commentChanged();
	void aspectDescriptionChanged(const AbstractAspect*);
	void plotRangeChanged(int index); // used in worksheet element docks

private:
	bool m_suppressPlotRetransform{false};

	friend class RetransformTest;
	friend class MultiRangeTest;
};

#endif
