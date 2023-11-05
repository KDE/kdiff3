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

class UndoRecord
{
  public:
    UndoRecord(MergeBlock oldData, Selection sel) { mOldData = oldData, mSel = sel; };

  private:
    MergeBlock mOldData;
    Selection mSel;
};

#endif /* UNDORECORD_H */
