/*
	File                 : ProjectDock.h
	Project              : LabPlot
	Description          : widget for worksheet properties
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 2012 Stefan Gerlach <stefan.gerlach@uni-konstanz.de>
	SPDX-FileCopyrightText: 2012-2021 Alexander Semke <alexander.semke@web.de>

	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROJECTDOCK_H
#define PROJECTDOCK_H

#include "kdefrontend/dockwidgets/BaseDock.h"
#include "ui_projectdock.h"

class Project;
class KConfig;

class ProjectDock : public BaseDock {
	Q_OBJECT

public:
	explicit ProjectDock(QWidget*);
	void setProject(Project*);

private:
	Ui::ProjectDock ui;
	Project* m_project{nullptr};

private Q_SLOTS:
	void authorChanged();
	void saveCalculationsChanged(bool);

	// SLOTs for changes triggered in Project
	void projectAuthorChanged(const QString&);
	void projectSaveCalculationsChanged(bool);
};

#endif // PROJECTDOCK_H
