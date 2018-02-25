/***************************************************************************
    File                 : OriginProjectParser.h
    Project              : LabPlot
    Description          : parser for Origin projects
    --------------------------------------------------------------------
    Copyright            : (C) 2017 Alexander Semke (alexander.semke@web.de)
    Copyright            : (C) 2017-2018 Stefan Gerlach (stefan.gerlach@uni.kn)

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

#include "backend/datasources/projects/OriginProjectParser.h"
#include "backend/core/column/Column.h"
#include "backend/core/datatypes/Double2StringFilter.h"
#include "backend/core/datatypes/DateTime2StringFilter.h"
#include "backend/core/Project.h"
#include "backend/core/Workbook.h"
#include "backend/matrix/Matrix.h"
#include "backend/note/Note.h"
#include "backend/spreadsheet/Spreadsheet.h"
#include "backend/worksheet/Worksheet.h"
#include "backend/worksheet/plots/cartesian/Axis.h"
#include "backend/worksheet/plots/cartesian/CartesianPlot.h"
#include "backend/worksheet/plots/cartesian/CartesianPlotLegend.h"
#include "backend/worksheet/plots/cartesian/XYCurve.h"
#include "backend/worksheet/TextLabel.h"

#include <liborigin/OriginFile.h>

#include <KLocale>

#include <QDir>
#include <QDateTime>
#include <QFontMetrics>

/*!
\class OriginProjectParser
\brief parser for Origin projects.

\ingroup datasources
*/

OriginProjectParser::OriginProjectParser() : ProjectParser(), m_originFile(nullptr), m_importUnusedObjects(false) {
	m_topLevelClasses << "Folder" << "Workbook" << "Spreadsheet" << "Matrix" << "Worksheet" << "Note";
}

bool OriginProjectParser::isOriginProject(const QString& fileName) {
	//TODO add opju later when liborigin supports it
	return fileName.endsWith(".opj", Qt::CaseInsensitive);
}

void OriginProjectParser::setImportUnusedObjects(bool importUnusedObjects) {
	m_importUnusedObjects = importUnusedObjects;
}

bool OriginProjectParser::hasUnusedObjects() {
	if (m_originFile)
		delete m_originFile;
	m_originFile = new OriginFile((const char*)m_projectFileName.toLocal8Bit());
	if (!m_originFile->parse())
		return false;

	for (unsigned int i = 0; i < m_originFile->spreadCount(); i++) {
		const Origin::SpreadSheet& spread = m_originFile->spread(i);
		if (spread.objectID < 0)
			return true;
	}
	for (unsigned int i = 0; i < m_originFile->excelCount(); i++) {
		const Origin::Excel& excel = m_originFile->excel(i);
		if (excel.objectID < 0)
			return true;
	}
	for (unsigned int i = 0; i < m_originFile->matrixCount(); i++) {
		const Origin::Matrix& originMatrix = m_originFile->matrix(i);
		if (originMatrix.objectID < 0)
			return true;
	}

	return false;
}

QString OriginProjectParser::supportedExtensions() {
	//TODO add opju later when liborigin supports it
	static const QString extensions = "*.opj *.OPJ";
	return extensions;
}

unsigned int OriginProjectParser::findSpreadByName(QString name) {
	for (unsigned int i = 0; i < m_originFile->spreadCount(); i++) {
		const Origin::SpreadSheet& spread = m_originFile->spread(i);
		if (spread.name == name.toStdString()) {
			m_spreadNameList << name;
			return i;
		}
	}
	return 0;
}
unsigned int OriginProjectParser::findMatrixByName(QString name) {
	for (unsigned int i = 0; i < m_originFile->matrixCount(); i++) {
		const Origin::Matrix& originMatrix = m_originFile->matrix(i);
		if (originMatrix.name == name.toStdString()) {
			m_matrixNameList << name;
			return i;
		}
	}
	return 0;
}
unsigned int OriginProjectParser::findExcelByName(QString name) {
	for (unsigned int i = 0; i < m_originFile->excelCount(); i++) {
		const Origin::Excel& excel = m_originFile->excel(i);
		if (excel.name == name.toStdString()) {
			m_excelNameList << name;
			return i;
		}
	}
	return 0;
}
unsigned int OriginProjectParser::findGraphByName(QString name) {
	for (unsigned int i = 0; i < m_originFile->graphCount(); i++) {
		const Origin::Graph& graph = m_originFile->graph(i);
		if (graph.name == name.toStdString()) {
			m_graphNameList << name;
			return i;
		}
	}
	return 0;
}
unsigned int OriginProjectParser::findNoteByName(QString name) {
	for (unsigned int i = 0; i < m_originFile->noteCount(); i++) {
		const Origin::Note& originNote = m_originFile->note(i);
		if (originNote.name == name.toStdString()) {
			m_noteNameList << name;
			return i;
		}
	}
	return 0;
}

//##############################################################################
//############## Deserialization from Origin's project tree ####################
//##############################################################################
bool OriginProjectParser::load(Project* project, bool preview) {
	DEBUG("OriginProjectParser::load()");
	//read and parse the m_originFile-file
	if (m_originFile)
		delete m_originFile;

	m_originFile = new OriginFile((const char*)m_projectFileName.toLocal8Bit());
	if (!m_originFile->parse())
		return false;

	//Origin project tree and the iterator pointing to the root node
	const tree<Origin::ProjectNode>* projectTree = m_originFile->project();
	tree<Origin::ProjectNode>::iterator projectIt = projectTree->begin(projectTree->begin());

	m_spreadNameList.clear();
	m_excelNameList.clear();
	m_matrixNameList.clear();
	m_graphNameList.clear();
	m_noteNameList.clear();

	//convert the project tree from liborigin's representation to LabPlot's project object
	if (projectIt.node) { // only opj files from version >= 6.0 do have project tree
		QString name(QString::fromLatin1(projectIt->name.c_str()));
		project->setName(name);
		project->setCreationTime(creationTime(projectIt));
		loadFolder(project, projectIt, preview);
	} else { // for lower versions put all windows on rootfolder
		int pos = m_projectFileName.lastIndexOf(QDir::separator()) + 1;
		project->setName((const char*)m_projectFileName.mid(pos).toLocal8Bit());
	}
	// imports all loose windows (like prior version 6 which has no project tree)
	handleLooseWindows(project, preview);

	return true;
}

