/*
	File                 : SearchReplaceWidget.cpp
	Project              : LabPlot
	Description          : Search&Replace widget for the spreadsheet
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2023 Alexander Semke <alexander.semke@web.de>

	SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "SearchReplaceWidget.h"
#include "backend/spreadsheet/Spreadsheet.h"
#include "backend/spreadsheet/SpreadsheetModel.h"
#include "commonfrontend/spreadsheet/SpreadsheetView.h"
#include "kdefrontend/GuiTools.h"

#include <QLineEdit>
#include <QMenu>
#include <QRadioButton>
#include <QStack>

enum SearchMode {
	// NOTE: Concrete values are important here to work with the combobox index!
	MODE_PLAIN_TEXT = 0,
	MODE_WHOLE_WORDS = 1,
	MODE_ESCAPE_SEQUENCES = 2,
	MODE_REGEX = 3
};

class AddMenuManager {
private:
	QVector<QString> m_insertBefore;
	QVector<QString> m_insertAfter;
	QSet<QAction*> m_actionPointers;
	uint m_indexWalker{0};
	QMenu* m_menu{nullptr};

public:
	AddMenuManager(QMenu* parent, int expectedItemCount)
		: m_insertBefore(QVector<QString>(expectedItemCount))
		, m_insertAfter(QVector<QString>(expectedItemCount)) {
		Q_ASSERT(parent != nullptr);

		m_menu = parent->addMenu(i18n("Add..."));
		if (!m_menu)
			return;

		m_menu->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
	}

	void enableMenu(bool enabled) {
		if (m_menu == nullptr)
			return;

		m_menu->setEnabled(enabled);
	}

	void addEntry(const QString& before,
				  const QString& after,
				  const QString& description,
				  const QString& realBefore = QString(),
				  const QString& realAfter = QString()) {
		if (!m_menu)
			return;

		auto* const action = m_menu->addAction(before + after + QLatin1Char('\t') + description);
		m_insertBefore[m_indexWalker] = QString(realBefore.isEmpty() ? before : realBefore);
		m_insertAfter[m_indexWalker] = QString(realAfter.isEmpty() ? after : realAfter);
		action->setData(QVariant(m_indexWalker++));
		m_actionPointers.insert(action);
	}

	void addSeparator() {
		if (!m_menu)
			return;

		m_menu->addSeparator();
	}

	void handle(QAction* action, QLineEdit* lineEdit) {
		if (!m_actionPointers.contains(action))
			return;

		const int cursorPos = lineEdit->cursorPosition();
		const int index = action->data().toUInt();
		const QString& before = m_insertBefore[index];
		const QString& after = m_insertAfter[index];
		lineEdit->insert(before + after);
		lineEdit->setCursorPosition(cursorPos + before.count());
		lineEdit->setFocus();
	}
};

struct ParInfo {
	int openIndex;
	bool capturing;
	int captureNumber; // 1..9
};

SearchReplaceWidget::SearchReplaceWidget(Spreadsheet* spreadsheet, QWidget* parent)
	: QWidget(parent)
	, m_spreadsheet(spreadsheet) {
	m_view = static_cast<SpreadsheetView*>(spreadsheet->view());

	auto* layout = new QVBoxLayout(this);
	this->setLayout(layout);
	QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	this->setSizePolicy(sizePolicy);
}

SearchReplaceWidget::~SearchReplaceWidget() {
}

void SearchReplaceWidget::setReplaceEnabled(bool enabled) {
	m_replaceEnabled = !enabled;
	switchFindReplace();
}

void SearchReplaceWidget::setFocus() {
	if (m_replaceEnabled)
		uiSearchReplace.leValueText->setFocus();
	else
		uiSearch.cbFind->setFocus();
}

void SearchReplaceWidget::clear() {
}

void SearchReplaceWidget::initSearchWidget() {
	m_searchWidget = new QWidget(this);
	uiSearch.setupUi(m_searchWidget);
	static_cast<QVBoxLayout*>(layout())->insertWidget(0, m_searchWidget);

	connect(uiSearch.cbFind->lineEdit(), &QLineEdit::returnPressed, this, [=]() {
		findNext(true);
	});
	connect(uiSearch.cbFind->lineEdit(), &QLineEdit::textChanged, this, [=]() {
		findNext(false);
	});

	connect(uiSearch.tbFindNext, &QToolButton::clicked, this, [=]() {
		findNext(true);
	});
	connect(uiSearch.tbFindPrev, &QToolButton::clicked, this, [=]() {
		findPrevious(true);
	});
	connect(uiSearch.tbMatchCase, &QToolButton::toggled, this, &SearchReplaceWidget::matchCaseToggled);

	connect(uiSearch.tbSwitchFindReplace, &QToolButton::clicked, this, &SearchReplaceWidget::switchFindReplace);
	connect(uiSearch.bCancel, &QPushButton::clicked, this, &SearchReplaceWidget::cancel);
}

void SearchReplaceWidget::initSearchReplaceWidget() {
	m_searchReplaceWidget = new QWidget(this);
	uiSearchReplace.setupUi(m_searchReplaceWidget);
	static_cast<QVBoxLayout*>(layout())->insertWidget(1, m_searchReplaceWidget);

	uiSearchReplace.cbOperatorText->addItem(i18n("Equal To"), int(OperatorText::EqualTo));
	uiSearchReplace.cbOperatorText->addItem(i18n("Not Equal To"), int(OperatorText::NotEqualTo));
	uiSearchReplace.cbOperatorText->addItem(i18n("Starts With"), int(OperatorText::StartsWith));
	uiSearchReplace.cbOperatorText->addItem(i18n("Ends With"), int(OperatorText::EndsWith));
	uiSearchReplace.cbOperatorText->addItem(i18n("Contains"), int(OperatorText::Contain));
	uiSearchReplace.cbOperatorText->addItem(i18n("Does Not Contain"), int(OperatorText::NotContain));
	uiSearchReplace.cbOperatorText->insertSeparator(6);
	uiSearchReplace.cbOperatorText->addItem(i18n("Regular Expression"), int(OperatorText::RegEx));

	uiSearchReplace.cbOperator->addItem(i18n("Equal to"), int(Operator::EqualTo));
	uiSearchReplace.cbOperator->addItem(i18n("Not Equal to"), int(Operator::NotEqualTo));
	uiSearchReplace.cbOperator->addItem(i18n("Between (Incl. End Points)"), int(Operator::BetweenIncl));
	uiSearchReplace.cbOperator->addItem(i18n("Between (Excl. End Points)"), int(Operator::BetweenExcl));
	uiSearchReplace.cbOperator->addItem(i18n("Greater than"), int(Operator::GreaterThan));
	uiSearchReplace.cbOperator->addItem(i18n("Greater than or Equal to"), int(Operator::GreaterThanEqualTo));
	uiSearchReplace.cbOperator->addItem(i18n("Less than"), int(Operator::LessThan));
	uiSearchReplace.cbOperator->addItem(i18n("Less than or Equal to"), int(Operator::LessThanEqualTo));

	uiSearchReplace.cbOperatorDateTime->addItem(i18n("Equal to"), int(Operator::EqualTo));
	uiSearchReplace.cbOperatorDateTime->addItem(i18n("Not Equal to"), int(Operator::NotEqualTo));
	uiSearchReplace.cbOperatorDateTime->addItem(i18n("Between (Incl. End Points)"), int(Operator::BetweenIncl));
	uiSearchReplace.cbOperatorDateTime->addItem(i18n("Between (Excl. End Points)"), int(Operator::BetweenExcl));
	uiSearchReplace.cbOperatorDateTime->addItem(i18n("Greater than"), int(Operator::GreaterThan));
	uiSearchReplace.cbOperatorDateTime->addItem(i18n("Greater than or Equal to"), int(Operator::GreaterThanEqualTo));
	uiSearchReplace.cbOperatorDateTime->addItem(i18n("Less than"), int(Operator::LessThan));
	uiSearchReplace.cbOperatorDateTime->addItem(i18n("Less than or Equal to"), int(Operator::LessThanEqualTo));

	uiSearchReplace.leValue1->setValidator(new QDoubleValidator(uiSearchReplace.leValue1));
	uiSearchReplace.leValue2->setValidator(new QDoubleValidator(uiSearchReplace.leValue2));

	// connections
	connect(uiSearchReplace.cbDataType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SearchReplaceWidget::dataTypeChanged);

	connect(uiSearchReplace.cbOperator, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SearchReplaceWidget::operatorChanged);
	connect(uiSearchReplace.cbOperatorText, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=]() {
		findNext(true);
	});
	connect(uiSearchReplace.cbOperatorDateTime, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SearchReplaceWidget::operatorDateTimeChanged);

	connect(uiSearchReplace.leValue1, &QLineEdit::returnPressed, this, [=]() {
		findNext(true);
	});
	connect(uiSearchReplace.leValue2, &QLineEdit::returnPressed, this, [=]() {
		findNext(true);
	});
	connect(uiSearchReplace.leValue1, &QLineEdit::textChanged, this, [=]() {
		findNext(false);
	});
	connect(uiSearchReplace.leValue2, &QLineEdit::textChanged, this, [=]() {
		findNext(false);
	});

	connect(uiSearchReplace.leValueText, &QLineEdit::returnPressed, this, [=]() {
		findNext(true);
	});
	connect(uiSearchReplace.leValueText, &QLineEdit::textChanged, this, [=]() {
		findNext(false);
	});

	connect(uiSearchReplace.dteValue1, &UTCDateTimeEdit::mSecsSinceEpochUTCChanged, this, [=]() {
		findNext(false);
	});
	connect(uiSearchReplace.dteValue2, &UTCDateTimeEdit::mSecsSinceEpochUTCChanged, this, [=]() {
		findNext(false);
	});

	connect(uiSearchReplace.tbFindNext, &QToolButton::clicked, this, [=]() {
		findNext(true);
	});
	connect(uiSearchReplace.tbFindPrev, &QToolButton::clicked, this, [=]() {
		findPrevious(true);
	});
	connect(uiSearchReplace.bFindAll, &QPushButton::clicked, this, &SearchReplaceWidget::findAll);

	connect(uiSearchReplace.bReplaceNext, &QPushButton::clicked, this, &SearchReplaceWidget::replaceNext);
	connect(uiSearchReplace.bReplaceAll, &QPushButton::clicked, this, &SearchReplaceWidget::replaceAll);
	connect(uiSearchReplace.tbMatchCase, &QToolButton::toggled, this, &SearchReplaceWidget::matchCaseToggled);

	connect(uiSearchReplace.tbSwitchFindReplace, &QToolButton::clicked, this, &SearchReplaceWidget::switchFindReplace);
	connect(uiSearchReplace.bCancel, &QPushButton::clicked, this, &SearchReplaceWidget::cancel);

	// custom context menus for LineEdit in ComboBox
	uiSearchReplace.leValueText->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(uiSearchReplace.leValueText,
			&QComboBox::customContextMenuRequested,
			this,
			QOverload<const QPoint&>::of(&SearchReplaceWidget::findContextMenuRequest));

	uiSearchReplace.cbReplace->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(uiSearchReplace.cbReplace,
			&QComboBox::customContextMenuRequested,
			this,
			QOverload<const QPoint&>::of(&SearchReplaceWidget::replaceContextMenuRequest));

	// read saved settings
	// TODO:

	dataTypeChanged(uiSearchReplace.cbDataType->currentIndex());
	operatorChanged(uiSearchReplace.cbOperator->currentIndex());
	operatorDateTimeChanged(uiSearchReplace.cbOperatorDateTime->currentIndex());
}

void SearchReplaceWidget::showEvent(QShowEvent* event) {
	QWidget::showEvent(event);
}

//**********************************************************
//************************* SLOTs **************************
//**********************************************************
void SearchReplaceWidget::findAll() {
	QString text;
	QLineEdit* lineEdit;
	if (m_replaceEnabled)
		lineEdit = uiSearchReplace.leValueText;
	else
		lineEdit = uiSearch.cbFind->lineEdit();

	text = lineEdit->text();

	m_spreadsheet->model()->setSearchText(text);
	m_view->setFocus(); // set the focus so the table gets updated with the highlighted found entries
	lineEdit->setFocus(); // set the focus back to the line edit so we can continue typing
}

void SearchReplaceWidget::replaceNext() {
}

void SearchReplaceWidget::replaceAll() {
}

void SearchReplaceWidget::cancel() {
	m_spreadsheet->model()->setSearchText(QString()); // clear the global search text that was potentialy set during "find all"
	close();
}

void SearchReplaceWidget::findContextMenuRequest(const QPoint& pos) {
	showExtendedContextMenu(false /* replace */, pos);
}

