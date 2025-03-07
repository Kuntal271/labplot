/*
	File                 : OriginProjectParser.h
	Project              : LabPlot
	Description          : parser for Origin projects
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2017-2018 Alexander Semke <alexander.semke@web.de>
	SPDX-FileCopyrightText: 2017-2023 Stefan Gerlach <stefan.gerlach@uni.kn>

	SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "backend/datasources/projects/OriginProjectParser.h"
#include "backend/core/Project.h"
#include "backend/core/Workbook.h"
#include "backend/core/column/Column.h"
#include "backend/core/datatypes/DateTime2StringFilter.h"
#include "backend/core/datatypes/Double2StringFilter.h"
#include "backend/matrix/Matrix.h"
#include "backend/note/Note.h"
#include "backend/spreadsheet/Spreadsheet.h"
#include "backend/worksheet/Line.h"
#include "backend/worksheet/TextLabel.h"
#include "backend/worksheet/Worksheet.h"
#include "backend/worksheet/WorksheetElement.h"
#include "backend/worksheet/plots/PlotArea.h"
#include "backend/worksheet/plots/cartesian/Axis.h"
#include "backend/worksheet/plots/cartesian/CartesianPlot.h"
#include "backend/worksheet/plots/cartesian/CartesianPlotLegend.h"
#include "backend/worksheet/plots/cartesian/Symbol.h"
#include "backend/worksheet/plots/cartesian/XYCurve.h"
#include "backend/worksheet/plots/cartesian/XYEquationCurve.h"

#include <KLocalizedString>

#include <QDateTime>
#include <QDir>
#include <QFontMetrics>
#include <QGraphicsScene>
#include <QRegularExpression>

/*!
\class OriginProjectParser
\brief parser for Origin projects.

\ingroup datasources
*/

OriginProjectParser::OriginProjectParser()
	: ProjectParser() {
	m_topLevelClasses = {AspectType::Folder, AspectType::Workbook, AspectType::Spreadsheet, AspectType::Matrix, AspectType::Worksheet, AspectType::Note};
}

bool OriginProjectParser::isOriginProject(const QString& fileName) {
	// TODO add opju later when liborigin supports it
	return fileName.endsWith(QLatin1String(".opj"), Qt::CaseInsensitive);
}

void OriginProjectParser::setImportUnusedObjects(bool importUnusedObjects) {
	m_importUnusedObjects = importUnusedObjects;
}

void OriginProjectParser::checkContent(bool& hasUnusedObjects, bool& hasMultiLayerGraphs) {
	m_originFile = new OriginFile(qPrintable(m_projectFileName));
	if (!m_originFile->parse()) {
		delete m_originFile;
		m_originFile = nullptr;
		hasUnusedObjects = false;
		hasMultiLayerGraphs = false;
		return;
	}

	DEBUG(Q_FUNC_INFO << "Project file name: " << m_projectFileName.toStdString());
	DEBUG(Q_FUNC_INFO << "Origin version: " << m_originFile->version());

	hasUnusedObjects = this->hasUnusedObjects();
	hasMultiLayerGraphs = this->hasMultiLayerGraphs();

	delete m_originFile;
	m_originFile = nullptr;
}

bool OriginProjectParser::hasUnusedObjects() {
	if (!m_originFile)
		return false;

	for (unsigned int i = 0; i < m_originFile->spreadCount(); i++) {
		const auto& spread = m_originFile->spread(i);
		if (spread.objectID < 0)
			return true;
	}
	for (unsigned int i = 0; i < m_originFile->excelCount(); i++) {
		const auto& excel = m_originFile->excel(i);
		if (excel.objectID < 0)
			return true;
	}
	for (unsigned int i = 0; i < m_originFile->matrixCount(); i++) {
		const auto& matrix = m_originFile->matrix(i);
		if (matrix.objectID < 0)
			return true;
	}

	return false;
}

bool OriginProjectParser::hasMultiLayerGraphs() {
	if (!m_originFile)
		return false;

	for (unsigned int i = 0; i < m_originFile->graphCount(); i++) {
		const auto& graph = m_originFile->graph(i);
		if (graph.layers.size() > 1)
			return true;
	}

	return false;
}

void OriginProjectParser::setGraphLayerAsPlotArea(bool value) {
	m_graphLayerAsPlotArea = value;
}

QString OriginProjectParser::supportedExtensions() {
	// TODO add opju later when liborigin supports it
	static const QString extensions = QStringLiteral("*.opj *.OPJ");
	return extensions;
}

// sets first found spread of given name
unsigned int OriginProjectParser::findSpreadsheetByName(const QString& name) {
	for (unsigned int i = 0; i < m_originFile->spreadCount(); i++) {
		const Origin::SpreadSheet& spread = m_originFile->spread(i);
		if (spread.name == name.toStdString()) {
			m_spreadsheetNameList << name;
			m_spreadsheetNameList.removeDuplicates();
			return i;
		}
	}
	return 0;
}
unsigned int OriginProjectParser::findMatrixByName(const QString& name) {
	for (unsigned int i = 0; i < m_originFile->matrixCount(); i++) {
		const Origin::Matrix& originMatrix = m_originFile->matrix(i);
		if (originMatrix.name == name.toStdString()) {
			m_matrixNameList << name;
			m_matrixNameList.removeDuplicates();
			return i;
		}
	}
	return 0;
}
unsigned int OriginProjectParser::findWorkbookByName(const QString& name) {
	// QDEBUG("WORKBOOK LIST: " << m_workbookNameList << ", name = " << name)
	for (unsigned int i = 0; i < m_originFile->excelCount(); i++) {
		const Origin::Excel& excel = m_originFile->excel(i);
		if (excel.name == name.toStdString()) {
			m_workbookNameList << name;
			m_workbookNameList.removeDuplicates();
			return i;
		}
	}
	return 0;
}
unsigned int OriginProjectParser::findWorksheetByName(const QString& name) {
	for (unsigned int i = 0; i < m_originFile->graphCount(); i++) {
		const Origin::Graph& graph = m_originFile->graph(i);
		if (graph.name == name.toStdString()) {
			m_worksheetNameList << name;
			m_worksheetNameList.removeDuplicates();
			return i;
		}
	}
	return 0;
}
unsigned int OriginProjectParser::findNoteByName(const QString& name) {
	for (unsigned int i = 0; i < m_originFile->noteCount(); i++) {
		const Origin::Note& originNote = m_originFile->note(i);
		if (originNote.name == name.toStdString()) {
			m_noteNameList << name;
			m_noteNameList.removeDuplicates();
			return i;
		}
	}
	return 0;
}

// ##############################################################################
// ############## Deserialization from Origin's project tree ####################
// ##############################################################################
bool OriginProjectParser::load(Project* project, bool preview) {
	DEBUG(Q_FUNC_INFO);

	// read and parse the m_originFile-file
	m_originFile = new OriginFile(qPrintable(m_projectFileName));
	if (!m_originFile->parse()) {
		delete m_originFile;
		m_originFile = nullptr;
		return false;
	}

	DEBUG(Q_FUNC_INFO << "Project file name: " << m_projectFileName.toStdString());
	DEBUG(Q_FUNC_INFO << "Origin version: " << m_originFile->version());

	// Origin project tree and the iterator pointing to the root node
	const auto* projectTree = m_originFile->project();
	auto projectIt = projectTree->begin(projectTree->begin());

	m_spreadsheetNameList.clear();
	m_workbookNameList.clear();
	m_matrixNameList.clear();
	m_worksheetNameList.clear();
	m_noteNameList.clear();

	// convert the project tree from liborigin's representation to LabPlot's project object
	project->setIsLoading(true);
	if (projectIt.node) { // only opj files from version >= 6.0 do have project tree
		DEBUG(Q_FUNC_INFO << ", project tree found");
		QString name(QString::fromLatin1(projectIt->name.c_str()));
		project->setName(name);
		project->setCreationTime(creationTime(projectIt));
		loadFolder(project, projectIt, preview);
	} else { // for older versions put all windows on rootfolder
		DEBUG(Q_FUNC_INFO << ", no project tree");
		int pos = m_projectFileName.lastIndexOf(QLatin1Char('/')) + 1;
		project->setName(m_projectFileName.mid(pos));
	}
	// imports all loose windows (like prior version 6 which has no project tree)
	handleLooseWindows(project, preview);

	// restore column pointers:
	// 1. extend the pathes to contain the parent structures first
	// 2. restore the pointers from the pathes
	const auto& columns = project->children<Column>(AbstractAspect::ChildIndexFlag::Recursive);
	const auto& spreadsheets = project->children<Spreadsheet>(AbstractAspect::ChildIndexFlag::Recursive);
	const auto& curves = project->children<XYCurve>(AbstractAspect::ChildIndexFlag::Recursive);
	DEBUG(Q_FUNC_INFO << ", NUMBER of spreadsheets/columns = "
					  << "/" << spreadsheets.count() << "/" << columns.count())
	for (auto* curve : curves) {
		DEBUG(Q_FUNC_INFO << ", RESTORE CURVE with x/y column path " << STDSTRING(curve->xColumnPath()) << " " << STDSTRING(curve->yColumnPath()))
		curve->setSuppressRetransform(true);

		// x-column
		QString spreadsheetName = curve->xColumnPath();
		spreadsheetName.truncate(curve->xColumnPath().lastIndexOf(QLatin1Char('/')));
		// DEBUG(Q_FUNC_INFO << ", SPREADSHEET name from column: " << STDSTRING(spreadsheetName))
		for (const auto* spreadsheet : spreadsheets) {
			QString container, containerPath = spreadsheet->parentAspect()->path();
			if (spreadsheetName.contains(QLatin1Char('/'))) { // part of a workbook
				container = containerPath.mid(containerPath.lastIndexOf(QLatin1Char('/')) + 1) + QLatin1Char('/');
				containerPath = containerPath.left(containerPath.lastIndexOf(QLatin1Char('/')));
			}
			// DEBUG("CONTAINER = " << STDSTRING(container))
			// DEBUG("CONTAINER PATH = " << STDSTRING(containerPath))
			// DEBUG(Q_FUNC_INFO << ", LOOP spreadsheet names = \"" << STDSTRING(container) +
			//	STDSTRING(spreadsheet->name()) << "\", path = " << STDSTRING(spreadsheetName))
			// DEBUG("SPREADSHEET parent path = " << STDSTRING(spreadsheet->parentAspect()->path()))
			if (container + spreadsheet->name() == spreadsheetName) {
				const QString& newPath = containerPath + QLatin1Char('/') + curve->xColumnPath();
				// const QString& newPath = QLatin1String("Project") + QLatin1Char('/') + curve->xColumnPath();
				DEBUG(Q_FUNC_INFO << ", SET COLUMN PATH to \"" << STDSTRING(newPath) << "\"")
				curve->setXColumnPath(newPath);

				for (auto* column : columns) {
					if (!column)
						continue;
					if (column->path() == newPath) {
						// DEBUG(Q_FUNC_INFO << ", set X column path = \"" << STDSTRING(column->path()) << "\"")
						curve->setXColumn(column);
						break;
					}
				}
				break;
			}
		}

		// y-column
		spreadsheetName = curve->yColumnPath();
		spreadsheetName.truncate(curve->yColumnPath().lastIndexOf(QLatin1Char('/')));
		for (const auto* spreadsheet : spreadsheets) {
			QString container, containerPath = spreadsheet->parentAspect()->path();
			if (spreadsheetName.contains(QLatin1Char('/'))) { // part of a workbook
				container = containerPath.mid(containerPath.lastIndexOf(QLatin1Char('/')) + 1) + QLatin1Char('/');
				containerPath = containerPath.left(containerPath.lastIndexOf(QLatin1Char('/')));
			}
			if (container + spreadsheet->name() == spreadsheetName) {
				const QString& newPath = containerPath + QLatin1Char('/') + curve->yColumnPath();
				curve->setYColumnPath(newPath);

				for (auto* column : columns) {
					if (!column)
						continue;
					// DEBUG(Q_FUNC_INFO << ", column paths = \"" << STDSTRING(column->path())
					//	<< "\" / \"" << STDSTRING(newPath) << "\"" )
					if (column->path() == newPath) {
						curve->setYColumn(column);
						break;
					}
				}
				break;
			}
		}
		DEBUG(Q_FUNC_INFO << ", curve x/y COLUMNS = " << curve->xColumn() << "/" << curve->yColumn())

		// TODO: error columns

		curve->setSuppressRetransform(false);
	}

	if (!preview) {
		const auto& plots = project->children<CartesianPlot>(AbstractAspect::ChildIndexFlag::Recursive);
		for (auto* plot : plots) {
			plot->setIsLoading(false);
			plot->retransform();
		}
	}

	project->setIsLoading(false);

	delete m_originFile;
	m_originFile = nullptr;

	return true;
}

