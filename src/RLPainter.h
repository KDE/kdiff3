/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef RLPAINTER_H
#define RLPAINTER_H

#include <QPainter>
// Helper class that swaps left and right for some commands.

/*
    This class assists with changing KDiff3 between RTL and LTR layout.
*/
class RLPainter : public QPainter
{
  private:
    int m_xOffset;
    int m_fontWidth;
    bool bRightToLeft = false;

  public:
    RLPainter(QPaintDevice* pd, bool bRTL, int width, int fontWidth)
        : QPainter(pd)
    {
        bRightToLeft = bRTL;
        if(bRTL)
        {
            m_fontWidth = fontWidth;
            m_xOffset = width - 1;
        }
        else
        {
            m_fontWidth = 0;
            m_xOffset = 0;
        }
    }

    void fillRect(int x, int y, int w, int h, const QBrush& b)
    {
        if(!bRightToLeft)
            QPainter::fillRect(m_xOffset + x, y, w, h, b);
        else
            QPainter::fillRect(m_xOffset - x - w, y, w, h, b);
    }

    void drawText(int x, int y, const QString& s, bool bAdapt = false)
    {
        Qt::LayoutDirection ld = (!bRightToLeft || !bAdapt) ? Qt::LeftToRight : Qt::RightToLeft;
        // Qt will automaticly reverse the text as needed just set the layout direction
        QPainter::setLayoutDirection(ld);
        if(ld == Qt::RightToLeft)
        {
            QPainter::drawText(m_xOffset - m_fontWidth * s.length() - x, y, s);
            return;
        }
        QPainter::drawText(m_xOffset - m_fontWidth * s.length() + x, y, s);
    }

    void drawLine(int x1, int y1, int x2, int y2)
    {
        if(bRightToLeft)
            QPainter::drawLine(m_xOffset - x1, y1, m_xOffset - x2, y2);
        else
            QPainter::drawLine(m_xOffset + x1, y1, m_xOffset + x2, y2);
    }
};

#endif
