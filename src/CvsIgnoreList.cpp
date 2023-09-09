/*
  class CvsIgnoreList from Cervisia cvsdir.cpp
     SPDX-FileCopyrightText: 1999-2002 Bernd Gehrmann <bernd at mail.berlios.de>
  with elements from class StringMatcher
     SPDX-FileCopyrightText: 2003 Andre Woebbeking <Woebbeking at web.de>
  Modifications for KDiff3 by Joachim Eibl

  SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
  SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "CvsIgnoreList.h"

#include "fileaccess.h"        // for FileAccess

#include <list>
#include <utility>             // for pair

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

CvsIgnoreList::CvsIgnoreList() = default;

CvsIgnoreList::~CvsIgnoreList() = default;

void CvsIgnoreList::enterDir(const QString& dir, const DirectoryList& directoryList)
{
    static const QString ignorestr = QString::fromLatin1(". .. core RCSLOG tags TAGS RCS SCCS .make.state "
                                   ".nse_depinfo #* .#* cvslog.* ,* CVS CVS.adm .del-* *.a *.olb *.o *.obj "
                                   "*.so *.Z *~ *.old *.elc *.ln *.bak *.BAK *.orig *.rej *.exe _$* *$");
    addEntriesFromString(dir, ignorestr);
    addEntriesFromFile(dir, QDir::homePath() + '/' + getGlobalIgnoreName());
    const char* varname = getVarName();
    if(qEnvironmentVariableIsSet(varname) && !qEnvironmentVariableIsEmpty(varname))
    {
        addEntriesFromString(dir, QString::fromLocal8Bit(qgetenv(varname)));
    }
    const bool bUseLocalCvsIgnore = ignoreExists(directoryList);
    if(bUseLocalCvsIgnore)
    {
        FileAccess file(dir);
        file.addPath(getIgnoreName());
        if(file.exists() && file.isLocal())
        {
            addEntriesFromFile(dir, file.absoluteFilePath());
        }
        else
        {
            file.createLocalCopy();
            addEntriesFromFile(dir, file.getTempName());
        }
    }
}

void CvsIgnoreList::addEntriesFromString(const QString& dir, const QString& str)
{
    const QStringList patternList = str.split(' ');
    for(const QString& pattern: patternList)
    {
        addEntry(dir, pattern);
    }
}

/*
    We don't have a real file in AUTOTEST mode
*/
void CvsIgnoreList::addEntriesFromFile(const QString& dir, const QString& name)
{
#ifdef AUTOTEST
    Q_UNUSED(name);
    Q_UNUSED(dir);
#else
    QFile file(name);

    if(file.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&file);
        while(!stream.atEnd())
        {
            addEntry(dir, stream.readLine());
        }
    }
#endif
}

void CvsIgnoreList::addEntry(const QString& dir, const QString& pattern)
{
    if(pattern != QString("!"))
    {
        if(pattern.isEmpty()) return;

        // The general match is general but slow.
        // Special tests for '*' and '?' at the beginning or end of a pattern
        // allow fast checks.

        // Count number of '*' and '?'
        quint32 nofMetaCharacters = 0;

        const QChar* pos;
        pos = pattern.unicode();
        const QChar* posEnd;
        posEnd = pos + pattern.length();
        while(pos < posEnd)
        {
            if(*pos == QChar('*') || *pos == QChar('?')) ++nofMetaCharacters;
            ++pos;
        }

        if(nofMetaCharacters == 0)
        {
            m_ignorePatterns[dir].m_exactPatterns.append(pattern);
        }
        else if(nofMetaCharacters == 1)
        {
            if(pattern.at(0) == QChar('*'))
            {
                m_ignorePatterns[dir].m_endPatterns.append(pattern.right(pattern.length() - 1));
            }
            else if(pattern.at(pattern.length() - 1) == QChar('*'))
            {
                m_ignorePatterns[dir].m_startPatterns.append(pattern.left(pattern.length() - 1));
            }
            else
            {
                m_ignorePatterns[dir].m_generalPatterns.append(pattern);
            }
        }
        else
        {
            m_ignorePatterns[dir].m_generalPatterns.append(pattern);
        }
    }
    else
    {
        m_ignorePatterns.erase(dir);
    }
}

bool CvsIgnoreList::matches(const QString& dir, const QString& text, bool bCaseSensitive) const
{
    const auto ignorePatternsIt = m_ignorePatterns.find(dir);
    if(ignorePatternsIt == m_ignorePatterns.end())
    {
        return false;
    }
    //Do not use QStringList::indexof here it has no case flag
    if(ignorePatternsIt->second.m_exactPatterns.contains(text, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive))
    {
        return true;
    }

    for(const QString& startPattern: ignorePatternsIt->second.m_startPatterns)
    {
        if(text.startsWith(startPattern, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive))
        {
            return true;
        }
    }

    for(const QString& endPattern: ignorePatternsIt->second.m_endPatterns)
    {
        if(text.endsWith(endPattern, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive))
        {
            return true;
        }
    }

    for(const QString& globStr: ignorePatternsIt->second.m_generalPatterns)
    {
        QRegularExpression pattern(QRegularExpression::wildcardToRegularExpression(globStr), bCaseSensitive ? QRegularExpression::UseUnicodePropertiesOption : QRegularExpression::UseUnicodePropertiesOption | QRegularExpression::CaseInsensitiveOption);
        if(pattern.match(text).hasMatch())
            return true;
    }

    return false;
}

bool CvsIgnoreList::ignoreExists(const DirectoryList& pDirList)
{
    for(const FileAccess& dir: pDirList)
    {
        if(dir.fileName() == getIgnoreName())
            return true;
    }
    return false;
}
