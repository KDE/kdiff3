/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "Overview.h"

#include "diff.h"
#include "kdiff3.h"
#include "options.h"

#include <algorithm> // for max

#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QSize>

Overview::Overview(const QSharedPointer<Options>& pOptions)
//: QWidget( pParent, 0, Qt::WNoAutoErase )
{
    m_pDiff3LineList = nullptr;
    m_pOptions = pOptions;
    mOverviewMode = e_OverviewMode::eOMNormal;
    m_nofLines = 1;
    setUpdatesEnabled(false);
    m_firstLine = 0;
    m_pageHeight = 0;

    setFixedWidth(20);
}

void Overview::init(Diff3LineList* pDiff3LineList)
{
    m_pDiff3LineList = pDiff3LineList;
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

void Overview::setRange(QtNumberType firstLine, QtNumberType pageHeight)
{
    m_firstLine = firstLine;
    m_pageHeight = pageHeight;
    update();
}
void Overview::setFirstLine(QtNumberType firstLine)
{
    QScrollBar* scrollBar = qobject_cast<QScrollBar*>(sender());

    if(Q_UNLIKELY(scrollBar == nullptr))
    {
        m_firstLine = firstLine;
        update();
    }
    else
        setRange(firstLine, scrollBar->pageStep());
}

void Overview::setOverviewMode(e_OverviewMode eOverviewMode)
{
    mOverviewMode = eOverviewMode;
    slotRedraw();
}

e_OverviewMode Overview::getOverviewMode()
{
    return mOverviewMode;
}

void Overview::mousePressEvent(QMouseEvent* e)
{
    int h = height() - 1;
    int h1 = h * m_pageHeight / std::max(1, m_nofLines) + 3;
    if(h > 0)
        Q_EMIT setLine((e->y() - h1 / 2) * m_nofLines / h);
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
        d3l.mergeOneLine(md, bConflict, bLineRemoved, src, !KDiff3App::isTripleDiff());

        QColor c = m_pOptions->m_bgColor;
        bool bWhiteSpaceChange = false;
        //if( bConflict )  c=m_pOptions->m_colorForConflict;
        //else
        if(eOverviewMode == e_OverviewMode::eOMNormal)
        {
            switch(md)
            {
                case e_MergeDetails::eDefault:
                case e_MergeDetails::eNoChange:
                    c = m_pOptions->m_bgColor;
                    break;

                case e_MergeDetails::eBAdded:
                case e_MergeDetails::eBDeleted:
                case e_MergeDetails::eBChanged:
                    c = bConflict ? m_pOptions->m_colorForConflict : m_pOptions->m_colorB;
                    bWhiteSpaceChange = d3l.isEqualAB() || (d3l.isWhiteLine(e_SrcSelector::A) && d3l.isWhiteLine(e_SrcSelector::B));
                    break;

                case e_MergeDetails::eCAdded:
                case e_MergeDetails::eCDeleted:
                case e_MergeDetails::eCChanged:
                    bWhiteSpaceChange = d3l.isEqualAC() || (d3l.isWhiteLine(e_SrcSelector::A) && d3l.isWhiteLine(e_SrcSelector::C));
                    c = bConflict ? m_pOptions->m_colorForConflict : m_pOptions->m_colorC;
                    break;

                case e_MergeDetails::eBCChanged:         // conflict
                case e_MergeDetails::eBCChangedAndEqual: // possible conflict
                case e_MergeDetails::eBCDeleted:         // possible conflict
                case e_MergeDetails::eBChanged_CDeleted: // conflict
                case e_MergeDetails::eCChanged_BDeleted: // conflict
                case e_MergeDetails::eBCAdded:           // conflict
                case e_MergeDetails::eBCAddedAndEqual:   // possible conflict
                    c = m_pOptions->m_colorForConflict;
                    break;
                default:
                    Q_ASSERT(true);
                    break;
            }
        }
        else if(eOverviewMode == e_OverviewMode::eOMAvsB)
        {
            switch(md)
            {
                case e_MergeDetails::eDefault:
                case e_MergeDetails::eNoChange:
                case e_MergeDetails::eCAdded:
                case e_MergeDetails::eCDeleted:
                case e_MergeDetails::eCChanged:
                    break;
                default:
                    c = m_pOptions->m_colorForConflict;
                    bWhiteSpaceChange = d3l.isEqualAB() || (d3l.isWhiteLine(e_SrcSelector::A) && d3l.isWhiteLine(e_SrcSelector::B));
                    break;
            }
        }
        else if(eOverviewMode == e_OverviewMode::eOMAvsC)
        {
            switch(md)
            {
                case e_MergeDetails::eDefault:
                case e_MergeDetails::eNoChange:
                case e_MergeDetails::eBAdded:
                case e_MergeDetails::eBDeleted:
                case e_MergeDetails::eBChanged:
                    break;
                default:
                    c = m_pOptions->m_colorForConflict;
                    bWhiteSpaceChange = d3l.isEqualAC() || (d3l.isWhiteLine(e_SrcSelector::A) && d3l.isWhiteLine(e_SrcSelector::C));
                    break;
            }
        }
        else if(eOverviewMode == e_OverviewMode::eOMBvsC)
        {
            switch(md)
            {
                case e_MergeDetails::eDefault:
                case e_MergeDetails::eNoChange:
                case e_MergeDetails::eBCChangedAndEqual:
                case e_MergeDetails::eBCDeleted:
                case e_MergeDetails::eBCAddedAndEqual:
                    break;
                default:
                    c = m_pOptions->m_colorForConflict;
                    bWhiteSpaceChange = d3l.isEqualBC() || (d3l.isWhiteLine(e_SrcSelector::B) && d3l.isWhiteLine(e_SrcSelector::C));
                    break;
            }
        }

        int x2 = x;
        int w2 = w;

        if(!KDiff3App::isTripleDiff())
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
        if(m_pOptions->wordWrapOn())
        {
            ++wrapLineIdx;
            if(wrapLineIdx >= d3l.linesNeededForDisplay())
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

    const auto dpr = devicePixelRatioF();
    if(m_pixmap.size() != size() * dpr)
    {
        m_nofLines = m_pDiff3LineList->numberOfLines(m_pOptions->wordWrapOn());

        m_pixmap = QPixmap(size() * dpr);
        m_pixmap.setDevicePixelRatio(dpr);

        QPainter p(&m_pixmap);
        p.fillRect(rect(), m_pOptions->m_bgColor);

        if(!KDiff3App::isTripleDiff() || mOverviewMode == e_OverviewMode::eOMNormal)
        {
            drawColumn(p, e_OverviewMode::eOMNormal, 0, w, h, m_nofLines);
        }
        else
        {
            drawColumn(p, e_OverviewMode::eOMNormal, 0, w / 2, h, m_nofLines);
            drawColumn(p, mOverviewMode, w / 2, w / 2, h, m_nofLines);
        }
    }

    QPainter painter(this);
    painter.drawPixmap(0, 0, m_pixmap);
    int y1 = 0, h1 = 0;
    if(m_nofLines > 0)
    {
        y1 = h * m_firstLine / m_nofLines - 1;
        h1 = h * m_pageHeight / m_nofLines + 3;
    }
    painter.setPen(Qt::black);
    painter.drawRect(1, y1, w - 1, h1);
}