void SearchReplaceWidget::replaceContextMenuRequest(const QPoint& pos) {
	showExtendedContextMenu(true /* replace */, pos);
}

void SearchReplaceWidget::dataTypeChanged(int index) {
	if (index == 0) { // text
		uiSearchReplace.frameNumeric->hide();
		uiSearchReplace.frameText->show();
		uiSearchReplace.frameDateTime->hide();
		uiSearchReplace.tbMatchCase->show();
	} else if (index == 1) { // numeric
		uiSearchReplace.frameNumeric->show();
		uiSearchReplace.frameText->hide();
		uiSearchReplace.frameDateTime->hide();
		uiSearchReplace.tbMatchCase->hide();
	} else { // datetime
		uiSearchReplace.frameNumeric->hide();
		uiSearchReplace.frameText->hide();
		uiSearchReplace.frameDateTime->show();
		uiSearchReplace.tbMatchCase->hide();
	}
}

void SearchReplaceWidget::operatorChanged(int /* index */) const {
	const auto op = static_cast<Operator>(uiSearchReplace.cbOperator->currentData().toInt());
	bool visible = (op == Operator::BetweenIncl) || (op == Operator::BetweenExcl);

	uiSearchReplace.lMin->setVisible(visible);
	uiSearchReplace.lMax->setVisible(visible);
	uiSearchReplace.lAnd->setVisible(visible);
	uiSearchReplace.leValue2->setVisible(visible);
}

