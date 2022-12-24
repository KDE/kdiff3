// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2022 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef RECENTFILELIST_H
#define RECENTFILELIST_H

#include <QStringList>

class RecentFileList: public QStringList
{
  public:
    using QStringList::QStringList;

    inline void addFile(const QString &file)
    {
        // If an item exist, remove it from the list and reinsert it at the beginning.
        removeAll(file);

        if(!file.isEmpty()) prepend(file);
        if(count() > maxNofRecentFiles) erase(begin() + maxNofRecentFiles, end());
    }

  private:
    constexpr static int maxNofRecentFiles = 10;
};

#endif /* RECENTFILELIST_H */