bool OriginProjectParser::loadFolder(Folder* folder, const tree<Origin::ProjectNode>::iterator& baseIt, bool preview) {
	DEBUG("OriginProjectParser::loadFolder()\n--------------------------------");
	const tree<Origin::ProjectNode>* projectTree = m_originFile->project();

	//load folder's children: logic for reading the selected objects only is similar to Folder::readChildAspectElement
	for (tree<Origin::ProjectNode>::sibling_iterator it = projectTree->begin(baseIt); it != projectTree->end(baseIt); ++it) {
		QString name(QString::fromLatin1(it->name.c_str())); //name of the current child
		DEBUG("	* folder item name = " << name.toStdString());

		//check whether we need to skip the loading of the current child
		if (!folder->pathesToLoad().isEmpty()) {
			//child's path is not available yet (child not added yet) -> construct the path manually
			const QString childPath = folder->path() + '/' + name;

			//skip the current child aspect it is not in the list of aspects to be loaded
			if (folder->pathesToLoad().indexOf(childPath) == -1)
				continue;
		}

		//load top-level children
		AbstractAspect* aspect = nullptr;
		switch (it->type) {
		case Origin::ProjectNode::Folder: {
			DEBUG("	top level folder");
			Folder* f = new Folder(name);

			if (!folder->pathesToLoad().isEmpty()) {
				//a child folder to be read -> provide the list of aspects to be loaded to the child folder, too.
				//since the child folder and all its children are not added yet (path() returns empty string),
				//we need to remove the path of the current child folder from the full pathes provided in pathesToLoad.
				//E.g. we want to import the path "Project/Folder/Spreadsheet" in the following project
				// Project
				//        \Spreadsheet
				//        \Folder
				//               \Spreadsheet
				//
				//Here, we remove the part "Project/Folder/" and proceed for this child folder with "Spreadsheet" only.
				//With this the logic above where it is determined whether to import the child aspect or not works out.

				//manually construct the path of the child folder to be read
				const QString& curFolderPath = folder->path()  + '/' + name;

				//remove the path of the current child folder
				QStringList pathesToLoadNew;
				for (auto path : folder->pathesToLoad()) {
					if (path.startsWith(curFolderPath))
						pathesToLoadNew << path.right(path.length() - curFolderPath.length());
				}

				f->setPathesToLoad(pathesToLoadNew);
			}

			loadFolder(f, it, preview);
			aspect = f;
			break;
		}
		case Origin::ProjectNode::SpreadSheet: {
			DEBUG("	top level spreadsheet");
			Spreadsheet* spreadsheet = new Spreadsheet(0, name);
			loadSpreadsheet(spreadsheet, preview, name);
			aspect = spreadsheet;
			break;
		}
		case Origin::ProjectNode::Graph: {
			DEBUG("	top level graph");
			Worksheet* worksheet = new Worksheet(0, name);
			loadWorksheet(worksheet, preview);
			aspect = worksheet;
			break;
		}
		case Origin::ProjectNode::Matrix: {
			DEBUG("	top level matrix");
			const Origin::Matrix& originMatrix = m_originFile->matrix(findMatrixByName(name));
			DEBUG("	matrix name = " << originMatrix.name);
			DEBUG("	number of sheets = " << originMatrix.sheets.size());
			if (originMatrix.sheets.size() == 1) {
				// single sheet -> load into a matrix
				Matrix* matrix = new Matrix(0, name);
				loadMatrix(matrix, preview);
				aspect = matrix;
			} else {
				// multiple sheets -> load into a workbook
				Workbook* workbook = new Workbook(0, name);
				loadMatrixWorkbook(workbook, preview);
				aspect = workbook;
			}
			break;
		}
		case Origin::ProjectNode::Excel: {
			DEBUG("	top level excel");
			Workbook* workbook = new Workbook(0, name);
			loadWorkbook(workbook, preview);
			aspect = workbook;
			break;
		}
		case Origin::ProjectNode::Note: {
			DEBUG("top level note");
			Note* note = new Note(name);
			loadNote(note, preview);
			aspect = note;
			break;
		}
		case Origin::ProjectNode::Graph3D:
		default:
			//TODO: add UnsupportedAspect
			break;
		}

		if (aspect) {
			folder->addChildFast(aspect);
			aspect->setCreationTime(creationTime(it));
		}
	}

	// ResultsLog
	QString resultsLog = QString::fromLatin1(m_originFile->resultsLogString().c_str());
	if (resultsLog.length() > 0) {
		DEBUG("Results log:\t\tyes");
		Note* note = new Note("ResultsLog");

		if (preview)
			folder->addChildFast(note);
		else {
			//only import the log if it is in the list of aspects to be loaded
			const QString childPath = folder->path() + '/' + note->name();
			if (folder->pathesToLoad().indexOf(childPath) != -1) {
				note->setText(resultsLog);
				folder->addChildFast(note);
			}
		}
	} else
		DEBUG("Results log:\t\tno");

	return folder;
}

void OriginProjectParser::handleLooseWindows(Folder* folder, bool preview) {
	DEBUG("OriginProjectParser::handleLooseWindows()");
	QDEBUG("pathes to load:" << folder->pathesToLoad());
	m_spreadNameList.removeDuplicates();
	m_excelNameList.removeDuplicates();
	m_matrixNameList.removeDuplicates();
	m_graphNameList.removeDuplicates();
	m_noteNameList.removeDuplicates();
	QDEBUG("	spreads =" << m_spreadNameList);
	QDEBUG("	excels =" << m_excelNameList);
	QDEBUG("	matrices =" << m_matrixNameList);
	QDEBUG("	graphs =" << m_graphNameList);
	QDEBUG("	notes =" << m_noteNameList);

	DEBUG("Number of spreads loaded:\t" << m_spreadNameList.size() << ", in file: " << m_originFile->spreadCount());
	DEBUG("Number of excels loaded:\t" << m_excelNameList.size() << ", in file: " << m_originFile->excelCount());
	DEBUG("Number of matrices loaded:\t" << m_matrixNameList.size() << ", in file: " << m_originFile->matrixCount());
	DEBUG("Number of graphs loaded:\t" << m_graphNameList.size() << ", in file: " << m_originFile->graphCount());
	DEBUG("Number of notes loaded:\t\t" << m_noteNameList.size() << ", in file: " << m_originFile->noteCount());

	// loop over all spreads to find loose ones
	for (unsigned int i = 0; i < m_originFile->spreadCount(); i++) {
		AbstractAspect* aspect = nullptr;
		const Origin::SpreadSheet& spread = m_originFile->spread(i);
		QString name = QString::fromStdString(spread.name);

		DEBUG("	spread.objectId = " << spread.objectID);
		// skip unused spreads if selected
		if (spread.objectID < 0 && !m_importUnusedObjects) {
			DEBUG("	Dropping unused loose spread: " << name.toStdString());
			continue;
		}

		const QString childPath = folder->path() + '/' + name;
		// we could also use spread.loose
		if (!m_spreadNameList.contains(name) && (preview || (!preview && folder->pathesToLoad().indexOf(childPath) != -1))) {
			DEBUG("	Adding loose spread: " << name.toStdString());

			Spreadsheet* spreadsheet = new Spreadsheet(0, name);
			loadSpreadsheet(spreadsheet, preview, name);
			aspect = spreadsheet;
		}
		if (aspect) {
			folder->addChildFast(aspect);
			DEBUG("	creation time as reported by liborigin: " << spread.creationDate);
			aspect->setCreationTime(QDateTime::fromTime_t(spread.creationDate));
		}
	}
	// loop over all excels to find loose ones
	for (unsigned int i = 0; i < m_originFile->excelCount(); i++) {
		AbstractAspect* aspect = nullptr;
		const Origin::Excel& excel = m_originFile->excel(i);
		QString name = QString::fromStdString(excel.name);

		DEBUG("	excel.objectId = " << excel.objectID);
		// skip unused data sets if selected
		if (excel.objectID < 0 && !m_importUnusedObjects) {
			DEBUG("	Dropping unused loose excel: " << name.toStdString());
			continue;
		}

		const QString childPath = folder->path() + '/' + name;
		// we could also use excel.loose
		if (!m_excelNameList.contains(name) && (preview || (!preview && folder->pathesToLoad().indexOf(childPath) != -1))) {
			DEBUG("	Adding loose excel: " << name.toStdString());
			DEBUG("	 containing number of sheets = " << excel.sheets.size());

			Workbook* workbook = new Workbook(0, name);
			loadWorkbook(workbook, preview);
			aspect = workbook;
		}
		if (aspect) {
			folder->addChildFast(aspect);
			DEBUG("	creation time as reported by liborigin: " << excel.creationDate);
			aspect->setCreationTime(QDateTime::fromTime_t(excel.creationDate));
		}
	}
	// loop over all matrices to find loose ones
	for (unsigned int i = 0; i < m_originFile->matrixCount(); i++) {
		AbstractAspect* aspect = nullptr;
		const Origin::Matrix& originMatrix = m_originFile->matrix(i);
		QString name = QString::fromStdString(originMatrix.name);

		DEBUG("	originMatrix.objectId = " << originMatrix.objectID);
		// skip unused data sets if selected
		if (originMatrix.objectID < 0 && !m_importUnusedObjects) {
			DEBUG("	Dropping unused loose matrix: " << name.toStdString());
			continue;
		}

		const QString childPath = folder->path() + '/' + name;
		if (!m_matrixNameList.contains(name) && (preview || (!preview && folder->pathesToLoad().indexOf(childPath) != -1))) {
			DEBUG("	Adding loose matrix: " << name.toStdString());
			DEBUG("	containing number of sheets = " << originMatrix.sheets.size());
			if (originMatrix.sheets.size() == 1) { // single sheet -> load into a matrix
				Matrix* matrix = new Matrix(0, name);
				loadMatrix(matrix, preview);
				aspect = matrix;
			} else { // multiple sheets -> load into a workbook
				Workbook* workbook = new Workbook(0, name);
				loadMatrixWorkbook(workbook, preview);
				aspect = workbook;
			}
		}
		if (aspect) {
			folder->addChildFast(aspect);
			aspect->setCreationTime(QDateTime::fromTime_t(originMatrix.creationDate));
		}
	}
	// handle loose graphs (is this even possible?)
	for (unsigned int i = 0; i < m_originFile->graphCount(); i++) {
		AbstractAspect* aspect = nullptr;
		const Origin::Graph& graph = m_originFile->graph(i);
		QString name = QString::fromStdString(graph.name);

		DEBUG("	graph.objectId = " << graph.objectID);
		// skip unused graph if selected
		if (graph.objectID < 0 && !m_importUnusedObjects) {
			DEBUG("	Dropping unused loose graph: " << name.toStdString());
			continue;
		}

		const QString childPath = folder->path() + '/' + name;
		if (!m_graphNameList.contains(name) && (preview || (!preview && folder->pathesToLoad().indexOf(childPath) != -1))) {
			DEBUG("	Adding loose graph: " << name.toStdString());
			Worksheet* worksheet = new Worksheet(0, name);
			loadWorksheet(worksheet, preview);
			aspect = worksheet;
		}
		if (aspect) {
			folder->addChildFast(aspect);
			aspect->setCreationTime(QDateTime::fromTime_t(graph.creationDate));
		}
	}
	// handle loose notes (is this even possible?)
	for (unsigned int i = 0; i < m_originFile->noteCount(); i++) {
		AbstractAspect* aspect = nullptr;
		const Origin::Note& originNote = m_originFile->note(i);
		QString name = QString::fromStdString(originNote.name);

		DEBUG("	originNote.objectId = " << originNote.objectID);
		// skip unused notes if selected
		if (originNote.objectID < 0 && !m_importUnusedObjects) {
			DEBUG("	Dropping unused loose note: " << name.toStdString());
			continue;
		}

		const QString childPath = folder->path() + '/' + name;
		if (!m_noteNameList.contains(name) && (preview || (!preview && folder->pathesToLoad().indexOf(childPath) != -1))) {
			DEBUG("	Adding loose note: " << name.toStdString());
			Note* note = new Note(name);
			loadNote(note, preview);
			aspect = note;
		}
		if (aspect) {
			folder->addChildFast(aspect);
			QDateTime dt = QDateTime::fromTime_t(originNote.creationDate);
			aspect->setCreationTime(QDateTime::fromTime_t(originNote.creationDate));
		}
	}
}

