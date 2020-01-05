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
#ifndef CVSIGNORELIST_H
#define CVSIGNORELIST_H

#ifndef AUTOTEST
#include "fileaccess.h"
#else
#include "MocIgnoreFile.h"
#endif

#include <QString>
#include <QStringList>

class CvsIgnoreList
{
  public:
    CvsIgnoreList() {}
    void init(FileAccess& dir, const t_DirectoryList* pDirList);
    bool matches(const QString& text, bool bCaseSensitive) const;

  private:
    friend class CvsIgnoreListTest;
    bool cvsIgnoreExists(const t_DirectoryList* pDirList);

    void addEntriesFromString(const QString& str);
    void addEntriesFromFile(const QString& name);
    void addEntry(const QString& pattern);

    QStringList m_exactPatterns;
    QStringList m_startPatterns;
    QStringList m_endPatterns;
    QStringList m_generalPatterns;
};

#endif
