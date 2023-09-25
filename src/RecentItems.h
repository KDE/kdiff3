// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2022 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef RECENTITEMS_H
#define RECENTITEMS_H

#include "TypeUtils.h"

#include <QStringList>

template <unsigned int N>
class RecentItems: public QStringList
{
  public:
    using QStringList::QStringList;

    inline void push_back(const QString &s) = delete;
    inline void append(const QString &) = delete;

    //since prepend is non-virual we must override push_front as well
    inline void push_front(const QString &s) { prepend(s); };

    inline void prepend(const QString &s)
    {
        // If an item exist, remove it from the list and reinsert it at the beginning.
        removeAll(s);

        if(!s.isEmpty()) QStringList::prepend(s);
        if(size() > maxNofRecent) removeLast();
        assert(size() <= maxNofRecent);
    }

  private:
    constexpr static qint32 maxNofRecent = N;
};

#endif /* RECENTITEMS_H */