void SearchReplaceWidget::operatorDateTimeChanged(int /* index */) const {
	const auto op = static_cast<Operator>(uiSearchReplace.cbOperatorDateTime->currentData().toInt());
	bool visible = (op == Operator::BetweenIncl) || (op == Operator::BetweenExcl);

	uiSearchReplace.lMinDateTime->setVisible(visible);
	uiSearchReplace.lMaxDateTime->setVisible(visible);
	uiSearchReplace.lAndDateTime->setVisible(visible);
	uiSearchReplace.dteValue2->setVisible(visible);
}

void SearchReplaceWidget::modeChanged() {
	findNext(false);
}

void SearchReplaceWidget::matchCaseToggled() {
	findNext(false);
}

// settings
void SearchReplaceWidget::switchFindReplace() {
	m_replaceEnabled = !m_replaceEnabled;
	if (m_replaceEnabled) { // show the find&replace widget
		if (!m_searchReplaceWidget)
			initSearchReplaceWidget();

		m_searchReplaceWidget->show();

		// TODO
		uiSearchReplace.cbDataType->setMinimumWidth(uiSearchReplace.cbOperator->width());
		uiSearchReplace.cbOrder->setMinimumWidth(uiSearchReplace.cbOperator->width());
		uiSearchReplace.cbOperatorText->setMinimumWidth(uiSearchReplace.cbOperator->width());
		uiSearchReplace.cbOperatorDateTime->setMinimumWidth(uiSearchReplace.cbOperator->width());

		if (m_searchWidget)
			m_searchWidget->hide();
	} else { // show the find widget
		if (!m_searchWidget)
			initSearchWidget();

		m_searchWidget->show();

		if (m_searchReplaceWidget)
			m_searchReplaceWidget->hide();
	}
}

