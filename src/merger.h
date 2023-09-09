// clang-format off
/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

#ifndef MERGER_H
#define MERGER_H

#include "diff.h"

#include <memory>

class Merger
{
  public:
    Merger(const std::shared_ptr<DiffList>& pDiffList1, const std::shared_ptr<DiffList>& pDiffList2);

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
        std::shared_ptr<DiffList> pDiffList;
        Diff d;
        qint32 idx;

      public:
        MergeData(const std::shared_ptr<DiffList>& p, qint32 i);
        [[nodiscard]] bool eq() const;
        void update();
        [[nodiscard]] bool isEnd() const;
    };

    MergeData md1;
    MergeData md2;
};

#endif
