/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2007  Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MERGER_H
#define MERGER_H

#include "diff.h"

class Merger
{
  public:
    Merger(const DiffList* pDiffList1, const DiffList* pDiffList2);

    /** Go one step. */
    void next();

    /** Information about what changed. Can be used for coloring.
       The return value is 0 if nothing changed here,
       bit 1 is set if a difference from pDiffList1 was detected,
       bit 2 is set if a difference from pDiffList2 was detected.
   */
    ChangeFlags whatChanged();

    /** End of both diff lists reached. */
    bool isEndReached();

  private:
    class MergeData
    {
      private:
        DiffList::const_iterator it;
        const DiffList* pDiffList = nullptr;
        Diff d;
        int idx;

      public:
        MergeData(const DiffList* p, int i);
        bool eq() const;
        void update();
        bool isEnd() const;
    };

    MergeData md1;
    MergeData md2;
};

#endif
