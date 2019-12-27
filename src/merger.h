/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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
