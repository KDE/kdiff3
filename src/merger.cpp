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

#include "merger.h"

#include <list>

Merger::Merger(const DiffList* pDiffList1, const DiffList* pDiffList2)
    : md1(pDiffList1, 0), md2(pDiffList2, 1)
{
}

Merger::MergeData::MergeData(const DiffList* p, int i)
    : d(0, 0, 0)
{
    idx = i;
    pDiffList = p;
    if(p != nullptr)
    {
        it = p->begin();
        update();
    }
}

bool Merger::MergeData::eq()
{
    return pDiffList == nullptr || d.nofEquals > 0;
}

bool Merger::MergeData::isEnd()
{
    return (pDiffList == nullptr || (it == pDiffList->end() && d.nofEquals == 0 &&
                                     (idx == 0 ? d.diff1 == 0 : d.diff2 == 0)));
}

void Merger::MergeData::update()
{
    if(d.nofEquals > 0)
        --d.nofEquals;
    else if(idx == 0 && d.diff1 > 0)
        --d.diff1;
    else if(idx == 1 && d.diff2 > 0)
        --d.diff2;

    while(d.nofEquals == 0 && ((idx == 0 && d.diff1 == 0) || (idx == 1 && d.diff2 == 0)) && pDiffList != nullptr && it != pDiffList->end())
    {
        d = *it;
        ++it;
    }
}

void Merger::next()
{
    md1.update();
    md2.update();
}

int Merger::whatChanged()
{
    int changed = 0;
    changed |= md1.eq() ? 0 : 1;
    changed |= md2.eq() ? 0 : 2;
    return changed;
}

bool Merger::isEndReached()
{
    return md1.isEnd() && md2.isEnd();
}
