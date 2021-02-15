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
#ifndef IGNORELIST_H
#define IGNORELIST_H

#include "fileaccess.h"

#include <QString>
#include <QStringList>

class IgnoreList
{
  public:
    IgnoreList() = default;
    void init(FileAccess& dir, const t_DirectoryList* pDirList);
    [[nodiscard]] bool matches(const QString& text, bool bCaseSensitive) const;

    virtual ~IgnoreList() = default;

  protected:
    bool ignoreExists(const t_DirectoryList* pDirList);

    void addEntriesFromString(const QString& str);
    virtual void addEntriesFromFile(const QString& name);
    void addEntry(const QString& pattern);

    QStringList m_exactPatterns;
    QStringList m_startPatterns;
    QStringList m_endPatterns;
    QStringList m_generalPatterns;

  private:
    /*
        The name of the users global ignore can be changed separately in some cases in the future
        kdiff will handle this through a user settings.
        For now just return the same thing as gerIngoreName. That works
    */
    [[nodiscard]] inline virtual const QString getGlobalIgnoreName() const { return getIgnoreName(); }
    [[nodiscard]] virtual const char* getVarName() const = 0;
    [[nodiscard]] virtual const QString getIgnoreName() const = 0;

};

#endif
