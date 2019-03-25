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
#include "Overview.h"
#include "mergeresultwindow.h"
#include "options.h"
#include "diff.h"

#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSize>

Overview::Overview(Options* pOptions)
//: QWidget( pParent, 0, Qt::WNoAutoErase )
{
    m_pDiff3LineList = nullptr;
    m_pOptions = pOptions;
    m_bTripleDiff = false;
    m_eOverviewMode = eOMNormal;
    m_nofLines = 1;
    setUpdatesEnabled(false);
    m_firstLine = 0;
    m_pageHeight = 0;

    setFixedWidth(20);
}

void Overview::init(Diff3LineList* pDiff3LineList, bool bTripleDiff)
{
    m_pDiff3LineList = pDiff3LineList;
    m_bTripleDiff = bTripleDiff;
    m_pixmap = QPixmap(QSize(0, 0)); // make sure that a redraw happens
    update();
}

void Overview::reset()
{
    m_pDiff3LineList = nullptr;
}

void Overview::slotRedraw()
{
    m_pixmap = QPixmap(QSize(0, 0)); // make sure that a redraw happens
    update();
}

void Overview::setRange(int firstLine, int pageHeight)
{
    m_firstLine = firstLine;
    m_pageHeight = pageHeight;
    update();
}
void Overview::setFirstLine(int firstLine)
{
    m_firstLine = firstLine;
    update();
}

void Overview::setOverviewMode(e_OverviewMode eOverviewMode)
{
    m_eOverviewMode = eOverviewMode;
    slotRedraw();
}

Overview::e_OverviewMode Overview::getOverviewMode()
{
    return m_eOverviewMode;
}

void Overview::mousePressEvent(QMouseEvent* e)
{
    int h = height() - 1;
    int h1 = h * m_pageHeight / std::max(1, m_nofLines) + 3;
    if(h > 0)
        emit setLine((e->y() - h1 / 2) * m_nofLines / h);
}

void Overview::mouseMoveEvent(QMouseEvent* e)
{
    mousePressEvent(e);
}

void Overview::setPaintingAllowed(bool bAllowPainting)
{
    if(updatesEnabled() != bAllowPainting)
    {

        setUpdatesEnabled(bAllowPainting);
        if(bAllowPainting)
            update();
        else
            reset();
    }
}