//**********************************************************
//****  data type specific functions implementing find  ***
//**********************************************************
bool SearchReplaceWidget::findNext(bool proceed) {
	// QLineEdit* lineEdit;
	// if (m_replaceEnabled)
	// 	lineEdit = uiSearchReplace.leValueText;
	// else
	// 	lineEdit = uiSearch.cbFind->lineEdit();
	//
	// // const QString& text = lineEdit->text();
	// //
	// // if (!text.isEmpty()) {
	// 	bool rc = findPrevImpl(proceed);
	// 	GuiTools::highlight(lineEdit, !rc);
	// // } else
	// // 	GuiTools::highlight(lineEdit, false);

	// settings
	const auto type = static_cast<DataType>(uiSearchReplace.cbDataType->currentIndex());
	const auto opText = static_cast<OperatorText>(uiSearchReplace.cbOperatorText->currentData().toInt());
	const auto opNumeric = static_cast<Operator>(uiSearchReplace.cbOperator->currentData().toInt());
	const auto opDateTime = static_cast<Operator>(uiSearchReplace.cbOperatorDateTime->currentData().toInt());
	const auto cs = uiSearchReplace.tbMatchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
	const bool columnMajor = (uiSearchReplace.cbOrder->currentIndex() == 0);

	// spreadsheet size and the start cell
	const int colCount = m_spreadsheet->columnCount();
	const int rowCount = m_spreadsheet->rowCount();
	int curRow = m_view->firstSelectedRow();
	int curCol = m_view->firstSelectedColumn();

	if (columnMajor && proceed)
		++curRow;

	if (!columnMajor && proceed)
		++curCol;

	// search pattern(s)
	QString pattern1;
	QString pattern2;
	switch (type) {
	case DataType::Text:
		pattern1 = uiSearchReplace.leValueText->text();
		break;
	case DataType::Numeric:
		pattern1 = uiSearchReplace.leValue1->text();
		pattern2 = uiSearchReplace.leValue2->text();
		break;
	case DataType::DateTime:
		pattern1 = uiSearchReplace.dteValue1->text();
		pattern1 = uiSearchReplace.dteValue2->text();
		break;
	}

	// all settings are determined -> search the next cell matching the specified pattern(s)
	const auto& columns = m_spreadsheet->children<Column>();
	bool startCol = true;
	bool startRow = true;
	bool match = false;

	if (columnMajor) {
		for (int col = 0; col < colCount; ++col) {
			if (startCol && col < curCol)
				continue;

			auto* column = columns.at(col);
			if (!checkColumnType(column, type))
				continue;

			for (int row = 0; row < rowCount; ++row) {
				if (startRow && row < curRow)
					continue;

				match = checkColumnRow(column, type, row, opText, opNumeric, opDateTime, pattern1, pattern2, cs);
				if (match) {
					m_view->goToCell(row, col);
					return true;
				}

				startRow = false;
			}

			startCol = false;
		}
	} else { // row-major
		for (int row = 0; row < rowCount; ++row) {
			if (startRow && row > curRow)
				continue;

			for (int col = 0; col < colCount; ++col) {
				if (startCol && col < curCol)
					continue;

				auto* column = columns.at(col);
				if (!checkColumnType(column, type))
					continue;

				match = checkColumnRow(column, type, row, opText, opNumeric, opDateTime, pattern1, pattern2, cs);
				if (match) {
					m_view->goToCell(row, col);
					return true;
				}

				startCol = false;
			}

			startRow = false;
		}
	}

	return false;
}