bool OriginProjectParser::loadWorkbook(Workbook* workbook, bool preview) {
	DEBUG("loadWorkbook()");
	//load workbook sheets
	const Origin::Excel& excel = m_originFile->excel(findExcelByName(workbook->name()));
	DEBUG(" excel name = " << excel.name);
	DEBUG(" number of sheets = " << excel.sheets.size());
	for (unsigned int s = 0; s < excel.sheets.size(); ++s) {
		Spreadsheet* spreadsheet = new Spreadsheet(0, QString::fromLatin1(excel.sheets[s].name.c_str()));
		loadSpreadsheet(spreadsheet, preview, workbook->name(), s);
		workbook->addChildFast(spreadsheet);
	}

	return true;
}

// load spreadsheet from spread (sheetIndex == -1) or from excel (only sheet sheetIndex)
bool OriginProjectParser::loadSpreadsheet(Spreadsheet* spreadsheet, bool preview, const QString& name, int sheetIndex) {
	DEBUG("loadSpreadsheet() sheetIndex = " << sheetIndex);

	//load spreadsheet data
	Origin::SpreadSheet spread;
	Origin::Excel excel;
	if (sheetIndex == -1)	// spread
		spread = m_originFile->spread(findSpreadByName(name));
	else {
		excel = m_originFile->excel(findExcelByName(name));
		spread = excel.sheets[sheetIndex];
	}

	if (preview)
		return true;

	const size_t cols = spread.columns.size();
	int rows = 0;
	for (size_t j = 0; j < cols; ++j)
		rows = std::max((int)spread.columns[j].data.size(), rows);
	// alternative: int rows = excel.maxRows;
	DEBUG("loadSpreadsheet() cols/maxRows = " << cols << "/" << rows);

	if (rows < 0 || (int)cols < 0)
		return false;

	//TODO QLocale locale = mw->locale();

	spreadsheet->setRowCount(rows);
	spreadsheet->setColumnCount((int)cols);
	if (sheetIndex == -1)
		spreadsheet->setComment(QString::fromLatin1(spread.label.c_str()));
	else
		spreadsheet->setComment(QString::fromLatin1(excel.label.c_str()));

	//in Origin column width is measured in characters, we need to convert to pixels
	//TODO: determine the font used in Origin in order to get the same column width as in Origin
	QFont font;
	QFontMetrics fm(font);
	const int scaling_factor = fm.maxWidth();

	for (size_t j = 0; j < cols; ++j) {
		Origin::SpreadColumn column = spread.columns[j];
		Column* col = spreadsheet->column((int)j);

		QString name(column.name.c_str());
		col->setName(name.replace(QRegExp(".*_"),""));

		//TODO: we don't support any formulas for cells yet.
// 		if (column.command.size() > 0)
// 			col->setFormula(Interval<int>(0, rows), QString(column.command.c_str()));

		col->setComment(QString::fromLatin1(column.comment.c_str()));
		col->setWidth((int)column.width * scaling_factor);

		//plot designation
		switch (column.type) {
		case Origin::SpreadColumn::X:
			col->setPlotDesignation(AbstractColumn::X);
			break;
		case Origin::SpreadColumn::Y:
			col->setPlotDesignation(AbstractColumn::Y);
			break;
		case Origin::SpreadColumn::Z:
			col->setPlotDesignation(AbstractColumn::Z);
			break;
		case Origin::SpreadColumn::XErr:
			col->setPlotDesignation(AbstractColumn::XError);
			break;
		case Origin::SpreadColumn::YErr:
			col->setPlotDesignation(AbstractColumn::YError);
			break;
		case Origin::SpreadColumn::Label:
		case Origin::SpreadColumn::NONE:
		default:
			col->setPlotDesignation(AbstractColumn::NoDesignation);
		}

		QString format;
		switch(column.valueType) {
		case Origin::Numeric:
		case Origin::TextNumeric: {
			/* TODO: check this
			A TextNumeric column in Origin is a column whose filled cells contain either a double or a string.
			Here there is no equivalent column type.
			Set the column type as 'Numeric' or 'Text' depending on the type of first element in column.
			IDEA: Add a "per column" flag, settable at import dialog, to choose between both types.
			 */
			double datavalue;
			bool setAsText = false;
			col->setColumnMode(AbstractColumn::Numeric);
			//printf("column has %ld rows\n", column.data.size());
			for (int i = 0; i < std::min((int)column.data.size(), rows); ++i) {
				Origin::variant v(column.data.at(i));
				//printf("i=%d type = %d\n", i, v.type);
				if (v.type() == Origin::Variant::V_DOUBLE) {
					//printf("DOUBLE !\n");
					datavalue = v.as_double();
					//printf("datavalue = %g\n", datavalue);
					if (datavalue == _ONAN) continue; // mark for empty cell
					if (!setAsText)
						col->setValueAt(i, datavalue);
//TODO					else	// convert double to string for Text columns
//						col->setTextAt(i, locale.toString(datavalue, 'g', 16));
				} else if (v.type() == Origin::Variant::V_STRING) {
					//printf("STRING !\n");
					if (!setAsText && i == 0) {
						col->setColumnMode(AbstractColumn::Text);
						setAsText = true;
					}
					col->setTextAt(i, v.as_string());
				} else {
					printf("ERROR: data type = %d unknown!\n", v.type());
				}
			}
			if (column.numericDisplayType != 0) {
				int fi = 0;
				switch (column.valueTypeSpecification) {
				case Origin::Decimal:
					fi = 1;
					break;
				case Origin::Scientific:
					fi = 2;
					break;
				case Origin::Engineering:
				case Origin::DecimalWithMarks:
					break;
				}

				Double2StringFilter *filter = static_cast<Double2StringFilter*>(col->outputFilter());
				filter->setNumericFormat(fi);
				filter->setNumDigits(column.decimalPlaces);
			}
			break;
		}
		case Origin::Text:
			col->setColumnMode(AbstractColumn::Text);
			for (int i = 0; i < min((int)column.data.size(), rows); ++i)
				col->setTextAt(i, column.data[i].as_string());
			break;
		case Origin::Time: {
			switch (column.valueTypeSpecification + 128) {
			case Origin::TIME_HH_MM:
				format="hh:mm";
				break;
			case Origin::TIME_HH:
				format="hh";
				break;
			case Origin::TIME_HH_MM_SS:
				format="hh:mm:ss";
				break;
			case Origin::TIME_HH_MM_SS_ZZ:
				format="hh:mm:ss.zzz";
				break;
			case Origin::TIME_HH_AP:
				format="hh ap";
				break;
			case Origin::TIME_HH_MM_AP:
				format="hh:mm ap";
				break;
			case Origin::TIME_MM_SS:
				format="mm:ss";
				break;
			case Origin::TIME_MM_SS_ZZ:
				format="mm:ss.zzz";
				break;
			case Origin::TIME_HHMM:
				format="hhmm";
				break;
			case Origin::TIME_HHMMSS:
				format="hhmmss";
				break;
			case Origin::TIME_HH_MM_SS_ZZZ:
				format="hh:mm:ss.zzz";
				break;
			}

			for (int i = 0; i < min((int)column.data.size(), rows); ++i)
				col->setValueAt(i, column.data[i].as_double());
			col->setColumnMode(AbstractColumn::DateTime);

			DateTime2StringFilter *filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
			filter->setFormat(format);
			break;
		}
		case Origin::Date: {
			switch (column.valueTypeSpecification) {
			case Origin::DATE_DD_MM_YYYY:
				format="dd/MM/yyyy";
				break;
			case Origin::DATE_DD_MM_YYYY_HH_MM:
				format="dd/MM/yyyy HH:mm";
				break;
			case Origin::DATE_DD_MM_YYYY_HH_MM_SS:
				format="dd/MM/yyyy HH:mm:ss";
				break;
			case Origin::DATE_DDMMYYYY:
			case Origin::DATE_DDMMYYYY_HH_MM:
			case Origin::DATE_DDMMYYYY_HH_MM_SS:
				format="dd.MM.yyyy";
					break;
			case Origin::DATE_MMM_D:
				format="MMM d";
				break;
			case Origin::DATE_M_D:
				format="M/d";
				break;
			case Origin::DATE_D:
				format="d";
				break;
			case Origin::DATE_DDD:
			case Origin::DATE_DAY_LETTER:
				format="ddd";
				break;
			case Origin::DATE_YYYY:
				format="yyyy";
				break;
			case Origin::DATE_YY:
				format="yy";
				break;
			case Origin::DATE_YYMMDD:
			case Origin::DATE_YYMMDD_HH_MM:
			case Origin::DATE_YYMMDD_HH_MM_SS:
			case Origin::DATE_YYMMDD_HHMM:
			case Origin::DATE_YYMMDD_HHMMSS:
				format="yyMMdd";
				break;
			case Origin::DATE_MMM:
			case Origin::DATE_MONTH_LETTER:
				format="MMM";
				break;
			case Origin::DATE_M_D_YYYY:
				format="M-d-yyyy";
				break;
			default:
				format="dd.MM.yyyy";
			}

			for (int i = 0; i < min((int)column.data.size(), rows); ++i)
				col->setValueAt(i, column.data[i].as_double());
			col->setColumnMode(AbstractColumn::DateTime);

			DateTime2StringFilter *filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
			filter->setFormat(format);
			break;
		}
		case Origin::Month: {
			switch (column.valueTypeSpecification) {
			case Origin::MONTH_MMM:
				format = "MMM";
				break;
			case Origin::MONTH_MMMM:
				format = "MMMM";
				break;
			case Origin::MONTH_LETTER:
				format = "M";
				break;
			}

			for (int i = 0; i < min((int)column.data.size(), rows); ++i)
				col->setValueAt(i, column.data[i].as_double());
			col->setColumnMode(AbstractColumn::Month);

			DateTime2StringFilter *filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
			filter->setFormat(format);
			break;
		}
		case Origin::Day: {
			switch (column.valueTypeSpecification) {
			case Origin::DAY_DDD:
				format = "ddd";
				break;
			case Origin::DAY_DDDD:
				format = "dddd";
				break;
			case Origin::DAY_LETTER:
				format = "d";
				break;
			}

			for (int i = 0; i < min((int)column.data.size(), rows); ++i)
				col->setValueAt(i, column.data[i].as_double());
			col->setColumnMode(AbstractColumn::Day);

			DateTime2StringFilter *filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
			filter->setFormat(format);
			break;
		}
		case Origin::ColumnHeading:
		case Origin::TickIndexedDataset:
		case Origin::Categorical:
			break;
		}
	}

	//TODO: "hidden" not supporrted yet
//	if (spread.hidden || spread.loose)
//		mw->hideWindow(spreadsheet);

	return true;
}


