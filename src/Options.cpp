/**
 * Copyright (C) 2019 Michael Reeves <reeves.87@gmail.com>
 *
 * This file is part of KDiff3.
 *
 * KDiff3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * KDiff3 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with KDiff3.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "options.h"

#include "ConfigValueMap.h"
#include "OptionItems.h"

#include <KSharedConfig>

#define KDIFF3_CONFIG_GROUP "KDiff3 Options"

void Options::init()
{
    /*
    TODO manage toolbar positioning
   */
    addOptionItem(new OptionNum<int>( Qt::TopToolBarArea, "ToolBarPos", (int*)&m_toolBarPos));
    addOptionItem(new OptionSize(QSize(600, 400), "Geometry", &m_geometry));
    addOptionItem(new OptionPoint(QPoint(0, 22), "Position", &m_position));
    addOptionItem(new OptionToggleAction(false, "WindowStateMaximised", &m_bMaximised));

    addOptionItem(new OptionToggleAction(true, "Show Toolbar", &m_bShowToolBar));
    addOptionItem(new OptionToggleAction(true, "Show Statusbar", &m_bShowStatusBar));
}

void Options::apply()
{
    std::list<OptionItemBase*>::const_iterator i;
    for(i = mOptionItemList.begin(); i != mOptionItemList.end(); ++i)
    {
        (*i)->apply();
    }

#ifdef Q_OS_WIN //TODO: Needed? If so move this to optionItemList like everything else.
    QString locale = m_options->m_language;
    if(locale == "Auto" || locale.isEmpty())
        locale = QLocale::system().name().left(2);
    int spacePos = locale.indexOf(' ');
    if(spacePos > 0) locale = locale.left(spacePos);
    QSettings settings(QLatin1String("HKEY_CURRENT_USER\\Software\\KDiff3\\diff-ext"), QSettings::NativeFormat);
    settings.setValue(QLatin1String("Language"), locale);
#endif
}

void Options::resetToDefaults()
{
    std::list<OptionItemBase*>::const_iterator i;
    for(i = mOptionItemList.begin(); i != mOptionItemList.end(); ++i)
    {
        (*i)->setToDefault();
    }
}

void Options::setToCurrent()
{
    std::list<OptionItemBase*>::const_iterator i;
    for(i = mOptionItemList.begin(); i != mOptionItemList.end(); ++i)
    {
        (*i)->setToCurrent();
    }
}

void Options::saveOptions(const KSharedConfigPtr config)
{
    // No i18n()-Translations here!

    ConfigValueMap cvm(config->group(KDIFF3_CONFIG_GROUP));
    std::list<OptionItemBase*>::const_iterator i;
    for(i = mOptionItemList.begin(); i != mOptionItemList.end(); ++i)
    {
        (*i)->doUnpreserve();
        (*i)->write(&cvm);
    }
}

void Options::readOptions(const KSharedConfigPtr config)
{
    // No i18n()-Translations here!

    ConfigValueMap cvm(config->group(KDIFF3_CONFIG_GROUP));
    std::list<OptionItemBase*>::const_iterator i;
    for(i = mOptionItemList.begin(); i != mOptionItemList.end(); ++i)
    {
        (*i)->read(&cvm);
    }
}


QString Options::parseOptions(const QStringList& optionList)
{
    QString result;
    QStringList::const_iterator i;
    for(i = optionList.begin(); i != optionList.end(); ++i)
    {
        QString s = *i;

        int pos = s.indexOf('=');
        if(pos > 0) // seems not to have a tag
        {
            QString key = s.left(pos);
            QString val = s.mid(pos + 1);
            std::list<OptionItemBase*>::iterator j;
            bool bFound = false;
            for(j = mOptionItemList.begin(); j != mOptionItemList.end(); ++j)
            {
                if((*j)->getSaveName() == key)
                {
                    (*j)->doPreserve();
                    ValueMap config;
                    config.writeEntry(key, val); // Write the value as a string and
                    (*j)->read(&config);         // use the internal conversion from string to the needed value.
                    bFound = true;
                    break;
                }
            }
            if(!bFound)
            {
                result += "No config item named \"" + key + "\"\n";
            }
        }
        else
        {
            result += "No '=' found in \"" + s + "\"\n";
        }
    }
    return result;
}

QString Options::calcOptionHelp()
{
    ValueMap config;
    std::list<OptionItemBase*>::const_iterator it;
    for(it = mOptionItemList.begin(); it != mOptionItemList.end(); ++it)
    {
        (*it)->write(&config);
    }
    return config.getAsString();
}

void Options::addOptionItem(OptionItemBase* inItem)
{
    mOptionItemList.push_back(inItem);
}

