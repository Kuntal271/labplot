/*
	File                 : BarPlot.h
	Project              : LabPlot
	Description          : Bar Plot
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2022-2023 Alexander Semke <alexander.semke@web.de>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef BARPLOT_H
#define BARPLOT_H

#include "backend/worksheet/plots/cartesian/Plot.h"

class BarPlotPrivate;
class AbstractColumn;
class Background;
class Line;
class Value;

#ifdef SDK
#include "labplot_export.h"
class LABPLOT_EXPORT BarPlot : Plot {
#else
class BarPlot : public Plot {
#endif
	Q_OBJECT

public:
	enum class Type { Grouped, Stacked, Stacked_100_Percent };

	explicit BarPlot(const QString&);
	~BarPlot() override;

	QIcon icon() const override;
	QMenu* createContextMenu() override;

	void save(QXmlStreamWriter*) const override;
	bool load(XmlStreamReader*, bool preview) override;
	void loadThemeConfig(const KConfig&) override;

	// general
	POINTER_D_ACCESSOR_DECL(const AbstractColumn, xColumn, XColumn)
	QString& xColumnPath() const;
	BASIC_D_ACCESSOR_DECL(QVector<const AbstractColumn*>, dataColumns, DataColumns)
	QVector<QString>& dataColumnPaths() const;
	BASIC_D_ACCESSOR_DECL(BarPlot::Type, type, Type)
	BASIC_D_ACCESSOR_DECL(BarPlot::Orientation, orientation, Orientation)
	BASIC_D_ACCESSOR_DECL(double, widthFactor, WidthFactor)

	// box filling
	Background* backgroundAt(int) const;

	// box border line
	Line* lineAt(int) const;

	// values
	Value* value() const;

	void retransform() override;
	void handleResize(double horizontalRatio, double verticalRatio, bool pageResize) override;

	double minimum(CartesianCoordinateSystem::Dimension) const override;
	double maximum(CartesianCoordinateSystem::Dimension) const override;
	bool hasData() const override;
	QColor color() const override;

	typedef BarPlotPrivate Private;

protected:
	BarPlot(const QString& name, BarPlotPrivate* dd);

private:
	Q_DECLARE_PRIVATE(BarPlot)
	void init();
	void initActions();
	void initMenus();

	QAction* orientationHorizontalAction{nullptr};
	QAction* orientationVerticalAction{nullptr};
	QMenu* orientationMenu{nullptr};

public Q_SLOTS:
	void recalc();

private Q_SLOTS:
	// SLOTs for changes triggered via QActions in the context menu
	void orientationChangedSlot(QAction*);
	void dataColumnAboutToBeRemoved(const AbstractAspect*);

Q_SIGNALS:
	// General-Tab
	void xColumnChanged(const AbstractColumn*);
	void dataColumnsChanged(const QVector<const AbstractColumn*>&);
	void typeChanged(BarPlot::Type);
	void orientationChanged(BarPlot::Orientation);
	void widthFactorChanged(double);

	// box border
	void borderPenChanged(QPen&);
	void borderOpacityChanged(float);
};

#endif
