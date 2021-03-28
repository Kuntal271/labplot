/***************************************************************************
File                 : ReadStatFilterPrivate.h
Project              : LabPlot
Description          : Private implementation class for ReadStatFilter.
--------------------------------------------------------------------
Copyright            : (C) 2021 Stefan Gerlach (stefan.gerlach@uni.kn)
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
#ifndef READSTATFILTERPRIVATE_H
#define READSTATFILTERPRIVATE_H

#ifdef HAVE_READSTAT
extern "C" {
#include <readstat.h>
}
#endif

class AbstractDataSource;

class ReadStatFilterPrivate {

public:
	explicit ReadStatFilterPrivate(ReadStatFilter*);

#ifdef HAVE_READSTAT
	// callbacks (get*)
	static int getMetaData(readstat_metadata_t *, void *);
	static int getVarName(int index, readstat_variable_t*, const char *val_labels, void *);
	static int getColumnModes(int obs_index, readstat_variable_t*, readstat_value_t, void *);
	static int getValuesPreview(int obs_index, readstat_variable_t*, readstat_value_t, void *);
	static int getValues(int obs_index, readstat_variable_t*, readstat_value_t, void *);
	readstat_error_t parse(const QString& fileName, bool preview = false, bool prepare = false);
#endif
	QVector<QStringList> preview(const QString& fileName, int lines);
	void readDataFromFile(const QString& fileName, AbstractDataSource* = nullptr,
			AbstractFileFilter::ImportMode = AbstractFileFilter::ImportMode::Replace);
	void write(const QString& fileName, AbstractDataSource*);
	static QStringList m_varNames;
	static QVector<AbstractColumn::ColumnMode> m_columnModes;
	static QVector<QStringList> m_dataStrings;

	const ReadStatFilter* q;

	static int m_startRow;
	static int m_endRow;
	static int m_startColumn;
	static int m_endColumn;
private:
	//int m_status;

	static int m_varCount;	// nr of cols (vars)
	static int m_rowCount;	// nr of rows
	static QStringList m_lineString;
	static std::vector<void*> m_dataContainer;
};

#endif