bool OriginProjectParser::loadFolder(Folder* folder, tree<Origin::ProjectNode>::iterator baseIt, bool preview) {
	DEBUG(Q_FUNC_INFO)
	const tree<Origin::ProjectNode>* projectTree = m_originFile->project();

	// do not skip anything if pathesToLoad() contains only root folder
	bool containsRootFolder = (folder->pathesToLoad().size() == 1 && folder->pathesToLoad().contains(folder->path()));
	if (containsRootFolder) {
		DEBUG("	pathesToLoad contains only folder path \"" << STDSTRING(folder->path()) << "\". Clearing pathes to load.")
		folder->setPathesToLoad(QStringList());
	}

	// load folder's children: logic for reading the selected objects only is similar to Folder::readChildAspectElement
	for (auto it = projectTree->begin(baseIt); it != projectTree->end(baseIt); ++it) {
		QString name(QString::fromLatin1(it->name.c_str())); // name of the current child
		DEBUG("	* folder item name = " << STDSTRING(name))

		// check whether we need to skip the loading of the current child
		if (!folder->pathesToLoad().isEmpty()) {
			// child's path is not available yet (child not added yet) -> construct the path manually
			const QString childPath = folder->path() + QLatin1Char('/') + name;
			DEBUG("		path = " << STDSTRING(childPath))

			// skip the current child aspect it is not in the list of aspects to be loaded
			if (folder->pathesToLoad().indexOf(childPath) == -1) {
				DEBUG("		skip it!")
				continue;
			}
		}

		// load top-level children.
		// use 'preview' as 'loading'-parameter in the constructors to skip the init() calls in Worksheet, Spreadsheet and Matrix:
		//* when doing the preview of the project we don't want to initialize the objects and skip init()'s
		//* when loading the project, 'preview' is false and we initialize all objects with our default values
		//   and set all possible properties from Origin additionally
		AbstractAspect* aspect = nullptr;
		switch (it->type) {
		case Origin::ProjectNode::Folder: {
			DEBUG(Q_FUNC_INFO << ", top level FOLDER");
			Folder* f = new Folder(name);

			if (!folder->pathesToLoad().isEmpty()) {
				// a child folder to be read -> provide the list of aspects to be loaded to the child folder, too.
				// since the child folder and all its children are not added yet (path() returns empty string),
				// we need to remove the path of the current child folder from the full pathes provided in pathesToLoad.
				// E.g. we want to import the path "Project/Folder/Spreadsheet" in the following project
				//  Project
				//         \Spreadsheet
				//         \Folder
				//                \Spreadsheet
				//
				// Here, we remove the part "Project/Folder/" and proceed for this child folder with "Spreadsheet" only.
				// With this the logic above where it is determined whether to import the child aspect or not works out.

				// manually construct the path of the child folder to be read
				const QString& curFolderPath = folder->path() + QLatin1Char('/') + name;

				// remove the path of the current child folder
				QStringList pathesToLoadNew;
				for (const auto& path : folder->pathesToLoad()) {
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
			DEBUG(Q_FUNC_INFO << ", top level SPREADSHEET");
			auto* spreadsheet = new Spreadsheet(name, preview);
			loadSpreadsheet(spreadsheet, preview, name);
			aspect = spreadsheet;
			break;
		}
		case Origin::ProjectNode::Graph: {
			DEBUG(Q_FUNC_INFO << ", top level GRAPH");
			auto* worksheet = new Worksheet(name, preview);
			if (!preview) {
				worksheet->setIsLoading(true);
				worksheet->setTheme(QString());
			}
			loadWorksheet(worksheet, preview);
			aspect = worksheet;
			break;
		}
		case Origin::ProjectNode::Matrix: {
			DEBUG(Q_FUNC_INFO << ", top level MATRIX");
			const Origin::Matrix& originMatrix = m_originFile->matrix(findMatrixByName(name));
			DEBUG("	matrix name = " << originMatrix.name);
			DEBUG("	number of sheets = " << originMatrix.sheets.size());
			if (originMatrix.sheets.size() == 1) {
				// single sheet -> load into a matrix
				Matrix* matrix = new Matrix(name, preview);
				loadMatrix(matrix, preview);
				aspect = matrix;
			} else {
				// multiple sheets -> load into a workbook
				Workbook* workbook = new Workbook(name);
				loadMatrixWorkbook(workbook, preview);
				aspect = workbook;
			}
			break;
		}
		case Origin::ProjectNode::Excel: {
			DEBUG(Q_FUNC_INFO << ", top level WORKBOOK");
			auto* workbook = new Workbook(name);
			loadWorkbook(workbook, preview);
			aspect = workbook;
			break;
		}
		case Origin::ProjectNode::Note: {
			DEBUG(Q_FUNC_INFO << ", top level NOTE");
			Note* note = new Note(name);
			loadNote(note, preview);
			aspect = note;
			break;
		}
		case Origin::ProjectNode::Graph3D:
		default:
			// TODO: add UnsupportedAspect
			break;
		}

		if (aspect) {
			folder->addChildFast(aspect);
			aspect->setCreationTime(creationTime(it));
			aspect->setIsLoading(false);
		}
	}

	// ResultsLog
	QString resultsLog = QString::fromLatin1(m_originFile->resultsLogString().c_str());
	if (resultsLog.length() > 0) {
		DEBUG("Results log:\t\tyes");
		Note* note = new Note(QStringLiteral("ResultsLog"));

		if (preview)
			folder->addChildFast(note);
		else {
			// only import the log if it is in the list of aspects to be loaded
			const QString childPath = folder->path() + QLatin1Char('/') + note->name();
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
	QDEBUG(Q_FUNC_INFO << ", paths to load:" << folder->pathesToLoad());
	QDEBUG("	spreads =" << m_spreadsheetNameList);
	QDEBUG("	workbooks =" << m_workbookNameList);
	QDEBUG("	matrices =" << m_matrixNameList);
	QDEBUG("	worksheets =" << m_worksheetNameList);
	QDEBUG("	notes =" << m_noteNameList);

	DEBUG("Number of spreads loaded:\t" << m_spreadsheetNameList.size() << ", in file: " << m_originFile->spreadCount());
	DEBUG("Number of excels loaded:\t" << m_workbookNameList.size() << ", in file: " << m_originFile->excelCount());
	DEBUG("Number of matrices loaded:\t" << m_matrixNameList.size() << ", in file: " << m_originFile->matrixCount());
	DEBUG("Number of graphs loaded:\t" << m_worksheetNameList.size() << ", in file: " << m_originFile->graphCount());
	DEBUG("Number of notes loaded:\t\t" << m_noteNameList.size() << ", in file: " << m_originFile->noteCount());

	// loop over all spreads to find loose ones
	for (unsigned int i = 0; i < m_originFile->spreadCount(); i++) {
		AbstractAspect* aspect = nullptr;
		const Origin::SpreadSheet& spread = m_originFile->spread(i);
		QString name = QString::fromStdString(spread.name);

		DEBUG("	spread.objectId = " << spread.objectID);
		// skip unused spreads if selected
		if (spread.objectID < 0 && !m_importUnusedObjects) {
			DEBUG("	Dropping unused loose spread: " << STDSTRING(name));
			continue;
		}

		const QString childPath = folder->path() + QLatin1Char('/') + name;
		// we could also use spread.loose
		if (!m_spreadsheetNameList.contains(name) && (preview || folder->pathesToLoad().indexOf(childPath) != -1)) {
			DEBUG("	Adding loose spread: " << STDSTRING(name));

			auto* spreadsheet = new Spreadsheet(name);
			loadSpreadsheet(spreadsheet, preview, name);
			aspect = spreadsheet;
		}
		if (aspect) {
			folder->addChildFast(aspect);
			DEBUG("	creation time as reported by liborigin: " << spread.creationDate);
			aspect->setCreationTime(QDateTime::fromSecsSinceEpoch(spread.creationDate));
		}
	}
	// loop over all workbooks to find loose ones
	for (unsigned int i = 0; i < m_originFile->excelCount(); i++) {
		AbstractAspect* aspect = nullptr;
		const Origin::Excel& excel = m_originFile->excel(i);
		QString name = QString::fromStdString(excel.name);

		DEBUG("	excel.objectId = " << excel.objectID);
		// skip unused data sets if selected
		if (excel.objectID < 0 && !m_importUnusedObjects) {
			DEBUG("	Dropping unused loose excel: " << STDSTRING(name));
			continue;
		}

		const QString childPath = folder->path() + QLatin1Char('/') + name;
		// we could also use excel.loose
		if (!m_workbookNameList.contains(name) && (preview || folder->pathesToLoad().indexOf(childPath) != -1)) {
			DEBUG("	Adding loose excel: " << STDSTRING(name));
			DEBUG("	 containing number of sheets = " << excel.sheets.size());

			auto* workbook = new Workbook(name);
			loadWorkbook(workbook, preview);
			aspect = workbook;
		}
		if (aspect) {
			folder->addChildFast(aspect);
			DEBUG("	creation time as reported by liborigin: " << excel.creationDate);
			aspect->setCreationTime(QDateTime::fromSecsSinceEpoch(excel.creationDate));
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
			DEBUG("	Dropping unused loose matrix: " << STDSTRING(name));
			continue;
		}

		const QString childPath = folder->path() + QLatin1Char('/') + name;
		if (!m_matrixNameList.contains(name) && (preview || folder->pathesToLoad().indexOf(childPath) != -1)) {
			DEBUG("	Adding loose matrix: " << STDSTRING(name));
			DEBUG("	containing number of sheets = " << originMatrix.sheets.size());
			if (originMatrix.sheets.size() == 1) { // single sheet -> load into a matrix
				Matrix* matrix = new Matrix(name);
				loadMatrix(matrix, preview);
				aspect = matrix;
			} else { // multiple sheets -> load into a workbook
				auto* workbook = new Workbook(name);
				loadMatrixWorkbook(workbook, preview);
				aspect = workbook;
			}
		}
		if (aspect) {
			folder->addChildFast(aspect);
			aspect->setCreationTime(QDateTime::fromSecsSinceEpoch(originMatrix.creationDate));
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
			DEBUG("	Dropping unused loose graph: " << STDSTRING(name));
			continue;
		}

		const QString childPath = folder->path() + QLatin1Char('/') + name;
		if (!m_worksheetNameList.contains(name) && (preview || folder->pathesToLoad().indexOf(childPath) != -1)) {
			DEBUG("	Adding loose graph: " << STDSTRING(name));
			auto* worksheet = new Worksheet(name);
			loadWorksheet(worksheet, preview);
			aspect = worksheet;
		}
		if (aspect) {
			folder->addChildFast(aspect);
			aspect->setCreationTime(QDateTime::fromSecsSinceEpoch(graph.creationDate));
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
			DEBUG("	Dropping unused loose note: " << STDSTRING(name));
			continue;
		}

		const QString childPath = folder->path() + QLatin1Char('/') + name;
		if (!m_noteNameList.contains(name) && (preview || folder->pathesToLoad().indexOf(childPath) != -1)) {
			DEBUG("	Adding loose note: " << STDSTRING(name));
			Note* note = new Note(name);
			loadNote(note, preview);
			aspect = note;
		}
		if (aspect) {
			folder->addChildFast(aspect);
			aspect->setCreationTime(QDateTime::fromSecsSinceEpoch(originNote.creationDate));
		}
	}
}

bool OriginProjectParser::loadWorkbook(Workbook* workbook, bool preview) {
	DEBUG(Q_FUNC_INFO);
	// load workbook sheets
	const Origin::Excel& excel = m_originFile->excel(findWorkbookByName(workbook->name()));
	DEBUG(Q_FUNC_INFO << ", workbook name = " << excel.name);
	DEBUG(Q_FUNC_INFO << ", number of sheets = " << excel.sheets.size());
	for (unsigned int s = 0; s < excel.sheets.size(); ++s) {
		// DEBUG(Q_FUNC_INFO << ", LOADING SHEET " << excel.sheets[s].name.c_str())
		auto* spreadsheet = new Spreadsheet(QString::fromLatin1(excel.sheets[s].name.c_str()));
		loadSpreadsheet(spreadsheet, preview, workbook->name(), s);
		workbook->addChildFast(spreadsheet);
	}

	return true;
}

// load spreadsheet from spread (sheetIndex == -1) or from workbook (only sheet sheetIndex)
// name is the spreadsheet name (spread) or the workbook name (if inside a workbook)
bool OriginProjectParser::loadSpreadsheet(Spreadsheet* spreadsheet, bool preview, const QString& name, int sheetIndex) {
	DEBUG(Q_FUNC_INFO << ", own/workbook name = " << STDSTRING(name) << ", sheetIndex = " << sheetIndex);

	// load spreadsheet data
	Origin::SpreadSheet spread;
	Origin::Excel excel;
	if (sheetIndex == -1) // spread
		spread = m_originFile->spread(findSpreadsheetByName(name));
	else {
		excel = m_originFile->excel(findWorkbookByName(name));
		spread = excel.sheets.at(sheetIndex);
	}

	const size_t cols = spread.columns.size();
	int rows = 0;
	for (size_t j = 0; j < cols; ++j)
		rows = std::max((int)spread.columns.at(j).data.size(), rows);
	// alternative: int rows = excel.maxRows;
	DEBUG(Q_FUNC_INFO << ", cols/maxRows = " << cols << "/" << rows);

	// TODO QLocale locale = mw->locale();

	spreadsheet->setRowCount(rows);
	spreadsheet->setColumnCount((int)cols);
	if (sheetIndex == -1)
		spreadsheet->setComment(QString::fromLatin1(spread.label.c_str()));
	else // TODO: only first spread should get the comments
		spreadsheet->setComment(QString::fromLatin1(excel.label.c_str()));

	// in Origin column width is measured in characters, we need to convert to pixels
	// TODO: determine the font used in Origin in order to get the same column width as in Origin
	QFont font;
	QFontMetrics fm(font);
	const int scaling_factor = fm.maxWidth();

	for (size_t j = 0; j < cols; ++j) {
		Origin::SpreadColumn column = spread.columns[j];
		Column* col = spreadsheet->column((int)j);

		DEBUG(Q_FUNC_INFO << ", column " << j << ", name = " << column.name.c_str())
		QString name(QLatin1String(column.name.c_str()));
		col->setName(name.replace(QRegularExpression(QStringLiteral(".*_")), QString()));

		if (preview)
			continue;

		// TODO: we don't support any formulas for cells yet.
		// 		if (column.command.size() > 0)
		// 			col->setFormula(Interval<int>(0, rows), QString(column.command.c_str()));

		col->setComment(QString::fromLatin1(column.comment.c_str()));
		col->setWidth((int)column.width * scaling_factor);

		// plot designation
		switch (column.type) {
		case Origin::SpreadColumn::X:
			col->setPlotDesignation(AbstractColumn::PlotDesignation::X);
			break;
		case Origin::SpreadColumn::Y:
			col->setPlotDesignation(AbstractColumn::PlotDesignation::Y);
			break;
		case Origin::SpreadColumn::Z:
			col->setPlotDesignation(AbstractColumn::PlotDesignation::Z);
			break;
		case Origin::SpreadColumn::XErr:
			col->setPlotDesignation(AbstractColumn::PlotDesignation::XError);
			break;
		case Origin::SpreadColumn::YErr:
			col->setPlotDesignation(AbstractColumn::PlotDesignation::YError);
			break;
		case Origin::SpreadColumn::Label:
		case Origin::SpreadColumn::NONE:
		default:
			col->setPlotDesignation(AbstractColumn::PlotDesignation::NoDesignation);
		}

		QString format;
		switch (column.valueType) {
		case Origin::Numeric: {
			for (unsigned int i = column.beginRow; i < column.endRow; ++i) {
				const double value = column.data.at(i).as_double();
				if (value != _ONAN)
					col->setValueAt(i, value);
			}

			loadColumnNumericFormat(column, col);
			break;
		}
		case Origin::TextNumeric: {
			// A TextNumeric column can contain numeric and string values, there is no equivalent column mode in LabPlot.
			//  -> Set the column mode as 'Numeric' or 'Text' depending on the type of first non-empty element in column.
			for (unsigned int i = column.beginRow; i < column.endRow; ++i) {
				const Origin::variant value(column.data.at(i));
				if (value.type() == Origin::Variant::V_DOUBLE) {
					if (value.as_double() != _ONAN)
						break;
				} else {
					if (value.as_string() != nullptr) {
						col->setColumnMode(AbstractColumn::ColumnMode::Text);
						break;
					}
				}
			}

			if (col->columnMode() == AbstractColumn::ColumnMode::Double) {
				for (unsigned int i = column.beginRow; i < column.endRow; ++i) {
					const double value = column.data.at(i).as_double();
					if (column.data.at(i).type() == Origin::Variant::V_DOUBLE && value != _ONAN)
						col->setValueAt(i, value);
				}
				loadColumnNumericFormat(column, col);
			} else {
				for (unsigned int i = column.beginRow; i < column.endRow; ++i) {
					const Origin::variant value(column.data.at(i));
					if (value.type() == Origin::Variant::V_STRING) {
						if (value.as_string() != nullptr)
							col->setTextAt(i, QLatin1String(value.as_string()));
					} else {
						if (value.as_double() != _ONAN)
							col->setTextAt(i, QString::number(value.as_double()));
					}
				}
			}
			break;
		}
		case Origin::Text:
			col->setColumnMode(AbstractColumn::ColumnMode::Text);
			for (int i = 0; i < std::min((int)column.data.size(), rows); ++i)
				col->setTextAt(i, QLatin1String(column.data[i].as_string()));
			break;
		case Origin::Time: {
			switch (column.valueTypeSpecification + 128) {
			case Origin::TIME_HH_MM:
				format = QStringLiteral("hh:mm");
				break;
			case Origin::TIME_HH:
				format = QStringLiteral("hh");
				break;
			case Origin::TIME_HH_MM_SS:
				format = QStringLiteral("hh:mm:ss");
				break;
			case Origin::TIME_HH_MM_SS_ZZ:
				format = QStringLiteral("hh:mm:ss.zzz");
				break;
			case Origin::TIME_HH_AP:
				format = QStringLiteral("hh ap");
				break;
			case Origin::TIME_HH_MM_AP:
				format = QStringLiteral("hh:mm ap");
				break;
			case Origin::TIME_MM_SS:
				format = QStringLiteral("mm:ss");
				break;
			case Origin::TIME_MM_SS_ZZ:
				format = QStringLiteral("mm:ss.zzz");
				break;
			case Origin::TIME_HHMM:
				format = QStringLiteral("hhmm");
				break;
			case Origin::TIME_HHMMSS:
				format = QStringLiteral("hhmmss");
				break;
			case Origin::TIME_HH_MM_SS_ZZZ:
				format = QStringLiteral("hh:mm:ss.zzz");
				break;
			}

			for (int i = 0; i < std::min((int)column.data.size(), rows); ++i)
				col->setValueAt(i, column.data[i].as_double());
			col->setColumnMode(AbstractColumn::ColumnMode::DateTime);

			auto* filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
			filter->setFormat(format);
			break;
		}
		case Origin::Date: {
			switch (column.valueTypeSpecification) {
			case Origin::DATE_DD_MM_YYYY:
				format = QStringLiteral("dd/MM/yyyy");
				break;
			case Origin::DATE_DD_MM_YYYY_HH_MM:
				format = QStringLiteral("dd/MM/yyyy HH:mm");
				break;
			case Origin::DATE_DD_MM_YYYY_HH_MM_SS:
				format = QStringLiteral("dd/MM/yyyy HH:mm:ss");
				break;
			case Origin::DATE_DDMMYYYY:
			case Origin::DATE_DDMMYYYY_HH_MM:
			case Origin::DATE_DDMMYYYY_HH_MM_SS:
				format = QStringLiteral("dd.MM.yyyy");
				break;
			case Origin::DATE_MMM_D:
				format = QStringLiteral("MMM d");
				break;
			case Origin::DATE_M_D:
				format = QStringLiteral("M/d");
				break;
			case Origin::DATE_D:
				format = QLatin1Char('d');
				break;
			case Origin::DATE_DDD:
			case Origin::DATE_DAY_LETTER:
				format = QStringLiteral("ddd");
				break;
			case Origin::DATE_YYYY:
				format = QStringLiteral("yyyy");
				break;
			case Origin::DATE_YY:
				format = QStringLiteral("yy");
				break;
			case Origin::DATE_YYMMDD:
			case Origin::DATE_YYMMDD_HH_MM:
			case Origin::DATE_YYMMDD_HH_MM_SS:
			case Origin::DATE_YYMMDD_HHMM:
			case Origin::DATE_YYMMDD_HHMMSS:
				format = QStringLiteral("yyMMdd");
				break;
			case Origin::DATE_MMM:
			case Origin::DATE_MONTH_LETTER:
				format = QStringLiteral("MMM");
				break;
			case Origin::DATE_M_D_YYYY:
				format = QStringLiteral("M-d-yyyy");
				break;
			default:
				format = QStringLiteral("dd.MM.yyyy");
			}

			for (int i = 0; i < std::min((int)column.data.size(), rows); ++i)
				col->setValueAt(i, column.data[i].as_double());
			col->setColumnMode(AbstractColumn::ColumnMode::DateTime);

			auto* filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
			filter->setFormat(format);
			break;
		}
		case Origin::Month: {
			switch (column.valueTypeSpecification) {
			case Origin::MONTH_MMM:
				format = QStringLiteral("MMM");
				break;
			case Origin::MONTH_MMMM:
				format = QStringLiteral("MMMM");
				break;
			case Origin::MONTH_LETTER:
				format = QLatin1Char('M');
				break;
			}

			for (int i = 0; i < std::min((int)column.data.size(), rows); ++i)
				col->setValueAt(i, column.data[i].as_double());
			col->setColumnMode(AbstractColumn::ColumnMode::Month);

			auto* filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
			filter->setFormat(format);
			break;
		}
		case Origin::Day: {
			switch (column.valueTypeSpecification) {
			case Origin::DAY_DDD:
				format = QStringLiteral("ddd");
				break;
			case Origin::DAY_DDDD:
				format = QStringLiteral("dddd");
				break;
			case Origin::DAY_LETTER:
				format = QLatin1Char('d');
				break;
			}

			for (int i = 0; i < std::min((int)column.data.size(), rows); ++i)
				col->setValueAt(i, column.data[i].as_double());
			col->setColumnMode(AbstractColumn::ColumnMode::Day);

			auto* filter = static_cast<DateTime2StringFilter*>(col->outputFilter());
			filter->setFormat(format);
			break;
		}
		case Origin::ColumnHeading:
		case Origin::TickIndexedDataset:
		case Origin::Categorical:
			break;
		}
	}

	// TODO: "hidden" not supported yet
	//	if (spread.hidden || spread.loose)
	//		mw->hideWindow(spreadsheet);

	return true;
}

void OriginProjectParser::loadColumnNumericFormat(const Origin::SpreadColumn& originColumn, Column* column) const {
	if (originColumn.numericDisplayType != 0) {
		int fi = 0;
		switch (originColumn.valueTypeSpecification) {
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

		auto* filter = static_cast<Double2StringFilter*>(column->outputFilter());
		filter->setNumericFormat(fi);
		filter->setNumDigits(originColumn.decimalPlaces);
	}
}

bool OriginProjectParser::loadMatrixWorkbook(Workbook* workbook, bool preview) {
	DEBUG(Q_FUNC_INFO)
	// load matrix workbook sheets
	const Origin::Matrix& originMatrix = m_originFile->matrix(findMatrixByName(workbook->name()));
	for (size_t s = 0; s < originMatrix.sheets.size(); ++s) {
		Matrix* matrix = new Matrix(QString::fromLatin1(originMatrix.sheets[s].name.c_str()));
		loadMatrix(matrix, preview, s, workbook->name());
		workbook->addChildFast(matrix);
	}

	return true;
}

bool OriginProjectParser::loadMatrix(Matrix* matrix, bool preview, size_t sheetIndex, const QString& mwbName) {
	DEBUG(Q_FUNC_INFO)
	// import matrix data
	const Origin::Matrix& originMatrix = m_originFile->matrix(findMatrixByName(mwbName));

	if (preview)
		return true;

	// in Origin column width is measured in characters, we need to convert to pixels
	// TODO: determine the font used in Origin in order to get the same column width as in Origin
	QFont font;
	QFontMetrics fm(font);
	const int scaling_factor = fm.maxWidth();

	const Origin::MatrixSheet& layer = originMatrix.sheets[sheetIndex];
	const int colCount = layer.columnCount;
	const int rowCount = layer.rowCount;

	matrix->setRowCount(rowCount);
	matrix->setColumnCount(colCount);
	matrix->setFormula(QLatin1String(layer.command.c_str()));

	// TODO: how to handle different widths for different columns?
	for (int j = 0; j < colCount; j++)
		matrix->setColumnWidth(j, layer.width * scaling_factor);

	// TODO: check column major vs. row major to improve the performance here
	for (int i = 0; i < rowCount; i++) {
		for (int j = 0; j < colCount; j++)
			matrix->setCell(i, j, layer.data[j + i * colCount]);
	}

	char format = 'g';
	// TODO: prec not support by Matrix
	// int prec = 6;
	switch (layer.valueTypeSpecification) {
	case 0: // Decimal 1000
		format = 'f';
		//	prec = layer.decimalPlaces;
		break;
	case 1: // Scientific
		format = 'e';
		//	prec = layer.decimalPlaces;
		break;
	case 2: // Engineering
	case 3: // Decimal 1,000
		format = 'g';
		//	prec = layer.significantDigits;
		break;
	}

	matrix->setNumericFormat(format);

	return true;
}

bool OriginProjectParser::loadWorksheet(Worksheet* worksheet, bool preview) {
	DEBUG(Q_FUNC_INFO << ", preview = " << preview)
	if (worksheet->parentAspect())
		DEBUG(Q_FUNC_INFO << ", parent PATH " << STDSTRING(worksheet->parentAspect()->path()))

	// load worksheet data
	const Origin::Graph& graph = m_originFile->graph(findWorksheetByName(worksheet->name()));
	DEBUG(Q_FUNC_INFO << ", worksheet name = " << graph.name);
	worksheet->setComment(QLatin1String(graph.label.c_str()));

	// TODO: width, height, view mode (print view, page view, window view, draft view)
	// Origin allows to freely resize the window and ajusts the size of the plot (layer) automatically
	// by keeping a certain width-to-height ratio. It's not clear what the actual size of the plot/layer is and how to handle this.
	// For now we simply create a new wokrsheet here with it's default size and make it using the whole view size.
	// Later we can decide to use one of the following properties:
	//  1) Window.frameRect gives Rect-corner coordinates (in pixels) of the Window object
	//  2) GraphLayer.clientRect gives Rect-corner coordinates (pixels) of the Layer inside the (printer?) page.
	//  3) Graph.width, Graph.height give the (printer?) page size in pixels.
	// 	const QRectF size(0, 0,
	// 					  Worksheet::convertToSceneUnits(graph.width/600., Worksheet::Inch),
	// 					  Worksheet::convertToSceneUnits(graph.height/600., Worksheet::Inch));
	// 	worksheet->setPageRect(size);
	worksheet->setUseViewSize(true);

	QHash<TextLabel*, QSizeF> textLabelPositions;

	// worksheet background color
	const Origin::ColorGradientDirection bckgColorGradient = graph.windowBackgroundColorGradient;
	const Origin::Color bckgBaseColor = graph.windowBackgroundColorBase;
	const Origin::Color bckgEndColor = graph.windowBackgroundColorEnd;
	worksheet->background()->setColorStyle(backgroundColorStyle(bckgColorGradient));
	switch (bckgColorGradient) {
	case Origin::ColorGradientDirection::NoGradient:
	case Origin::ColorGradientDirection::TopLeft:
	case Origin::ColorGradientDirection::Left:
	case Origin::ColorGradientDirection::BottomLeft:
	case Origin::ColorGradientDirection::Top:
		worksheet->background()->setFirstColor(color(bckgEndColor));
		worksheet->background()->setSecondColor(color(bckgBaseColor));
		break;
	case Origin::ColorGradientDirection::Center:
		break;
	case Origin::ColorGradientDirection::Bottom:
	case Origin::ColorGradientDirection::TopRight:
	case Origin::ColorGradientDirection::Right:
	case Origin::ColorGradientDirection::BottomRight:
		worksheet->background()->setFirstColor(color(bckgBaseColor));
		worksheet->background()->setSecondColor(color(bckgEndColor));
	}

	// TODO: do we need changes on the worksheet layout?

	// process Origin's graph layers - add new plot areas or new coordinate system in the same plot area, depending on the global setting
	// https://www.originlab.com/doc/Origin-Help/MultiLayer-Graph
	int layerIndex = 0; // index of the graph layer
	CartesianPlot* plot = nullptr;
	for (const auto& layer : graph.layers) {
		DEBUG(Q_FUNC_INFO << ", Graph Layer " << layerIndex + 1)
		if (layer.is3D()) {
			// TODO: add an "UnsupportedAspect" here since we don't support 3D yet
			break;
		}

		// create a new plot if we're
		// 1. interpreting every layer as a new plot
		// 2. interpreting every layer as a new coordinate system in the same and single plot and no plot was created yet
		DEBUG(Q_FUNC_INFO << ", layer as plot area = " << m_graphLayerAsPlotArea)
		if (m_graphLayerAsPlotArea || (!m_graphLayerAsPlotArea && !plot)) {
			plot = new CartesianPlot(i18n("Plot%1", QString::number(layerIndex + 1)));
			worksheet->addChildFast(plot);
			plot->setIsLoading(true);
		}

		// TODO: if we skip, the curves are not loaded below and we cannot track the dependencies on columns
		if (preview)
			continue;

		loadGraphLayer(layer, plot, layerIndex, textLabelPositions, preview);
		++layerIndex;
	}

	if (!preview) {
		worksheet->updateLayout();

		// worksheet and plots got their sizes,
		//-> position all text labels inside the plots correctly by converting
		// the relative positions determined above to the absolute values
		QHash<TextLabel*, QSizeF>::const_iterator it = textLabelPositions.constBegin();
		while (it != textLabelPositions.constEnd()) {
			TextLabel* label = it.key();
			const QSizeF& ratios = it.value();
			const auto* plot = static_cast<const CartesianPlot*>(label->parentAspect());

			TextLabel::PositionWrapper position = label->position();
			position.point.setX(plot->dataRect().width() * (ratios.width() - 0.5));
			position.point.setY(plot->dataRect().height() * (ratios.height() - 0.5));
			label->setPosition(position);

			++it;
		}
	}

	DEBUG(Q_FUNC_INFO << " DONE");
	return true;
}

void OriginProjectParser::loadGraphLayer(const Origin::GraphLayer& layer,
										 CartesianPlot* plot,
										 int layerIndex,
										 QHash<TextLabel*, QSizeF> textLabelPositions,
										 bool preview) {
	DEBUG(Q_FUNC_INFO << ", NEW GRAPH LAYER")
	// TODO: width, height

	// background color
	const auto& regColor = layer.backgroundColor;
	if (regColor.type == Origin::Color::None)
		plot->plotArea()->background()->setOpacity(0);
	else
		plot->plotArea()->background()->setFirstColor(color(regColor));

	// border
	if (layer.borderType == Origin::BorderType::None)
		plot->plotArea()->borderLine()->setStyle(Qt::NoPen);
	else
		plot->plotArea()->borderLine()->setStyle(Qt::SolidLine);

	// ranges
	const auto& originXAxis = layer.xAxis;
	const auto& originYAxis = layer.yAxis;

	Range<double> xRange(originXAxis.min, originXAxis.max);
	Range<double> yRange(originYAxis.min, originYAxis.max);
	xRange.setAutoScale(false);
	yRange.setAutoScale(false);

	if (m_graphLayerAsPlotArea) { // graph layer is read as a new plot area
		// set the ranges for default coordinate system
		plot->setRangeDefault(Dimension::X, xRange);
		plot->setRangeDefault(Dimension::Y, yRange);
	} else { // graph layer is read as a new coordinate system in the same plot area
		// create a new coordinate systems and set the ranges for it
		if (layerIndex > 0) {
			// check if identical range already exists
			int selectedXRangeIndex = -1;
			for (int i = 0; i < plot->rangeCount(Dimension::X); i++) {
				const auto& range = plot->range(Dimension::X, i);
				if (range == xRange) {
					selectedXRangeIndex = i;
					break;
				}
			}
			int selectedYRangeIndex = -1;
			for (int i = 0; i < plot->rangeCount(Dimension::Y); i++) {
				const auto& range = plot->range(Dimension::Y, i);
				if (range == yRange) {
					selectedYRangeIndex = i;
					break;
				}
			}

			if (selectedXRangeIndex < 0) {
				plot->addXRange();
				selectedXRangeIndex = plot->rangeCount(Dimension::X) - 1;
			}
			if (selectedYRangeIndex < 0) {
				plot->addYRange();
				selectedYRangeIndex = plot->rangeCount(Dimension::Y) - 1;
			}

			plot->addCoordinateSystem();
			// set ranges for new coordinate system
			plot->setCoordinateSystemRangeIndex(layerIndex, Dimension::X, selectedXRangeIndex);
			plot->setCoordinateSystemRangeIndex(layerIndex, Dimension::Y, selectedYRangeIndex);
		}
		plot->setRange(Dimension::X, layerIndex, xRange);
		plot->setRange(Dimension::Y, layerIndex, yRange);
	}

	// scales
	switch (originXAxis.scale) {
	case Origin::GraphAxis::Linear:
		plot->setXRangeScale(RangeT::Scale::Linear);
		break;
	case Origin::GraphAxis::Log10:
		plot->setXRangeScale(RangeT::Scale::Log10);
		break;
	case Origin::GraphAxis::Ln:
		plot->setXRangeScale(RangeT::Scale::Ln);
		break;
	case Origin::GraphAxis::Log2:
		plot->setXRangeScale(RangeT::Scale::Log2);
		break;
	case Origin::GraphAxis::Reciprocal:
		plot->setXRangeScale(RangeT::Scale::Inverse);
		break;
	case Origin::GraphAxis::Probability:
	case Origin::GraphAxis::Probit:
	case Origin::GraphAxis::OffsetReciprocal:
	case Origin::GraphAxis::Logit:
		// TODO:
		plot->setXRangeScale(RangeT::Scale::Linear);
		break;
	}

	switch (originYAxis.scale) {
	case Origin::GraphAxis::Linear:
		plot->setYRangeScale(RangeT::Scale::Linear);
		break;
	case Origin::GraphAxis::Log10:
		plot->setYRangeScale(RangeT::Scale::Log10);
		break;
	case Origin::GraphAxis::Ln:
		plot->setYRangeScale(RangeT::Scale::Ln);
		break;
	case Origin::GraphAxis::Log2:
		plot->setYRangeScale(RangeT::Scale::Log2);
		break;
	case Origin::GraphAxis::Reciprocal:
		plot->setYRangeScale(RangeT::Scale::Inverse);
		break;
	case Origin::GraphAxis::Probability:
	case Origin::GraphAxis::Probit:
	case Origin::GraphAxis::OffsetReciprocal:
	case Origin::GraphAxis::Logit:
		// TODO:
		plot->setYRangeScale(RangeT::Scale::Linear);
		break;
	}

	// axes
	DEBUG(Q_FUNC_INFO << ", layer.curves.size() = " << layer.curves.size())
	if (layer.curves.empty()) // no curves, just axes
		loadAxes(layer, plot, layerIndex, QLatin1String("X Axis Title"), QLatin1String("Y Axis Title"));
	else {
		auto originCurve = layer.curves.at(0);
		QString xColumnName = QString::fromLatin1(originCurve.xColumnName.c_str());
		// TODO: "Partikelgrö"
		DEBUG("	xColumnName = " << STDSTRING(xColumnName));
		//				xColumnName.replace("%(?X,@LL)", );	// Long Name

		QDEBUG("	UTF8 xColumnName = " << xColumnName.toUtf8());
		QString yColumnName = QString::fromLatin1(originCurve.yColumnName.c_str());

		loadAxes(layer, plot, layerIndex, xColumnName, yColumnName);
	}

	// range breaks
	// TODO

	// add legend if available
	const auto& originLegend = layer.legend;
	const QString& legendText = QString::fromLatin1(originLegend.text.c_str());
	DEBUG(Q_FUNC_INFO << ", legend text = \"" << STDSTRING(legendText) << "\"");
	if (!originLegend.text.empty()) {
		auto* legend = new CartesianPlotLegend(i18n("legend"));
		plot->addLegend(legend);

		// Origin's legend uses "\l(...)" or "\L(...)" string to format the legend symbol
		//  and "%(...) to format the legend text for each curve
		// s. a. https://www.originlab.com/doc/Origin-Help/Legend-ManualControl
		// the text before these formatting tags, if available, is interpreted as the legend title
		QString legendTitle;

		// search for the first occurrence of the legend symbol substring
		int index = legendText.indexOf(QLatin1String("\\l("), 0, Qt::CaseInsensitive);
		if (index != -1)
			legendTitle = legendText.left(index);
		else {
			// check legend text
			index = legendText.indexOf(QLatin1String("%("));
			if (index != -1)
				legendTitle = legendText.left(index);
		}

		legendTitle = legendTitle.trimmed();
		if (!legendTitle.isEmpty())
			legendTitle = parseOriginText(legendTitle);
		if (!legendTitle.isEmpty()) {
			DEBUG(Q_FUNC_INFO << ", legend title = \"" << STDSTRING(legendTitle) << "\"");
			legend->title()->setText(legendTitle);
		} else {
			DEBUG(Q_FUNC_INFO << ", legend title is empty");
		}

		// TODO: text color
		// const Origin::Color& originColor = originLegend.color;

		// position
		// TODO: for the first release with OPJ support we put the legend to the bottom left corner,
		// in the next release we'll evaluate originLegend.clientRect giving the position inside of the whole page in Origin.
		// In Origin the legend can be placed outside of the plot which is not possible in LabPlot.
		// To achieve this we'll need to increase padding area in the plot and to place the legend outside of the plot area.
		CartesianPlotLegend::PositionWrapper position;
		position.horizontalPosition = WorksheetElement::HorizontalPosition::Right;
		position.verticalPosition = WorksheetElement::VerticalPosition::Top;
		legend->setPosition(position);

		// rotation
		legend->setRotationAngle(originLegend.rotation);

		// border line
		if (originLegend.borderType == Origin::BorderType::None)
			legend->borderLine()->setStyle(Qt::NoPen);
		else
			legend->borderLine()->setStyle(Qt::SolidLine);

		// background color, determine it with the help of the border type
		if (originLegend.borderType == Origin::BorderType::DarkMarble)
			legend->background()->setFirstColor(Qt::darkGray);
		else if (originLegend.borderType == Origin::BorderType::BlackOut)
			legend->background()->setFirstColor(Qt::black);
		else
			legend->background()->setFirstColor(Qt::white);
	}

	// texts
	for (const auto& t : layer.texts) {
		DEBUG("EXTRA TEXT = " << t.text.c_str());
		auto* label = new TextLabel(QStringLiteral("text label"));
		QTextEdit te(parseOriginText(QString::fromLatin1(t.text.c_str())));
		// label settings (with resonable font size scaling)
		te.selectAll();
		te.setFontPointSize(int(t.fontSize * 0.4));
		te.setTextColor(OriginProjectParser::color(t.color));
		label->setText(te.toHtml());
		// DEBUG(" TEXT = " << STDSTRING(label->text().text))

		plot->addChild(label);
		label->setParentGraphicsItem(plot->graphicsItem());

		// position
		// determine the relative position inside of the layer rect
		const qreal horRatio = (qreal)(t.clientRect.left - layer.clientRect.left) / (layer.clientRect.right - layer.clientRect.left);
		const qreal vertRatio = (qreal)(t.clientRect.top - layer.clientRect.top) / (layer.clientRect.bottom - layer.clientRect.top);
		textLabelPositions[label] = QSizeF(horRatio, vertRatio);
		DEBUG("horizontal/vertical ratio = " << horRatio << ", " << vertRatio);

		// rotation
		label->setRotationAngle(t.rotation);

		// TODO:
		// 				int tab;
		// 				BorderType borderType;
		// 				Attach attach;
	}

	// curves
	int curveIndex = 1;
	for (const auto& originCurve : layer.curves) {
		QString data(QLatin1String(originCurve.dataName.c_str()));
		DEBUG(Q_FUNC_INFO << ", NEW CURVE " << STDSTRING(data))
		switch (data.at(0).toLatin1()) {
		case 'T': // Spreadsheet
		case 'E': { // Workbook
			if (originCurve.type == Origin::GraphCurve::Line || originCurve.type == Origin::GraphCurve::Scatter
				|| originCurve.type == Origin::GraphCurve::LineSymbol || originCurve.type == Origin::GraphCurve::ErrorBar
				|| originCurve.type == Origin::GraphCurve::XErrorBar) {
				// parse and use legend text
				// find substring between %c{curveIndex} and %c{curveIndex+1}
				int pos1 = legendText.indexOf(QStringLiteral("\\c{%1}").arg(curveIndex)) + 5;
				int pos2 = legendText.indexOf(QStringLiteral("\\c{%1}").arg(curveIndex + 1));
				QString curveText = legendText.mid(pos1, pos2 - pos1);
				// replace %(1), %(2), etc. with curve name
				curveText.replace(QStringLiteral("%(%1)").arg(curveIndex), QLatin1String(originCurve.yColumnName.c_str()));
				curveText = curveText.trimmed();
				DEBUG(" curve " << curveIndex << " text = \"" << STDSTRING(curveText) << "\"");

				// XYCurve* xyCurve = new XYCurve(i18n("Curve%1", QString::number(curveIndex)));
				// TODO: curve (legend) does not support HTML text yet.
				// XYCurve* xyCurve = new XYCurve(curveText);
				XYCurve* curve = new XYCurve(QString::fromLatin1(originCurve.yColumnName.c_str()));
				if (m_graphLayerAsPlotArea)
					curve->setCoordinateSystemIndex(plot->defaultCoordinateSystemIndex());
				else
					curve->setCoordinateSystemIndex(layerIndex);
				// DEBUG("CURVE path = " << STDSTRING(data))
				QString containerName = data.right(data.length() - 2); // strip "E_" or "T_"
				int sheetIndex = 0; // which sheet? "@..."
				const int atIndex = containerName.indexOf(QLatin1Char('@'));
				if (atIndex != -1) {
					sheetIndex = containerName.mid(atIndex + 1).toInt() - 1;
					containerName.truncate(atIndex);
				}
				// DEBUG("CONTAINER = " << STDSTRING(containerName) << ", SHEET = " << sheetIndex)
				int workbookIndex = findWorkbookByName(containerName);
				// if workbook not found, findWorkbookByName() returns 0: check this
				if (workbookIndex == 0 && (m_originFile->excelCount() == 0 || containerName.toStdString() != m_originFile->excel(0).name))
					workbookIndex = -1;
				// DEBUG("WORKBOOK  index = " << workbookIndex)
				QString tableName = containerName;
				if (workbookIndex != -1) // container is a workbook
					tableName = containerName + QLatin1Char('/') + QLatin1String(m_originFile->excel(workbookIndex).sheets[sheetIndex].name.c_str());
				// DEBUG("SPREADSHEET name = " << STDSTRING(tableName))
				curve->setXColumnPath(tableName + QLatin1Char('/') + QLatin1String(originCurve.xColumnName.c_str()));
				curve->setYColumnPath(tableName + QLatin1Char('/') + QLatin1String(originCurve.yColumnName.c_str()));
				DEBUG(Q_FUNC_INFO << ", x/y column path = \"" << STDSTRING(curve->xColumnPath()) << "\" \"" << STDSTRING(curve->yColumnPath()) << "\"")

				curve->setSuppressRetransform(true);
				if (!preview)
					loadCurve(originCurve, curve);
				plot->addChildFast(curve);
				curve->setSuppressRetransform(false);
			} else if (originCurve.type == Origin::GraphCurve::Column) {
				// vertical bars

			} else if (originCurve.type == Origin::GraphCurve::Bar) {
				// horizontal bars

			} else if (originCurve.type == Origin::GraphCurve::Histogram) {
			}
		} break;
		case 'F': {
			Origin::Function function;
			const auto funcIndex = m_originFile->functionIndex(data.right(data.length() - 2).toStdString().c_str());
			if (funcIndex < 0) {
				++curveIndex;
				continue;
			}

			function = m_originFile->function(funcIndex);

			auto* xyEqCurve = new XYEquationCurve(QLatin1String(function.name.c_str()));
			if (m_graphLayerAsPlotArea)
				xyEqCurve->setCoordinateSystemIndex(plot->defaultCoordinateSystemIndex());
			else
				xyEqCurve->setCoordinateSystemIndex(layerIndex);
			XYEquationCurve::EquationData eqData;

			eqData.count = function.totalPoints;
			eqData.expression1 = QLatin1String(function.formula.c_str());

			if (function.type == Origin::Function::Polar) {
				eqData.type = XYEquationCurve::EquationType::Polar;

				// replace 'x' by 'phi'
				eqData.expression1 = eqData.expression1.replace(QLatin1Char('x'), QLatin1String("phi"));

				// convert from degrees to radians
				eqData.min = QString::number(function.begin / 180) + QLatin1String("*pi");
				eqData.max = QString::number(function.end / 180) + QLatin1String("*pi");
			} else {
				eqData.expression1 = QLatin1String(function.formula.c_str());
				eqData.min = QString::number(function.begin);
				eqData.max = QString::number(function.end);
			}

			xyEqCurve->setSuppressRetransform(true);
			xyEqCurve->setEquationData(eqData);
			if (!preview)
				loadCurve(originCurve, xyEqCurve);
			plot->addChildFast(xyEqCurve);
			xyEqCurve->setSuppressRetransform(false);
		}
			// TODO case 'M': Matrix
		}

		++curveIndex;
	}
}

void OriginProjectParser::loadAxes(const Origin::GraphLayer& layer,
								   CartesianPlot* plot,
								   int layerIndex,
								   const QString& xColumnName,
								   const QString& yColumnName) {
	const auto& originXAxis = layer.xAxis;
	const auto& originYAxis = layer.yAxis;

	// x bottom
	if (!originXAxis.formatAxis[0].hidden || originXAxis.tickAxis[0].showMajorLabels) {
		Axis* axis = new Axis(QStringLiteral("x"), Axis::Orientation::Horizontal);
		axis->setSuppressRetransform(true);
		axis->setPosition(Axis::Position::Bottom);
		plot->addChildFast(axis);
		loadAxis(originXAxis, axis, 0, xColumnName);
		if (!m_graphLayerAsPlotArea)
			axis->setCoordinateSystemIndex(layerIndex);
		axis->setSuppressRetransform(false);
	}

	// x top
	if (!originXAxis.formatAxis[1].hidden || originXAxis.tickAxis[1].showMajorLabels) {
		Axis* axis = new Axis(QStringLiteral("x top"), Axis::Orientation::Horizontal);
		axis->setPosition(Axis::Position::Top);
		axis->setSuppressRetransform(true);
		plot->addChildFast(axis);
		loadAxis(originXAxis, axis, 1, xColumnName);
		if (!m_graphLayerAsPlotArea)
			axis->setCoordinateSystemIndex(layerIndex);
		axis->setSuppressRetransform(false);
	}

	// y left
	if (!originYAxis.formatAxis[0].hidden || originYAxis.tickAxis[0].showMajorLabels) {
		Axis* axis = new Axis(QStringLiteral("y"), Axis::Orientation::Vertical);
		axis->setSuppressRetransform(true);
		axis->setPosition(Axis::Position::Left);
		plot->addChildFast(axis);
		loadAxis(originYAxis, axis, 0, yColumnName);
		if (!m_graphLayerAsPlotArea)
			axis->setCoordinateSystemIndex(layerIndex);
		axis->setSuppressRetransform(false);
	}

	// y right
	if (!originYAxis.formatAxis[1].hidden || originYAxis.tickAxis[1].showMajorLabels) {
		Axis* axis = new Axis(QStringLiteral("y right"), Axis::Orientation::Vertical);
		axis->setSuppressRetransform(true);
		axis->setPosition(Axis::Position::Right);
		plot->addChildFast(axis);
		loadAxis(originYAxis, axis, 1, yColumnName);
		if (!m_graphLayerAsPlotArea)
			axis->setCoordinateSystemIndex(layerIndex);
		axis->setSuppressRetransform(false);
	}
}

/*
 * sets the axis properties (format and ticks) as defined in \c originAxis in \c axis,
 * \c index being 0 or 1 for "bottom" and "top" or "left" and "right" for horizontal or vertical axes, respectively.
 */
void OriginProjectParser::loadAxis(const Origin::GraphAxis& originAxis, Axis* axis, int index, const QString& axisTitle) const {
	// 	int axisPosition;
	//		possible values:
	//			0: Axis is at default position
	//			1: Axis is at (axisPositionValue)% from standard position
	//			2: Axis is at (axisPositionValue) position of orthogonal axis
	// 		double axisPositionValue;

	// 		bool zeroLine;
	// 		bool oppositeLine;

	// ranges
	axis->setRange(originAxis.min, originAxis.max);

	// ticks
	axis->setMajorTicksType(Axis::TicksType::Spacing);
	axis->setMajorTicksSpacing(originAxis.step);
	DEBUG(Q_FUNC_INFO << ", index = " << index)
	DEBUG(Q_FUNC_INFO << ", position = " << originAxis.position)
	// TODO: set offset from step and anchor (not currently available in liborigin)
	DEBUG(Q_FUNC_INFO << ", anchor = " << originAxis.anchor)
	axis->setMajorTickStartOffset(0.0);
	axis->setMinorTicksType(Axis::TicksType::TotalNumber);
	axis->setMinorTicksNumber(originAxis.minorTicks);

	// scale
	switch (originAxis.scale) {
	case Origin::GraphAxis::Linear:
		axis->setScale(RangeT::Scale::Linear);
		break;
	case Origin::GraphAxis::Log10:
		axis->setScale(RangeT::Scale::Log10);
		break;
	case Origin::GraphAxis::Ln:
		axis->setScale(RangeT::Scale::Ln);
		break;
	case Origin::GraphAxis::Log2:
		axis->setScale(RangeT::Scale::Log2);
		break;
	case Origin::GraphAxis::Reciprocal:
		axis->setScale(RangeT::Scale::Inverse);
		break;
	case Origin::GraphAxis::Probability:
	case Origin::GraphAxis::Probit:
	case Origin::GraphAxis::OffsetReciprocal:
	case Origin::GraphAxis::Logit:
		// TODO: set if implemented
		axis->setScale(RangeT::Scale::Linear);
		break;
	}

	// major grid
	const Origin::GraphGrid& majorGrid = originAxis.majorGrid;
	Qt::PenStyle penStyle(Qt::NoPen);
	if (!majorGrid.hidden) {
		switch (majorGrid.style) {
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
	}
	axis->majorGridLine()->setStyle(penStyle);

	Origin::Color gridColor;
	gridColor.type = Origin::Color::ColorType::Regular;
	gridColor.regular = majorGrid.color;
	axis->majorGridLine()->setColor(OriginProjectParser::color(gridColor));
	axis->majorGridLine()->setWidth(Worksheet::convertToSceneUnits(majorGrid.width, Worksheet::Unit::Point));

	// minor grid
	const Origin::GraphGrid& minorGrid = originAxis.minorGrid;
	penStyle = Qt::NoPen;
	if (!minorGrid.hidden) {
		switch (minorGrid.style) {
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
	}
	axis->minorGridLine()->setStyle(penStyle);

	gridColor.regular = minorGrid.color;
	axis->minorGridLine()->setColor(OriginProjectParser::color(gridColor));
	axis->minorGridLine()->setWidth(Worksheet::convertToSceneUnits(minorGrid.width, Worksheet::Unit::Point));

	// process Origin::GraphAxisFormat
	const Origin::GraphAxisFormat& axisFormat = originAxis.formatAxis[index];

	Origin::Color color;
	color.type = Origin::Color::ColorType::Regular;
	color.regular = axisFormat.color;
	axis->line()->setColor(OriginProjectParser::color(color));
	axis->line()->setWidth(Worksheet::convertToSceneUnits(axisFormat.thickness, Worksheet::Unit::Point));
	if (axisFormat.hidden)
		axis->line()->setStyle(Qt::NoPen);
	// TODO: read line style properties? (solid line, dashed line, etc.)

	axis->setMajorTicksLength(Worksheet::convertToSceneUnits(axisFormat.majorTickLength, Worksheet::Unit::Point));
	axis->setMajorTicksDirection((Axis::TicksFlags)axisFormat.majorTicksType);
	axis->majorTicksLine()->setStyle(axis->line()->style());
	axis->majorTicksLine()->setColor(axis->line()->color());
	axis->majorTicksLine()->setWidth(axis->line()->width());
	axis->setMinorTicksLength(axis->majorTicksLength() / 2); // minorTicksLength is half of majorTicksLength
	axis->setMinorTicksDirection((Axis::TicksFlags)axisFormat.minorTicksType);
	axis->minorTicksLine()->setStyle(axis->line()->style());
	axis->minorTicksLine()->setColor(axis->line()->color());
	axis->minorTicksLine()->setWidth(axis->line()->width());

	QString titleText = parseOriginText(QString::fromLatin1(axisFormat.label.text.c_str()));
	DEBUG("	axis title text = " << STDSTRING(titleText));
	// TODO: parseOriginText() returns html formatted string. What is axisFormat.color used for?
	// TODO: use axisFormat.fontSize to override the global font size for the hmtl string?
	// TODO: convert special character here too
	DEBUG("	curve name = " << STDSTRING(axisTitle));
	titleText.replace(QLatin1String("%(?X)"), axisTitle);
	titleText.replace(QLatin1String("%(?Y)"), axisTitle);
	DEBUG(" axis title = " << STDSTRING(titleText));
	axis->title()->setText(titleText);
	axis->title()->setRotationAngle(axisFormat.label.rotation);

	axis->setLabelsPrefix(QLatin1String(axisFormat.prefix.c_str()));
	axis->setLabelsSuffix(QLatin1String(axisFormat.suffix.c_str()));
	// TODO: handle string factor member in GraphAxisFormat

	// process Origin::GraphAxisTick
	const Origin::GraphAxisTick& tickAxis = originAxis.tickAxis[index];
	if (tickAxis.showMajorLabels) {
		color.type = Origin::Color::ColorType::Regular;
		color.regular = tickAxis.color;
		axis->setLabelsColor(OriginProjectParser::color(color));
		if (index == 0) // left
			axis->setLabelsPosition(Axis::LabelsPosition::Out);
		else // right
			axis->setLabelsPosition(Axis::LabelsPosition::In);
	} else {
		axis->setLabelsPosition(Axis::LabelsPosition::NoLabels);
	}

	// TODO: handle ValueType valueType member in GraphAxisTick
	// TODO: handle int valueTypeSpecification in GraphAxisTick

	// precision
	if (tickAxis.decimalPlaces == -1)
		axis->setLabelsAutoPrecision(true);
	else {
		axis->setLabelsPrecision(tickAxis.decimalPlaces);
		axis->setLabelsAutoPrecision(false);
	}

	QFont font;
	// TODO: font family?
	//  use half the font size to be closer to original
	font.setPixelSize(Worksheet::convertToSceneUnits(tickAxis.fontSize / 2, Worksheet::Unit::Point));
	font.setBold(tickAxis.fontBold);
	axis->setLabelsFont(font);
	// TODO: handle string dataName member in GraphAxisTick
	// TODO: handle string columnName member in GraphAxisTick
	axis->setLabelsRotationAngle(tickAxis.rotation);
}

void OriginProjectParser::loadCurve(const Origin::GraphCurve& originCurve, XYCurve* curve) const {
	DEBUG(Q_FUNC_INFO)
	// line properties
	Qt::PenStyle penStyle(Qt::NoPen);
	if (originCurve.type == Origin::GraphCurve::Line || originCurve.type == Origin::GraphCurve::LineSymbol) {
		switch (originCurve.lineConnect) {
		case Origin::GraphCurve::NoLine:
			curve->setLineType(XYCurve::LineType::NoLine);
			break;
		case Origin::GraphCurve::Straight:
			curve->setLineType(XYCurve::LineType::Line);
			break;
		case Origin::GraphCurve::TwoPointSegment:
			curve->setLineType(XYCurve::LineType::Segments2);
			break;
		case Origin::GraphCurve::ThreePointSegment:
			curve->setLineType(XYCurve::LineType::Segments3);
			break;
		case Origin::GraphCurve::BSpline:
		case Origin::GraphCurve::Bezier:
		case Origin::GraphCurve::Spline:
			curve->setLineType(XYCurve::LineType::SplineCubicNatural);
			break;
		case Origin::GraphCurve::StepHorizontal:
			curve->setLineType(XYCurve::LineType::StartHorizontal);
			break;
		case Origin::GraphCurve::StepVertical:
			curve->setLineType(XYCurve::LineType::StartVertical);
			break;
		case Origin::GraphCurve::StepHCenter:
			curve->setLineType(XYCurve::LineType::MidpointHorizontal);
			break;
		case Origin::GraphCurve::StepVCenter:
			curve->setLineType(XYCurve::LineType::MidpointVertical);
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

		curve->line()->setStyle(penStyle);
		curve->line()->setWidth(Worksheet::convertToSceneUnits(originCurve.lineWidth, Worksheet::Unit::Point));
		curve->line()->setColor(color(originCurve.lineColor));
		curve->line()->setOpacity(1 - originCurve.lineTransparency / 255);

		// TODO: handle unsigned char boxWidth of Origin::GraphCurve
	} else
		curve->line()->setStyle(penStyle);

	// symbol properties
	auto* symbol = curve->symbol();
	if (originCurve.type == Origin::GraphCurve::Scatter || originCurve.type == Origin::GraphCurve::LineSymbol) {
		// try to map the different symbols, mapping is not exact
		symbol->setRotationAngle(0);
		switch (originCurve.symbolShape) { // see https://www.originlab.com/doc/Labtalk/Ref/List-of-Symbol-Shapes
		case 0: // NoSymbol
			symbol->setStyle(Symbol::Style::NoSymbols);
			break;
		case 1: // Square
			switch (originCurve.symbolInterior) {
			case 0: // solid
			case 1: // open
			case 3: // hollow
				symbol->setStyle(Symbol::Style::Square);
				break;
			case 2: // dot
				symbol->setStyle(Symbol::Style::SquareDot);
				break;
			case 4: // plus
				symbol->setStyle(Symbol::Style::SquarePlus);
				break;
			case 5: // X
				symbol->setStyle(Symbol::Style::SquareX);
				break;
			case 6: // minus
			case 10: // down
				symbol->setStyle(Symbol::Style::SquareHalf);
				break;
			case 7: // pipe
				symbol->setStyle(Symbol::Style::SquareHalf);
				symbol->setRotationAngle(90);
				break;
			case 8: // up
				symbol->setStyle(Symbol::Style::SquareHalf);
				symbol->setRotationAngle(180);
				break;
			case 9: // right
				symbol->setStyle(Symbol::Style::SquareHalf);
				symbol->setRotationAngle(-90);
				break;
			case 11: // left
				symbol->setStyle(Symbol::Style::SquareHalf);
				symbol->setRotationAngle(90);
				break;
			}
			break;
		case 2: // Ellipse
		case 20: // Sphere
			switch (originCurve.symbolInterior) {
			case 0: // solid
			case 1: // open
			case 3: // hollow
				symbol->setStyle(Symbol::Style::Circle);
				break;
			case 2: // dot
				symbol->setStyle(Symbol::Style::CircleDot);
				break;
			case 4: // plus
				symbol->setStyle(Symbol::Style::CircleX);
				symbol->setRotationAngle(45);
				break;
			case 5: // X
				symbol->setStyle(Symbol::Style::CircleX);
				break;
			case 6: // minus
				symbol->setStyle(Symbol::Style::CircleHalf);
				symbol->setRotationAngle(90);
				break;
			case 7: // pipe
			case 11: // left
				symbol->setStyle(Symbol::Style::CircleHalf);
				break;
			case 8: // up
				symbol->setStyle(Symbol::Style::CircleHalf);
				symbol->setRotationAngle(90);
				break;
			case 9: // right
				symbol->setStyle(Symbol::Style::CircleHalf);
				symbol->setRotationAngle(180);
				break;
			case 10: // down
				symbol->setStyle(Symbol::Style::CircleHalf);
				symbol->setRotationAngle(-90);
				break;
			}
			break;
		case 3: // UTriangle
			switch (originCurve.symbolInterior) {
			case 0: // solid
			case 1: // open
			case 3: // hollow
			case 4: // plus	TODO
			case 5: // X	TODO
				symbol->setStyle(Symbol::Style::EquilateralTriangle);
				break;
			case 2: // dot
				symbol->setStyle(Symbol::Style::TriangleDot);
				break;
			case 7: // pipe
			case 11: // left
				symbol->setStyle(Symbol::Style::TriangleLine);
				break;
			case 6: // minus
			case 8: // up
				symbol->setStyle(Symbol::Style::TriangleHalf);
				break;
			case 9: // right	TODO
				symbol->setStyle(Symbol::Style::TriangleLine);
				// symbol->setRotationAngle(180);
				break;
			case 10: // down	TODO
				symbol->setStyle(Symbol::Style::TriangleHalf);
				// symbol->setRotationAngle(180);
				break;
			}
			break;
		case 4: // DTriangle
			switch (originCurve.symbolInterior) {
			case 0: // solid
			case 1: // open
			case 3: // hollow
			case 4: // plus	TODO
			case 5: // X	TODO
				symbol->setStyle(Symbol::Style::EquilateralTriangle);
				symbol->setRotationAngle(180);
				break;
			case 2: // dot
				symbol->setStyle(Symbol::Style::TriangleDot);
				symbol->setRotationAngle(180);
				break;
			case 7: // pipe
			case 11: // left
				symbol->setStyle(Symbol::Style::TriangleLine);
				symbol->setRotationAngle(180);
				break;
			case 6: // minus
			case 8: // up
				symbol->setStyle(Symbol::Style::TriangleHalf);
				symbol->setRotationAngle(180);
				break;
			case 9: // right	TODO
				symbol->setStyle(Symbol::Style::TriangleLine);
				symbol->setRotationAngle(180);
				break;
			case 10: // down	TODO
				symbol->setStyle(Symbol::Style::TriangleHalf);
				symbol->setRotationAngle(180);
				break;
			}
			break;
		case 5: // Diamond
			symbol->setStyle(Symbol::Style::Diamond);
			switch (originCurve.symbolInterior) {
			case 0: // solid
			case 1: // open
			case 3: // hollow
				symbol->setStyle(Symbol::Style::Diamond);
				break;
			case 2: // dot
				symbol->setStyle(Symbol::Style::SquareDot);
				symbol->setRotationAngle(45);
				break;
			case 4: // plus
				symbol->setStyle(Symbol::Style::SquareX);
				symbol->setRotationAngle(45);
				break;
			case 5: // X
				symbol->setStyle(Symbol::Style::SquarePlus);
				symbol->setRotationAngle(45);
				break;
			case 6: // minus
			case 10: // down
				symbol->setStyle(Symbol::Style::SquareHalf);
				break;
			case 7: // pipe
				symbol->setStyle(Symbol::Style::SquareHalf);
				symbol->setRotationAngle(90);
				break;
			case 8: // up
				symbol->setStyle(Symbol::Style::SquareHalf);
				symbol->setRotationAngle(180);
				break;
			case 9: // right
				symbol->setStyle(Symbol::Style::SquareHalf);
				symbol->setRotationAngle(-90);
				break;
			case 11: // left
				symbol->setStyle(Symbol::Style::SquareHalf);
				symbol->setRotationAngle(90);
				break;
			}
			break;
		case 6: // Cross +
			symbol->setStyle(Symbol::Style::Cross);
			break;
		case 7: // Cross x
			symbol->setStyle(Symbol::Style::Cross);
			symbol->setRotationAngle(45);
			break;
		case 8: // Snow
			symbol->setStyle(Symbol::Style::XPlus);
			break;
		case 9: // Horizontal -
			symbol->setStyle(Symbol::Style::Line);
			symbol->setRotationAngle(90);
			break;
		case 10: // Vertical |
			symbol->setStyle(Symbol::Style::Line);
			break;
		case 15: // LTriangle
			switch (originCurve.symbolInterior) {
			case 0: // solid
			case 1: // open
			case 3: // hollow
			case 4: // plus	TODO
			case 5: // X	TODO
				symbol->setStyle(Symbol::Style::EquilateralTriangle);
				symbol->setRotationAngle(-90);
				break;
			case 2: // dot
				symbol->setStyle(Symbol::Style::TriangleDot);
				symbol->setRotationAngle(-90);
				break;
			case 7: // pipe
			case 11: // left
				symbol->setStyle(Symbol::Style::TriangleLine);
				symbol->setRotationAngle(-90);
				break;
			case 6: // minus
			case 8: // up
				symbol->setStyle(Symbol::Style::TriangleHalf);
				symbol->setRotationAngle(-90);
				break;
			case 9: // right	TODO
				symbol->setStyle(Symbol::Style::TriangleLine);
				symbol->setRotationAngle(-90);
				break;
			case 10: // down	TODO
				symbol->setStyle(Symbol::Style::TriangleHalf);
				symbol->setRotationAngle(-90);
				break;
			}
			break;
		case 16: // RTriangle
			switch (originCurve.symbolInterior) {
			case 0: // solid
			case 1: // open
			case 3: // hollow
			case 4: // plus	TODO
			case 5: // X	TODO
				symbol->setStyle(Symbol::Style::EquilateralTriangle);
				symbol->setRotationAngle(90);
				break;
			case 2: // dot
				symbol->setStyle(Symbol::Style::TriangleDot);
				symbol->setRotationAngle(90);
				break;
			case 7: // pipe
			case 11: // left
				symbol->setStyle(Symbol::Style::TriangleLine);
				symbol->setRotationAngle(90);
				break;
			case 6: // minus
			case 8: // up
				symbol->setStyle(Symbol::Style::TriangleHalf);
				symbol->setRotationAngle(90);
				break;
			case 9: // right	TODO
				symbol->setStyle(Symbol::Style::TriangleLine);
				symbol->setRotationAngle(90);
				break;
			case 10: // down	TODO
				symbol->setStyle(Symbol::Style::TriangleHalf);
				symbol->setRotationAngle(90);
				break;
			}
			break;
		case 17: // Hexagon
			symbol->setStyle(Symbol::Style::Hexagon);
			break;
		case 18: // Star
			symbol->setStyle(Symbol::Style::Star);
			break;
		case 19: // Pentagon
			symbol->setStyle(Symbol::Style::Pentagon);
			break;
		default:
			symbol->setStyle(Symbol::Style::NoSymbols);
		}
		// symbol size
		const double sizeScaleFactor = 0.5; // match size
		symbol->setSize(Worksheet::convertToSceneUnits(originCurve.symbolSize * sizeScaleFactor, Worksheet::Unit::Point));

		// symbol fill color
		QBrush brush = symbol->brush();
		if (originCurve.symbolFillColor.type == Origin::Color::ColorType::Automatic) {
			// DEBUG(Q_FUNC_INFO << ", AUTOMATIC fill color")
			//"automatic" color -> the color of the line, if available, is used, and black otherwise
			if (curve->lineType() != XYCurve::LineType::NoLine)
				brush.setColor(curve->line()->pen().color());
			else
				brush.setColor(Qt::black);
		} else
			brush.setColor(color(originCurve.symbolFillColor));
		if (originCurve.symbolInterior > 0 && originCurve.symbolInterior < 8) // unfilled styles
			brush.setStyle(Qt::NoBrush);
		symbol->setBrush(brush);

		// symbol border/edge color and width
		QPen pen = symbol->pen();
		if (originCurve.symbolColor.type == Origin::Color::ColorType::Automatic) {
			//"automatic" color -> the color of the line, if available, has to be used, black otherwise
			if (curve->lineType() != XYCurve::LineType::NoLine)
				pen.setColor(curve->line()->pen().color());
			else
				pen.setColor(Qt::black);
		} else
			pen.setColor(color(originCurve.symbolColor));

		// DEBUG(Q_FUNC_INFO << ", SYMBOL THICKNESS = " << (int)originCurve.symbolThickness)
		// DEBUG(Q_FUNC_INFO << ", BORDER THICKNESS = " << borderScaleFactor * originCurve.symbolThickness/100.*symbol->size()/scaleFactor)
		// border width (edge thickness in Origin) is given as percentage of the symbol radius
		const double borderScaleFactor = 5.; // match size
		pen.setWidthF(borderScaleFactor * originCurve.symbolThickness / 100. * symbol->size() / sizeScaleFactor);

		symbol->setPen(pen);

		// handle unsigned char pointOffset member
		// handle bool connectSymbols member
	} else {
		symbol->setStyle(Symbol::Style::NoSymbols);
	}

	// filling properties
	if (originCurve.fillArea) {
		// TODO: handle unsigned char fillAreaType;
		// with 'fillAreaType'=0x10 the area between the curve and the x-axis is filled
		// with 'fillAreaType'=0x14 the area included inside the curve is filled. First and last curve points are joined by a line to close the otherwise open
		// area. with 'fillAreaType'=0x12 the area excluded outside the curve is filled. The inverse of fillAreaType=0x14 is filled. At the moment we only
		// support the first type, so set it to XYCurve::FillingBelow
		curve->background()->setPosition(Background::Position::Below);
		auto* background = curve->background();

		if (originCurve.fillAreaPattern == 0) {
			background->setType(Background::Type::Color);
		} else {
			background->setType(Background::Type::Pattern);

			// map different patterns in originCurve.fillAreaPattern (has the values of Origin::FillPattern) to Qt::BrushStyle;
			switch (originCurve.fillAreaPattern) {
			case 0:
				background->setBrushStyle(Qt::NoBrush);
				break;
			case 1:
			case 2:
			case 3:
				background->setBrushStyle(Qt::BDiagPattern);
				break;
			case 4:
			case 5:
			case 6:
				background->setBrushStyle(Qt::FDiagPattern);
				break;
			case 7:
			case 8:
			case 9:
				background->setBrushStyle(Qt::DiagCrossPattern);
				break;
			case 10:
			case 11:
			case 12:
				background->setBrushStyle(Qt::HorPattern);
				break;
			case 13:
			case 14:
			case 15:
				background->setBrushStyle(Qt::VerPattern);
				break;
			case 16:
			case 17:
			case 18:
				background->setBrushStyle(Qt::CrossPattern);
				break;
			}
		}

		background->setFirstColor(color(originCurve.fillAreaColor));
		background->setOpacity(1 - originCurve.fillAreaTransparency / 255);

		// Color fillAreaPatternColor - color for the pattern lines, not supported
		// double fillAreaPatternWidth - width of the pattern lines, not supported
		// bool fillAreaWithLineTransparency - transparency of the pattern lines independent of the area transparency, not supported

		// TODO:
		// unsigned char fillAreaPatternBorderStyle;
		// Color fillAreaPatternBorderColor;
		// double fillAreaPatternBorderWidth;
		// The Border properties are used only in "Column/Bar" (histogram) plots. Those properties are:
		// fillAreaPatternBorderStyle   for the line style (use enum Origin::LineStyle here)
		// fillAreaPatternBorderColor   for the line color
		// fillAreaPatternBorderWidth   for the line width
	} else
		curve->background()->setPosition(Background::Position::No);
}

bool OriginProjectParser::loadNote(Note* note, bool preview) {
	DEBUG(Q_FUNC_INFO);
	// load note data
	const Origin::Note& originNote = m_originFile->note(findNoteByName(note->name()));

	if (preview)
		return true;

	note->setComment(QLatin1String(originNote.label.c_str()));
	note->setNote(QLatin1String(originNote.text.c_str()));

	return true;
}

// ##############################################################################
// ########################### Helper functions  ################################
// ##############################################################################
QDateTime OriginProjectParser::creationTime(tree<Origin::ProjectNode>::iterator it) const {
	// this logic seems to be correct only for the first node (project node). For other nodes the current time is returned.
	char time_str[21];
	strftime(time_str, sizeof(time_str), "%F %T", gmtime(&(*it).creationDate));
	return QDateTime::fromString(QLatin1String(time_str), Qt::ISODate);
}

QString OriginProjectParser::parseOriginText(const QString& str) const {
	DEBUG(Q_FUNC_INFO);
	QStringList lines = str.split(QLatin1Char('\n'));
	QString text;
	for (int i = 0; i < lines.size(); ++i) {
		if (i > 0)
			text.append(QLatin1String("<br>"));
		text.append(parseOriginTags(lines[i]));
	}

	DEBUG(Q_FUNC_INFO << ", PARSED TEXT = " << STDSTRING(text));

	return text;
}

QColor OriginProjectParser::color(Origin::Color color) const {
	switch (color.type) {
	case Origin::Color::ColorType::Regular:
		switch (color.regular) {
		case Origin::Color::Black:
			return QColor{Qt::black};
		case Origin::Color::Red:
			return QColor{Qt::red};
		case Origin::Color::Green:
			return QColor{Qt::green};
		case Origin::Color::Blue:
			return QColor{Qt::blue};
		case Origin::Color::Cyan:
			return QColor{Qt::cyan};
		case Origin::Color::Magenta:
			return QColor{Qt::magenta};
		case Origin::Color::Yellow:
			return QColor{Qt::yellow};
		case Origin::Color::DarkYellow:
			return QColor{Qt::darkYellow};
		case Origin::Color::Navy:
			return QColor{0, 0, 128};
		case Origin::Color::Purple:
			return QColor{128, 0, 128};
		case Origin::Color::Wine:
			return QColor{128, 0, 0};
		case Origin::Color::Olive:
			return QColor{0, 128, 0};
		case Origin::Color::DarkCyan:
			return QColor{Qt::darkCyan};
		case Origin::Color::Royal:
			return QColor{0, 0, 160};
		case Origin::Color::Orange:
			return QColor{255, 128, 0};
		case Origin::Color::Violet:
			return QColor{128, 0, 255};
		case Origin::Color::Pink:
			return QColor{255, 0, 128};
		case Origin::Color::White:
			return QColor{Qt::white};
		case Origin::Color::LightGray:
			return QColor{Qt::lightGray};
		case Origin::Color::Gray:
			return QColor{Qt::gray};
		case Origin::Color::LTYellow:
			return QColor{255, 0, 128};
		case Origin::Color::LTCyan:
			return QColor{128, 255, 255};
		case Origin::Color::LTMagenta:
			return QColor{255, 128, 255};
		case Origin::Color::DarkGray:
			return QColor{Qt::darkGray};
		case Origin::Color::SpecialV7Axis:
			return QColor{Qt::black};
		}
		break;
	case Origin::Color::ColorType::Custom:
		return QColor{color.custom[0], color.custom[1], color.custom[2]};
	case Origin::Color::ColorType::None:
	case Origin::Color::ColorType::Automatic:
	case Origin::Color::ColorType::Increment:
	case Origin::Color::ColorType::Indexing:
	case Origin::Color::ColorType::RGB:
	case Origin::Color::ColorType::Mapping:
		break;
	}

	return Qt::white;
}

Background::ColorStyle OriginProjectParser::backgroundColorStyle(Origin::ColorGradientDirection colorGradient) const {
	switch (colorGradient) {
	case Origin::ColorGradientDirection::NoGradient:
		return Background::ColorStyle::SingleColor;
	case Origin::ColorGradientDirection::TopLeft:
		return Background::ColorStyle::TopLeftDiagonalLinearGradient;
	case Origin::ColorGradientDirection::Left:
		return Background::ColorStyle::HorizontalLinearGradient;
	case Origin::ColorGradientDirection::BottomLeft:
		return Background::ColorStyle::BottomLeftDiagonalLinearGradient;
	case Origin::ColorGradientDirection::Top:
		return Background::ColorStyle::VerticalLinearGradient;
	case Origin::ColorGradientDirection::Center:
		return Background::ColorStyle::RadialGradient;
	case Origin::ColorGradientDirection::Bottom:
		return Background::ColorStyle::VerticalLinearGradient;
	case Origin::ColorGradientDirection::TopRight:
		return Background::ColorStyle::BottomLeftDiagonalLinearGradient;
	case Origin::ColorGradientDirection::Right:
		return Background::ColorStyle::HorizontalLinearGradient;
	case Origin::ColorGradientDirection::BottomRight:
		return Background::ColorStyle::TopLeftDiagonalLinearGradient;
	}

	return Background::ColorStyle::SingleColor;
}

QString strreverse(const QString& str) { // QString reversing
	QByteArray ba = str.toLocal8Bit();
	std::reverse(ba.begin(), ba.end());

	return QLatin1String(ba);
}

QList<QPair<QString, QString>> OriginProjectParser::charReplacementList() const {
	QList<QPair<QString, QString>> replacements;

	// TODO: probably missed some. Is there any generic method?
	replacements << qMakePair(QStringLiteral("ä"), QStringLiteral("&auml;"));
	replacements << qMakePair(QStringLiteral("ö"), QStringLiteral("&ouml;"));
	replacements << qMakePair(QStringLiteral("ü"), QStringLiteral("&uuml;"));
	replacements << qMakePair(QStringLiteral("Ä"), QStringLiteral("&Auml;"));
	replacements << qMakePair(QStringLiteral("Ö"), QStringLiteral("&Ouml;"));
	replacements << qMakePair(QStringLiteral("Ü"), QStringLiteral("&Uuml;"));
	replacements << qMakePair(QStringLiteral("ß"), QStringLiteral("&szlig;"));
	replacements << qMakePair(QStringLiteral("€"), QStringLiteral("&euro;"));
	replacements << qMakePair(QStringLiteral("£"), QStringLiteral("&pound;"));
	replacements << qMakePair(QStringLiteral("¥"), QStringLiteral("&yen;"));
	replacements << qMakePair(QStringLiteral("¤"), QStringLiteral("&curren;")); // krazy:exclude=spelling
	replacements << qMakePair(QStringLiteral("¦"), QStringLiteral("&brvbar;"));
	replacements << qMakePair(QStringLiteral("§"), QStringLiteral("&sect;"));
	replacements << qMakePair(QStringLiteral("µ"), QStringLiteral("&micro;"));
	replacements << qMakePair(QStringLiteral("¹"), QStringLiteral("&sup1;"));
	replacements << qMakePair(QStringLiteral("²"), QStringLiteral("&sup2;"));
	replacements << qMakePair(QStringLiteral("³"), QStringLiteral("&sup3;"));
	replacements << qMakePair(QStringLiteral("¶"), QStringLiteral("&para;"));
	replacements << qMakePair(QStringLiteral("ø"), QStringLiteral("&oslash;"));
	replacements << qMakePair(QStringLiteral("æ"), QStringLiteral("&aelig;"));
	replacements << qMakePair(QStringLiteral("ð"), QStringLiteral("&eth;"));
	replacements << qMakePair(QStringLiteral("ħ"), QStringLiteral("&hbar;"));
	replacements << qMakePair(QStringLiteral("ĸ"), QStringLiteral("&kappa;"));
	replacements << qMakePair(QStringLiteral("¢"), QStringLiteral("&cent;"));
	replacements << qMakePair(QStringLiteral("¼"), QStringLiteral("&frac14;"));
	replacements << qMakePair(QStringLiteral("½"), QStringLiteral("&frac12;"));
	replacements << qMakePair(QStringLiteral("¾"), QStringLiteral("&frac34;"));
	replacements << qMakePair(QStringLiteral("¬"), QStringLiteral("&not;"));
	replacements << qMakePair(QStringLiteral("©"), QStringLiteral("&copy;"));
	replacements << qMakePair(QStringLiteral("®"), QStringLiteral("&reg;"));
	replacements << qMakePair(QStringLiteral("ª"), QStringLiteral("&ordf;"));
	replacements << qMakePair(QStringLiteral("º"), QStringLiteral("&ordm;"));
	replacements << qMakePair(QStringLiteral("±"), QStringLiteral("&plusmn;"));
	replacements << qMakePair(QStringLiteral("¿"), QStringLiteral("&iquest;"));
	replacements << qMakePair(QStringLiteral("×"), QStringLiteral("&times;"));
	replacements << qMakePair(QStringLiteral("°"), QStringLiteral("&deg;"));
	replacements << qMakePair(QStringLiteral("«"), QStringLiteral("&laquo;"));
	replacements << qMakePair(QStringLiteral("»"), QStringLiteral("&raquo;"));
	replacements << qMakePair(QStringLiteral("¯"), QStringLiteral("&macr;"));
	replacements << qMakePair(QStringLiteral("¸"), QStringLiteral("&cedil;"));
	replacements << qMakePair(QStringLiteral("À"), QStringLiteral("&Agrave;"));
	replacements << qMakePair(QStringLiteral("Á"), QStringLiteral("&Aacute;"));
	replacements << qMakePair(QStringLiteral("Â"), QStringLiteral("&Acirc;"));
	replacements << qMakePair(QStringLiteral("Ã"), QStringLiteral("&Atilde;"));
	replacements << qMakePair(QStringLiteral("Å"), QStringLiteral("&Aring;"));
	replacements << qMakePair(QStringLiteral("Æ"), QStringLiteral("&AElig;"));
	replacements << qMakePair(QStringLiteral("Ç"), QStringLiteral("&Ccedil;"));
	replacements << qMakePair(QStringLiteral("È"), QStringLiteral("&Egrave;"));
	replacements << qMakePair(QStringLiteral("É"), QStringLiteral("&Eacute;"));
	replacements << qMakePair(QStringLiteral("Ê"), QStringLiteral("&Ecirc;"));
	replacements << qMakePair(QStringLiteral("Ë"), QStringLiteral("&Euml;"));
	replacements << qMakePair(QStringLiteral("Ì"), QStringLiteral("&Igrave;"));
	replacements << qMakePair(QStringLiteral("Í"), QStringLiteral("&Iacute;"));
	replacements << qMakePair(QStringLiteral("Î"), QStringLiteral("&Icirc;"));
	replacements << qMakePair(QStringLiteral("Ï"), QStringLiteral("&Iuml;"));
	replacements << qMakePair(QStringLiteral("Ð"), QStringLiteral("&ETH;"));
	replacements << qMakePair(QStringLiteral("Ñ"), QStringLiteral("&Ntilde;"));
	replacements << qMakePair(QStringLiteral("Ò"), QStringLiteral("&Ograve;"));
	replacements << qMakePair(QStringLiteral("Ó"), QStringLiteral("&Oacute;"));
	replacements << qMakePair(QStringLiteral("Ô"), QStringLiteral("&Ocirc;"));
	replacements << qMakePair(QStringLiteral("Õ"), QStringLiteral("&Otilde;"));
	replacements << qMakePair(QStringLiteral("Ù"), QStringLiteral("&Ugrave;"));
	replacements << qMakePair(QStringLiteral("Ú"), QStringLiteral("&Uacute;"));
	replacements << qMakePair(QStringLiteral("Û"), QStringLiteral("&Ucirc;"));
	replacements << qMakePair(QStringLiteral("Ý"), QStringLiteral("&Yacute;"));
	replacements << qMakePair(QStringLiteral("Þ"), QStringLiteral("&THORN;"));
	replacements << qMakePair(QStringLiteral("à"), QStringLiteral("&agrave;"));
	replacements << qMakePair(QStringLiteral("á"), QStringLiteral("&aacute;"));
	replacements << qMakePair(QStringLiteral("â"), QStringLiteral("&acirc;"));
	replacements << qMakePair(QStringLiteral("ã"), QStringLiteral("&atilde;"));
	replacements << qMakePair(QStringLiteral("å"), QStringLiteral("&aring;"));
	replacements << qMakePair(QStringLiteral("ç"), QStringLiteral("&ccedil;"));
	replacements << qMakePair(QStringLiteral("è"), QStringLiteral("&egrave;"));
	replacements << qMakePair(QStringLiteral("é"), QStringLiteral("&eacute;"));
	replacements << qMakePair(QStringLiteral("ê"), QStringLiteral("&ecirc;"));
	replacements << qMakePair(QStringLiteral("ë"), QStringLiteral("&euml;"));
	replacements << qMakePair(QStringLiteral("ì"), QStringLiteral("&igrave;"));
	replacements << qMakePair(QStringLiteral("í"), QStringLiteral("&iacute;"));
	replacements << qMakePair(QStringLiteral("î"), QStringLiteral("&icirc;"));
	replacements << qMakePair(QStringLiteral("ï"), QStringLiteral("&iuml;"));
	replacements << qMakePair(QStringLiteral("ñ"), QStringLiteral("&ntilde;"));
	replacements << qMakePair(QStringLiteral("ò"), QStringLiteral("&ograve;"));
	replacements << qMakePair(QStringLiteral("ó"), QStringLiteral("&oacute;"));
	replacements << qMakePair(QStringLiteral("ô"), QStringLiteral("&ocirc;"));
	replacements << qMakePair(QStringLiteral("õ"), QStringLiteral("&otilde;"));
	replacements << qMakePair(QStringLiteral("÷"), QStringLiteral("&divide;"));
	replacements << qMakePair(QStringLiteral("ù"), QStringLiteral("&ugrave;"));
	replacements << qMakePair(QStringLiteral("ú"), QStringLiteral("&uacute;"));
	replacements << qMakePair(QStringLiteral("û"), QStringLiteral("&ucirc;"));
	replacements << qMakePair(QStringLiteral("ý"), QStringLiteral("&yacute;"));
	replacements << qMakePair(QStringLiteral("þ"), QStringLiteral("&thorn;"));
	replacements << qMakePair(QStringLiteral("ÿ"), QStringLiteral("&yuml;"));
	replacements << qMakePair(QStringLiteral("Œ"), QStringLiteral("&#338;"));
	replacements << qMakePair(QStringLiteral("œ"), QStringLiteral("&#339;"));
	replacements << qMakePair(QStringLiteral("Š"), QStringLiteral("&#352;"));
	replacements << qMakePair(QStringLiteral("š"), QStringLiteral("&#353;"));
	replacements << qMakePair(QStringLiteral("Ÿ"), QStringLiteral("&#376;"));
	replacements << qMakePair(QStringLiteral("†"), QStringLiteral("&#8224;"));
	replacements << qMakePair(QStringLiteral("‡"), QStringLiteral("&#8225;"));
	replacements << qMakePair(QStringLiteral("…"), QStringLiteral("&#8230;"));
	replacements << qMakePair(QStringLiteral("‰"), QStringLiteral("&#8240;"));
	replacements << qMakePair(QStringLiteral("™"), QStringLiteral("&#8482;"));

	return replacements;
}

QString OriginProjectParser::replaceSpecialChars(const QString& text) const {
	QString t = text;
	for (const auto& r : charReplacementList())
		t.replace(r.first, r.second);
	return t;
}

/*!
 * helper function mapping the characters from the Symbol font (outdated and shouldn't be used for html)
 * to Unicode characters, s.a. https://www.alanwood.net/demos/symbol.html
 */
QString greekSymbol(const QString& symbol) {
	// characters in the Symbol-font
	static QStringList symbols{// letters
							   QStringLiteral("A"),
							   QStringLiteral("a"),
							   QStringLiteral("B"),
							   QStringLiteral("b"),
							   QStringLiteral("G"),
							   QStringLiteral("g"),
							   QStringLiteral("D"),
							   QStringLiteral("d"),
							   QStringLiteral("E"),
							   QStringLiteral("e"),
							   QStringLiteral("Z"),
							   QStringLiteral("z"),
							   QStringLiteral("H"),
							   QStringLiteral("h"),
							   QStringLiteral("Q"),
							   QStringLiteral("q"),
							   QStringLiteral("I"),
							   QStringLiteral("i"),
							   QStringLiteral("K"),
							   QStringLiteral("k"),
							   QStringLiteral("L"),
							   QStringLiteral("l"),
							   QStringLiteral("M"),
							   QStringLiteral("m"),
							   QStringLiteral("N"),
							   QStringLiteral("n"),
							   QStringLiteral("X"),
							   QStringLiteral("x"),
							   QStringLiteral("O"),
							   QStringLiteral("o"),
							   QStringLiteral("P"),
							   QStringLiteral("p"),
							   QStringLiteral("R"),
							   QStringLiteral("r"),
							   QStringLiteral("S"),
							   QStringLiteral("s"),
							   QStringLiteral("T"),
							   QStringLiteral("t"),
							   QStringLiteral("U"),
							   QStringLiteral("u"),
							   QStringLiteral("F"),
							   QStringLiteral("f"),
							   QStringLiteral("C"),
							   QStringLiteral("c"),
							   QStringLiteral("Y"),
							   QStringLiteral("y"),
							   QStringLiteral("W"),
							   QStringLiteral("w"),

							   // extra symbols
							   QStringLiteral("V"),
							   QStringLiteral("J"),
							   QStringLiteral("j"),
							   QStringLiteral("v"),
							   QStringLiteral("i")};

	// Unicode friendy codes for greek letters and symbols
	static QStringList unicodeFriendlyCode{// letters
										   QStringLiteral("&Alpha;"),
										   QStringLiteral("&alpha;"),
										   QStringLiteral("&Beta;"),
										   QStringLiteral("&beta;"),
										   QStringLiteral("&Gamma;"),
										   QStringLiteral("&gamma;"),
										   QStringLiteral("&Delta;"),
										   QStringLiteral("&delta;"),
										   QStringLiteral("&Epsilon;"),
										   QStringLiteral("&epsilon;"),
										   QStringLiteral("&Zeta;"),
										   QStringLiteral("&zeta;"),
										   QStringLiteral("&Eta;"),
										   QStringLiteral("&eta;"),
										   QStringLiteral("&Theta;"),
										   QStringLiteral("&theta;"),
										   QStringLiteral("&Iota;"),
										   QStringLiteral("Iota;"),
										   QStringLiteral("&Kappa;"),
										   QStringLiteral("&kappa;"),
										   QStringLiteral("&Lambda;"),
										   QStringLiteral("&lambda;"),
										   QStringLiteral("&Mu;"),
										   QStringLiteral("&mu;"),
										   QStringLiteral("&Nu;"),
										   QStringLiteral("&nu;"),
										   QStringLiteral("&Xi;"),
										   QStringLiteral("&xi;"),
										   QStringLiteral("&Omicron;"),
										   QStringLiteral("&omicron;"),
										   QStringLiteral("&Pi;"),
										   QStringLiteral("&pi;"),
										   QStringLiteral("&Rho;"),
										   QStringLiteral("&rho;"),
										   QStringLiteral("&Sigma;"),
										   QStringLiteral("&sigma;"),
										   QStringLiteral("&Tua;"),
										   QStringLiteral("&tau;"),
										   QStringLiteral("&Upsilon;"),
										   QStringLiteral("&upsilon;"),
										   QStringLiteral("&Phi;"),
										   QStringLiteral("&phi;"),
										   QStringLiteral("&Chi;"),
										   QStringLiteral("&chi;"),
										   QStringLiteral("&Psi;"),
										   QStringLiteral("&psi;"),
										   QStringLiteral("&Omega;"),
										   QStringLiteral("&omega;"),

										   // extra symbols
										   QStringLiteral("&sigmaf;"),
										   QStringLiteral("&thetasym;"),
										   QStringLiteral("&#981;;") /* phi symbol, no friendly code */,
										   QStringLiteral("&piv;"),
										   QStringLiteral("&upsih;")};

	int index = symbols.indexOf(symbol);
	if (index != -1)
		return unicodeFriendlyCode.at(index);
	else
		return QString();
}

/*!
 * converts the string with Origin's syntax for text formatting/highlighting
 * to a string in the richtext/html format supported by Qt.
 * For the supported syntax, see:
 * https://www.originlab.com/doc/LabTalk/ref/Label-cmd
 * https://www.originlab.com/doc/Origin-Help/TextOb-Prop-Text-tab
 * https://doc.qt.io/qt-5/richtext-html-subset.html
 */
QString OriginProjectParser::parseOriginTags(const QString& str) const {
	DEBUG(Q_FUNC_INFO << ", string = " << STDSTRING(str));
	QDEBUG("	UTF8 string: " << str.toUtf8());
	QString line = str;

	// replace %(...) tags
	// 	QRegExp rxcol("\\%\\(\\d+\\)");

	// replace \l(x) (plot legend tags) with \\c{x}, where x is a digit
	line.replace(QRegularExpression(QStringLiteral("\\\\\\s*l\\s*\\(\\s*(\\d+)\\s*\\)")), QStringLiteral("\\c{\\1}"));

	// replace umlauts etc.
	line = replaceSpecialChars(line);

	// replace tabs	(not really supported)
	line.replace(QLatin1Char('\t'), QLatin1String("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"));

	// In PCRE2 (which is what QRegularExpression uses) variable-length lookbehind is supposed to be
	// exprimental in Perl 5.30; which means it doesn't work at the moment, i.e. using a variable-length
	// negative lookbehind isn't valid syntax from QRegularExpression POV.
	// Ultimately we have to reverse the string and use a negative _lookahead_ instead.
	// The goal is to temporatily replace '(' and ')' that don't denote tags; this is so that we
	// can handle parenthesis that are inside the tag, e.g. '\b(bold (cf))', we want the '(cf)' part
	// to remain as is.
	const QRegularExpression nonTagsRe(QLatin1String(R"(\)([^)(]*)\((?!\s*([buigs\+\-]|\d{1,3}\s*[pc]|[\w ]+\s*:\s*f)\s*\\))"));
	QString linerev = strreverse(line);
	const QString lBracket = strreverse(QStringLiteral("&lbracket;"));
	const QString rBracket = strreverse(QStringLiteral("&rbracket;"));
	linerev.replace(nonTagsRe, rBracket + QStringLiteral("\\1") + lBracket);

	// change the line back to normal
	line = strreverse(linerev);

	// replace \-(...), \+(...), \b(...), \i(...), \u(...), \s(....), \g(...), \f:font(...),
	//  \c'number'(...), \p'size'(...) tags with equivalent supported HTML syntax
	const QRegularExpression tagsRe(QStringLiteral("\\\\\\s*([-+bgisu]|f:(\\w[\\w ]+)|[pc]\\s*(\\d+))\\s*\\(([^()]+?)\\)"));
	QRegularExpressionMatch rmatch;
	while (line.contains(tagsRe, &rmatch)) {
		QString rep;
		const QString tagText = rmatch.captured(4);
		const QString marker = rmatch.captured(1);
		if (marker.startsWith(QLatin1Char('-'))) {
			rep = QStringLiteral("<sub>%1</sub>").arg(tagText);
		} else if (marker.startsWith(QLatin1Char('+'))) {
			rep = QStringLiteral("<sup>%1</sup>").arg(tagText);
		} else if (marker.startsWith(QLatin1Char('b'))) {
			rep = QStringLiteral("<b>%1</b>").arg(tagText);
		} else if (marker.startsWith(QLatin1Char('g'))) { // greek symbols e.g. α φ
			rep = greekSymbol(tagText);
		} else if (marker.startsWith(QLatin1Char('i'))) {
			rep = QStringLiteral("<i>%1</i>").arg(tagText);
		} else if (marker.startsWith(QLatin1Char('s'))) {
			rep = QStringLiteral("<s>%1</s>").arg(tagText);
		} else if (marker.startsWith(QLatin1Char('u'))) {
			rep = QStringLiteral("<u>%1</u>").arg(tagText);
		} else if (marker.startsWith(QLatin1Char('f'))) {
			rep = QStringLiteral("<font face=\"%1\">%2</font>").arg(rmatch.captured(2).trimmed(), tagText);
		} else if (marker.startsWith(QLatin1Char('p'))) { // e.g. \p200(...), means use font-size 200%
			rep = QStringLiteral("<span style=\"font-size: %1%\">%2</span>").arg(rmatch.captured(3), tagText);
		} else if (marker.startsWith(QLatin1Char('c'))) {
			// e.g. \c12(...), set the text color to the corresponding color from
			// the color drop-down list in OriginLab
			const int colorIndex = rmatch.captured(3).toInt();
			Origin::Color c;
			c.type = Origin::Color::ColorType::Regular;
			c.regular = colorIndex <= 23 ? static_cast<Origin::Color::RegularColor>(colorIndex) : Origin::Color::RegularColor::Black;
			QColor color = OriginProjectParser::color(c);
			rep = QStringLiteral("<span style=\"color: %1\">%2</span>").arg(color.name(), tagText);
		}
		line.replace(rmatch.capturedStart(0), rmatch.capturedLength(0), rep);
	}

	// put non-tag '(' and ')' back in their places
	line.replace(QLatin1String("&lbracket;"), QLatin1String("("));
	line.replace(QLatin1String("&rbracket;"), QLatin1String(")"));

	// special characters
	line.replace(QRegularExpression(QStringLiteral("\\\\\\((\\d+)\\)")), QLatin1String("&#\\1;"));

	DEBUG("	result: " << STDSTRING(line));

	return line;
}