bool OriginProjectParser::loadMatrixWorkbook(Workbook* workbook, bool preview) {
	DEBUG("loadMatrixWorkbook()");
	//load matrix workbook sheets
	const Origin::Matrix& originMatrix = m_originFile->matrix(findMatrixByName(workbook->name()));
	for (size_t s = 0; s < originMatrix.sheets.size(); ++s) {
		Matrix* matrix = new Matrix(0, QString::fromLatin1(originMatrix.sheets[s].name.c_str()));
		loadMatrix(matrix, preview, s, workbook->name());
		workbook->addChildFast(matrix);
	}

	return true;
}

bool OriginProjectParser::loadMatrix(Matrix* matrix, bool preview, size_t sheetIndex, const QString& mwbName) {
	DEBUG("loadMatrix()");
	//import matrix data
	const Origin::Matrix& originMatrix = m_originFile->matrix(findMatrixByName(mwbName));

	if (preview)
		return true;


	//in Origin column width is measured in characters, we need to convert to pixels
	//TODO: determine the font used in Origin in order to get the same column width as in Origin
	QFont font;
	QFontMetrics fm(font);
	const int scaling_factor = fm.maxWidth();

	const Origin::MatrixSheet& layer = originMatrix.sheets[sheetIndex];
	const int colCount = layer.columnCount;
	const int rowCount = layer.rowCount;

	matrix->setRowCount(rowCount);
	matrix->setColumnCount(colCount);
	matrix->setFormula(layer.command.c_str());

	//TODO: how to handle different widths for different columns?
	for (int j = 0; j < colCount; j++)
		matrix->setColumnWidth(j, layer.width * scaling_factor);

	//TODO: check colum major vs. row major to improve the performance here
	for (int i = 0; i < rowCount; i++) {
		for (int j = 0; j < colCount; j++)
			matrix->setCell(i, j, layer.data[j + i*colCount]);
	}

	char format = 'g';
	//TODO: prec not support by Matrix
	//int prec = 6;
	switch (layer.valueTypeSpecification) {
	case 0: //Decimal 1000
		format='f';
	//	prec = layer.decimalPlaces;
		break;
	case 1: //Scientific
		format='e';
	//	prec = layer.decimalPlaces;
		break;
	case 2: //Engineering
	case 3: //Decimal 1,000
		format='g';
	//	prec = layer.significantDigits;
		break;
	}

	matrix->setNumericFormat(format);

	return true;
}

