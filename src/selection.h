// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on
#ifndef SELECTION_H
#define SELECTION_H

#include "LineRef.h"

#include <algorithm> // for max, min
#include <QtGlobal>

class Selection
{
  public:
    Selection() = default;

  private:
    LineRef firstLine;
    LineRef lastLine;

    qsizetype firstPos = -1;
    qsizetype lastPos = -1;

    LineRef oldFirstLine;
    LineRef oldLastLine;

  public:
    [[nodiscard]] LineRef getFirstLine() const { return firstLine; };
    [[nodiscard]] LineRef getLastLine() const { return lastLine; };

    [[nodiscard]] qsizetype getFirstPos() const { return firstPos; };
    [[nodiscard]] qsizetype getLastPos() const { return lastPos; };

    [[nodiscard]] bool isValidFirstLine() const { return firstLine.isValid(); }
    void clearOldSelection() { oldLastLine.invalidate(), oldFirstLine.invalidate(); };

    [[nodiscard]] LineRef getOldLastLine() const { return oldLastLine; };
    [[nodiscard]] LineRef getOldFirstLine() const { return oldFirstLine; };
    [[nodiscard]] bool isEmpty() const { return !firstLine.isValid() || (firstLine == lastLine && firstPos == lastPos); }

    void reset()
    {
        oldLastLine = lastLine;
        oldFirstLine = firstLine;
        firstLine.invalidate();
        lastLine.invalidate();
    }

    void start(LineRef l, qsizetype p)
    {
        firstLine = l;
        firstPos = p;
    }

    void end(LineRef l, qsizetype p)
    {
        if(!oldLastLine.isValid())
            oldLastLine = lastLine;
        lastLine = l;
        lastPos = p;
    }

    [[nodiscard]] bool within(LineRef l, qsizetype p) const;
    [[nodiscard]] bool lineWithin(LineRef l) const;

    [[nodiscard]] qsizetype firstPosInLine(LineRef l) const;
    [[nodiscard]] qsizetype lastPosInLine(LineRef l) const;

    [[nodiscard]] LineRef beginLine() const
    {
        if(!firstLine.isValid() && !lastLine.isValid()) return LineRef();
        return std::max((LineRef)0, std::min(firstLine, lastLine));
    }

    [[nodiscard]] LineRef endLine() const
    {
        if(!firstLine.isValid() && !lastLine.isValid()) return LineRef();
        return std::max(firstLine, lastLine);
    }

    [[nodiscard]] qsizetype beginPos() const { return firstLine == lastLine ? std::min(firstPos, lastPos) :
                                                      firstLine < lastLine  ? (!firstLine.isValid() ? 0 : firstPos) :
                                                                              (!lastLine.isValid() ? 0 : lastPos); }
    [[nodiscard]] qsizetype endPos() const { return firstLine == lastLine ? std::max(firstPos, lastPos) :
                                                    firstLine < lastLine  ? lastPos :
                                                                            firstPos; }
};

#endif
