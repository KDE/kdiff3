/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2019-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include "options.h"

#include "combiners.h"
#include "ConfigValueMap.h"
#include "diff.h"
#include "OptionItems.h"

#include <boost/signals2.hpp>
#include <memory>

#include <KSharedConfig>

boost::signals2::signal<void ()> Options::apply;
boost::signals2::signal<void ()> Options::resetToDefaults;
boost::signals2::signal<void ()> Options::setToCurrent;
boost::signals2::signal<void (ValueMap*)> Options::read;
boost::signals2::signal<void (ValueMap*)> Options::write;

boost::signals2::signal<void ()> Options::preserve;
boost::signals2::signal<void ()> Options::unpreserve;

boost::signals2::signal<bool (const QString&, const QString&), find> Options::accept;

OptionItemBase::OptionItemBase(const QString& saveName)
{
    m_saveName = saveName;
    m_bPreserved = false;

    connections.push_back(Options::apply.connect(boost::bind(&OptionItemBase::apply, this)));
    connections.push_back(Options::setToCurrent.connect(boost::bind(&OptionItemBase::setToCurrent, this)));
    connections.push_back(Options::resetToDefaults.connect(boost::bind(&OptionItemBase::setToDefault, this)));

    connections.push_back(Options::read.connect(boost::bind(&OptionItemBase::read, this, boost::placeholders::_1)));
    connections.push_back(Options::write.connect(boost::bind(&OptionItemBase::write, this, boost::placeholders::_1)));

    connections.push_back(Options::preserve.connect(boost::bind(&OptionItemBase::preserve, this)));
    connections.push_back(Options::unpreserve.connect(boost::bind(&OptionItemBase::unpreserve, this)));

    connections.push_back(Options::accept.connect(boost::bind(&OptionItemBase::accept, this, boost::placeholders::_1, boost::placeholders::_2)));
}

bool OptionItemBase::accept(const QString& key, const QString& val)
{
    if(getSaveName() != key)
        return false;

    preserve();

    ValueMap config;
    config.writeEntry(key, val); // Write the value as a string and
    read(&config);               // use the internal conversion from string to the needed value.

    return true;
}

void Options::init()
{
    //Settings not in Options dialog
    addOptionItem(std::make_shared<OptionSize>(QSize(600, 400), "Geometry", &m_geometry));
    addOptionItem(std::make_shared<OptionPoint>(QPoint(0, 22), "Position", &m_position));
    addOptionItem(std::make_shared<OptionToggleAction>(false, "WindowStateFullScreen", &m_bFullScreen));
    addOptionItem(std::make_shared<OptionToggleAction>(false, "WindowStateMaximised", &m_bMaximised));

    addOptionItem(std::make_shared<OptionToggleAction>(true, "Show Statusbar", &m_bShowStatusBar));

    //Options in Options dialog.
    addOptionItem(std::make_shared<OptionToggleAction>(false, "AutoAdvance", &m_bAutoAdvance));
    addOptionItem(std::make_shared<OptionToggleAction>(true, "ShowWhiteSpaceCharacters", &m_bShowWhiteSpaceCharacters));
    addOptionItem(std::make_shared<OptionToggleAction>(true, "ShowWhiteSpace", &m_bShowWhiteSpace));
    addOptionItem(std::make_shared<OptionToggleAction>(false, "ShowLineNumbers", &m_bShowLineNumbers));
    addOptionItem(std::make_shared<OptionToggleAction>(true, "HorizDiffWindowSplitting", &m_bHorizDiffWindowSplitting));
    addOptionItem(std::make_shared<OptionToggleAction>(false, "WordWrap", &m_bWordWrap));

    addOptionItem(std::make_shared<OptionToggleAction>(true, "ShowIdenticalFiles", &m_bDmShowIdenticalFiles));

    addOptionItem(std::make_shared<OptionStringList>(&m_recentAFiles, "RecentAFiles"));
    addOptionItem(std::make_shared<OptionStringList>(&m_recentBFiles, "RecentBFiles"));
    addOptionItem(std::make_shared<OptionStringList>(&m_recentCFiles, "RecentCFiles"));
    addOptionItem(std::make_shared<OptionStringList>(&m_recentOutputFiles, "RecentOutputFiles"));
    addOptionItem(std::make_shared<OptionStringList>(&m_recentEncodings, "RecentEncodings"));
}

void Options::saveOptions(const KSharedConfigPtr config)
{
    // No i18n()-Translations here!

    ConfigValueMap cvm(config->group(KDIFF3_CONFIG_GROUP));

    unpreserve();
    write(&cvm);
}

void Options::readOptions(const KSharedConfigPtr config)
{
    // No i18n()-Translations here!

    ConfigValueMap cvm(config->group(KDIFF3_CONFIG_GROUP));

    read(&cvm);

    if(m_whiteSpace2FileMergeDefault <= (qint32)e_SrcSelector::Min)
        m_whiteSpace2FileMergeDefault = (qint32)e_SrcSelector::None;

    if(m_whiteSpace2FileMergeDefault > (qint32)e_SrcSelector::Max)
        m_whiteSpace2FileMergeDefault = (qint32)e_SrcSelector::C;
}

const QString Options::parseOptions(const QStringList& optionList)
{
    QString result;

    for(const QString& optionString: optionList)
    {
        qint32 pos = optionString.indexOf('=');
        if(pos > 0) // seems not to have a tag
        {
            const QString key = optionString.left(pos);
            const QString val = optionString.mid(pos + 1);

            bool bFound = accept(key, val);

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

    write(&config);

    return config.getAsString();
}

void Options::addOptionItem(std::shared_ptr<OptionItemBase> inItem)
{
    mOptionItemList.push_back(inItem);
}
