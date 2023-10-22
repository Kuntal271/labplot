#include "UTCDateTimeEdit.h"

UTCDateTimeEdit::UTCDateTimeEdit(QWidget* parent)
	: QDateTimeEdit(parent) {
	setMinimumDate(QDate(100, 1, 1));
	connect(this, &QDateTimeEdit::dateTimeChanged, this, &UTCDateTimeEdit::dateTimeChanged);
}

QDateTime UTCDateTimeEdit::dateTime() const {
	QDateTime dt = QDateTimeEdit::dateTime();
	dt.setTimeSpec(Qt::TimeSpec::UTC);
	return dt;
}

void UTCDateTimeEdit::setMSecsSinceEpochUTC(qint64 value) {
	QDateTimeEdit::setDateTime(QDateTime::fromMSecsSinceEpoch(value, Qt::UTC));
}

void UTCDateTimeEdit::dateTimeChanged(const QDateTime& datetime) {
	QDateTime dt = datetime;
	dt.setTimeSpec(Qt::TimeSpec::UTC);
	Q_EMIT mSecsSinceEpochUTCChanged(dt.toMSecsSinceEpoch());
}
