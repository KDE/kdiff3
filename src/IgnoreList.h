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

#include "DirectoryList.h"

#include <QString>

class IgnoreList
{
public:
    virtual ~IgnoreList() = default;
    virtual void enterDir(const QString& dir, const DirectoryList& directoryList) = 0;
    [[nodiscard]] virtual bool matches(const QString& dir, const QString& text, bool bCaseSensitive) const = 0;
};

#endif
