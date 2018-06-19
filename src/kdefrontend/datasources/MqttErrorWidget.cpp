#include "MqttErrorWidget.h"

#ifdef HAVE_MQTT
#include <QtMqtt/QMqttClient>
#include <QtMqtt/qmqttclient.h>
#include <QtMqtt/QMqttSubscription>
#include <QtMqtt/qmqttsubscription.h>
#include <QMessageBox>
#include <QtMqtt/QMqttTopicFilter>
#include <QtMqtt/QMqttMessage>
#endif

MqttErrorWidget::MqttErrorWidget(QWidget *parent, int errorType, LiveDataSource * source) : QWidget(parent),
	m_source(source),
	#ifdef HAVE_MQTT
	m_error (static_cast<QMqttClient::ClientError>(errorType))
  #endif
{
	ui.setupUi(this);
	switch (m_error) {
	case QMqttClient::ClientError::IdRejected:
		ui.lePassword->hide();
		ui.lPassword->hide();
		ui.leUserName->hide();
		ui.lUserName->hide();
		ui.lErrorType->setText("The client ID is malformed. This might be related to its length.\nSet new ID!");
		break;
	case QMqttClient::ClientError::BadUsernameOrPassword:
		ui.lId->hide();
		ui.leId->hide();
		ui.lErrorType->setText("The data in the username or password is malformed.\nSet new username and password!");
		break;
	case QMqttClient::ClientError::NotAuthorized:
		ui.lId->hide();
		ui.leId->hide();
		ui.lErrorType->setText("The client is not authorized to connect.");
		break;
	default:
		break;
	}
	connect(ui.bChange, &QPushButton::clicked, this, &MqttErrorWidget::makeChange);
}

MqttErrorWidget::~MqttErrorWidget() {
}

void MqttErrorWidget::makeChange(){
	bool ok = false;
	switch (m_error) {
	case QMqttClient::ClientError::IdRejected:
		if(!ui.leId->text().isEmpty()) {
			m_source->setMqttClientId(ui.leId->text());
			m_source->read();
			ok = true;
		}
		break;
	case QMqttClient::ClientError::BadUsernameOrPassword:
		if(!ui.lePassword->text().isEmpty() && !ui.leUserName->text().isEmpty()) {
			m_source->setMqttClientAuthentication(ui.leUserName->text(), ui.lePassword->text());
			m_source->read();
			ok = true;
		}
		break;
	case QMqttClient::ClientError::NotAuthorized:
		if(!ui.lePassword->text().isEmpty() && !ui.leUserName->text().isEmpty()) {
			m_source->setMqttClientAuthentication(ui.leUserName->text(), ui.lePassword->text());
			m_source->read();
			ok = true;
		}
		break;
	default:
		break;
	}
	if (ok)
		this->close();
}
