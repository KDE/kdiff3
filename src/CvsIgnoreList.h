/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef CVSIGNORELIST_H
#define CVSIGNORELIST_H

#include "fileaccess.h"

#include "IgnoreList.h"

#include <QString>
#include <QStringList>

class CvsIgnoreList: public IgnoreList
{
  private:
    friend class CvsIgnoreListTest;
    [[nodiscard]] inline const char* getVarName() const override { return "CVSIGNORE"; }
    [[nodiscard]] inline const QString getIgnoreName() const override { return QStringLiteral(".cvsignore"); }
};

#endif /* CVSIGNORELIST_H */
