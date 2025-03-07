/*
	File                 : VectorBLFFilter.cpp
	Project              : LabPlot
	Description          : Vector BLF I/O-filter
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2023 Martin Marmsoler <martin.marmsoler@gmail.com>
	SPDX-License-Identifier: GPL-3.0-or-later
*/

/* TODO:
 * Feature: implement missing data types and ranks
 * Performance: only fill dataPointer or dataStrings (not both)
 */

#include "backend/datasources/filters/VectorBLFFilter.h"
#include "backend/datasources/filters/VectorBLFFilterPrivate.h"
#include "backend/lib/XmlStreamReader.h"
#include "backend/lib/macros.h"
#include "backend/lib/trace.h"

#include <KLocalizedString>

#ifdef HAVE_VECTOR_BLF
#include <Vector/BLF/Exceptions.h>
#include <Vector/BLF/File.h>
#endif

//////////////////////////////////////////////////////////////////////

/*!
	\class VectorBLFFilter
	\brief Manages the import/export of data from/to a Vector BLF file.

	\ingroup datasources
*/
VectorBLFFilter::VectorBLFFilter()
	: CANFilter(FileType::VECTOR_BLF, new VectorBLFFilterPrivate(this)) {
}

VectorBLFFilter::~VectorBLFFilter() = default;

QString VectorBLFFilter::fileInfoString(const QString& fileName) {
	DEBUG(Q_FUNC_INFO);
	QString info;

	Q_UNUSED(fileName);
	return info;
}

bool VectorBLFFilter::isValid(const QString& filename) {
#ifdef HAVE_VECTOR_BLF
	try {
		Vector::BLF::File f;
		f.open(filename.toLocal8Bit().data());
		if (!f.is_open())
			return false; // No file
		f.close();
		return true;
	} catch (Vector::BLF::Exception& e) {
		return false; // Signature was invalid or something else
	}
#else
	Q_UNUSED(filename)
#endif
	return false;
}

// #####################################################################
// ################### Private implementation ##########################
// #####################################################################

VectorBLFFilterPrivate::VectorBLFFilterPrivate(VectorBLFFilter* owner)
	: CANFilterPrivate(owner)
#ifdef HAVE_VECTOR_BLF
	, q(owner)
#endif
{
}

bool VectorBLFFilterPrivate::isValid(const QString& filename) const {
	return VectorBLFFilter::isValid(filename);
}

QStringList VectorBLFFilterPrivate::lastErrors() const {
	QStringList r;
	for (const auto& e : errors) {
		switch (e.e) {
		case ParseStatus::DBCBigEndian:
			r.append(i18n("Big Endian not supported. CAN id: %1", QStringLiteral("0x%1").arg(e.CANId, 0, 16)));
			break;
		case ParseStatus::DBCMessageToLong:
			r.append(i18n("Message too long. CAN id: %1", QStringLiteral("0x%1").arg(e.CANId, 0, 16)));
			break;
		case ParseStatus::DBCUnknownID:
			r.append(i18n("Unknown id: %1", QStringLiteral("0x%1").arg(e.CANId, 0, 16)));
			break;
		case ParseStatus::ErrorInvalidFile:
			r.append(i18n("Invalid blf file"));
			break;
		case ParseStatus::DBCInvalidConversion:
			r.append(i18n("Unable to calculate conversion: %1", QStringLiteral("0x%1").arg(e.CANId, 0, 16)));
			break;
		case ParseStatus::DBCParserUnsupported:
			r.append(i18n("No dbc parser installed"));
			break;
		case ParseStatus::DBCInvalidFile:
			r.append(i18n("Invalid dbc file"));
			break;
		case ParseStatus::ErrorUnknown:
			r.append(i18n("Unknown error"));
			break;
		case ParseStatus::Success:
			break;
		}
	}
	return r;
}

VectorBLFFilterPrivate::ParseStatus VectorBLFFilterPrivate::DBCParserParseStatusToVectorBLFStatus(DbcParser::ParseStatus s) {
	switch (s) {
	case DbcParser::ParseStatus::ErrorBigEndian:
		return ParseStatus::DBCBigEndian;
	case DbcParser::ParseStatus::ErrorMessageToLong:
		return ParseStatus::DBCMessageToLong;
	case DbcParser::ParseStatus::ErrorUnknownID:
		return ParseStatus::DBCUnknownID;
	case DbcParser::ParseStatus::ErrorInvalidFile:
		return ParseStatus::DBCInvalidFile;
	case DbcParser::ParseStatus::ErrorDBCParserUnsupported:
		return ParseStatus::DBCParserUnsupported;
	case DbcParser::ParseStatus::ErrorInvalidConversion:
		return ParseStatus::DBCInvalidConversion;
	case DbcParser::ParseStatus::Success:
		return ParseStatus::Success;
	}
	return ParseStatus::ErrorUnknown;
}

