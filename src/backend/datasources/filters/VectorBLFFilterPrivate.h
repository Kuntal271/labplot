/*
	File                 : VectorBLFFilterPrivate.h
	Project              : LabPlot
	Description          : Private implementation class for VectorBLFFilter.
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2023 <martin.marmsoler@gmail.com>

	SPDX-License-Identifier: GPL-3.0-or-later
*/
#ifndef VECTORBLFFILTERPRIVATE_H
#define VECTORBLFFILTERPRIVATE_H

#include <QList>

#include "AbstractFileFilter.h"
#include "DBCParser.h"
#include "VectorBLFFilter.h"
#include <cmath>
#include <libdbc/dbc.hpp>

class AbstractDataSource;
class VectorBLFFilter;
class QTreeWidgetItem;

class VectorBLFFilterPrivate {
public:
	explicit VectorBLFFilterPrivate(VectorBLFFilter*);
	QVector<QStringList> preview(const QString& fileName, int lines);
	void readDataFromFile(const QString& fileName,
						  AbstractDataSource* = nullptr,
						  AbstractFileFilter::ImportMode = AbstractFileFilter::ImportMode::Replace,
						  int lines = -1);

	/*!
	 * \brief readDataFromFile
	 * Read data from file and store them in the datacontainer \p m_dataContainer
	 * \param fileName
	 * \param lines
	 * \return number of messages read
	 */
	int readDataFromFile(const QString& fileName, int lines = -1);
	int readDataFromFileCommonTime(const QString& fileName, int lines = -1);
	int readDataFromFileSeparateTime(const QString& fileName, int lines = -1);
	void write(const QString& fileName, AbstractDataSource*);
	bool setDBCFile(const QString& filename);

	const QVector<AbstractColumn::ColumnMode> columnModes();

	static bool isValid(const QString& filename);

	void clearParseState();

	const VectorBLFFilter* q;

	QString currentDataSetName;
	int startRow{1};
	int endRow{-1};
	int startColumn{1};
	int endColumn{-1};
	QStringList vectorNames;
	VectorBLFFilter::TimeHandling timeHandlingMode{VectorBLFFilter::TimeHandling::ConcatPrevious};
	bool convertTimeToSeconds{true};

private:
	DbcParser m_dbcParser;
	struct DataContainer {
		void clear();

		template<class T>
		void appendVector(QVector<T>* data, AbstractColumn::ColumnMode cm) {
			m_dataContainer.push_back(data);
			m_columnModes.append(cm);
		};

		template<class T>
		void setData(int indexDataContainer, int indexData, T value) {
			auto* v = static_cast<QVector<T>*>(m_dataContainer.at(indexDataContainer));
			v->operator[](indexData) = value;
		}

		template<class T>
		T data(int indexDataContainer, int indexData) {
			auto* v = static_cast<QVector<T>*>(m_dataContainer.at(indexDataContainer));
			return v->at(indexData);
		}

		int size() const;
		const QVector<AbstractColumn::ColumnMode> columnModes() const;

		/*!
		 * \brief dataContainer
		 * Do not modify outside as long as DataContainer exists!
		 * \return
		 */
		std::vector<void*> dataContainer() const;
		AbstractColumn::ColumnMode columnMode(int index) const;
		const void* datas(int index) const;
		bool squeeze() const;

	private:
		QVector<AbstractColumn::ColumnMode> m_columnModes;
		std::vector<void*> m_dataContainer; // pointers to the actual data containers
	};
	DataContainer m_DataContainer;

	struct ParseState {
		ParseState() {
		}
		ParseState(int lines)
			: ready(true)
			, lines(lines) {
		}
		bool ready{false}; // If true m_DataContainer is up to date and it is not needed to update
		int lines{0};
	};

	ParseState m_parseState;
};

#endif // VECTORBLFFILTERPRIVATE_H