void Overview::drawColumn(QPainter& p, e_OverviewMode eOverviewMode, int x, int w, int h, int nofLines)
{
    p.setPen(Qt::black);
    p.drawLine(x, 0, x, h);

    if(nofLines == 0) return;

    int line = 0;
    int oldY = 0;
    int oldConflictY = -1;
    int wrapLineIdx = 0;
    Diff3LineList::const_iterator i;
    for(i = m_pDiff3LineList->begin(); i != m_pDiff3LineList->end();)
    {
        const Diff3Line& d3l = *i;
        int y = h * (line + 1) / nofLines;
        e_MergeDetails md;
        bool bConflict;
        bool bLineRemoved;
        e_SrcSelector src;
        d3l.mergeOneLine(md, bConflict, bLineRemoved, src, !m_bTripleDiff);

        QColor c = m_pOptions->m_bgColor;
        bool bWhiteSpaceChange = false;
        //if( bConflict )  c=m_pOptions->m_colorForConflict;
        //else
        if(eOverviewMode == eOMNormal)
        {
            switch(md)
            {
            case eDefault:
            case eNoChange:
                c = m_pOptions->m_bgColor;
                break;

            case eBAdded:
            case eBDeleted:
            case eBChanged:
                c = bConflict ? m_pOptions->m_colorForConflict : m_pOptions->m_colorB;
                bWhiteSpaceChange = d3l.isEqualAB() || (d3l.bWhiteLineA && d3l.bWhiteLineB);
                break;

            case eCAdded:
            case eCDeleted:
            case eCChanged:
                bWhiteSpaceChange = d3l.isEqualAC() || (d3l.bWhiteLineA && d3l.bWhiteLineC);
                c = bConflict ? m_pOptions->m_colorForConflict : m_pOptions->m_colorC;
                break;

            case eBCChanged:         // conflict
            case eBCChangedAndEqual: // possible conflict
            case eBCDeleted:         // possible conflict
            case eBChanged_CDeleted: // conflict
            case eCChanged_BDeleted: // conflict
            case eBCAdded:           // conflict
            case eBCAddedAndEqual:   // possible conflict
                c = m_pOptions->m_colorForConflict;
                break;
            default:
                Q_ASSERT(true);
                break;
            }
        }
        else if(eOverviewMode == eOMAvsB)
        {
            switch(md)
            {
            case eDefault:
            case eNoChange:
            case eCAdded:
            case eCDeleted:
            case eCChanged:
                break;
            default:
                c = m_pOptions->m_colorForConflict;
                bWhiteSpaceChange = d3l.isEqualAB() || (d3l.bWhiteLineA && d3l.bWhiteLineB);
                break;
            }
        }
        else if(eOverviewMode == eOMAvsC)
        {
            switch(md)
            {
            case eDefault:
            case eNoChange:
            case eBAdded:
            case eBDeleted:
            case eBChanged:
                break;
            default:
                c = m_pOptions->m_colorForConflict;
                bWhiteSpaceChange = d3l.isEqualAC() || (d3l.bWhiteLineA && d3l.bWhiteLineC);
                break;
            }
        }
        else if(eOverviewMode == eOMBvsC)
        {
            switch(md)
            {
            case eDefault:
            case eNoChange:
            case eBCChangedAndEqual:
            case eBCDeleted:
            case eBCAddedAndEqual:
                break;
            default:
                c = m_pOptions->m_colorForConflict;
                bWhiteSpaceChange = d3l.isEqualBC() || (d3l.bWhiteLineB && d3l.bWhiteLineC);
                break;
            }
        }

        int x2 = x;
        int w2 = w;

        if(!m_bTripleDiff)
        {
            if(!d3l.getLineA().isValid() && d3l.getLineB().isValid())
            {
                c = m_pOptions->m_colorA;
                x2 = w / 2;
                w2 = x2;
            }
            if(d3l.getLineA().isValid() && !d3l.getLineB().isValid())
            {
                c = m_pOptions->m_colorB;
                w2 = w / 2;
            }
        }

        if(!bWhiteSpaceChange || m_pOptions->m_bShowWhiteSpace)
        {
            // Make sure that lines with conflict are not overwritten.
            if(c == m_pOptions->m_colorForConflict)
            {
                p.fillRect(x2 + 1, oldY, w2, std::max(1, y - oldY), bWhiteSpaceChange ? QBrush(c, Qt::Dense4Pattern) : QBrush(c));
                oldConflictY = oldY;
            }
            else if(c != m_pOptions->m_bgColor && oldY > oldConflictY)
            {
                p.fillRect(x2 + 1, oldY, w2, std::max(1, y - oldY), bWhiteSpaceChange ? QBrush(c, Qt::Dense4Pattern) : QBrush(c));
            }
        }

        oldY = y;

        ++line;
        if(m_pOptions->m_bWordWrap)
        {
            ++wrapLineIdx;
            if(wrapLineIdx >= d3l.linesNeededForDisplay)
            {
                wrapLineIdx = 0;
                ++i;
            }
        }
        else
        {
            ++i;
        }
    }
}

void Overview::paintEvent(QPaintEvent*)
{
    if(m_pDiff3LineList == nullptr) return;
    int h = height() - 1;
    int w = width();

    if(m_pixmap.size() != size())
    {
        if(m_pOptions->m_bWordWrap)
        {
            m_nofLines = 0;
            Diff3LineList::const_iterator i;
            for(i = m_pDiff3LineList->begin(); i != m_pDiff3LineList->end(); ++i)
            {
                m_nofLines += i->linesNeededForDisplay;
            }
        }
        else
        {
            m_nofLines = m_pDiff3LineList->size();
        }

        m_pixmap = QPixmap(size());

        QPainter p(&m_pixmap);
        p.fillRect(rect(), m_pOptions->m_bgColor);

        if(!m_bTripleDiff || m_eOverviewMode == eOMNormal)
        {
            drawColumn(p, eOMNormal, 0, w, h, m_nofLines);
        }
        else
        {
            drawColumn(p, eOMNormal, 0, w / 2, h, m_nofLines);
            drawColumn(p, m_eOverviewMode, w / 2, w / 2, h, m_nofLines);
        }
    }

    QPainter painter(this);
    painter.drawPixmap(0, 0, m_pixmap);
    int y1=0, h1=0;
    if(m_nofLines > 0)
    {
        y1 = h * m_firstLine / m_nofLines - 1;
        h1 = h * m_pageHeight / m_nofLines + 3;
    }
    painter.setPen(Qt::black);
    painter.drawRect(1, y1, w - 1, h1);
}
