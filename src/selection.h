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

#include <algorithm>  // for max, min
#include <QtGlobal>

class Selection
{
public:
  Selection() = default;
private:
  LineRef firstLine;
  LineRef lastLine;

  qint32 firstPos = -1;
  qint32 lastPos = -1;

  LineRef oldFirstLine;
  LineRef oldLastLine;
public:
//private:
  bool bSelectionContainsData = false;
public:
  [[nodiscard]] inline LineRef getFirstLine() const { return firstLine; };
  [[nodiscard]] inline LineRef getLastLine() const { return lastLine; };

  [[nodiscard]] inline qint32 getFirstPos() const { return firstPos; };
  [[nodiscard]] inline qint32 getLastPos() const { return lastPos; };

  inline bool isValidFirstLine() { return firstLine.isValid(); }
  inline void clearOldSelection() { oldLastLine.invalidate(), oldFirstLine.invalidate(); };

  [[nodiscard]] inline LineRef getOldLastLine() const { return oldLastLine; };
  [[nodiscard]] inline LineRef getOldFirstLine() const  { return oldFirstLine; };
  [[nodiscard]] inline bool selectionContainsData() const { return bSelectionContainsData; };
  [[nodiscard]] bool isEmpty() const { return !firstLine.isValid() || (firstLine == lastLine && firstPos == lastPos) || !bSelectionContainsData; }
  void reset()
  {
      oldLastLine = lastLine;
      oldFirstLine = firstLine;
      firstLine.invalidate();
      lastLine.invalidate();
      bSelectionContainsData = false;
   }
   void start( LineRef l, qint32 p ) { firstLine = l; firstPos = p; }
   void end( LineRef l, qint32 p )  {
      if ( !oldLastLine.isValid() )
         oldLastLine = lastLine;
      lastLine  = l;
      lastPos  = p;
      //bSelectionContainsData = (firstLine == lastLine && firstPos == lastPos);
   }
   [[nodiscard]] bool within( LineRef l, qint32 p ) const;

   [[nodiscard]] bool lineWithin( LineRef l ) const;
   [[nodiscard]] qint32 firstPosInLine(LineRef l) const;
   [[nodiscard]] qint32 lastPosInLine(LineRef l) const;

   [[nodiscard]] LineRef beginLine() const
   {
      if (!firstLine.isValid() && !lastLine.isValid()) return LineRef();
      return std::max((LineRef)0,std::min(firstLine,lastLine));
   }

   [[nodiscard]] LineRef endLine() const
   {
      if (!firstLine.isValid() && !lastLine.isValid()) return LineRef();
      return std::max(firstLine,lastLine);
   }

   [[nodiscard]] qint32 beginPos() const { return firstLine==lastLine ? std::min(firstPos,lastPos) :
                           firstLine<lastLine ? (!firstLine.isValid()?0:firstPos) : (!lastLine.isValid()?0:lastPos);  }
   [[nodiscard]] qint32 endPos() const { return firstLine==lastLine ? std::max(firstPos,lastPos) :
                           firstLine<lastLine ? lastPos : firstPos;      }
};

#endif