bool SearchReplaceWidget::findPrevious(bool proceed) {
	// settings
	const auto type = static_cast<DataType>(uiSearchReplace.cbDataType->currentIndex());
	const auto opText = static_cast<OperatorText>(uiSearchReplace.cbOperatorText->currentData().toInt());
	const auto opNumeric = static_cast<Operator>(uiSearchReplace.cbOperator->currentData().toInt());
	const auto opDateTime = static_cast<Operator>(uiSearchReplace.cbOperatorDateTime->currentData().toInt());
	const auto cs = uiSearchReplace.tbMatchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
	const bool columnMajor = (uiSearchReplace.cbOrder->currentIndex() == 0);

	// spreadsheet size and the start cell
	const int colCount = m_spreadsheet->columnCount();
	const int rowCount = m_spreadsheet->rowCount();
	int curRow = m_view->firstSelectedRow();
	int curCol = m_view->firstSelectedColumn();

	if (columnMajor && proceed)
		++curRow;

	if (!columnMajor && proceed)
		++curCol;

	// search pattern(s)
	QString pattern1;
	QString pattern2;
	switch (type) {
	case DataType::Text:
		pattern1 = uiSearchReplace.leValueText->text();
		break;
	case DataType::Numeric:
		pattern1 = uiSearchReplace.leValue1->text();
		pattern2 = uiSearchReplace.leValue2->text();
		break;
	case DataType::DateTime:
		pattern1 = uiSearchReplace.dteValue1->text();
		pattern1 = uiSearchReplace.dteValue2->text();
		break;
	}

	// all settings are determined -> search the next cell matching the specified pattern(s)
	const auto& columns = m_spreadsheet->children<Column>();
	bool startCol = true;
	bool startRow = true;
	bool match = false;

	if (columnMajor) {
		for (int col = colCount; col >= 0; --col) {
			if (startCol && col > curCol)
				continue;

			auto* column = columns.at(col);
			if (!checkColumnType(column, type))
				continue;

			for (int row = rowCount; row >= 0; --row) {
				if (startRow && row > curRow)
					continue;

				match = checkColumnRow(column, type, row, opText, opNumeric, opDateTime, pattern1, pattern2, cs);
				if (match) {
					m_view->goToCell(row, col);
					return true;
				}

				startRow = false;
			}

			startCol = false;
		}
	} else { // row-major
		for (int row = rowCount; row >= 0; --row) {
			if (startRow && row > curRow)
				continue;

			for (int col = curCol; col >= 0; --col) {
				if (startCol && col > curCol)
					continue;

				auto* column = columns.at(col);
				if (!checkColumnType(column, type))
					continue;

				match = checkColumnRow(column, type, row, opText, opNumeric, opDateTime, pattern1, pattern2, cs);
				if (match) {
					m_view->goToCell(row, col);
					return true;
				}

				startCol = false;
			}

			startRow = false;
		}
	}

	return false;
}

bool SearchReplaceWidget::checkColumnType(Column* column, DataType type) {
	bool valid = false;

	switch (type) {
	case DataType::Text:
		valid = (column->columnMode() == AbstractColumn::ColumnMode::Text);
		break;
	case DataType::Numeric:
		valid = column->isNumeric();
		break;
	case DataType::DateTime:
		valid = (column->columnMode() == AbstractColumn::ColumnMode::DateTime);
		break;
	}

	return valid;
}

