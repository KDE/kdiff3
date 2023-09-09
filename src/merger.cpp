// clang-format off
/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

#include "merger.h"

#include <list>
#include <memory>

Merger::Merger(const std::shared_ptr<DiffList>& pDiffList1, const std::shared_ptr<DiffList>& pDiffList2):
    md1(pDiffList1, 0), md2(pDiffList2, 1)
{
}

Merger::MergeData::MergeData(const std::shared_ptr<DiffList>& p, qint32 i)
{
    idx = i;
    pDiffList = p;
    if(p != nullptr)
    {
        it = p->begin();
        update();
    }
}

bool Merger::MergeData::eq() const
{
    return pDiffList == nullptr || d.numberOfEquals() > 0;
}

bool Merger::MergeData::isEnd() const
{
    return (pDiffList == nullptr || (it == pDiffList->end() && d.numberOfEquals() == 0 &&
                                     (idx == 0 ? d.diff1() == 0 : d.diff2() == 0)));
}

void Merger::MergeData::update()
{
    if(d.numberOfEquals() > 0)
        d.adjustNumberOfEquals(-1);
    else if(idx == 0 && d.diff1() > 0)
        d.adjustDiff1(-1);
    else if(idx == 1 && d.diff2() > 0)
        d.adjustDiff2(-1);

    while(d.numberOfEquals() == 0 && ((idx == 0 && d.diff1() == 0) || (idx == 1 && d.diff2() == 0)) && pDiffList != nullptr && it != pDiffList->end())
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

ChangeFlags Merger::whatChanged()
{
    ChangeFlags changed = ChangeFlag::NoChange;
    changed |= md1.eq() ? ChangeFlag::NoChange : ChangeFlag::AChanged;
    changed |= md2.eq() ? ChangeFlag::NoChange : ChangeFlag::BChanged;
    return changed;
}

bool Merger::isEndReached()
{
    return md1.isEnd() && md2.isEnd();
}
