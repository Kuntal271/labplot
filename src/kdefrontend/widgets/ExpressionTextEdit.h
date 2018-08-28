/***************************************************************************
    File             : ExpressionTextEdit.h
    Project          : LabPlot
    --------------------------------------------------------------------
    Copyright        : (C) 2014-2017 Alexander Semke (alexander.semke@web.de)
    Description      : widget for defining mathematical expressions
					   modified version of
					   http://qt-project.org/doc/qt-4.8/tools-customcompleter.html
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

/****************************************************************************
 **
 ** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the examples of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:BSD$
 ** You may use this file under the terms of the BSD license as follows:
 **
 ** "Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions are
 ** met:
 **   * Redistributions of source code must retain the above copyright
 **     notice, this list of conditions and the following disclaimer.
 **   * Redistributions in binary form must reproduce the above copyright
 **     notice, this list of conditions and the following disclaimer in
 **     the documentation and/or other materials provided with the
 **     distribution.
 **   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
 **     of its contributors may be used to endorse or promote products derived
 **     from this software without specific prior written permission.
 **
 **
 ** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 ** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 ** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 ** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 ** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 ** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 ** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 ** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 **
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/

#ifndef EXPRESSIONTEXTEDIT_H
#define EXPRESSIONTEXTEDIT_H

#include "backend/worksheet/plots/cartesian/XYEquationCurve.h"
#include <KTextWidgets/KTextEdit>

class QCompleter;
class EquationHighlighter;

class ExpressionTextEdit : public KTextEdit {
	Q_OBJECT

public:
	explicit ExpressionTextEdit(QWidget *parent = nullptr);
	EquationHighlighter* highlighter();
	void setExpressionType(XYEquationCurve::EquationType);
	void setVariables(const QStringList&);
	bool isValid() const;

protected:
	void keyPressEvent(QKeyEvent*) override;
	void focusInEvent(QFocusEvent*) override;
	void focusOutEvent(QFocusEvent*) override;
	void mouseMoveEvent(QMouseEvent*) override;

private slots:
	void insertCompletion(const QString&);
	void validateExpression(bool force=false);

private:
	EquationHighlighter* m_highlighter;
	XYEquationCurve::EquationType m_expressionType;
	QStringList m_variables;
	QCompleter* m_completer;
	bool m_isValid;
	QString m_currentExpression;

signals:
	void expressionChanged();
};

#endif