bool SearchReplaceWidget::checkColumnRow(Column* column,
										 DataType type,
										 int row,
										 OperatorText opText,
										 Operator opNumeric,
										 Operator opDateTime,
										 const QString& pattern1,
										 const QString pattern2,
										 Qt::CaseSensitivity cs) {
	bool match = false;
	switch (type) {
	case DataType::Text:
		match = checkCellText(column->textAt(row), pattern1, opText, cs);
		break;
	case DataType::Numeric:
		match = checkCellNumeric(column->valueAt(row), pattern1, pattern2, opNumeric);
		break;
	case DataType::DateTime:
		match = checkCellDateTime(column->dateTimeAt(row), pattern1, pattern2, opDateTime);
		break;
	}

	// qDebug()<<"checkColumnRow " << column->name() << "  " << row << "  " << match;
	return match;
}

bool SearchReplaceWidget::checkCellText(const QString& cellText, const QString& pattern, OperatorText op, Qt::CaseSensitivity cs) {
	bool match = false;

	switch (op) {
	case OperatorText::EqualTo: {
		match = (cellText.compare(pattern, cs) == 0);
		break;
	}
	case OperatorText::NotEqualTo: {
		match = (cellText.compare(pattern, cs) != 0);
		break;
	}
	case OperatorText::StartsWith: {
		match = cellText.startsWith(pattern, cs);
		break;
	}
	case OperatorText::EndsWith: {
		match = cellText.endsWith(pattern, cs);
		break;
	}
	case OperatorText::Contain: {
		match = (cellText.indexOf(pattern, cs) != -1);
		break;
	}
	case OperatorText::NotContain: {
		match = (cellText.indexOf(pattern, cs) == -1);
		break;
	}
	case OperatorText::RegEx: {
		QRegularExpression re(pattern);
		if (cs == Qt::CaseInsensitive)
			re.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
		match = re.match(cellText).hasMatch();
		break;
	}
	}

	return match;
}

bool SearchReplaceWidget::checkCellNumeric(double cellValue, const QString& pattern1, const QString& pattern2, Operator op) {
	bool match = false;
	bool ok;
	const auto numberLocale = QLocale();

	const double patternValue1 = numberLocale.toDouble(pattern1, &ok);
	if (!ok)
		return false;

	const double patternValue2 = numberLocale.toDouble(pattern2, &ok);
	if (!ok)
		return false;

	switch (op) {
	case Operator::EqualTo: {
		match = (cellValue == patternValue1);
		break;
	}
	case Operator::NotEqualTo: {
		match = (cellValue != patternValue1);
		break;
	}
	case Operator::BetweenIncl: {
		match = (cellValue >= patternValue1 && cellValue <= patternValue2);
		break;
	}
	case Operator::BetweenExcl: {
		match = (cellValue > patternValue1 && cellValue < patternValue2);
		break;
	}
	case Operator::GreaterThan: {
		match = (cellValue > patternValue1);
		break;
	}
	case Operator::GreaterThanEqualTo: {
		match = (cellValue >= patternValue1);
		break;
	}
	case Operator::LessThan: {
		match = (cellValue < patternValue1);
		break;
	}
	case Operator::LessThanEqualTo: {
		match = (cellValue <= patternValue1);
		break;
	}
	}

	return match;
}

bool SearchReplaceWidget::checkCellDateTime(const QDateTime& cellValueDateTime, const QString& pattern1, const QString& pattern2, Operator op) {
	bool match = false;
	bool ok;
	const auto numberLocale = QLocale();

	const double patternValue1 = numberLocale.toDouble(pattern1, &ok);
	if (!ok)
		return false;

	const double patternValue2 = numberLocale.toDouble(pattern2, &ok);
	if (!ok)
		return false;

	double cellValue = cellValueDateTime.toMSecsSinceEpoch();

	switch (op) {
	case Operator::EqualTo: {
		match = (cellValue == patternValue1);
		break;
	}
	case Operator::NotEqualTo: {
		match = (cellValue != patternValue1);
		break;
	}
	case Operator::BetweenIncl: {
		match = (cellValue >= patternValue1 && cellValue <= patternValue2);
		break;
	}
	case Operator::BetweenExcl: {
		match = (cellValue > patternValue1 && cellValue < patternValue2);
		break;
	}
	case Operator::GreaterThan: {
		match = (cellValue > patternValue1);
		break;
	}
	case Operator::GreaterThanEqualTo: {
		match = (cellValue >= patternValue1);
		break;
	}
	case Operator::LessThan: {
		match = (cellValue < patternValue1);
		break;
	}
	case Operator::LessThanEqualTo: {
		match = (cellValue <= patternValue1);
		break;
	}
	}

	return match;
}

