/*
	File                 : LollipopPlot.h
	Project              : LabPlot
	Description          : Lollipop Plot
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2023 Alexander Semke <alexander.semke@web.de>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LOLLIPOPPLOT_H
#define LOLLIPOPPLOT_H

#include "backend/worksheet/plots/cartesian/CartesianCoordinateSystem.h"
#include "backend/worksheet/plots/cartesian/Plot.h"

class LollipopPlotPrivate;
class AbstractColumn;
class Line;
class Symbol;
class Value;

#ifdef SDK
#include "labplot_export.h"
class LABPLOT_EXPORT LollipopPlot : Plot {
#else
class LollipopPlot : public Plot {
#endif
	Q_OBJECT

public:
	explicit LollipopPlot(const QString&);
	~LollipopPlot() override;

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
	BASIC_D_ACCESSOR_DECL(LollipopPlot::Orientation, orientation, Orientation)
	BASIC_D_ACCESSOR_DECL(double, widthFactor, WidthFactor)

	Line* lineAt(int) const;
	Symbol* symbolAt(int) const;
	Value* value() const;

	void retransform() override;
	void handleResize(double horizontalRatio, double verticalRatio, bool pageResize) override;

	double minimum(CartesianCoordinateSystem::Dimension) const override;
	double maximum(CartesianCoordinateSystem::Dimension) const override;
	bool hasData() const override;
	QColor color() const override;

	typedef LollipopPlotPrivate Private;

protected:
	LollipopPlot(const QString& name, LollipopPlotPrivate* dd);

private:
	Q_DECLARE_PRIVATE(LollipopPlot)
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
	void orientationChanged(LollipopPlot::Orientation);

	// box border
	// void borderPenChanged(QPen&);
	// void borderOpacityChanged(float);
};

#endif
