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

#include <QList>

template <unsigned int N, typename T = QString>
class RecentItems: public QList<T>
{
  public:
    using QList<T>::QList;
    using QList<T>::removeAll;
    using QList<T>::size;
    using QList<T>::removeLast;

    void push_back(const T &s) = delete;
    void append(const T &) = delete;

    //since prepend is non-virual we must override push_front as well
    void push_front(const T &s) { prepend(s); };

    void prepend(const T &s)
    {
        // If an item exist, remove it from the list and reinsert it at the beginning.
        removeAll(s);

        if(!s.isEmpty()) QList<T>::prepend(s);
        if(size() > maxNofRecent) removeLast();
        assert(size() <= maxNofRecent);
    }

  private:
    constexpr static qint32 maxNofRecent = N;
};

#endif /* RECENTITEMS_H */
