/***************************************************************************
                          merger.h  -  description
                             -------------------
    begin                : Sun Mar 24 2002
    copyright            : (C) 2002-2007 by Joachim Eibl
    email                : joachim.eibl at gmx.de
 ***************************************************************************/

/***************************************************************************
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

   Merger( const DiffList* pDiffList1, const DiffList* pDiffList2 );

   /** Go one step. */
   void next();

   /** Information about what changed. Can be used for coloring.
       The return value is 0 if nothing changed here,
       bit 1 is set if a difference from pDiffList1 was detected,
       bit 2 is set if a difference from pDiffList2 was detected.
   */
   int whatChanged();

   /** End of both diff lists reached. */
   bool isEndReached();
private:

   struct MergeData
   {
      DiffList::const_iterator it;
      const DiffList* pDiffList;
      Diff d;
      int idx;
    
      MergeData( const DiffList* p, int i );
      bool eq();
      void update();
      bool isEnd();
   };

   MergeData md1;
   MergeData md2;
};

#endif