bool OriginProjectParser::loadWorksheet(Worksheet* worksheet, bool preview) {
	DEBUG("OriginProjectParser::loadWorksheet()");

	//load worksheet data
	const Origin::Graph& graph = m_originFile->graph(findGraphByName(worksheet->name()));
	DEBUG("	graph name = " << graph.name);

	if (preview)
		return true;

	worksheet->setComment(graph.label.c_str());

	//TODO: width, height, view mode (print view, page view, window view, draft view)

	// worksheet background color
	const Origin::ColorGradientDirection bckgColorGradient = graph.windowBackgroundColorGradient;
	const Origin::Color bckgBaseColor = graph.windowBackgroundColorBase;
	const Origin::Color bckgEndColor =  graph.windowBackgroundColorEnd;
	worksheet->setBackgroundColorStyle(backgroundColorStyle(bckgColorGradient));
	switch (bckgColorGradient) {
		case Origin::ColorGradientDirection::NoGradient:
		case Origin::ColorGradientDirection::TopLeft:
		case Origin::ColorGradientDirection::Left:
		case Origin::ColorGradientDirection::BottomLeft:
		case Origin::ColorGradientDirection::Top:
			worksheet->setBackgroundFirstColor(color(bckgEndColor));
			worksheet->setBackgroundSecondColor(color(bckgBaseColor));
			break;
		case Origin::ColorGradientDirection::Center:
			break;
		case Origin::ColorGradientDirection::Bottom:
		case Origin::ColorGradientDirection::TopRight:
		case Origin::ColorGradientDirection::Right:
		case Origin::ColorGradientDirection::BottomRight:
			worksheet->setBackgroundFirstColor(color(bckgBaseColor));
			worksheet->setBackgroundSecondColor(color(bckgEndColor));
	}

	//TODO: do we need changes on the worksheet layout?

	//add plots
	int index = 1;
	for (const auto& layer: graph.layers) {
		if (!layer.is3D()) {
			CartesianPlot* plot = new CartesianPlot(i18n("Plot") + QString::number(index));

			//TODO: width, height

			//background color
			const Origin::Color& regColor = layer.backgroundColor;
			if (regColor.type == Origin::Color::None)
				plot->plotArea()->setBackgroundOpacity(0);
			else
				plot->plotArea()->setBackgroundFirstColor(color(regColor));

			//border
			if (layer.borderType == Origin::BorderType::None)
				plot->plotArea()->setBorderPen(QPen(Qt::NoPen));
			else
				plot->plotArea()->setBorderPen(QPen(Qt::SolidLine));

			//ranges/scales
			const Origin::GraphAxis& originXAxis = layer.xAxis;
			const Origin::GraphAxis& originYAxis = layer.yAxis;
			plot->setXMin(originXAxis.min);
			plot->setXMax(originXAxis.max);
			plot->setYMin(originYAxis.min);
			plot->setYMax(originYAxis.max);

			//axes
			//x bottom
			if (!originXAxis.formatAxis[0].hidden) {
				Axis* axis = new Axis("x", plot, Axis::AxisHorizontal);
				axis->setSuppressRetransform(true);
				axis->setPosition(Axis::AxisBottom);
				plot->addChild(axis);
				loadAxis(originXAxis, axis, 0);
				axis->setSuppressRetransform(false);
			}

			//x top
			if (!originXAxis.formatAxis[1].hidden) {
				Axis* axis = new Axis("x top", plot, Axis::AxisHorizontal);
				axis->setPosition(Axis::AxisTop);
				axis->setSuppressRetransform(true);
				plot->addChild(axis);
				loadAxis(originXAxis, axis, 1);
				axis->setSuppressRetransform(false);
			}

			//y left
			if (!originYAxis.formatAxis[0].hidden) {
				Axis* axis = new Axis("y", plot, Axis::AxisVertical);
				axis->setSuppressRetransform(true);
				axis->setPosition(Axis::AxisLeft);
				plot->addChild(axis);
				loadAxis(originYAxis, axis, 0);
				axis->setSuppressRetransform(false);
			}

			//y right
			if (!originYAxis.formatAxis[1].hidden) {
				Axis* axis = new Axis("y", plot, Axis::AxisVertical);
				axis->setSuppressRetransform(true);
				axis->setPosition(Axis::AxisRight);
				plot->addChild(axis);
				loadAxis(originYAxis, axis, 1);
				axis->setSuppressRetransform(false);
			}

			//range breaks
			//TODO

			//add legend if available
			const Origin::TextBox& originLegend = layer.legend;
			if (!originLegend.text.empty()) {
				CartesianPlotLegend* legend = new CartesianPlotLegend(plot, i18n("legend"));
				legend->title()->setText( parseOriginText(QString::fromLatin1(layer.legend.text.c_str())) );

				const Origin::Color& originColor = originLegend.color;
				if (originColor.type == Origin::Color::None)
					legend->setBackgroundOpacity(0);
				else
					legend->setBackgroundFirstColor(color(originColor));

				//TODO: position, rotation, etc.
				plot->addChild(legend);
			}

			//add texts
			// TODO: not supported yet...
	/*			for (const auto &s: layer.texts) {
				DEBUG("EXTRA TEXT =" << s.text.c_str());
			//	plot->newLegend(parseOriginText(QString::fromLocal8Bit(s.text.c_str())));
			}
	*/

			//curves
			int curveIndex = 1;
			for (const auto& curve: layer.curves) {
				if (curve.type == Origin::GraphCurve::Line || curve.type == Origin::GraphCurve::Scatter || curve.type == Origin::GraphCurve::LineSymbol
					|| curve.type == Origin::GraphCurve::ErrorBar || curve.type == Origin::GraphCurve::XErrorBar) {
					XYCurve* xyCurve = new XYCurve(i18n("Curve") + QString::number(curveIndex));
					xyCurve->suppressRetransform(true);
					loadCurve(curve, xyCurve);
					plot->addChild(xyCurve);
					xyCurve->suppressRetransform(false);
				} else if (curve.type == Origin::GraphCurve::Column) {
					//vertical bars

				} else if (curve.type == Origin::GraphCurve::Bar) {
					//horizontal bars

				} else if (curve.type == Origin::GraphCurve::Histogram) {

				}
				++curveIndex;
			}

			worksheet->addChild(plot);
		} else {
			//no support for 3D plots yet
			//TODO: add an "UnsupportedAspect" here
		}

		++index;
	}

	return true;
}

