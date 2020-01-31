/*
 * KDiff3 - Text Diff And Merge Tool
 * 
 * SPDX-FileCopyrightText: 2002-2007  Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef SELECTION_H
#define SELECTION_H

#include "LineRef.h"

#include <algorithm>  // for max, min

class Selection
{
public:
  Selection(){}
private:
  const LineRef invalidRef = -1;
  LineRef firstLine = invalidRef;
  LineRef lastLine = invalidRef;

  int firstPos = -1;
  int lastPos = -1;

  LineRef oldFirstLine = invalidRef;
  LineRef oldLastLine = invalidRef;
public:
//private:
  bool bSelectionContainsData = false;
public:
  inline LineRef getFirstLine() { return firstLine; };
  inline LineRef getLastLine() { return lastLine; };

  inline int getFirstPos() const { return firstPos; };
  inline int getLastPos() const { return lastPos; };

  inline bool isValidFirstLine() { return firstLine != invalidRef; }
  inline void clearOldSelection() { oldLastLine = invalidRef, oldFirstLine = invalidRef; };

  inline LineRef getOldLastLine() { return oldLastLine; };
  inline LineRef getOldFirstLine() { return oldFirstLine; };
  inline bool selectionContainsData() const { return bSelectionContainsData; };
  bool isEmpty() { return firstLine == invalidRef || (firstLine == lastLine && firstPos == lastPos) || !bSelectionContainsData; }
  void reset()
  {
      oldLastLine = lastLine;
      oldFirstLine = firstLine;
      firstLine = invalidRef;
      lastLine = invalidRef;
      bSelectionContainsData = false;
   }
   void start( LineRef l, int p ) { firstLine = l; firstPos = p; }
   void end( LineRef l, int p )  {
      if ( oldLastLine == invalidRef )
         oldLastLine = lastLine;
      lastLine  = l;
      lastPos  = p;
      //bSelectionContainsData = (firstLine == lastLine && firstPos == lastPos);
   }
   bool within( LineRef l, LineRef p );

   bool lineWithin( LineRef l );
   int firstPosInLine(LineRef l);
   int lastPosInLine(LineRef l);
   LineRef beginLine(){
      if (firstLine<0 && lastLine<0) return invalidRef;
      return std::max((LineRef)0,std::min(firstLine,lastLine));
   }
   LineRef endLine(){
      if (firstLine<0 && lastLine<0) return invalidRef;
      return std::max(firstLine,lastLine);
   }
   int beginPos() { return firstLine==lastLine ? std::min(firstPos,lastPos) :
                           firstLine<lastLine ? (firstLine<0?0:firstPos) : (lastLine<0?0:lastPos);  }
   int endPos()   { return firstLine==lastLine ? std::max(firstPos,lastPos) :
                           firstLine<lastLine ? lastPos : firstPos;      }
};

#endif
