/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2009, Anthony Minessale II <anthm@freeswitch.org>
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

#ifndef MOD_QSETTINGS_H
#define MOD_QSETTINGS_H

#include <QString>
#include <QSettings>
#include <switch.h>

class QXmlStreamWriter;

class XMLBinding
{
public:
    XMLBinding(QString binding) : _binding(binding), _settings(new QSettings) {}
    QString getBinding(void) { return _binding; }
    switch_xml_t getConfigXML(QString);
private:
    void parseGroup(QXmlStreamWriter *,QString);
    QString _binding;
    QSettings* _settings;
};

switch_status_t mod_qsettings_load(void);
void setQSettingsDefaults(void);
void setGlobals(void);

#endif // MOD_QSETTINGS_H