/*
 * sets the axis properties (format and ticks) as defined in \c originAxis in \c axis,
 * \c index being 0 or 1 for "top" and "bottom" or "left" and "right" for horizontal or vertical axes, respectively.
 */
void OriginProjectParser::loadAxis(const Origin::GraphAxis& originAxis, Axis* axis, int index) const {
	//ranges
	axis->setStart(originAxis.min);
	axis->setEnd(originAxis.max);

	//process Origin::GraphAxisFormat
	const Origin::GraphAxisFormat& axisFormat = originAxis.formatAxis[index];

	QPen pen;
	Origin::Color color;
	color.type = Origin::Color::ColorType::Regular;
	color.regular = axisFormat.color;
	pen.setColor(OriginProjectParser::color(color));
	pen.setWidthF(Worksheet::convertToSceneUnits(axisFormat.thickness, Worksheet::Point));
	axis->setLinePen(pen);

	axis->setMajorTicksLength( Worksheet::convertToSceneUnits(axisFormat.majorTickLength, Worksheet::Point) );
	axis->setMajorTicksDirection( (Axis::TicksFlags) axisFormat.majorTicksType);
	axis->setMajorTicksPen(pen);
	axis->setMinorTicksLength( axis->majorTicksLength()/2); // minorTicksLength is half of majorTicksLength
	axis->setMinorTicksDirection( (Axis::TicksFlags) axisFormat.minorTicksType);
	axis->setMinorTicksPen(pen);

// 	int axisPosition;
//		possible values:
//			0: Axis is at default position
//			1: Axis is at (axisPositionValue)% from standard position
//			2: Axis is at (axisPositionValue) position of ortogonal axis
// 		double axisPositionValue;


	QString titleText = parseOriginText(QString::fromLocal8Bit(axisFormat.label.text.c_str()));
	//TODO: parseOriginText() returns html formatted string. What is axisFormat.color used for?
	//TODO: use axisFormat.fontSize to override the global font size for the hmtl string?
	axis->title()->setRotationAngle(axisFormat.label.rotation);

	axis->setLabelsPrefix(axisFormat.prefix.c_str());
	axis->setLabelsSuffix(axisFormat.suffix.c_str());
	//TODO: handle string factor member in GraphAxisFormat

	//process Origin::GraphAxisTick
	const Origin::GraphAxisTick& tickAxis = originAxis.tickAxis[index];
	if (tickAxis.showMajorLabels) {
		color.type = Origin::Color::ColorType::Regular;
		color.regular = tickAxis.color;
		axis->setLabelsColor(OriginProjectParser::color(color));
		//TODO: how to set labels position (top vs. bottom)?
	} else {
		axis->setLabelsPosition(Axis::LabelsPosition::NoLabels);
	}

	//TODO: handle ValueType valueType member in GraphAxisTick
	//TODO: handle int valueTypeSpecification in GraphAxisTick
	axis->setLabelsPrecision(tickAxis.decimalPlaces);
	QFont font;
	//TODO: font family?
	font.setPixelSize( Worksheet::convertToSceneUnits(tickAxis.fontSize, Worksheet::Point) );
	font.setBold(tickAxis.fontBold);
	axis->setLabelsFont(font);
	//TODO: handle string dataName member in GraphAxisTick
	//TODO: handle string columnName member in GraphAxisTick
	axis->setLabelsRotationAngle(tickAxis.rotation);
}

