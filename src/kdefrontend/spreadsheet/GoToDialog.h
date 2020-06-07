/***************************************************************************
    File                 : GoToDialog.h
    Project              : LabPlot
    Description          : Dialog to provide the cell coordinates to navigate to
    --------------------------------------------------------------------
    Copyright            : (C) 2020 by Alexander Semke (alexander.semke@web.de)

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
#ifndef GOTODIALOG_H
#define GOTODIALOG_H

#include <QDialog>
class QLineEdit;

class GoToDialog : public QDialog {
	Q_OBJECT

public:
	explicit GoToDialog(QWidget* parent = nullptr);
	~GoToDialog() override;

	int row();
	int column();

private:
	QLineEdit* leRow;
	QLineEdit* leColumn;
};

#endif
