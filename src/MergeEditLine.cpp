/***************************************************************************
 *   Copyright (C) 2002-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "MergeEditLine.h"

QString MergeEditLine::getString(const QVector<LineData>* pLineDataA, const QVector<LineData>* pLineDataB, const QVector<LineData>* pLineDataC)
{
    if(isRemoved())
    {
        return QString();
    }

    if(!isModified())
    {
        int src = m_src;
        if(src == 0)
        {
            return QString();
        }
        const Diff3Line& d3l = *m_id3l;
        const LineData* pld = nullptr;
        Q_ASSERT(src == A || src == B || src == C);
        if(src == A && d3l.getLineA().isValid())
            pld = &(*pLineDataA)[d3l.getLineA()];
        else if(src == B && d3l.getLineB().isValid())
            pld = &(*pLineDataB)[d3l.getLineB()];
        else if(src == C && d3l.getLineC().isValid())
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
