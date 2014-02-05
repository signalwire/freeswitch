/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Joao Mesquita <jmesquita@freeswitch.org>
 *
 */

#include <QtGui/QApplication>
#include <QSplashScreen>
#include "mainwindow.h"
#include "isettings.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setOrganizationDomain("freeswitch.org");
    QPixmap image(":/images/splash.png");
    QSplashScreen *splash = new QSplashScreen(image);
    splash->show();
    splash->showMessage("Loading core, please wait...", Qt::AlignRight|Qt::AlignBottom, Qt::blue);

    g_FSHost = new FSHost();

    QObject::connect(g_FSHost, SIGNAL(loadingModules(QString,int,QColor)), splash, SLOT(showMessage(QString,int,QColor)));
    QObject::connect(g_FSHost, SIGNAL(ready()), splash, SLOT(close()));
    MainWindow w;    
    QObject::connect(g_FSHost, SIGNAL(ready()), &w, SLOT(show()));
    g_FSHost->start();
    return a.exec();
}
