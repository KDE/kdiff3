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

#ifndef RLPAINTER_H
#define RLPAINTER_H

#include <QPainter>
// Helper class that swaps left and right for some commands.
class RLPainter : public QPainter
{
    int m_factor;
    int m_xOffset;
    int m_fontWidth;

  public:
    RLPainter(QPaintDevice* pd, bool bRTL, int width, int fontWidth)
        : QPainter(pd)
    {
        if(bRTL)
        {
            m_fontWidth = fontWidth;
            m_factor = -1;
            m_xOffset = width - 1;
        }
        else
        {
            m_fontWidth = 0;
            m_factor = 1;
            m_xOffset = 0;
        }
    }

    void fillRect(int x, int y, int w, int h, const QBrush& b)
    {
        if(m_factor == 1)
            QPainter::fillRect(m_xOffset + x, y, w, h, b);
        else
            QPainter::fillRect(m_xOffset - x - w, y, w, h, b);
    }

    void drawText(int x, int y, const QString& s, bool bAdapt = false)
    {
        Qt::LayoutDirection ld = (m_factor == 1 || !bAdapt) ? Qt::LeftToRight : Qt::RightToLeft;
        //QPainter::setLayoutDirection( ld );
        if(ld == Qt::RightToLeft) // Reverse the text
        {
            QString s2;
            for(int i = s.length() - 1; i >= 0; --i)
            {
                s2 += s[i];
            }
            QPainter::drawText(m_xOffset - m_fontWidth * s.length() + m_factor * x, y, s2);
            return;
        }
        QPainter::drawText(m_xOffset - m_fontWidth * s.length() + m_factor * x, y, s);
    }

    void drawLine(int x1, int y1, int x2, int y2)
    {
        QPainter::drawLine(m_xOffset + m_factor * x1, y1, m_xOffset + m_factor * x2, y2);
    }
};

#endif
