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

#include "OptionItems.h"

void Options::init(std::list<OptionItemBase*> &optionItemList)
{
    /*
    TODO manage toolbar positioning
   */
    optionItemList.push_back(new OptionNum<int>( Qt::TopToolBarArea, "ToolBarPos", (int*)&m_toolBarPos));
    optionItemList.push_back(new OptionSize(QSize(600, 400), "Geometry", &m_geometry));
    optionItemList.push_back(new OptionPoint(QPoint(0, 22), "Position", &m_position));
    optionItemList.push_back(new OptionToggleAction(false, "WindowStateMaximised", &m_bMaximised));

    optionItemList.push_back(new OptionToggleAction(true, "Show Toolbar", &m_bShowToolBar));
    optionItemList.push_back(new OptionToggleAction(true, "Show Statusbar", &m_bShowStatusBar));
}

void Options::apply(const std::list<OptionItemBase*> &optionItemList)
{
    std::list<OptionItemBase*>::const_iterator i;
    for(i = optionItemList.begin(); i != optionItemList.end(); ++i)
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

void Options::resetToDefaults(const std::list<OptionItemBase*> &optionItemList)
{
    std::list<OptionItemBase*>::const_iterator i;
    for(i = optionItemList.begin(); i != optionItemList.end(); ++i)
    {
        (*i)->setToDefault();
    }
}

void Options::setToCurrent(const std::list<OptionItemBase*> &optionItemList)
{
    std::list<OptionItemBase*>::const_iterator i;
    for(i = optionItemList.begin(); i != optionItemList.end(); ++i)
    {
        (*i)->setToCurrent();
    }
}
