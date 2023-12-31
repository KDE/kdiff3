/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2023 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef UNDORECORD_H
#define UNDORECORD_H

#include "MergeEditLine.h"
#include "selection.h"

#include <deque>

class UndoRecord
{
  public:
    UndoRecord(Selection sel, MergeBlockList::iterator start)
    {
        mSel = sel;
        mEnd = mStart = start;
    };

    void push(const MergeBlock& mb) { savedLines.push_back(mb); }
    void push(const MergeBlock&& mb)
    {
        savedLines.push_back(mb);
        ++mEnd;
    }

    const MergeBlock& pop()
    {
        const MergeBlock& mb = savedLines.front();
        savedLines.pop_front();
        return mb;
    }

    void undo()
    {
        std::deque<MergeBlock>::iterator savedRec = savedLines.begin();
        for(MergeBlockList::iterator it = mStart; it == mEnd; it++)
        {
            std::swap(*it, *savedRec);
            savedRec++;
        }
    }

  private:
    MergeBlockList::iterator mStart, mEnd;
    Selection mSel;
    std::deque<MergeBlock> savedLines;
};

#endif /* UNDORECORD_H */
