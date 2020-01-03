/***************************************************************************
 * class CvsIgnoreList from Cervisia cvsdir.cpp                            *
 *    Copyright (C) 1999-2002 Bernd Gehrmann <bernd at mail.berlios.de>    *
 * with elements from class StringMatcher                                  *
 *    Copyright (c) 2003 Andre Woebbeking <Woebbeking at web.de>           *
 * Modifications for KDiff3 by Joachim Eibl                                *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
#include "cvsignorelist.h"
#include "TypeUtils.h"

#include <QDir>
#include <QTextStream>

void CvsIgnoreList::init(FileAccess& dir, const t_DirectoryList* pDirList)
{
    static const char* ignorestr = ". .. core RCSLOG tags TAGS RCS SCCS .make.state "
                                   ".nse_depinfo #* .#* cvslog.* ,* CVS CVS.adm .del-* *.a *.olb *.o *.obj "
                                   "*.so *.Z *~ *.old *.elc *.ln *.bak *.BAK *.orig *.rej *.exe _$* *$";
    static const char* varname = "CVSIGNORE";

    addEntriesFromString(QString::fromLatin1(ignorestr));
    addEntriesFromFile(QDir::homePath() + "/.cvsignore");
    if(qEnvironmentVariableIsSet(varname) && !qEnvironmentVariableIsEmpty(varname))
    {
        addEntriesFromString(QString::fromLocal8Bit(qgetenv(varname)));
    }

    const bool bUseLocalCvsIgnore = cvsIgnoreExists(pDirList);
    if(bUseLocalCvsIgnore)
    {
        //TODO: Use QTextStream here.
        FileAccess file(dir);
        file.addPath(".cvsignore");
        qint64 size = file.sizeForReading();
        if(size > 0)
        {
            char* buf = new char[size];
            if(buf != nullptr)
            {
                file.readFile(buf, size);
                qint64 pos1 = 0;
                for(qint64 pos = 0; pos <= size; ++pos)
                {
                    if(pos == size || buf[pos] == ' ' || buf[pos] == '\t' || buf[pos] == '\n' || buf[pos] == '\r')
                    {
                        if(pos > pos1 && pos - pos1 <= TYPE_MAX(QtNumberType))
                        {
                            addEntry(QString::fromLatin1(&buf[pos1], (QtNumberType)(pos - pos1)));
                        }
                        ++pos1;
                    }
                }
                delete[] buf;
            }
        }
    }
}

void CvsIgnoreList::addEntriesFromString(const QString& str)
{
    QtNumberType posLast = 0;
    QtNumberType pos;
    
    while((pos = str.indexOf(' ', posLast)) >= 0)
    {
        if(pos > posLast)
            addEntry(str.mid(posLast, pos - posLast));
        posLast = pos + 1;
    }

    if(posLast < str.length())
        addEntry(str.mid(posLast));
}

void CvsIgnoreList::addEntriesFromFile(const QString& name)
{
    QFile file(name);

    if(file.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&file);
        while(!stream.atEnd())
        {
            addEntriesFromString(stream.readLine());
        }
    }
}

void CvsIgnoreList::addEntry(const QString& pattern)
{
    if(pattern != QString("!"))
    {
        if(pattern.isEmpty()) return;

        // The general match is general but slow.
        // Special tests for '*' and '?' at the beginning or end of a pattern
        // allow fast checks.

        // Count number of '*' and '?'
        unsigned int nofMetaCharacters = 0;

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
            m_exactPatterns.append(pattern);
        }
        else if(nofMetaCharacters == 1)
        {
            if(pattern.at(0) == QChar('*'))
            {
                m_endPatterns.append(pattern.right(pattern.length() - 1));
            }
            else if(pattern.at(pattern.length() - 1) == QChar('*'))
            {
                m_startPatterns.append(pattern.left(pattern.length() - 1));
            }
            else
            {
                m_generalPatterns.append(pattern);
            }
        }
        else
        {
            m_generalPatterns.append(pattern);
        }
    }
    else
    {
        m_exactPatterns.clear();
        m_startPatterns.clear();
        m_endPatterns.clear();
        m_generalPatterns.clear();
    }
}

bool CvsIgnoreList::matches(const QString& text, bool bCaseSensitive) const
{
    if(m_exactPatterns.indexOf(text) >= 0)
    {
        return true;
    }


    for(const QString& startPattern: m_startPatterns)
    {
        if(text.startsWith(startPattern))
        {
            return true;
        }
    }

    for(const QString& endPattern: m_endPatterns)
    {
        if(text.mid(text.length() - endPattern.length()) == endPattern) //(text.endsWith(*it))
        {
            return true;
        }
    }

    /*
    for (QValueList<QCString>::const_iterator it(m_generalPatterns.begin()),
                                              itEnd(m_generalPatterns.end());
         it != itEnd; ++it)
    {
        if (::fnmatch(*it, text.local8Bit(), FNM_PATHNAME) == 0)
        {
            return true;
        }
    }
    */

    for(const QString& globStr : m_generalPatterns)
    {
        QRegExp pattern(globStr, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive, QRegExp::Wildcard);
        if(pattern.exactMatch(text))
            return true;
    }

    return false;
}

bool CvsIgnoreList::cvsIgnoreExists(const t_DirectoryList* pDirList)
{
    t_DirectoryList::const_iterator i;
    for(const FileAccess& dir : *pDirList)
    {
        if(dir.fileName() == ".cvsignore")
            return true;
    }
    return false;
}