void OriginProjectParser::loadCurve(const Origin::GraphCurve& originCurve, XYCurve* curve) const {
	//line properties
	QPen pen = curve->linePen();
	Qt::PenStyle penStyle(Qt::NoPen);
	if (originCurve.type == Origin::GraphCurve::Line || originCurve.type == Origin::GraphCurve::LineSymbol) {
		switch(originCurve.lineConnect) {
		case Origin::GraphCurve::NoLine:
			curve->setLineType(XYCurve::NoLine);
			break;
		case Origin::GraphCurve::Straight:
			curve->setLineType(XYCurve::Line);
			break;
		case Origin::GraphCurve::TwoPointSegment:
			curve->setLineType(XYCurve::Segments2);
			break;
		case Origin::GraphCurve::ThreePointSegment:
			curve->setLineType(XYCurve::Segments3);
			break;
		case Origin::GraphCurve::BSpline:
		case Origin::GraphCurve::Bezier:
		case Origin::GraphCurve::Spline:
			curve->setLineType(XYCurve::SplineCubicNatural);
			break;
		case Origin::GraphCurve::StepHorizontal:
			curve->setLineType(XYCurve::StartHorizontal);
			break;
		case Origin::GraphCurve::StepVertical:
			curve->setLineType(XYCurve::StartVertical);
			break;
		case Origin::GraphCurve::StepHCenter:
			curve->setLineType(XYCurve::MidpointHorizontal);
			break;
		case Origin::GraphCurve::StepVCenter:
			curve->setLineType(XYCurve::MidpointVertical);
			break;
		}

		switch (originCurve.lineStyle) {
		case Origin::GraphCurve::Solid:
			penStyle = Qt::SolidLine;
			break;
		case Origin::GraphCurve::Dash:
		case Origin::GraphCurve::ShortDash:
			penStyle = Qt::DashLine;
			break;
		case Origin::GraphCurve::Dot:
		case Origin::GraphCurve::ShortDot:
			penStyle = Qt::DotLine;
			break;
		case Origin::GraphCurve::DashDot:
		case Origin::GraphCurve::ShortDashDot:
			penStyle = Qt::DashDotLine;
			break;
		case Origin::GraphCurve::DashDotDot:
			penStyle = Qt::DashDotDotLine;
			break;
		}

		pen.setStyle(penStyle);
		pen.setWidthF( Worksheet::convertToSceneUnits(originCurve.lineWidth, Worksheet::Point) );
		pen.setColor(color(originCurve.lineColor));
		curve->setLineOpacity(originCurve.lineTransparency/255.);

		//TODO: handle unsigned char boxWidth of Origin::GraphCurve
	}
	pen.setStyle(penStyle);
	curve->setLinePen(pen);


	//symbol properties
	if (originCurve.type == Origin::GraphCurve::Scatter || originCurve.type == Origin::GraphCurve::LineSymbol) {
		//try to map the different symbols, mapping is not exact
		curve->setSymbolsRotationAngle(0);
		switch(originCurve.symbolType) {
		case 0: //NoSymbol
			curve->setSymbolsStyle(Symbol::NoSymbols);
			break;
		case 1: //Rect
			curve->setSymbolsStyle(Symbol::Square);
			break;
		case 2: //Ellipse
		case 20://Sphere
			curve->setSymbolsStyle(Symbol::Circle);
			break;
		case 3: //UTriangle
			curve->setSymbolsStyle(Symbol::EquilateralTriangle);
			break;
		case 4: //DTriangle
			curve->setSymbolsStyle(Symbol::EquilateralTriangle);
			break;
		case 5: //Diamond
			curve->setSymbolsStyle(Symbol::Diamond);
			break;
		case 6: //Cross +
			curve->setSymbolsStyle(Symbol::Cross);
			break;
		case 7: //Cross x
			curve->setSymbolsStyle(Symbol::Cross);
			break;
		case 8: //Snow
			curve->setSymbolsStyle(Symbol::Star4);
			break;
		case 9: //Horizontal -
			curve->setSymbolsStyle(Symbol::Line);
			curve->setSymbolsRotationAngle(90);
			break;
		case 10: //Vertical |
			curve->setSymbolsStyle(Symbol::Line);
			break;
		case 15: //LTriangle
			curve->setSymbolsStyle(Symbol::EquilateralTriangle);
			break;
		case 16: //RTriangle
			curve->setSymbolsStyle(Symbol::EquilateralTriangle);
			break;
		case 17: //Hexagon
		case 19: //Pentagon
			curve->setSymbolsStyle(Symbol::Square);
			break;
		case 18: //Star
			curve->setSymbolsStyle(Symbol::Star5);
			break;
		default:
			curve->setSymbolsStyle(Symbol::NoSymbols);
		}

		//Color symbolColor;
		//Color symbolFillColor;
		//TODO: doesn't work
		QBrush brush = curve->symbolsBrush();
		brush.setColor(color(originCurve.symbolColor));
		curve->setSymbolsBrush(brush);

		curve->setSymbolsSize(Worksheet::convertToSceneUnits(originCurve.symbolSize, Worksheet::Point));

		QPen pen = curve->symbolsPen();
		pen.setWidthF(Worksheet::convertToSceneUnits(originCurve.symbolThickness, Worksheet::Point));

		//handle unsigned char pointOffset member
		//handle bool connectSymbols member
	} else {
		curve->setSymbolsStyle(Symbol::NoSymbols);
	}

	//filling properties
	if(originCurve.fillArea) {
		//TODO: handle unsigned char fillAreaType;
		//with 'fillAreaType'=0x10 the area between the curve and the x-axis is filled
		//with 'fillAreaType'=0x14 the area included inside the curve is filled. First and last curve points are joined by a line to close the otherwise open area.
		//with 'fillAreaType'=0x12 the area excluded outside the curve is filled. The inverse of fillAreaType=0x14 is filled.
		//At the moment we only support the first type, so set it to XYCurve::FillingBelow
		curve->setFillingPosition(XYCurve::FillingBelow);

		if (originCurve.fillAreaPattern == 0) {
			curve->setFillingType(PlotArea::Color);
		} else {
			curve->setFillingType(PlotArea::Pattern);

			//map different patterns in originCurve.fillAreaPattern (has the values of Origin::FillPattern) to Qt::BrushStyle;
			switch(originCurve.fillAreaPattern) {
			case 0:
				curve->setFillingBrushStyle(Qt::NoBrush);
				break;
			case 1:
			case 2:
			case 3:
				curve->setFillingBrushStyle(Qt::BDiagPattern);
				break;
			case 4:
			case 5:
			case 6:
				curve->setFillingBrushStyle(Qt::FDiagPattern);
				break;
			case 7:
			case 8:
			case 9:
				curve->setFillingBrushStyle(Qt::DiagCrossPattern);
				break;
			case 10:
			case 11:
			case 12:
				curve->setFillingBrushStyle(Qt::HorPattern);
				break;
			case 13:
			case 14:
			case 15:
				curve->setFillingBrushStyle(Qt::VerPattern);
				break;
			case 16:
			case 17:
			case 18:
				curve->setFillingBrushStyle(Qt::CrossPattern);
				break;
			}
		}

		curve->setFillingFirstColor(color(originCurve.fillAreaColor));
		curve->setFillingOpacity(originCurve.fillAreaTransparency/255.);

		//Color fillAreaPatternColor - color for the pattern lines, not supported
		//double fillAreaPatternWidth - width of the pattern lines, not supported
		//bool fillAreaWithLineTransparency - transparency of the pattern lines indepetendent of the area transparency, not supported

		//TODO:
		//unsigned char fillAreaPatternBorderStyle;
		//Color fillAreaPatternBorderColor;
		//double fillAreaPatternBorderWidth;
		//The Border properties are used only in "Column/Bar" (histogram) plots. Those properties are:
		//fillAreaPatternBorderStyle   for the line style (use enum Origin::LineStyle here)
		//fillAreaPatternBorderColor   for the line color
		//fillAreaPatternBorderWidth   for the line width
	} else
		curve->setFillingPosition(XYCurve::NoFilling);
}

bool OriginProjectParser::loadNote(Note* note, bool preview) {
	DEBUG("OriginProjectParser::loadNote()");
	//load note data
	const Origin::Note& originNote = m_originFile->note(findNoteByName(note->name()));

	if (preview)
		return true;

	note->setComment(originNote.label.c_str());
	note->setNote(QString::fromLatin1(originNote.text.c_str()));

	return true;
}

//##############################################################################
//########################### Helper functions  ################################
//##############################################################################
QDateTime OriginProjectParser::creationTime(const tree<Origin::ProjectNode>::iterator& it) const {
	//this logic seems to be correct only for the first node (project node). For other nodes the current time is returned.
	char time_str[21];
	strftime(time_str, sizeof(time_str), "%F %T", gmtime(&(*it).creationDate));
	return QDateTime::fromString(QString(time_str), Qt::ISODate);
}

QString OriginProjectParser::parseOriginText(const QString &str) const {
	QStringList lines = str.split("\n");
	QString text = "";
	for (int i = 0; i < lines.size(); ++i) {
		if (i > 0)
			text.append("\n");
		text.append(parseOriginTags(lines[i]));
	}

	return text;
}

QColor OriginProjectParser::color(const Origin::Color& color) const {
	switch (color.type) {
		case Origin::Color::ColorType::Regular:
			switch (color.regular) {
				case Origin::Color::Black:
					return QColor(Qt::black);
				case Origin::Color::Red:
					return QColor(Qt::red);
				case Origin::Color::Green:
					return QColor(Qt::green);
				case Origin::Color::Blue:
					return QColor(Qt::blue);
				case Origin::Color::Cyan:
					return QColor(Qt::cyan);
				case Origin::Color::Magenta:
					return QColor(Qt::magenta);
				case Origin::Color::Yellow:
					return QColor(Qt::yellow);
				case Origin::Color::DarkYellow:
					return QColor(Qt::darkYellow);
				case Origin::Color::Navy:
					return QColor(0, 0, 128);
				case Origin::Color::Purple:
					return QColor(128, 0, 128);
				case Origin::Color::Wine:
					return QColor(128, 0, 0);
				case Origin::Color::Olive:
					return QColor(0, 128, 0);
				case Origin::Color::DarkCyan:
					return QColor(Qt::darkCyan);
				case Origin::Color::Royal:
					return QColor(0, 0, 160);
				case Origin::Color::Orange:
					return QColor(255, 128, 0);
				case Origin::Color::Violet:
					return QColor(128, 0, 255);
				case Origin::Color::Pink:
					return QColor(255, 0, 128);
				case Origin::Color::White:
					return QColor(Qt::white);
				case Origin::Color::LightGray:
					return QColor(Qt::lightGray);
				case Origin::Color::Gray:
					return QColor(Qt::gray);
				case Origin::Color::LTYellow:
					return QColor(255, 0, 128);
				case Origin::Color::LTCyan:
					return QColor(128, 255, 255);
				case Origin::Color::LTMagenta:
					return QColor(255, 128, 255);
				case Origin::Color::DarkGray:
					return QColor(Qt::darkGray);
			}
			break;
		case Origin::Color::ColorType::Custom:
			return QColor(color.custom[0], color.custom[1], color.custom[2]);
		case Origin::Color::ColorType::None:
		case Origin::Color::ColorType::Automatic:
		case Origin::Color::ColorType::Increment:
		case Origin::Color::ColorType::Indexing:
		case Origin::Color::ColorType::RGB:
		case Origin::Color::ColorType::Mapping:
			break;
	}

	return QColor(Qt::white);
}

