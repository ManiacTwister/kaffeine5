/*
 * main.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <QPointer>
#include <QApplication>
#include <KAboutData>
#include <KLocalizedString>
#include "mainwindow.h"
#include "sqlhelper.h"

int main(int argc, char *argv[])
{
	KAboutData aboutData("kaffeine", i18n("Kaffeine"), "1.3-git",
		i18n("A media player for KDE with digital TV support."),
		KAboutLicense::GPL_V2, i18n("(C) 2007-2011 The Kaffeine Authors"),
		QString(), "http://kaffeine.kde.org");

	aboutData.addAuthor(i18n("Christoph Pfister"), i18n("Maintainer"),
		"christophpfister@gmail.com");

    KAboutData::setApplicationData(aboutData);

	QApplication app(argc, argv);

    SqlHelper::createInstance();

    MainWindow* mainWindow = new MainWindow;
    mainWindow->parseArgs();

	return app.exec();
}
