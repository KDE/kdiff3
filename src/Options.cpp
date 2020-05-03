/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2019-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "options.h"

#include "ConfigValueMap.h"
#include "diff.h"
#include "OptionItems.h"

#include <KSharedConfig>

#define KDIFF3_CONFIG_GROUP "KDiff3 Options"

void Options::init()
{
    addOptionItem(new OptionSize(QSize(600, 400), "Geometry", &m_geometry));
    addOptionItem(new OptionPoint(QPoint(0, 22), "Position", &m_position));
    addOptionItem(new OptionToggleAction(false, "WindowStateMaximised", &m_bMaximised));

    addOptionItem(new OptionToggleAction(true, "Show Toolbar", &m_bShowToolBar));
    addOptionItem(new OptionToggleAction(true, "Show Statusbar", &m_bShowStatusBar));
}

void Options::apply()
{
    for(OptionItemBase* item : mOptionItemList)
    {
        item->apply();
    }
}

void Options::resetToDefaults()
{
    for(OptionItemBase* item : mOptionItemList)
    {
        item->setToDefault();
    }
}

void Options::setToCurrent()
{
    for(OptionItemBase* item : mOptionItemList)
    {
        item->setToCurrent();
    }
}

void Options::saveOptions(const KSharedConfigPtr config)
{
    // No i18n()-Translations here!

    ConfigValueMap cvm(config->group(KDIFF3_CONFIG_GROUP));

    for(OptionItemBase* item : mOptionItemList)
    {
        item->doUnpreserve();
        item->write(&cvm);
    }
}

void Options::readOptions(const KSharedConfigPtr config)
{
    // No i18n()-Translations here!

    ConfigValueMap cvm(config->group(KDIFF3_CONFIG_GROUP));

    for(OptionItemBase* item : mOptionItemList)
    {
        item->read(&cvm);
    }

    if(m_whiteSpace2FileMergeDefault <= (int)e_SrcSelector::Min)
        m_whiteSpace2FileMergeDefault = (int)e_SrcSelector::None;

    if(m_whiteSpace2FileMergeDefault >= (int)e_SrcSelector::Max)
        m_whiteSpace2FileMergeDefault = (int)e_SrcSelector::C;
}


const QString Options::parseOptions(const QStringList& optionList)
{
    QString result;

    for(const QString &optionString : optionList)
    {
        int pos = optionString.indexOf('=');
        if(pos > 0) // seems not to have a tag
        {
            const QString key = optionString.left(pos);
            const QString val = optionString.mid(pos + 1);

            bool bFound = false;
            for(OptionItemBase* item : mOptionItemList)
            {
                if(item->getSaveName() == key)
                {
                    item->doPreserve();
                    ValueMap config;
                    config.writeEntry(key, val); // Write the value as a string and
                    item->read(&config);         // use the internal conversion from string to the needed value.
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
            result += "No '=' found in \"" + optionString + "\"\n";
        }
    }
    return result;
}

QString Options::calcOptionHelp()
{
    ValueMap config;

    for(OptionItemBase* item : mOptionItemList)
    {
        item->write(&config);
    }
    return config.getAsString();
}

void Options::addOptionItem(OptionItemBase* inItem)
{
    mOptionItemList.push_back(inItem);
}