PlotArea::BackgroundColorStyle OriginProjectParser::backgroundColorStyle(const Origin::ColorGradientDirection& colorGradient) const {
	switch (colorGradient) {
		case Origin::ColorGradientDirection::NoGradient:
			return PlotArea::BackgroundColorStyle::SingleColor;
		case Origin::ColorGradientDirection::TopLeft:
			return PlotArea::BackgroundColorStyle::TopLeftDiagonalLinearGradient;
		case Origin::ColorGradientDirection::Left:
			return PlotArea::BackgroundColorStyle::HorizontalLinearGradient;
		case Origin::ColorGradientDirection::BottomLeft:
			return PlotArea::BackgroundColorStyle::BottomLeftDiagonalLinearGradient;
		case Origin::ColorGradientDirection::Top:
			return PlotArea::BackgroundColorStyle::VerticalLinearGradient;
		case Origin::ColorGradientDirection::Center:
			return PlotArea::BackgroundColorStyle::RadialGradient;
		case Origin::ColorGradientDirection::Bottom:
			return PlotArea::BackgroundColorStyle::VerticalLinearGradient;
		case Origin::ColorGradientDirection::TopRight:
			return PlotArea::BackgroundColorStyle::BottomLeftDiagonalLinearGradient;
		case Origin::ColorGradientDirection::Right:
			return PlotArea::BackgroundColorStyle::HorizontalLinearGradient;
		case Origin::ColorGradientDirection::BottomRight:
			return PlotArea::BackgroundColorStyle::TopLeftDiagonalLinearGradient;
	}

	return PlotArea::BackgroundColorStyle::SingleColor;
}

QString strreverse(const QString &str) {	//QString reversing
	QByteArray ba = str.toLocal8Bit();
	std::reverse(ba.begin(), ba.end());

	return QString(ba);
}

// taken from SciDAVis
QString OriginProjectParser::parseOriginTags(const QString &str) const {
	DEBUG("parseOriginTags()");
	DEBUG("	string: " << str.toStdString());
	QString line = str;

	//replace \l(...) and %(...) tags
	QRegExp rxline("\\\\\\s*l\\s*\\(\\s*\\d+\\s*\\)");
	QRegExp rxcol("\\%\\(\\d+\\)");
	int pos = rxline.indexIn(line);
	while (pos > -1) {
		QString value = rxline.cap(0);
		int len = value.length();
		value.replace(QRegExp(" "),"");
		value = "\\c{" + value.mid(3, value.length()-4) + "}";
		line.replace(pos, len, value);
		pos = rxline.indexIn(line);
	}
	//Lookbehind conditions are not supported - so need to reverse string
	QRegExp rx("\\)[^\\)\\(]*\\((?!\\s*[buig\\+\\-]\\s*\\\\)");
	QRegExp rxfont("\\)[^\\)\\(]*\\((?![^\\:]*\\:f\\s*\\\\)");
	QString linerev = strreverse(line);
	QString lBracket = strreverse("&lbracket;");
	QString rBracket = strreverse("&rbracket;");
	QString ltagBracket = strreverse("&ltagbracket;");
	QString rtagBracket = strreverse("&rtagbracket;");
	int pos1 = rx.indexIn(linerev);
	int pos2 = rxfont.indexIn(linerev);

	while (pos1 > -1 || pos2 > -1) {
		if (pos1 == pos2) {
			QString value = rx.cap(0);
			int len = value.length();
			value = rBracket + value.mid(1, len-2) + lBracket;
			linerev.replace(pos1, len, value);
		}
		else if ((pos1 > pos2 && pos2 != -1) || pos1 == -1) {
			QString value = rxfont.cap(0);
			int len = value.length();
			value = rtagBracket + value.mid(1, len-2) + ltagBracket;
			linerev.replace(pos2, len, value);
		}
		else if ((pos2 > pos1 && pos1 != -1) || pos2 == -1) {
			QString value = rx.cap(0);
			int len = value.length();
			value = rtagBracket + value.mid(1, len-2) + ltagBracket;
			linerev.replace(pos1, len, value);
		}

		pos1 = rx.indexIn(linerev);
		pos2 = rxfont.indexIn(linerev);
	}
	linerev.replace(ltagBracket, "(");
	linerev.replace(rtagBracket, ")");

	line = strreverse(linerev);


	//replace \b(...), \i(...), \u(...), \g(...), \+(...), \-(...), \f:font(...) tags
	const QString rxstr[] = { "\\\\\\s*b\\s*\\(", "\\\\\\s*i\\s*\\(", "\\\\\\s*u\\s*\\(", "\\\\\\s*g\\s*\\(", "\\\\\\s*\\+\\s*\\(", "\\\\\\s*\\-\\s*\\(", "\\\\\\s*f\\:[^\\(]*\\("};

	int postag[] = {0, 0, 0, 0, 0, 0, 0};
	QString ltag[] = {"<b>","<i>","<u>","<font face=Symbol>","<sup>","<sub>","<font face=%1>"};
	QString rtag[] = {"</b>","</i>","</u>","</font>","</sup>","</sub>","</font>"};
	QRegExp rxtags[7];
	for (int i = 0; i < 7; ++i)
		rxtags[i].setPattern(rxstr[i]+"[^\\(\\)]*\\)");

	bool flag = true;
	while (flag) {
		for (int i = 0; i < 7; ++i) {
			postag[i] = rxtags[i].indexIn(line);
			while (postag[i] > -1) {
				QString value = rxtags[i].cap(0);
				int len = value.length();
				pos2 = value.indexOf("(");
				if (i < 6)
					value = ltag[i] + value.mid(pos2+1, len-pos2-2) + rtag[i];
				else {
					int posfont = value.indexOf("f:");
					value = ltag[i].arg(value.mid(posfont+2, pos2-posfont-2)) + value.mid(pos2+1, len-pos2-2) + rtag[i];
				}
				line.replace(postag[i], len, value);
				postag[i] = rxtags[i].indexIn(line);
			}
		}
		flag = false;
		for (int i = 0; i < 7; ++i) {
			if (rxtags[i].indexIn(line) > -1) {
				flag = true;
				break;
			}
		}
	}

	//replace unclosed tags
	for (int i = 0; i < 6; ++i)
		line.replace(QRegExp(rxstr[i]), ltag[i]);
	rxfont.setPattern(rxstr[6]);
	pos = rxfont.indexIn(line);
	while (pos > -1) {
		QString value = rxfont.cap(0);
		int len = value.length();
		int posfont = value.indexOf("f:");
		value = ltag[6].arg(value.mid(posfont+2, len-posfont-3));
		line.replace(pos, len, value);
		pos = rxfont.indexIn(line);
	}

	line.replace("&lbracket;", "(");
	line.replace("&rbracket;", ")");

	DEBUG("	result: " << line.toStdString());

	return line;
}
