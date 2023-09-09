// clang-format off
/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on
#include "Overview.h"

#include "diff.h"
#include "kdiff3.h"
#include "MergeEditLine.h"
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

void Overview::setRange(qint32 firstLine, qint32 pageHeight)
{
    m_firstLine = firstLine;
    m_pageHeight = pageHeight;
    update();
}
void Overview::setFirstLine(qint32 firstLine)
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
    qint32 h = height() - 1;
    qint32 h1 = h * m_pageHeight / std::max(1, m_nofLines) + 3;
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

void Overview::drawColumn(QPainter& p, e_OverviewMode eOverviewMode, qint32 x, qint32 w, qint32 h, qint32 nofLines)
{
    p.setPen(Qt::black);
    p.drawLine(x, 0, x, h);

    if(nofLines == 0) return;

    qint32 line = 0;
    qint32 oldY = 0;
    qint32 oldConflictY = -1;
    qint32 wrapLineIdx = 0;
    Diff3LineList::const_iterator i;

    for(i = m_pDiff3LineList->begin(); i != m_pDiff3LineList->end();)
    {
        const Diff3Line& d3l = *i;
        qint32 y = h * (line + 1) / nofLines;
        MergeLine lMergeLine;
        e_MergeDetails md;
        bool bConflict;
        bool bLineRemoved;

        lMergeLine.mergeOneLine(d3l, bLineRemoved, !KDiff3App::isTripleDiff());
        md = lMergeLine.details();
        bConflict = lMergeLine.isConflict();

        QColor c = m_pOptions->backgroundColor();
        bool bWhiteSpaceChange = false;
        //if( bConflict )  c=m_pOptions->conflictColor();
        //else
        switch(eOverviewMode)
        {
            case e_OverviewMode::eOMNormal:
                switch(md)
                {
                    case e_MergeDetails::eDefault:
                    case e_MergeDetails::eNoChange:
                        c = m_pOptions->backgroundColor();
                        break;

                    case e_MergeDetails::eBAdded:
                    case e_MergeDetails::eBDeleted:
                    case e_MergeDetails::eBChanged:
                        c = bConflict ? m_pOptions->conflictColor() : m_pOptions->bColor();
                        bWhiteSpaceChange = d3l.isEqualAB() || (d3l.isWhiteLine(e_SrcSelector::A) && d3l.isWhiteLine(e_SrcSelector::B));
                        break;

                    case e_MergeDetails::eCAdded:
                    case e_MergeDetails::eCDeleted:
                    case e_MergeDetails::eCChanged:
                        bWhiteSpaceChange = d3l.isEqualAC() || (d3l.isWhiteLine(e_SrcSelector::A) && d3l.isWhiteLine(e_SrcSelector::C));
                        c = bConflict ? m_pOptions->conflictColor() : m_pOptions->cColor();
                        break;

                    case e_MergeDetails::eBCChanged:         // conflict
                    case e_MergeDetails::eBCChangedAndEqual: // possible conflict
                    case e_MergeDetails::eBCDeleted:         // possible conflict
                    case e_MergeDetails::eBChanged_CDeleted: // conflict
                    case e_MergeDetails::eCChanged_BDeleted: // conflict
                    case e_MergeDetails::eBCAdded:           // conflict
                    case e_MergeDetails::eBCAddedAndEqual:   // possible conflict
                        c = m_pOptions->conflictColor();
                        break;
                }
                break;
            case e_OverviewMode::eOMAvsB:
                switch(md)
                {
                    case e_MergeDetails::eDefault:
                    case e_MergeDetails::eNoChange:
                    case e_MergeDetails::eCAdded:
                    case e_MergeDetails::eCDeleted:
                    case e_MergeDetails::eCChanged:
                        break;
                    default:
                        c = m_pOptions->conflictColor();
                        bWhiteSpaceChange = d3l.isEqualAB() || (d3l.isWhiteLine(e_SrcSelector::A) && d3l.isWhiteLine(e_SrcSelector::B));
                        break;
                }
                break;
            case e_OverviewMode::eOMAvsC:
                switch(md)
                {
                    case e_MergeDetails::eDefault:
                    case e_MergeDetails::eNoChange:
                    case e_MergeDetails::eBAdded:
                    case e_MergeDetails::eBDeleted:
                    case e_MergeDetails::eBChanged:
                        break;
                    default:
                        c = m_pOptions->conflictColor();
                        bWhiteSpaceChange = d3l.isEqualAC() || (d3l.isWhiteLine(e_SrcSelector::A) && d3l.isWhiteLine(e_SrcSelector::C));
                        break;
                }
                break;
            case e_OverviewMode::eOMBvsC:
                switch(md)
                {
                    case e_MergeDetails::eDefault:
                    case e_MergeDetails::eNoChange:
                    case e_MergeDetails::eBCChangedAndEqual:
                    case e_MergeDetails::eBCDeleted:
                    case e_MergeDetails::eBCAddedAndEqual:
                        break;
                    default:
                        c = m_pOptions->conflictColor();
                        bWhiteSpaceChange = d3l.isEqualBC() || (d3l.isWhiteLine(e_SrcSelector::B) && d3l.isWhiteLine(e_SrcSelector::C));
                        break;
                }
                break;
        }

        qint32 x2 = x;
        qint32 w2 = w;

        if(!KDiff3App::isTripleDiff())
        {
            if(!d3l.getLineA().isValid() && d3l.getLineB().isValid())
            {
                c = m_pOptions->aColor();
                x2 = w / 2;
                w2 = x2;
            }
            if(d3l.getLineA().isValid() && !d3l.getLineB().isValid())
            {
                c = m_pOptions->bColor();
                w2 = w / 2;
            }
        }

        if(!bWhiteSpaceChange || m_pOptions->m_bShowWhiteSpace)
        {
            // Make sure that lines with conflict are not overwritten.
            if(c == m_pOptions->conflictColor())
            {
                p.fillRect(x2 + 1, oldY, w2, std::max(1, y - oldY), bWhiteSpaceChange ? QBrush(c, Qt::Dense4Pattern) : QBrush(c));
                oldConflictY = oldY;
            }
            else if(c != m_pOptions->backgroundColor() && oldY > oldConflictY)
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
    qint32 h = height() - 1;
    qint32 w = width();

    const auto dpr = devicePixelRatioF();
    if(m_pixmap.size() != size() * dpr)
    {
        m_nofLines = m_pDiff3LineList->numberOfLines(m_pOptions->wordWrapOn());

        m_pixmap = QPixmap(size() * dpr);
        m_pixmap.setDevicePixelRatio(dpr);

        QPainter p(&m_pixmap);
        p.fillRect(rect(), m_pOptions->backgroundColor());

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
    qint32 y1 = 0, h1 = 0;
    if(m_nofLines > 0)
    {
        y1 = h * m_firstLine / m_nofLines - 1;
        h1 = h * m_pageHeight / m_nofLines + 3;
    }
    painter.setPen(Qt::black);
    painter.drawRect(1, y1, w - 1, h1);
}