//**********************************************************
//**** SLOTs for changes triggered in CustomPointDock ******
//**********************************************************
void SearchReplaceWidget::showExtendedContextMenu(bool replace, const QPoint& pos) {
	// Make original menu
	QLineEdit* lineEdit;
	if (replace)
		lineEdit = uiSearchReplace.cbReplace->lineEdit();
	else
		lineEdit = uiSearchReplace.leValueText;

	auto* const contextMenu = lineEdit->createStandardContextMenu();
	if (!contextMenu)
		return;

	bool extendMenu = false;
	bool regexMode = false;
	if (uiSearchReplace.cbDataType->currentIndex() == 0) {
		// TODO
		extendMenu = true;
		regexMode = true;
		// switch (uiSearchReplace.cbMode->currentIndex()) {
		// case MODE_REGEX:
		// 	regexMode = true;
		// 	// FALLTHROUGH
		// case MODE_ESCAPE_SEQUENCES:
		// 	extendMenu = true;
		// 	break;
		// default:
		// 	break;
		// }
	}

	AddMenuManager addMenuManager(contextMenu, 37);
	if (!extendMenu)
		addMenuManager.enableMenu(extendMenu);
	else {
		// Build menu
		if (!replace) {
			if (regexMode) {
				addMenuManager.addEntry(QStringLiteral("^"), QString(), i18n("Beginning of line"));
				addMenuManager.addEntry(QStringLiteral("$"), QString(), i18n("End of line"));
				addMenuManager.addSeparator();
				addMenuManager.addEntry(QStringLiteral("."), QString(), i18n("Match any character excluding new line (by default)"));
				addMenuManager.addEntry(QStringLiteral("+"), QString(), i18n("One or more occurrences"));
				addMenuManager.addEntry(QStringLiteral("*"), QString(), i18n("Zero or more occurrences"));
				addMenuManager.addEntry(QStringLiteral("?"), QString(), i18n("Zero or one occurrences"));
				addMenuManager.addEntry(QStringLiteral("{a"),
										QStringLiteral(",b}"),
										i18n("<a> through <b> occurrences"),
										QStringLiteral("{"),
										QStringLiteral(",}"));

				addMenuManager.addSeparator();
				addMenuManager.addSeparator();
				addMenuManager.addEntry(QStringLiteral("("), QStringLiteral(")"), i18n("Group, capturing"));
				addMenuManager.addEntry(QStringLiteral("|"), QString(), i18n("Or"));
				addMenuManager.addEntry(QStringLiteral("["), QStringLiteral("]"), i18n("Set of characters"));
				addMenuManager.addEntry(QStringLiteral("[^"), QStringLiteral("]"), i18n("Negative set of characters"));
				addMenuManager.addSeparator();
			}
		} else {
			addMenuManager.addEntry(QStringLiteral("\\0"), QString(), i18n("Whole match reference"));
			addMenuManager.addSeparator();
			if (regexMode) {
				const QString pattern = uiSearchReplace.cbReplace->currentText();
				const QVector<QString> capturePatterns = this->capturePatterns(pattern);

				const int captureCount = capturePatterns.count();
				for (int i = 1; i <= 9; i++) {
					const QString number = QString::number(i);
					const QString& captureDetails =
						(i <= captureCount) ? QLatin1String(" = (") + QStringView(capturePatterns[i - 1]).left(30) + QLatin1Char(')') : QString();
					addMenuManager.addEntry(QLatin1String("\\") + number, QString(), i18n("Reference") + QLatin1Char(' ') + number + captureDetails);
				}

				addMenuManager.addSeparator();
			}
		}

		addMenuManager.addEntry(QStringLiteral("\\n"), QString(), i18n("Line break"));
		addMenuManager.addEntry(QStringLiteral("\\t"), QString(), i18n("Tab"));

		if (!replace && regexMode) {
			addMenuManager.addEntry(QStringLiteral("\\b"), QString(), i18n("Word boundary"));
			addMenuManager.addEntry(QStringLiteral("\\B"), QString(), i18n("Not word boundary"));
			addMenuManager.addEntry(QStringLiteral("\\d"), QString(), i18n("Digit"));
			addMenuManager.addEntry(QStringLiteral("\\D"), QString(), i18n("Non-digit"));
			addMenuManager.addEntry(QStringLiteral("\\s"), QString(), i18n("Whitespace (excluding line breaks)"));
			addMenuManager.addEntry(QStringLiteral("\\S"), QString(), i18n("Non-whitespace"));
			addMenuManager.addEntry(QStringLiteral("\\w"), QString(), i18n("Word character (alphanumerics plus '_')"));
			addMenuManager.addEntry(QStringLiteral("\\W"), QString(), i18n("Non-word character"));
		}

		addMenuManager.addEntry(QStringLiteral("\\0???"), QString(), i18n("Octal character 000 to 377 (2^8-1)"), QStringLiteral("\\0"));
		addMenuManager.addEntry(QStringLiteral("\\x{????}"), QString(), i18n("Hex character 0000 to FFFF (2^16-1)"), QStringLiteral("\\x{....}"));
		addMenuManager.addEntry(QStringLiteral("\\\\"), QString(), i18n("Backslash"));

		if (!replace && regexMode) {
			addMenuManager.addSeparator();
			addMenuManager.addEntry(QStringLiteral("(?:E"), QStringLiteral(")"), i18n("Group, non-capturing"), QStringLiteral("(?:"));
			addMenuManager.addEntry(QStringLiteral("(?=E"), QStringLiteral(")"), i18n("Positive Lookahead"), QStringLiteral("(?="));
			addMenuManager.addEntry(QStringLiteral("(?!E"), QStringLiteral(")"), i18n("Negative lookahead"), QStringLiteral("(?!"));
			// variable length positive/negative lookbehind is an experimental feature in Perl 5.30
			// see: https://perldoc.perl.org/perlre.html
			// currently QRegularExpression only supports fixed-length positive/negative lookbehind (2020-03-01)
			addMenuManager.addEntry(QStringLiteral("(?<=E"), QStringLiteral(")"), i18n("Fixed-length positive lookbehind"), QStringLiteral("(?<="));
			addMenuManager.addEntry(QStringLiteral("(?<!E"), QStringLiteral(")"), i18n("Fixed-length negative lookbehind"), QStringLiteral("(?<!"));
		}

		if (replace) {
			addMenuManager.addSeparator();
			addMenuManager.addEntry(QStringLiteral("\\L"), QString(), i18n("Begin lowercase conversion"));
			addMenuManager.addEntry(QStringLiteral("\\U"), QString(), i18n("Begin uppercase conversion"));
			addMenuManager.addEntry(QStringLiteral("\\E"), QString(), i18n("End case conversion"));
			addMenuManager.addEntry(QStringLiteral("\\l"), QString(), i18n("Lowercase first character conversion"));
			addMenuManager.addEntry(QStringLiteral("\\u"), QString(), i18n("Uppercase first character conversion"));
			addMenuManager.addEntry(QStringLiteral("\\#[#..]"), QString(), i18n("Replacement counter (for Replace All)"), QStringLiteral("\\#"));
		}
	}

	// Show menu
	auto* const result = contextMenu->exec(lineEdit->mapToGlobal(pos));
	if (result)
		addMenuManager.handle(result, lineEdit);
}