#ifdef HAVE_VECTOR_BLF
bool getTime(const Vector::BLF::ObjectHeaderBase* ohb, uint64_t& timestamp) {
	/* ObjectHeader */
	auto* oh = dynamic_cast<const Vector::BLF::ObjectHeader*>(ohb);
	if (oh != nullptr) {
		timestamp = oh->objectTimeStamp;
		switch (oh->objectFlags) {
		case Vector::BLF::ObjectHeader::ObjectFlags::TimeTenMics:
			return false;
		case Vector::BLF::ObjectHeader::ObjectFlags::TimeOneNans:
			return true;
		}
	}

	/* ObjectHeader2 */
	auto* oh2 = dynamic_cast<const Vector::BLF::ObjectHeader2*>(ohb);
	if (oh2 != nullptr) {
		timestamp = oh2->objectTimeStamp;
		switch (oh2->objectFlags) {
		case Vector::BLF::ObjectHeader2::ObjectFlags::TimeTenMics:
			return false;
		case Vector::BLF::ObjectHeader2::ObjectFlags::TimeOneNans:
			return true;
		}
	}
	return true;
}
#endif

int VectorBLFFilterPrivate::readDataFromFileCommonTime(const QString& fileName, int lines) {
	PERFTRACE(QLatin1String(Q_FUNC_INFO));

	errors.clear();

	if (!isValid(fileName)) {
		errors.append({ParseStatus::ErrorInvalidFile, 0});
		return 0;
	}

	const auto status = m_dbcParser.isValid();
	if (status != DbcParser::ParseStatus::Success) {
		errors.append({DBCParserParseStatusToVectorBLFStatus(status), 0});
		return 0;
	}

	if (m_parseState.ready && m_parseState.requestedLines == lines)
		return m_parseState.readLines;

	m_DataContainer.clear();

#ifdef HAVE_VECTOR_BLF
	Vector::BLF::File file;
	file.open(fileName.toLocal8Bit().data());

	// 1. Reading in messages
	QVector<const Vector::BLF::ObjectHeaderBase*> v;
	Vector::BLF::ObjectHeaderBase* ohb = nullptr;
	QVector<uint32_t> ids;
	int message_counter = 0;
	{
		PERFTRACE(QLatin1String(Q_FUNC_INFO) + QLatin1String("Parsing BLF file"));
		while (file.good() && ((lines >= 0 && message_counter < lines) || lines < 0)) {
			try {
				ohb = file.read();
			} catch (std::runtime_error& e) { DEBUG("Exception: " << e.what() << std::endl); }
			if (ohb == nullptr)
				break;

			if (ohb->objectType != Vector::BLF::ObjectType::CAN_MESSAGE2)
				continue;

			int id;
			if (ohb->objectType == Vector::BLF::ObjectType::CAN_MESSAGE2) {
				const auto message = reinterpret_cast<Vector::BLF::CanMessage2*>(ohb);
				id = message->id;
			} else
				return 0;

			v.append(ohb);
			if (!ids.contains(id))
				ids.append(id);
			message_counter++;
		}
	}

	// 2. Create vector names
	QHash<uint32_t, int> idIndexTable;
	m_dbcParser.getSignals(ids, DbcParser::PrefixType::None, DbcParser::SuffixType::Unit, idIndexTable, m_signals);

	// 3. allocate memory
	if (convertTimeToSeconds) {
		auto* vector = new QVector<double>();
		vector->resize(message_counter);
		m_DataContainer.appendVector<double>(vector, AbstractColumn::ColumnMode::Double);
	} else {
		auto* vector = new QVector<qint64>();
		vector->resize(message_counter);
		m_DataContainer.appendVector<qint64>(vector, AbstractColumn::ColumnMode::BigInt); // BigInt is qint64 and not quint64!
	}
	for (int i = 0; i < m_signals.signal_names.length(); i++) {
		auto* vector = new QVector<double>();
		vector->resize(message_counter);
		m_DataContainer.appendVector(vector, AbstractColumn::ColumnMode::Double);
	}

	// 4. fill datacontainer
	int message_index = 0;
	bool timeInNS = true;
	if (timeHandlingMode == CANFilter::TimeHandling::ConcatNAN) {
		for (const auto ohb : v) {
			uint32_t id;
			std::vector<double> values;
			DbcParser::ParseStatus status;
			if (ohb->objectType == Vector::BLF::ObjectType::CAN_MESSAGE2) {
				const auto message = reinterpret_cast<const Vector::BLF::CanMessage2*>(ohb);
				id = message->id;
				status = m_dbcParser.parseMessage(message->id, message->data, values);
			} else
				return 0;

			if (status != DbcParser::ParseStatus::Success) {
				// id is not available in the dbc file, so it is not possible to decode
				DEBUG("Unable to decode message: " << id << ": " << (int)status);
				errors.append({DBCParserParseStatusToVectorBLFStatus(status), id});
				continue;
			}

			uint64_t timestamp;
			timeInNS = getTime(ohb, timestamp);
			if (convertTimeToSeconds) {
				double timestamp_seconds;
				if (timeInNS)
					timestamp_seconds = (double)timestamp / pow(10, 9); // TimeOneNans
				else
					timestamp_seconds = (double)timestamp / pow(10, 5); // TimeTenMics

				m_DataContainer.setData<double>(0, message_index, timestamp_seconds);
			} else
				m_DataContainer.setData<qint64>(0, message_index, timestamp);

			const size_t startIndex = idIndexTable.value(id) + 1; // +1 because of time
			for (size_t i = 1; i < startIndex; i++) {
				m_DataContainer.setData<double>(i, message_index, std::nan("0"));
			}
			for (size_t i = startIndex; i < startIndex + values.size(); i++) {
				m_DataContainer.setData<double>(i, message_index, values.at(i - startIndex));
			}
			for (size_t i = startIndex + values.size(); i < m_DataContainer.size(); i++) {
				m_DataContainer.setData<double>(i, message_index, std::nan("0"));
			}
			message_index++;
		}
	} else {
		bool firstMessageValid = false;
		for (const auto ohb : v) {
			uint32_t id;
			std::vector<double> values;
			DbcParser::ParseStatus status;
			if (ohb->objectType == Vector::BLF::ObjectType::CAN_MESSAGE2) {
				const auto message = reinterpret_cast<const Vector::BLF::CanMessage2*>(ohb);
				id = message->id;
				status = m_dbcParser.parseMessage(message->id, message->data, values);
			} else
				return 0;

			if (status != DbcParser::ParseStatus::Success) {
				// id is not available in the dbc file, so it is not possible to decode
				errors.append({DBCParserParseStatusToVectorBLFStatus(status), id});
				continue;
			}

			uint64_t timestamp;
			timeInNS = getTime(ohb, timestamp);
			if (convertTimeToSeconds) {
				double timestamp_seconds;
				if (timeInNS)
					timestamp_seconds = (double)timestamp / pow(10, 9); // TimeOneNans
				else
					timestamp_seconds = (double)timestamp / pow(10, 5); // TimeTenMics

				m_DataContainer.setData<double>(0, message_index, timestamp_seconds);
			} else
				m_DataContainer.setData<qint64>(0, message_index, timestamp);

			const std::vector<double>::size_type startIndex = idIndexTable.value(id) + 1; // +1 because of time
			if (firstMessageValid) {
				for (std::vector<double>::size_type i = 1; i < startIndex; i++) {
					const auto prevValue = m_DataContainer.data<double>(i, message_index - 1);
					m_DataContainer.setData<double>(i, message_index, prevValue);
				}
				for (std::vector<double>::size_type i = startIndex; i < startIndex + values.size(); i++) {
					m_DataContainer.setData<double>(i, message_index, values.at(i - startIndex));
				}
				for (std::vector<double>::size_type i = startIndex + values.size(); i < m_DataContainer.size(); i++) {
					const auto prevValue = m_DataContainer.data<double>(i, message_index - 1);
					m_DataContainer.setData<double>(i, message_index, prevValue);
				}
			} else {
				for (std::vector<double>::size_type i = 1; i < startIndex; i++)
					m_DataContainer.setData<double>(i, message_index, std::nan("0"));
				for (std::vector<double>::size_type i = startIndex; i < startIndex + values.size(); i++)
					m_DataContainer.setData<double>(i, message_index, values.at(i - startIndex));
				for (std::vector<double>::size_type i = startIndex + values.size(); i < m_DataContainer.size(); i++)
					m_DataContainer.setData<double>(i, message_index, std::nan("0"));
				firstMessageValid = true;
			}
			message_index++;
		}
	}

	// 5. add Time column to vector Names
	if (convertTimeToSeconds)
		m_signals.signal_names.prepend(i18n("Time_s")); // Must be done after allocating memory
	else if (timeInNS)
		m_signals.signal_names.prepend(i18n("Time_ns")); // Must be done after allocating memory
	else
		m_signals.signal_names.prepend(i18n("Time_10µs")); // Must be done after allocating memory
	m_signals.value_descriptions.insert(m_signals.value_descriptions.begin(), std::vector<DbcParser::ValueDescriptions>()); // Time does not have any labels

	for (const auto& message : v)
		delete message;

	if (!m_DataContainer.resize(message_index))
		return 0;

	// Use message_counter here, because it will be used as reference for caching
	m_parseState = ParseState(message_counter, message_index);
	return message_index;
#else
	return 0;
#endif // HAVE_VECTOR_BLF
}

int VectorBLFFilterPrivate::readDataFromFileSeparateTime(const QString& /*fileName*/, int /*lines*/) {
	return 0; // Not implemented yet
}

// ##############################################################################
// ##################  Serialization/Deserialization  ###########################
// ##############################################################################

/*!
  Saves as XML.
 */
void VectorBLFFilter::save(QXmlStreamWriter* writer) const {
	writer->writeStartElement(QStringLiteral("VectorBLFFilter"));
	writer->writeEndElement();
}

/*!
  Loads from XML.
*/
bool VectorBLFFilter::load(XmlStreamReader*) {
	return true;
}
