/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "MergeEditLine.h"

QString MergeEditLine::getString(const std::shared_ptr<LineDataVector> &pLineDataA, const std::shared_ptr<LineDataVector> &pLineDataB, const std::shared_ptr<LineDataVector> &pLineDataC) const
{
    //Triggered by resize event during early init. Ignore these calls.
    if((m_src == e_SrcSelector::A && pLineDataA->empty()) || (m_src == e_SrcSelector::B && pLineDataB->empty()) || (m_src == e_SrcSelector::C && pLineDataC->empty()))
        return QString();

    if(isRemoved())
    {
        return QString();
    }

    if(!isModified())
    {
        e_SrcSelector src = m_src;
        if(src == e_SrcSelector::None)
        {
            return QString();
        }
        const Diff3Line& d3l = *m_id3l;
        const LineData* pld = nullptr;
        Q_ASSERT(src == e_SrcSelector::A || src == e_SrcSelector::B || src == e_SrcSelector::C);

        if(src == e_SrcSelector::A && d3l.getLineA().isValid())
            pld = &(*pLineDataA)[d3l.getLineA()];
        else if(src == e_SrcSelector::B && d3l.getLineB().isValid())
            pld = &(*pLineDataB)[d3l.getLineB()];
        else if(src == e_SrcSelector::C && d3l.getLineC().isValid())
            pld = &(*pLineDataC)[d3l.getLineC()];

        //Not an error.
        if(pld == nullptr)
        {
            return QString();
        }

        return pld->getLine();
    }
    else
    {
        return m_str;
    }
    return QString();
}