QVector<QString> SearchReplaceWidget::capturePatterns(const QString& pattern) const {
	QVector<QString> capturePatterns;
	capturePatterns.reserve(9);
	QStack<ParInfo> parInfos;

	const int inputLen = pattern.length();
	int input = 0; // walker index
	bool insideClass = false;
	int captureCount = 0;

	while (input < inputLen) {
		if (insideClass) {
			// Wait for closing, unescaped ']'
			if (pattern[input].unicode() == L']')
				insideClass = false;

			input++;
		} else {
			switch (pattern[input].unicode()) {
			case L'\\':
				// Skip this and any next character
				input += 2;
				break;

			case L'(':
				ParInfo curInfo;
				curInfo.openIndex = input;
				curInfo.capturing = (input + 1 >= inputLen) || (pattern[input + 1].unicode() != '?');
				if (curInfo.capturing) {
					captureCount++;
				}
				curInfo.captureNumber = captureCount;
				parInfos.push(curInfo);

				input++;
				break;

			case L')':
				if (!parInfos.empty()) {
					ParInfo& top = parInfos.top();
					if (top.capturing && (top.captureNumber <= 9)) {
						const int start = top.openIndex + 1;
						const int len = input - start;
						if (capturePatterns.size() < top.captureNumber) {
							capturePatterns.resize(top.captureNumber);
						}
						capturePatterns[top.captureNumber - 1] = pattern.mid(start, len);
					}
					parInfos.pop();
				}

				input++;
				break;

			case L'[':
				input++;
				insideClass = true;
				break;

			default:
				input++;
				break;
			}
		}
	}

	return capturePatterns;
}
