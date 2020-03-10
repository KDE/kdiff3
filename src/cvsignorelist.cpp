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

#include <QDir>
#include <QTextStream>
#include <qregexp.h>

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
        FileAccess file(dir);
        file.addPath(".cvsignore");
        qint64 size = file.exists() ? file.sizeForReading() : 0;
        if(size > 0 && size <= (qint64)std::numeric_limits<int>::max())
        {
            char* buf = new char[size];
            if(buf != nullptr)
            {
                file.readFile(buf, size);
                int pos1 = 0;
                for(int pos = 0; pos <= size; ++pos)
                {
                    if(pos == size || buf[pos] == ' ' || buf[pos] == '\t' || buf[pos] == '\n' || buf[pos] == '\r')
                    {
                        if(pos > pos1)
                        {
                            addEntry(QString::fromLatin1(&buf[pos1], pos - pos1));
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
    int posLast(0);
    int pos;
    while((pos = str.indexOf(' ', posLast)) >= 0)
    {
        if(pos > posLast)
            addEntry(str.mid(posLast, pos - posLast));
        posLast = pos + 1;
    }

    if(posLast < static_cast<int>(str.length()))
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
    /*
        Don't let the compilier create a temporary QRegExp explictly create one to prevent a possiable crash
        QString::indexOf and therefor QStringList::indexOf may modify the QRegExp passed to it.
    */
    QRegExp regexp(text, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
    if(m_exactPatterns.indexOf(regexp) >= 0)
    {
        return true;
    }

    QStringList::ConstIterator it;
    QStringList::ConstIterator itEnd;
    for(it = m_startPatterns.begin(), itEnd = m_startPatterns.end(); it != itEnd; ++it)
    {
        if(text.startsWith(*it, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive))
        {
            return true;
        }
    }

    for(it = m_endPatterns.begin(), itEnd = m_endPatterns.end(); it != itEnd; ++it)
    {
        if(text.midRef(text.length() - (*it).length()).compare(*it, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive) == 0) //(text.endsWith(*it))
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

    for(it = m_generalPatterns.begin(); it != m_generalPatterns.end(); ++it)
    {
        QRegExp pattern(*it, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive, QRegExp::Wildcard);
        if(pattern.exactMatch(text))
            return true;
    }

    return false;
}

bool CvsIgnoreList::cvsIgnoreExists(const t_DirectoryList* pDirList)
{
    t_DirectoryList::const_iterator i;
    for(i = pDirList->begin(); i != pDirList->end(); ++i)
    {
        if(i->fileName() == ".cvsignore")
            return true;
    }
    return false;
}
