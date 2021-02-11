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
#ifndef CVSIGNORELIST_H
#define CVSIGNORELIST_H

#include "fileaccess.h"

#include <QString>
#include <QStringList>

class CvsIgnoreList
{
  public:
    CvsIgnoreList() = default;
    void init(FileAccess& dir, const t_DirectoryList* pDirList);
    bool matches(const QString& text, bool bCaseSensitive) const;

    virtual ~CvsIgnoreList() = default;

  private:
    friend class CvsIgnoreListTest;

    //The name of the users global ingore
    inline virtual const QString getGlobalIgnoreName() const { return getIgnoreName(); }
    inline virtual const char* getVarName() const { return "CVSIGNORE";}
    inline virtual const QString getIgnoreName() const { return QStringLiteral(".cvsignore");}
    bool ignoreExists(const t_DirectoryList* pDirList);

    void addEntriesFromString(const QString& str);
    void addEntriesFromFile(const QString& name);
    void addEntry(const QString& pattern);

    QStringList m_exactPatterns;
    QStringList m_startPatterns;
    QStringList m_endPatterns;
    QStringList m_generalPatterns;
};

#endif
