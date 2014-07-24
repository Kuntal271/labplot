/***************************************************************************
    File                 : CartesianPlot.h
    Project              : LabPlot
    Description          : Cartesian plot
    --------------------------------------------------------------------
    Copyright            : (C) 2011-2014 by Alexander Semke (alexander.semke@web.de)

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

#ifndef CARTESIANPLOT_H
#define CARTESIANPLOT_H

#include "backend/worksheet/plots/AbstractPlot.h"

class QToolBar;
class CartesianPlotPrivate;
class CartesianPlotLegend;
class XYCurve;
class XYEquationCurve;
class XYFitCurve;

class CartesianPlot:public AbstractPlot{
	Q_OBJECT

	public:
		explicit CartesianPlot(const QString &name);
		virtual ~CartesianPlot();

		enum Scale {ScaleLinear, ScaleLog10, ScaleLog2, ScaleLn, ScaleSqrt, ScaleX2};
		enum Type {FourAxes, TwoAxes, TwoAxesCentered, TwoAxesCenteredZero};
		enum ScaleBreakingStyle {ScaleBreakingSimple, ScaleBreakingVertical, ScaleBreakingSloped};
		enum MouseMode {SelectionMode, ZoomSelectionMode, ZoomXSelectionMode, ZoomYSelectionMode};

		struct ScaleBreaking {
			ScaleBreaking() : start(0), end(0), position(0.5), isValid(true) {};
			float start;
			float end;
			float position;
			ScaleBreakingStyle style;
			bool isValid;
		};

		//simple wrapper for QList<ScaleBreaking> in order to get our macros working
		struct ScaleBreakings {
			QList<ScaleBreaking> list;
		};

		void initDefault(Type=FourAxes);
		QIcon icon() const;
		QMenu* createContextMenu();
		void fillToolBar(QToolBar*) const;
		void setRect(const QRectF&);
		QRectF plotRect();
		MouseMode mouseMode() const;

		virtual void save(QXmlStreamWriter*) const;
		virtual bool load(XmlStreamReader*);

		BASIC_D_ACCESSOR_DECL(bool, autoScaleX, AutoScaleX)
		BASIC_D_ACCESSOR_DECL(bool, autoScaleY, AutoScaleY)
		BASIC_D_ACCESSOR_DECL(float, xMin, XMin)
		BASIC_D_ACCESSOR_DECL(float, xMax, XMax)
		BASIC_D_ACCESSOR_DECL(float, yMin, YMin)
		BASIC_D_ACCESSOR_DECL(float, yMax, YMax)
		BASIC_D_ACCESSOR_DECL(CartesianPlot::Scale, xScale, XScale)
		BASIC_D_ACCESSOR_DECL(CartesianPlot::Scale, yScale, YScale)
		CLASS_D_ACCESSOR_DECL(ScaleBreakings, xScaleBreakings, XScaleBreakings);
		CLASS_D_ACCESSOR_DECL(ScaleBreakings, yScaleBreakings, YScaleBreakings);

		typedef CartesianPlot BaseClass;
		typedef CartesianPlotPrivate Private;

	private:
		void init();
		void initActions();
		void initMenus();

		CartesianPlotLegend* m_legend;
		float m_zoomFactor;

		QAction* visibilityAction;

		QAction* selectionModeAction;
		QAction* zoomSelectionModeAction;
		QAction* zoomXSelectionModeAction;
		QAction* zoomYSelectionModeAction;

		QAction* addCurveAction;
		QAction* addEquationCurveAction;
		QAction* addFitCurveAction;
		QAction* addHorizontalAxisAction;
		QAction* addVerticalAxisAction;
 		QAction* addLegendAction;

		QAction* scaleAutoXAction;
		QAction* scaleAutoYAction;
		QAction* scaleAutoAction;
		QAction* zoomInAction;
		QAction* zoomOutAction;
		QAction* zoomInXAction;
		QAction* zoomOutXAction;
		QAction* zoomInYAction;
		QAction* zoomOutYAction;
		QAction* shiftLeftXAction;
		QAction* shiftRightXAction;
		QAction* shiftUpYAction;
		QAction* shiftDownYAction;

		QMenu* addNewMenu;
		QMenu* zoomMenu;

		Q_DECLARE_PRIVATE(CartesianPlot)

	private slots:
		void addAxis();
		XYCurve* addCurve();
		XYEquationCurve* addEquationCurve();
		XYFitCurve* addFitCurve();
		void addLegend();
		void updateLegend();
		void childAdded(const AbstractAspect*);
		void childRemoved(const AbstractAspect* parent, const AbstractAspect* before, const AbstractAspect* child);

		void dataChanged();
		void xDataChanged();
		void yDataChanged();

		void mouseModeChanged(QAction*);
		void scaleAuto();
		void scaleAutoX();
		void scaleAutoY();
		void zoomIn();
		void zoomOut();
		void zoomInX();
		void zoomOutX();
		void zoomInY();
		void zoomOutY();
		void shiftLeftX();
		void shiftRightX();
		void shiftUpY();
		void shiftDownY();

		//SLOTs for changes triggered via QActions in the context menu
		void visibilityChanged();

	protected:
		CartesianPlot(const QString &name, CartesianPlotPrivate *dd);

	signals:
		friend class CartesianPlotSetRectCmd;
		friend class CartesianPlotSetXMinCmd;
		friend class CartesianPlotSetXMaxCmd;
		friend class CartesianPlotSetXScaleCmd;
		friend class CartesianPlotSetYMinCmd;
		friend class CartesianPlotSetYMaxCmd;
		friend class CartesianPlotSetYScaleCmd;
		friend class CartesianPlotSetXScaleBreakingsCmd;
		friend class CartesianPlotSetYScaleBreakingsCmd;
		void rectChanged(QRectF&);
		void xMinChanged(float);
		void xMaxChanged(float);
		void xScaleChanged(int);
		void yMinChanged(float);
		void yMaxChanged(float);
		void yScaleChanged(int);
		void xScaleBreakingsChanged(const CartesianPlot::ScaleBreakings&);
		void yScaleBreakingsChanged(const CartesianPlot::ScaleBreakings&);
};

#endif
