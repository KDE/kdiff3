/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "diff.h"

#include "gnudiff_diff.h"
#include "merger.h"
#include "options.h"
#include "progress.h"

#include <cstdlib>
#include <ctype.h>

#include <KLocalizedString>
#include <KMessageBox>

#include <QtGlobal>
#include <QSharedPointer>

constexpr bool g_bIgnoreWhiteSpace = true;

QSharedPointer<DiffBufferInfo> Diff3Line::m_pDiffBufferInfo = nullptr;

int LineData::width(int tabSize) const
{
    QString pLine = getLine();
    int w = 0;
    int j = 0;
    for(int i = 0; i < size(); ++i)
    {
        if(pLine[i] == '\t')
        {
            for(j %= tabSize; j < tabSize; ++j)
                ++w;
            j = 0;
        }
        else
        {
            ++w;
            ++j;
        }
    }
    return w;
}

/*
    Implement support for g_bIgnoreWhiteSpace
*/
bool LineData::equal(const LineData& l1, const LineData& l2)
{
    if(l1.getLine() == nullptr || l2.getLine() == nullptr) return false;

    if(g_bIgnoreWhiteSpace)
    {
        // Ignore white space diff
        const QString line1 = l1.getLine(), line2 = l2.getLine();
        QString::const_iterator p1 = line1.begin();
        QString::const_iterator p1End = line1.end();
        QString::const_iterator p2 = line2.begin();
        QString::const_iterator p2End = line2.end();

        for(; p1 != p1End && p2 != p2End; p1++, p2++)
        {
            while(isspace(p1->unicode()) && p1 != p1End) ++p1;
            while(isspace(p2->unicode()) && p2 != p2End) ++p2;

            if(*p1 != *p2)
                return false;
        }

        return (p1 == p1End && p2 == p2End);
    }
    else
    {
        const QString line1 = l1.getLine(), line2 = l2.getLine();
        return (l1.size() == l2.size() && QString::compare(line1, line2) == 0);
    }
}

// First step
void Diff3LineList::calcDiff3LineListUsingAB(const DiffList* pDiffListAB)
{
    // First make d3ll for AB (from pDiffListAB)

    DiffList::const_iterator i = pDiffListAB->begin();
    LineRef::LineType lineA = 0;
    LineRef::LineType lineB = 0;
    Diff d;

    qCInfo(kdiffMain) << "Enter: calcDiff3LineListUsingAB" ;
    for(;;)
    {
        if(d.numberOfEquals() == 0 && d.diff1() == 0 && d.diff2() == 0)
        {
            if(i != pDiffListAB->end())
            {
                d = *i;
                ++i;
            }
            else
                break;
        }

        Diff3Line d3l;
        if(d.numberOfEquals() > 0)
        {
            d3l.bAEqB = true;
            d3l.setLineA(lineA);
            d3l.setLineB(lineB);
            d.adjustNumberOfEquals(-1);
            ++lineA;
            ++lineB;
        }
        else if(d.diff1() > 0 && d.diff2() > 0)
        {
            d3l.setLineA(lineA);
            d3l.setLineB(lineB);
            d.adjustDiff1(-1);
            d.adjustDiff2(-1);
            ++lineA;
            ++lineB;
        }
        else if(d.diff1() > 0)
        {
            d3l.setLineA(lineA);
            d.adjustDiff1(-1);
            ++lineA;
        }
        else if(d.diff2() > 0)
        {
            d3l.setLineB(lineB);
            d.adjustDiff2(-1);
            ++lineB;
        }

        Q_ASSERT(d.numberOfEquals() >= 0);

        qCDebug(kdiffCore) << "lineA = " << d3l.getLineA() << ", lineB = " << d3l.getLineB() ;
        push_back(d3l);
    }
    qCInfo(kdiffMain) << "Leave: calcDiff3LineListUsingAB" ;
}

// Second step
void Diff3LineList::calcDiff3LineListUsingAC(const DiffList* pDiffListAC)
{
    ////////////////
    // Now insert data from C using pDiffListAC

    DiffList::const_iterator i = pDiffListAC->begin();
    Diff3LineList::iterator i3 = begin();
    LineRef::LineType lineA = 0;
    LineRef::LineType lineC = 0;
    Diff d;

    for(;;)
    {
        if(d.numberOfEquals() == 0 && d.diff1() == 0 && d.diff2() == 0)
        {
            if(i != pDiffListAC->end())
            {
                d = *i;
                ++i;
            }
            else
                break;
        }

        Diff3Line d3l;
        if(d.numberOfEquals() > 0)
        {
            // Find the corresponding lineA
            while(i3->getLineA() != lineA)
                ++i3;

            i3->setLineC(lineC);
            i3->bAEqC = true;
            i3->bBEqC = i3->isEqualAB();

            d.adjustNumberOfEquals(-1);
            ++lineA;
            ++lineC;
            ++i3;
        }
        else if(d.diff1() > 0 && d.diff2() > 0)
        {
            d3l.setLineC(lineC);
            insert(i3, d3l);
            d.adjustDiff1(-1);
            d.adjustDiff2(-1);
            ++lineA;
            ++lineC;
        }
        else if(d.diff1() > 0)
        {
            d.adjustDiff1(-1);
            ++lineA;
        }
        else if(d.diff2() > 0)
        {
            d3l.setLineC(lineC);
            insert(i3, d3l);
            d.adjustDiff2(-1);
            ++lineC;
        }
    }
}

// Third step
void Diff3LineList::calcDiff3LineListUsingBC(const DiffList* pDiffListBC)
{
    ////////////////
    // Now improve the position of data from C using pDiffListBC
    // If a line from C equals a line from A then it is in the
    // same Diff3Line already.
    // If a line from C equals a line from B but not A, this
    // information will be used here.

    DiffList::const_iterator i = pDiffListBC->begin();
    Diff3LineList::iterator i3b = begin();
    Diff3LineList::iterator i3c = begin();
    LineRef::LineType lineB = 0;
    LineRef::LineType lineC = 0;
    Diff d;

    for(;;)
    {
        if(d.numberOfEquals() == 0 && d.diff1() == 0 && d.diff2() == 0)
        {
            if(i != pDiffListBC->end())
            {
                d = *i;
                ++i;
            }
            else
                break;
        }

        Diff3Line d3l;
        if(d.numberOfEquals() > 0)
        {
            // Find the corresponding lineB and lineC
            while(i3b != end() && i3b->getLineB() != lineB)
                ++i3b;

            while(i3c != end() && i3c->getLineC() != lineC)
                ++i3c;

            Q_ASSERT(i3b != end());
            Q_ASSERT(i3c != end());

            if(i3b == i3c)
            {
                Q_ASSERT(i3b->getLineC() == lineC);
                i3b->bBEqC = true;
            }
            else
            {
                // Is it possible to move this line up?
                // Test if no other B's are used between i3c and i3b

                // First test which is before: i3c or i3b ?
                Diff3LineList::iterator i3c1 = i3c;
                Diff3LineList::iterator i3b1 = i3b;
                while(i3c1 != i3b && i3b1 != i3c)
                {
                    Q_ASSERT(i3b1 != end() || i3c1 != end());
                    if(i3c1 != end()) ++i3c1;
                    if(i3b1 != end()) ++i3b1;
                }

                if(i3c1 == i3b && !i3b->isEqualAB()) // i3c before i3b
                {
                    Diff3LineList::iterator i3 = i3c;
                    int nofDisturbingLines = 0;
                    while(i3 != i3b && i3 != end())
                    {
                        if(i3->getLineB().isValid())
                            ++nofDisturbingLines;
                        ++i3;
                    }

                    if(nofDisturbingLines > 0) //&& nofDisturbingLines < d.nofEquals*d.nofEquals+4 )
                    {
                        Diff3LineList::iterator i3_last_equal_A = end();

                        i3 = i3c;
                        while(i3 != i3b)
                        {
                            if(i3->isEqualAB())
                            {
                                i3_last_equal_A = i3;
                            }
                            ++i3;
                        }

                        /* If i3_last_equal_A isn't still set to d3ll.end(), then
                   * we've found a line in A that is equal to one in B
                   * somewhere between i3c and i3b
                   */
                        bool before_or_on_equal_line_in_A = (i3_last_equal_A != end());

                        // Move the disturbing lines up, out of sight.
                        i3 = i3c;
                        while(i3 != i3b)
                        {
                            if(i3->getLineB().isValid() ||
                               (before_or_on_equal_line_in_A && i3->getLineA().isValid()))
                            {
                                d3l.setLineB(i3->getLineB());
                                i3->setLineB(LineRef::invalid);

                                // Move A along if it matched B
                                if(before_or_on_equal_line_in_A)
                                {
                                    d3l.setLineA(i3->getLineA());
                                    d3l.bAEqB = i3->isEqualAB();
                                    i3->setLineA(LineRef::invalid);
                                    i3->bAEqC = false;
                                }

                                i3->bAEqB = false;
                                i3->bBEqC = false;
                                insert(i3c, d3l);
                            }

                            if(i3 == i3_last_equal_A)
                            {
                                before_or_on_equal_line_in_A = false;
                            }

                            ++i3;
                        }
                        nofDisturbingLines = 0;
                    }

                    if(nofDisturbingLines == 0)
                    {
                        // Yes, the line from B can be moved.
                        i3b->setLineB(LineRef::invalid); // This might leave an empty line: removed later.
                        i3b->bAEqB = false;
                        i3b->bBEqC = false;
                        i3c->setLineB(lineB);
                        i3c->bBEqC = true;
                        i3c->bAEqB = i3c->isEqualAC();
                    }
                }
                else if(i3b1 == i3c && !i3c->isEqualAC())
                {
                    Diff3LineList::iterator i3 = i3b;
                    int nofDisturbingLines = 0;
                    while(i3 != i3c && i3 != end())
                    {
                        if(i3->getLineC().isValid())
                            ++nofDisturbingLines;
                        ++i3;
                    }

                    if(nofDisturbingLines > 0) //&& nofDisturbingLines < d.nofEquals*d.nofEquals+4 )
                    {
                        Diff3LineList::iterator i3_last_equal_A = end();

                        i3 = i3b;
                        while(i3 != i3c)
                        {
                            if(i3->isEqualAC())
                            {
                                i3_last_equal_A = i3;
                            }
                            ++i3;
                        }

                        /* If i3_last_equal_A isn't still set to d3ll.end(), then
                   * we've found a line in A that is equal to one in C
                   * somewhere between i3b and i3c
                   */
                        bool before_or_on_equal_line_in_A = (i3_last_equal_A != end());

                        // Move the disturbing lines up.
                        i3 = i3b;
                        while(i3 != i3c)
                        {
                            if(i3->getLineC().isValid() ||
                               (before_or_on_equal_line_in_A && i3->getLineA().isValid()))
                            {
                                d3l.setLineC(i3->getLineC());
                                i3->setLineC(LineRef::invalid);

                                // Move A along if it matched C
                                if(before_or_on_equal_line_in_A)
                                {
                                    d3l.setLineA(i3->getLineA());
                                    d3l.bAEqC = i3->isEqualAC();
                                    i3->setLineA(LineRef::invalid);
                                    i3->bAEqB = false;
                                }

                                i3->bAEqC = false;
                                i3->bBEqC = false;
                                insert(i3b, d3l);
                            }

                            if(i3 == i3_last_equal_A)
                            {
                                before_or_on_equal_line_in_A = false;
                            }

                            ++i3;
                        }
                        nofDisturbingLines = 0;
                    }

                    if(nofDisturbingLines == 0)
                    {
                        // Yes, the line from C can be moved.
                        i3c->setLineC(LineRef::invalid); // This might leave an empty line: removed later.
                        i3c->bAEqC = false;
                        i3c->bBEqC = false;
                        i3b->setLineC(lineC);
                        i3b->bBEqC = true;
                        i3b->bAEqC = i3b->isEqualAB();
                    }
                }
            }

            d.adjustNumberOfEquals(-1);
            ++lineB;
            ++lineC;
            ++i3b;
            ++i3c;
        }
        else if(d.diff1() > 0)
        {
            Diff3LineList::iterator i3 = i3b;
            while(i3->getLineB() != lineB)
                ++i3;
            if(i3 != i3b && !i3->isEqualAB())
            {
                // Take B from this line and move it up as far as possible
                d3l.setLineB(lineB);
                insert(i3b, d3l);
                i3->setLineB(LineRef::invalid);
            }
            else
            {
                i3b = i3;
            }
            d.adjustDiff1(-1);
            ++lineB;
            ++i3b;

            if(d.diff2() > 0)
            {
                d.adjustDiff2(-1);
                ++lineC;
            }
        }
        else if(d.diff2() > 0)
        {
            d.adjustDiff2(-1);
            ++lineC;
        }
    }
    /*
   Diff3LineList::iterator it = d3ll.begin();
   int li=0;
   for( ; it!=d3ll.end(); ++it, ++li )
   {
      printf( "%4d %4d %4d %4d  A%c=B A%c=C B%c=C\n",
         li, it->getLineA(), it->getLineB(), it->getLineC(),
         it->isEqualAB() ? '=' : '!', it->isEqualAC() ? '=' : '!', it->isEqualBC() ? '=' : '!' );
   }
   printf("\n");*/
}

// Test if the move would pass a barrier. Return true if not.
bool ManualDiffHelpList::isValidMove(int line1, int line2, e_SrcSelector winIdx1, e_SrcSelector winIdx2) const
{
    if(line1 >= 0 && line2 >= 0)
    {
        ManualDiffHelpList::const_iterator i;
        for(i = begin(); i != end(); ++i)
        {
            const ManualDiffHelpEntry& mdhe = *i;

            if(!mdhe.isValidMove(line1, line2, winIdx1, winIdx2)) return false;
        }
    }
    return true; // no barrier passed.
}

void ManualDiffHelpList::insertEntry(e_SrcSelector winIdx, LineRef firstLine, LineRef lastLine)
{
    // The manual diff help list must be sorted and compact.
    // "Compact" means that upper items can't be empty if lower items contain data.

    // First insert the new item without regarding compactness.
    // If the new item overlaps with previous items then the previous items will be removed.

    ManualDiffHelpEntry mdhe;
    mdhe.firstLine(winIdx) = firstLine;
    mdhe.lastLine(winIdx) = lastLine;

    ManualDiffHelpList::iterator i;
    for(i = begin(); i != end(); ++i)
    {
        LineRef& l1 = i->firstLine(winIdx);
        LineRef& l2 = i->lastLine(winIdx);
        if(l1.isValid() && l2.isValid())
        {
            if((firstLine <= l1 && lastLine >= l1) || (firstLine <= l2 && lastLine >= l2))
            {
                // overlap
                l1.invalidate();
                l2.invalidate();
            }
            if(firstLine < l1 && lastLine < l1)
            {
                // insert before this position
                insert(i, mdhe);
                break;
            }
        }
    }
    if(i == end())
    {
        insert(i, mdhe);
    }

    // Now make the list compact
    for(e_SrcSelector wIdx = e_SrcSelector::A; wIdx != e_SrcSelector::Invalid; wIdx = nextSelector(wIdx))
    {
        ManualDiffHelpList::iterator iEmpty = begin();
        for(i = begin(); i != end(); ++i)
        {
            if(iEmpty->firstLine(wIdx).isValid())
            {
                ++iEmpty;
                continue;
            }
            if(i->firstLine(wIdx).isValid()) // Current item is not empty -> move it to the empty place
            {
                std::swap(iEmpty->firstLine(wIdx), i->firstLine(wIdx));
                std::swap(iEmpty->lastLine(wIdx), i->lastLine(wIdx));
                ++iEmpty;
            }
        }
    }
    remove(ManualDiffHelpEntry()); // Remove all completely empty items.
}

bool ManualDiffHelpEntry::isValidMove(int line1, int line2, e_SrcSelector winIdx1, e_SrcSelector winIdx2) const
{
    // Barrier
    int l1 = winIdx1 == e_SrcSelector::A ? lineA1 : winIdx1 == e_SrcSelector::B ? lineB1 : lineC1;
    int l2 = winIdx2 == e_SrcSelector::A ? lineA1 : winIdx2 == e_SrcSelector::B ? lineB1 : lineC1;

    if(l1 >= 0 && l2 >= 0)
    {
        if((line1 >= l1 && line2 < l2) || (line1 < l1 && line2 >= l2))
            return false;
        l1 = winIdx1 == e_SrcSelector::A ? lineA2 : winIdx1 == e_SrcSelector::B ? lineB2 : lineC2;
        l2 = winIdx2 == e_SrcSelector::A ? lineA2 : winIdx2 == e_SrcSelector::B ? lineB2 : lineC2;
        ++l1;
        ++l2;
        if((line1 >= l1 && line2 < l2) || (line1 < l1 && line2 >= l2))
            return false;
    }

    return true;
}

int ManualDiffHelpEntry::calcManualDiffFirstDiff3LineIdx(const Diff3LineVector& d3lv)
{
    int i;
    for(i = 0; i < d3lv.size(); ++i)
    {
        const Diff3Line& d3l = *d3lv[i];
        if((lineA1.isValid() && lineA1 == d3l.getLineA()) ||
           (lineB1.isValid() && lineB1 == d3l.getLineB()) ||
           (lineC1.isValid() && lineC1 == d3l.getLineC()))
            return i;
    }
    return -1;
}

void DiffList::runDiff(const QVector<LineData>* p1, const qint32 index1, LineRef size1, const QVector<LineData>* p2, const qint32 index2, LineRef size2,
                    const QSharedPointer<Options> &pOptions)
{
    ProgressProxy pp;
    static GnuDiff gnuDiff; // All values are initialized with zeros.

    pp.setCurrent(0);

    clear();
    if(p1 == nullptr || (*p1)[index1].getBuffer() == nullptr || p2 == nullptr || (*p2)[index2].getBuffer() == nullptr || size1 == 0 || size2 == 0)
    {
        if(p1 != nullptr && p2 != nullptr && (*p1)[index1].getBuffer() == nullptr && (*p2)[index2].getBuffer() == nullptr && size1 == size2)
            push_back(Diff(size1, 0, 0));
        else
        {
            push_back(Diff(0, size1, size2));
        }
    }
    else
    {
        assert(size1 < p1->size() && size2 < p2->size());

        GnuDiff::comparison comparisonInput;
        memset(&comparisonInput, 0, sizeof(comparisonInput));
        comparisonInput.parent = nullptr;
        comparisonInput.file[0].buffer = (*p1)[index1].getBuffer()->unicode() + (*p1)[index1].getOffset();                                                      //ptr to buffer
        comparisonInput.file[0].buffered = ((*p1)[index1 + size1 - 1].getOffset() + (*p1)[index1 + size1 - 1].size() - (*p1)[index1].getOffset()); // size of buffer
        comparisonInput.file[1].buffer = (*p2)[index2].getBuffer()->unicode() + (*p2)[index2].getOffset();                                                      //ptr to buffer
        comparisonInput.file[1].buffered = ((*p2)[index2 + size2 - 1].getOffset() + (*p2)[index2 + size2 - 1].size() - (*p2)[index2].getOffset()); // size of buffer

        gnuDiff.ignore_white_space = GnuDiff::IGNORE_ALL_SPACE; // I think nobody needs anything else ...
        gnuDiff.bIgnoreWhiteSpace = true;
        gnuDiff.bIgnoreNumbers = pOptions->m_bIgnoreNumbers;
        gnuDiff.minimal = pOptions->m_bTryHard;
        gnuDiff.ignore_case = false;
        GnuDiff::change* script = gnuDiff.diff_2_files(&comparisonInput);

        LineRef equalLinesAtStart = (LineRef)comparisonInput.file[0].prefix_lines;
        LineRef currentLine1 = 0;
        LineRef currentLine2 = 0;
        GnuDiff::change* p = nullptr;
        for(GnuDiff::change* e = script; e; e = p)
        {
            Diff d((LineCount)(e->line0 - currentLine1), e->deleted, e->inserted);
            Q_ASSERT(d.numberOfEquals() == e->line1 - currentLine2);

            currentLine1 += (LineRef)(d.numberOfEquals() + d.diff1());
            currentLine2 += (LineRef)(d.numberOfEquals() + d.diff2());
            push_back(d);

            p = e->link;
            free(e);
        }

        if(empty())
        {
            qint32 numofEquals = std::min(size1, size2);
            Diff d(numofEquals, size1 - numofEquals, size2 - numofEquals);

            push_back(d);
        }
        else
        {
            front().adjustNumberOfEquals(equalLinesAtStart);
            currentLine1 += equalLinesAtStart;
            currentLine2 += equalLinesAtStart;

            LineCount nofEquals = std::min(size1 - currentLine1, size2 - currentLine2);
            if(nofEquals == 0)
            {
                back().adjustDiff1(size1 - currentLine1);
                back().adjustDiff2(size2 - currentLine2);
            }
            else
            {
                Diff d(nofEquals, size1 - currentLine1 - nofEquals, size2 - currentLine2 - nofEquals);
                push_back(d);
            }
        }
    }

    // Verify difflist
    {
        LineRef::LineType l1 = 0;
        LineRef::LineType l2 = 0;
        DiffList::iterator i;
        for(i = begin(); i != end(); ++i)
        {
            l1 += i->numberOfEquals() + i->diff1();
            l2 += i->numberOfEquals() + i->diff2();
        }

        Q_ASSERT(l1 == size1 && l2 == size2);
    }

    pp.setCurrent(1);
}

void ManualDiffHelpList::runDiff(const QVector<LineData>* p1, LineRef size1, const QVector<LineData>* p2, LineRef size2, DiffList& diffList,
                                 e_SrcSelector winIdx1, e_SrcSelector winIdx2,
                                 const QSharedPointer<Options> &pOptions)
{
    diffList.clear();
    DiffList diffList2;

    int l1begin = 0;
    int l2begin = 0;
    ManualDiffHelpList::const_iterator i;
    for(i = begin(); i != end(); ++i)
    {
        const ManualDiffHelpEntry& mdhe = *i;

        LineRef l1end = mdhe.getLine1(winIdx1);
        LineRef l2end = mdhe.getLine1(winIdx2);

        if(l1end.isValid() && l2end.isValid())
        {
            diffList2.runDiff(p1, l1begin, l1end - l1begin, p2, l2begin, l2end - l2begin, pOptions);
            diffList.splice(diffList.end(), diffList2);
            l1begin = l1end;
            l2begin = l2end;

            l1end = mdhe.getLine2(winIdx1);
            l2end = mdhe.getLine2(winIdx2);

            if(l1end.isValid() && l2end.isValid())
            {
                ++l1end; // point to line after last selected line
                ++l2end;
                diffList2.runDiff(p1, l1begin, l1end - l1begin, p2, l2begin, l2end - l2begin, pOptions);
                diffList.splice(diffList.end(), diffList2);
                l1begin = l1end;
                l2begin = l2end;
            }
        }
    }
    diffList2.runDiff(p1, l1begin, size1 - l1begin, p2, l2begin, size2 - l2begin, pOptions);
    diffList.splice(diffList.end(), diffList2);
}

void Diff3LineList::correctManualDiffAlignment(ManualDiffHelpList* pManualDiffHelpList)
{
    if(pManualDiffHelpList->empty())
        return;

    // If a line appears unaligned in comparison to the manual alignment, correct this.

    ManualDiffHelpList::iterator iMDHL;
    for(iMDHL = pManualDiffHelpList->begin(); iMDHL != pManualDiffHelpList->end(); ++iMDHL)
    {
        Diff3LineList::iterator i3 = begin();
        e_SrcSelector missingWinIdx = e_SrcSelector::None;
        int alignedSum = (!iMDHL->getLine1(e_SrcSelector::A).isValid() ? 0 : 1) + (!iMDHL->getLine1(e_SrcSelector::B).isValid() ? 0 : 1) + (!iMDHL->getLine1(e_SrcSelector::C).isValid() ? 0 : 1);
        if(alignedSum == 2)
        {
            // If only A & B are aligned then let C rather be aligned with A
            // If only A & C are aligned then let B rather be aligned with A
            // If only B & C are aligned then let A rather be aligned with B
            missingWinIdx = !iMDHL->getLine1(e_SrcSelector::A).isValid() ? e_SrcSelector::A : (!iMDHL->getLine1(e_SrcSelector::B).isValid() ? e_SrcSelector::B : e_SrcSelector::C);
        }
        else if(alignedSum <= 1)
        {
            return;
        }

        // At the first aligned line, move up the two other lines into new d3ls until the second input is aligned
        // Then move up the third input until all three lines are aligned.
        e_SrcSelector wi = e_SrcSelector::None;
        for(; i3 != end(); ++i3)
        {
            for(wi = e_SrcSelector::A; wi != e_SrcSelector::Invalid; wi=nextSelector(wi))
            {
                if(i3->getLineInFile(wi).isValid() && iMDHL->firstLine(wi) == i3->getLineInFile(wi))
                    break;
            }
            if(wi != e_SrcSelector::Invalid)
                break;
        }

        if(wi >= e_SrcSelector::A && wi <= e_SrcSelector::Max)
        {
            // Found manual alignment for one source
            Diff3LineList::iterator iDest = i3;

            // Move lines up until the next firstLine is found. Omit wi from move and search.
            e_SrcSelector wi2 = e_SrcSelector::None;
            for(; i3 != end(); ++i3)
            {
                for(wi2 = e_SrcSelector::A; wi2 != e_SrcSelector::Invalid; wi2 = nextSelector(wi2))
                {
                    if(wi != wi2 && i3->getLineInFile(wi2).isValid() && iMDHL->firstLine(wi2) == i3->getLineInFile(wi2))
                        break;
                }
                if(wi2 == e_SrcSelector::Invalid)
                { // Not yet found
                    // Move both others up
                    Diff3Line d3l;
                    // Move both up
                    if(wi == e_SrcSelector::A) // Move B and C up
                    {
                        d3l.bBEqC = i3->isEqualBC();
                        d3l.setLineB(i3->getLineB());
                        d3l.setLineC(i3->getLineC());
                        i3->setLineB(LineRef::invalid);
                        i3->setLineC(LineRef::invalid);
                    }
                    if(wi == e_SrcSelector::B) // Move A and C up
                    {
                        d3l.bAEqC = i3->isEqualAC();
                        d3l.setLineA(i3->getLineA());
                        d3l.setLineC(i3->getLineC());
                        i3->setLineA(LineRef::invalid);
                        i3->setLineC(LineRef::invalid);
                    }
                    if(wi == e_SrcSelector::C) // Move A and B up
                    {
                        d3l.bAEqB = i3->isEqualAB();
                        d3l.setLineA(i3->getLineA());
                        d3l.setLineB(i3->getLineB());
                        i3->setLineA(LineRef::invalid);
                        i3->setLineB(LineRef::invalid);
                    }
                    i3->bAEqB = false;
                    i3->bAEqC = false;
                    i3->bBEqC = false;
                    insert(iDest, d3l);
                }
                else
                {
                    // align the found line with the line we already have here
                    if(i3 != iDest)
                    {
                        if(wi2 == e_SrcSelector::A)
                        {
                            iDest->setLineA(i3->getLineA());
                            i3->setLineA(LineRef::invalid);
                            i3->bAEqB = false;
                            i3->bAEqC = false;
                        }
                        else if(wi2 == e_SrcSelector::B)
                        {
                            iDest->setLineB(i3->getLineB());
                            i3->setLineB(LineRef::invalid);
                            i3->bAEqB = false;
                            i3->bBEqC = false;
                        }
                        else if(wi2 == e_SrcSelector::C)
                        {
                            iDest->setLineC(i3->getLineC());
                            i3->setLineC(LineRef::invalid);
                            i3->bBEqC = false;
                            i3->bAEqC = false;
                        }
                    }

                    if(missingWinIdx != e_SrcSelector::None)
                    {
                        for(; i3 != end(); ++i3)
                        {
                            e_SrcSelector wi3 = missingWinIdx;
                            if(i3->getLineInFile(wi3).isValid())
                            {
                                // not found, move the line before iDest
                                Diff3Line d3l;
                                if(wi3 == e_SrcSelector::A)
                                {
                                    if(i3->isEqualAB()) // Stop moving lines up if one equal is found.
                                        break;
                                    d3l.setLineA(i3->getLineA());
                                    i3->setLineA(LineRef::invalid);
                                    i3->bAEqB = false;
                                    i3->bAEqC = false;
                                }
                                if(wi3 == e_SrcSelector::B)
                                {
                                    if(i3->isEqualAB())
                                        break;
                                    d3l.setLineB(i3->getLineB());
                                    i3->setLineB(LineRef::invalid);
                                    i3->bAEqB = false;
                                    i3->bBEqC = false;
                                }
                                if(wi3 == e_SrcSelector::C)
                                {
                                    if(i3->isEqualAC())
                                        break;
                                    d3l.setLineC(i3->getLineC());
                                    i3->setLineC(LineRef::invalid);
                                    i3->bAEqC = false;
                                    i3->bBEqC = false;
                                }
                                insert(iDest, d3l);
                            }
                        } // for(), searching for wi3
                    }
                    break;
                }
            } // for(), searching for wi2
        }     // if, wi found
    }         // for (iMDHL)
}

// Fourth step
void Diff3LineList::calcDiff3LineListTrim(
    const QVector<LineData>* pldA, const QVector<LineData>* pldB, const QVector<LineData>* pldC, ManualDiffHelpList* pManualDiffHelpList)
{
    const Diff3Line d3l_empty = Diff3Line();//gcc 6.3 is over zealous about insisisting on explict initialization of a const.
    remove(d3l_empty);

    Diff3LineList::iterator i3 = begin();
    Diff3LineList::iterator i3A = begin();
    Diff3LineList::iterator i3B = begin();
    Diff3LineList::iterator i3C = begin();

    int line = 0;  // diff3line counters
    int lineA = 0; //
    int lineB = 0;
    int lineC = 0;

    ManualDiffHelpList::iterator iMDHL = pManualDiffHelpList->begin();
    // The iterator i3 and the variable line look ahead.
    // The iterators i3A, i3B, i3C and corresponding lineA, lineB and lineC stop at empty lines, if found.
    // If possible, then the texts from the look ahead will be moved back to the empty places.

    for(; i3 != end(); ++i3, ++line)
    {
        if(iMDHL != pManualDiffHelpList->end())
        {
            if((i3->getLineA().isValid() && i3->getLineA() == iMDHL->getLine1(e_SrcSelector::A)) ||
               (i3->getLineB().isValid() && i3->getLineB() == iMDHL->getLine1(e_SrcSelector::B)) ||
               (i3->getLineC().isValid() && i3->getLineC() == iMDHL->getLine1(e_SrcSelector::C)))
            {
                i3A = i3;
                i3B = i3;
                i3C = i3;
                lineA = line;
                lineB = line;
                lineC = line;
                ++iMDHL;
            }
        }

        if(line > lineA && i3->getLineA().isValid() && i3A->getLineB().isValid() && i3A->isEqualBC() &&
           LineData::equal((*pldA)[i3->getLineA()], (*pldB)[i3A->getLineB()]) &&
           pManualDiffHelpList->isValidMove(i3->getLineA(), i3A->getLineB(), e_SrcSelector::A, e_SrcSelector::B) &&
           pManualDiffHelpList->isValidMove(i3->getLineA(), i3A->getLineC(), e_SrcSelector::A, e_SrcSelector::C))
        {
            // Empty space for A. A matches B and C in the empty line. Move it up.
            i3A->setLineA(i3->getLineA());
            i3A->bAEqB = true;
            i3A->bAEqC = true;

            i3->setLineA(LineRef::invalid);
            i3->bAEqB = false;
            i3->bAEqC = false;
            ++i3A;
            ++lineA;
        }

        if(line > lineB && i3->getLineB().isValid() && i3B->getLineA().isValid() && i3B->isEqualAC() &&
           LineData::equal((*pldB)[i3->getLineB()], (*pldA)[i3B->getLineA()]) &&
           pManualDiffHelpList->isValidMove(i3->getLineB(), i3B->getLineA(), e_SrcSelector::B, e_SrcSelector::A) &&
           pManualDiffHelpList->isValidMove(i3->getLineB(), i3B->getLineC(), e_SrcSelector::B, e_SrcSelector::C))
        {
            // Empty space for B. B matches A and C in the empty line. Move it up.
            i3B->setLineB(i3->getLineB());
            i3B->bAEqB = true;
            i3B->bBEqC = true;
            i3->setLineB(LineRef::invalid);
            i3->bAEqB = false;
            i3->bBEqC = false;
            ++i3B;
            ++lineB;
        }

        if(line > lineC && i3->getLineC().isValid() && i3C->getLineA().isValid() && i3C->isEqualAB() &&
           LineData::equal((*pldC)[i3->getLineC()], (*pldA)[i3C->getLineA()]) &&
           pManualDiffHelpList->isValidMove(i3->getLineC(), i3C->getLineA(), e_SrcSelector::C, e_SrcSelector::A) &&
           pManualDiffHelpList->isValidMove(i3->getLineC(), i3C->getLineB(), e_SrcSelector::C, e_SrcSelector::B))
        {
            // Empty space for C. C matches A and B in the empty line. Move it up.
            i3C->setLineC(i3->getLineC());
            i3C->bAEqC = true;
            i3C->bBEqC = true;
            i3->setLineC(LineRef::invalid);
            i3->bAEqC = false;
            i3->bBEqC = false;
            ++i3C;
            ++lineC;
        }

        if(line > lineA && i3->getLineA().isValid() && !i3->isEqualAB() && !i3->isEqualAC() &&
           pManualDiffHelpList->isValidMove(i3->getLineA(), i3A->getLineB(), e_SrcSelector::A, e_SrcSelector::B) &&
           pManualDiffHelpList->isValidMove(i3->getLineA(), i3A->getLineC(), e_SrcSelector::A, e_SrcSelector::C))
        {
            // Empty space for A. A doesn't match B or C. Move it up.
            i3A->setLineA(i3->getLineA());
            i3->setLineA(LineRef::invalid);

            if(i3A->getLineB().isValid() && LineData::equal((*pldA)[i3A->getLineA()], (*pldB)[i3A->getLineB()]))
            {
                i3A->bAEqB = true;
            }
            if((i3A->isEqualAB() && i3A->isEqualBC()) ||
               (i3A->getLineC().isValid() && LineData::equal((*pldA)[i3A->getLineA()], (*pldC)[i3A->getLineC()])))
            {
                i3A->bAEqC = true;
            }

            ++i3A;
            ++lineA;
        }

        if(line > lineB && i3->getLineB().isValid() && !i3->isEqualAB() && !i3->isEqualBC() &&
           pManualDiffHelpList->isValidMove(i3->getLineB(), i3B->getLineA(), e_SrcSelector::B, e_SrcSelector::A) &&
           pManualDiffHelpList->isValidMove(i3->getLineB(), i3B->getLineC(), e_SrcSelector::B, e_SrcSelector::C))
        {
            // Empty space for B. B matches neither A nor C. Move B up.
            i3B->setLineB(i3->getLineB());
            i3->setLineB(LineRef::invalid);

            if(i3B->getLineA().isValid() && LineData::equal((*pldA)[i3B->getLineA()], (*pldB)[i3B->getLineB()]))
            {
                i3B->bAEqB = true;
            }
            if((i3B->isEqualAB() && i3B->isEqualAC()) ||
               (i3B->getLineC().isValid() && LineData::equal((*pldB)[i3B->getLineB()], (*pldC)[i3B->getLineC()])))
            {
                i3B->bBEqC = true;
            }

            ++i3B;
            ++lineB;
        }

        if(line > lineC && i3->getLineC().isValid() && !i3->isEqualAC() && !i3->isEqualBC() &&
           pManualDiffHelpList->isValidMove(i3->getLineC(), i3C->getLineA(), e_SrcSelector::C, e_SrcSelector::A) &&
           pManualDiffHelpList->isValidMove(i3->getLineC(), i3C->getLineB(), e_SrcSelector::C, e_SrcSelector::B))
        {
            // Empty space for C. C matches neither A nor B. Move C up.
            i3C->setLineC(i3->getLineC());
            i3->setLineC(LineRef::invalid);

            if(i3C->getLineA().isValid() && LineData::equal((*pldA)[i3C->getLineA()], (*pldC)[i3C->getLineC()]))
            {
                i3C->bAEqC = true;
            }
            if((i3C->isEqualAC() && i3C->isEqualAB()) ||
               (i3C->getLineB().isValid() && LineData::equal((*pldB)[i3C->getLineB()], (*pldC)[i3C->getLineC()])))
            {
                i3C->bBEqC = true;
            }

            ++i3C;
            ++lineC;
        }

        if(line > lineA && line > lineB && i3->getLineA().isValid() && i3->isEqualAB() && !i3->isEqualAC())
        {
            // Empty space for A and B. A matches B, but not C. Move A & B up.
            Diff3LineList::iterator i = lineA > lineB ? i3A : i3B;
            int l = lineA > lineB ? lineA : lineB;

            if(pManualDiffHelpList->isValidMove(i->getLineC(), i3->getLineA(), e_SrcSelector::C, e_SrcSelector::A) &&
               pManualDiffHelpList->isValidMove(i->getLineC(), i3->getLineB(), e_SrcSelector::C, e_SrcSelector::B))
            {
                i->setLineA(i3->getLineA());
                i->setLineB(i3->getLineB());
                i->bAEqB = true;

                if(i->getLineC().isValid() && LineData::equal((*pldA)[i->getLineA()], (*pldC)[i->getLineC()]))
                {
                    i->bAEqC = true;
                    i->bBEqC = true;
                }

                i3->setLineA(LineRef::invalid);
                i3->setLineB(LineRef::invalid);
                i3->bAEqB = false;
                i3A = i;
                i3B = i;
                ++i3A;
                ++i3B;
                lineA = l + 1;
                lineB = l + 1;
            }
        }
        else if(line > lineA && line > lineC && i3->getLineA().isValid() && i3->isEqualAC() && !i3->isEqualAB())
        {
            // Empty space for A and C. A matches C, but not B. Move A & C up.
            Diff3LineList::iterator i = lineA > lineC ? i3A : i3C;
            int l = lineA > lineC ? lineA : lineC;

            if(pManualDiffHelpList->isValidMove(i->getLineB(), i3->getLineA(), e_SrcSelector::B, e_SrcSelector::A) &&
               pManualDiffHelpList->isValidMove(i->getLineB(), i3->getLineC(), e_SrcSelector::B, e_SrcSelector::C))
            {
                i->setLineA(i3->getLineA());
                i->setLineC(i3->getLineC());
                i->bAEqC = true;

                if(i->getLineB().isValid() && LineData::equal((*pldA)[i->getLineA()], (*pldB)[i->getLineB()]))
                {
                    i->bAEqB = true;
                    i->bBEqC = true;
                }

                i3->setLineA(LineRef::invalid);
                i3->setLineC(LineRef::invalid);
                i3->bAEqC = false;
                i3A = i;
                i3C = i;
                ++i3A;
                ++i3C;
                lineA = l + 1;
                lineC = l + 1;
            }
        }
        else if(line > lineB && line > lineC && i3->getLineB().isValid() && i3->isEqualBC() && !i3->isEqualAC())
        {
            // Empty space for B and C. B matches C, but not A. Move B & C up.
            Diff3LineList::iterator i = lineB > lineC ? i3B : i3C;
            int l = lineB > lineC ? lineB : lineC;
            if(pManualDiffHelpList->isValidMove(i->getLineA(), i3->getLineB(), e_SrcSelector::A, e_SrcSelector::B) &&
               pManualDiffHelpList->isValidMove(i->getLineA(), i3->getLineC(), e_SrcSelector::A, e_SrcSelector::C))
            {
                i->setLineB(i3->getLineB());
                i->setLineC(i3->getLineC());
                i->bBEqC = true;

                if(i->getLineA().isValid() && LineData::equal((*pldA)[i->getLineA()], (*pldB)[i->getLineB()]))
                {
                    i->bAEqB = true;
                    i->bAEqC = true;
                }

                i3->setLineB(LineRef::invalid);
                i3->setLineC(LineRef::invalid);
                i3->bBEqC = false;
                i3B = i;
                i3C = i;
                ++i3B;
                ++i3C;
                lineB = l + 1;
                lineC = l + 1;
            }
        }

        if(i3->getLineA().isValid())
        {
            lineA = line + 1;
            i3A = i3;
            ++i3A;
        }
        if(i3->getLineB().isValid())
        {
            lineB = line + 1;
            i3B = i3;
            ++i3B;
        }
        if(i3->getLineC().isValid())
        {
            lineC = line + 1;
            i3C = i3;
            ++i3C;
        }
    }

    remove(d3l_empty);

    /*

   Diff3LineList::iterator it = d3ll.begin();
   int li=0;
   for( ; it!=d3ll.end(); ++it, ++li )
   {
      printf( "%4d %4d %4d %4d  A%c=B A%c=C B%c=C\n",
         li, it->getLineA(), it->getLineB(), it->getLineC(),
         it->isEqualAB() ? '=' : '!', it->isEqualAC() ? '=' : '!', it->isEqualBC() ? '=' : '!' );

   }
*/
}

void DiffBufferInfo::init(Diff3LineList* pD3ll, const Diff3LineVector* pD3lv,
                          const QVector<LineData>* pldA, LineCount sizeA, const QVector<LineData>* pldB, LineCount sizeB, const QVector<LineData>* pldC, LineCount sizeC)
{
    m_pDiff3LineList = pD3ll;
    m_pDiff3LineVector = pD3lv;
    mLineDataA = pldA;
    mLineDataB = pldB;
    mLineDataC = pldC;
    m_sizeA = sizeA;
    m_sizeB = sizeB;
    m_sizeC = sizeC;
}

void Diff3LineList::calcWhiteDiff3Lines(
    const QVector<LineData>* pldA, const QVector<LineData>* pldB, const QVector<LineData>* pldC, const bool bIgnoreComments)
{
    Diff3LineList::iterator i3;

    for(i3 = begin(); i3 != end(); ++i3)
    {
        i3->bIsPureCommentA = (i3->getLineA().isValid() && pldA != nullptr  && (*pldA)[i3->getLineA()].isPureComment());
        i3->bIsPureCommentB = (i3->getLineB().isValid() && pldB != nullptr  && (*pldB)[i3->getLineB()].isPureComment());
        i3->bIsPureCommentC = (i3->getLineC().isValid() && pldC != nullptr  && (*pldC)[i3->getLineC()].isPureComment());

        i3->bWhiteLineA = (!i3->getLineA().isValid() || pldA == nullptr || (*pldA)[i3->getLineA()].whiteLine() || (bIgnoreComments && (*pldA)[i3->getLineA()].isPureComment()));
        i3->bWhiteLineB = (!i3->getLineB().isValid() || pldB == nullptr || (*pldB)[i3->getLineB()].whiteLine() || (bIgnoreComments && (*pldB)[i3->getLineB()].isPureComment()));
        i3->bWhiteLineC = (!i3->getLineC().isValid() || pldC == nullptr || (*pldC)[i3->getLineC()].whiteLine() || (bIgnoreComments && (*pldC)[i3->getLineC()].isPureComment()));
    }
}

// My own diff-invention:
void calcDiff(const QString& line1, const QString& line2, DiffList& diffList, int match, int maxSearchRange)
{
    diffList.clear();

    const QChar* p1end = line1.constData() + line1.size();
    const QChar* p2end = line2.constData() + line2.size();

    QString::const_iterator p1=line1.begin(), p2=line2.begin();

    /*
        This loop should never reach the exit condition specified here. However it must have a hard wired
        stopping point to prevent runaway allocation if something unexpected happens.
        diffList is therefor hard capped at aprox 50 MB in size.
     */
    for(; diffList.size() * sizeof(Diff) + sizeof(DiffList) < (50 << 20);)
    {
        int nofEquals = 0;
        while(p1 != line1.end() && p2 != line2.end() &&  *p1 == *p2)
        {
            ++p1;
            ++p2;
            ++nofEquals;
        }

        bool bBestValid = false;
        int bestI1 = 0;
        int bestI2 = 0;
        int i1 = 0;
        int i2 = 0;

        for(i1 = 0;; ++i1)
        {
            if(&p1[i1] == p1end || (bBestValid && i1 >= bestI1 + bestI2))
            {
                break;
            }
            for(i2 = 0; i2 < maxSearchRange; ++i2)
            {
                if(&p2[i2] == p2end || (bBestValid && i1 + i2 >= bestI1 + bestI2))
                {
                    break;
                }
                else if(p2[i2] == p1[i1] &&
                        (match == 1 || abs(i1 - i2) < 3 || (&p2[i2 + 1] == p2end && &p1[i1 + 1] == p1end) ||
                         (&p2[i2 + 1] != p2end && &p1[i1 + 1] != p1end && p2[i2 + 1] == p1[i1 + 1])))
                {
                    if(i1 + i2 < bestI1 + bestI2 || !bBestValid)
                    {
                        bestI1 = i1;
                        bestI2 = i2;
                        bBestValid = true;
                        break;
                    }
                }
            }
        }

        // The match was found using the strict search. Go back if there are non-strict
        // matches.
        while(bestI1 >= 1 && bestI2 >= 1 && p1[bestI1 - 1] == p2[bestI2 - 1])
        {
            --bestI1;
            --bestI2;
        }

        bool bEndReached = false;
        if(bBestValid)
        {
            // continue somehow
            Diff d(nofEquals, bestI1, bestI2);
            Q_ASSERT(nofEquals + bestI1 + bestI2 != 0);
            diffList.push_back(d);

            p1 += bestI1;
            p2 += bestI2;
        }
        else
        {
            // Nothing else to match.
            Diff d(nofEquals, line1.end() - p1, line2.end() - p2);
            diffList.push_back(d);

            bEndReached = true; //break;
        }

        // Sometimes the algorithm that chooses the first match unfortunately chooses
        // a match where later actually equal parts don't match anymore.
        // A different match could be achieved, if we start at the end.
        // Do it, if it would be a better match.
        int nofUnmatched = 0;
        QString::const_iterator pu1 = p1 - 1;
        QString::const_iterator pu2 = p2 - 1;

        while(pu1 >= line1.begin() && pu2 >= line2.begin() && *pu1 == *pu2)
        {
            ++nofUnmatched;
            --pu1;
            --pu2;
        }

        Diff d = diffList.back();
        if(nofUnmatched > 0)
        {
            // We want to go backwards the nofUnmatched elements and redo
            // the matching
            d = diffList.back();
            Diff origBack = d;
            diffList.pop_back();

            while(nofUnmatched > 0)
            {
                if(d.diff1() > 0 && d.diff2() > 0)
                {
                    d.adjustDiff1(-1);
                    d.adjustDiff2(-1);
                    --nofUnmatched;
                }
                else if(d.numberOfEquals() > 0)
                {
                    d.adjustNumberOfEquals(-1);
                    --nofUnmatched;
                }

                if(d.numberOfEquals() == 0 && (d.diff1() == 0 || d.diff2() == 0) && nofUnmatched > 0)
                {
                    if(diffList.empty())
                        break;
                    d.adjustNumberOfEquals(diffList.back().numberOfEquals());
                    d.adjustDiff1(diffList.back().diff1());
                    d.adjustDiff2(diffList.back().diff2());
                    diffList.pop_back();
                    bEndReached = false;
                }
            }

            if(bEndReached)
                diffList.push_back(origBack);
            else
            {

                p1 = pu1 + 1 + nofUnmatched;
                p2 = pu2 + 1 + nofUnmatched;
                diffList.push_back(d);
            }
        }
        if(bEndReached)
            break;
    }

    Q_ASSERT(diffList.size() * sizeof(Diff) + sizeof(DiffList) <= (50 << 20));

    // Verify difflist
    {
        qint32 l1 = 0;
        qint32 l2 = 0;

        for(const Diff& theDiff: diffList)
        {
            l1 += (theDiff.numberOfEquals() + theDiff.diff1());
            l2 += (theDiff.numberOfEquals() + theDiff.diff2());
        }

        Q_ASSERT(l1 == line1.size() && l2 == line2.size());
    }
}

bool Diff3Line::fineDiff(bool inBTextsTotalEqual, const e_SrcSelector selector, const QVector<LineData>* v1, const QVector<LineData>* v2, const IgnoreFlags eIgnoreFlags)
{
    LineRef k1 = 0;
    LineRef k2 = 0;
    int maxSearchLength = 500;
    bool bTextsTotalEqual = inBTextsTotalEqual;
    bool bIgnoreComments = eIgnoreFlags & IgnoreFlag::ignoreComments;
    bool bIgnoreWhiteSpace = eIgnoreFlags & IgnoreFlag::ignoreWhiteSpace;

    Q_ASSERT(selector == e_SrcSelector::A || selector == e_SrcSelector::B || selector == e_SrcSelector::C);

    if(selector == e_SrcSelector::A)
    {
        k1 = getLineA();
        k2 = getLineB();
    }
    else if(selector == e_SrcSelector::B)
    {
        k1 = getLineB();
        k2 = getLineC();
    }
    else if(selector == e_SrcSelector::C)
    {
        k1 = getLineC();
        k2 = getLineA();
    }

    qDebug(kdiffCore) << "k1 = " << k1 << ", k2 = " << k2;
    if((!k1.isValid() && k2.isValid()) || (k1.isValid() && !k2.isValid())) bTextsTotalEqual = false;
    if(k1.isValid() && k2.isValid())
    {
        if((*v1)[k1].size() != (*v2)[k2].size() || QString::compare((*v1)[k1].getLine(), (*v2)[k2].getLine()) != 0)
        {
            bTextsTotalEqual = false;
            DiffList* pDiffList = new DiffList;
            calcDiff((*v1)[k1].getLine(), (*v2)[k2].getLine(), *pDiffList, 2, maxSearchLength);

            // Optimize the diff list.
            DiffList::iterator dli;
            bool bUsefulFineDiff = false;
            for(dli = pDiffList->begin(); dli != pDiffList->end(); ++dli)
            {
                if(dli->numberOfEquals() >= 4)
                {
                    bUsefulFineDiff = true;
                    break;
                }
            }

            for(dli = pDiffList->begin(); dli != pDiffList->end(); ++dli)
            {
                if(dli->numberOfEquals() < 4 && (dli->diff1() > 0 || dli->diff2() > 0) && !(bUsefulFineDiff && dli == pDiffList->begin()))
                {
                    dli->adjustDiff1(dli->numberOfEquals());
                    dli->adjustDiff2(dli->numberOfEquals());
                    dli->setNumberOfEquals(0);
                }
            }

            setFineDiff(selector, pDiffList);
        }
        /*
            Override default euality for white lines and comments.
        */
        if(((bIgnoreComments && (*v1)[k1].isSkipable()) || (bIgnoreWhiteSpace && (*v1)[k1].whiteLine())) && ((bIgnoreComments && (*v2)[k2].isSkipable()) || (bIgnoreWhiteSpace && (*v2)[k2].whiteLine())))
        {
            if(selector == e_SrcSelector::A)
            {
                bAEqB = true;
            }
            else if(selector == e_SrcSelector::B)
            {
                bBEqC = true;
            }
            else if(selector == e_SrcSelector::C)
            {
                bAEqC = true;
            }
        }
    }

    return bTextsTotalEqual;
}

void Diff3Line::getLineInfo(const e_SrcSelector winIdx, const bool isTriple, LineRef& lineIdx,
                            DiffList*& pFineDiff1, DiffList*& pFineDiff2, // return values
                            ChangeFlags& changed, ChangeFlags& changed2) const
{
    changed = NoChange;
    changed2 = NoChange;
    bool bAEqualB = this->isEqualAB() || (bWhiteLineA && bWhiteLineB);
    bool bAEqualC = this->isEqualAC() || (bWhiteLineA && bWhiteLineC);
    bool bBEqualC = this->isEqualBC() || (bWhiteLineB && bWhiteLineC);

    Q_ASSERT(winIdx >= e_SrcSelector::A && winIdx <= e_SrcSelector::C);
    if(winIdx == e_SrcSelector::A)
    {
        lineIdx = getLineA();
        pFineDiff1 = pFineAB;
        pFineDiff2 = pFineCA;

        changed = ((!getLineB().isValid()) != (!lineIdx.isValid()) ? AChanged : NoChange) |
                   ((!getLineC().isValid()) != (!lineIdx.isValid()) && isTriple ? BChanged : NoChange);
        changed2 = (bAEqualB ? NoChange : AChanged) | (bAEqualC || !isTriple ? NoChange : BChanged);
    }
    else if(winIdx == e_SrcSelector::B)
    {
        lineIdx = getLineB();
        pFineDiff1 = pFineBC;
        pFineDiff2 = pFineAB;
        changed = ((!getLineC().isValid()) != (!lineIdx.isValid()) && isTriple ? AChanged : NoChange) |
                   ((!getLineA().isValid()) != (!lineIdx.isValid()) ? BChanged : NoChange);
        changed2 = (bBEqualC || !isTriple ? NoChange : AChanged) | (bAEqualB ? NoChange : BChanged);
    }
    else if(winIdx == e_SrcSelector::C)
    {
        lineIdx = getLineC();
        pFineDiff1 = pFineCA;
        pFineDiff2 = pFineBC;
        changed = ((!getLineA().isValid()) != (!lineIdx.isValid()) ? AChanged : NoChange) |
                   ((!getLineB().isValid()) != (!lineIdx.isValid()) ? BChanged : NoChange);
        changed2 = (bAEqualC ? NoChange : AChanged) | (bBEqualC ? NoChange : BChanged);
    }
}

bool Diff3LineList::fineDiff(const e_SrcSelector selector, const QVector<LineData>* v1, const QVector<LineData>* v2, const IgnoreFlags eIgnoreFlags)
{
    // Finetuning: Diff each line with deltas
    ProgressProxy pp;
    Diff3LineList::iterator i;
    bool bTextsTotalEqual = true;
    int listSize = size();
    pp.setMaxNofSteps(listSize);
    int listIdx = 0;
    for(i = begin(); i != end(); ++i)
    {
        bTextsTotalEqual = i->fineDiff(bTextsTotalEqual, selector, v1, v2, eIgnoreFlags);
        ++listIdx;
        pp.step();
    }
    return bTextsTotalEqual;
}

// Convert the list to a vector of pointers
void Diff3LineList::calcDiff3LineVector(Diff3LineVector& d3lv)
{
    d3lv.resize(size());
    Diff3LineList::iterator i;
    int j = 0;
    for(i = begin(); i != end(); ++i, ++j)
    {
        d3lv[j] = &(*i);
    }
    Q_ASSERT(j == d3lv.size());
}

// Just make sure that all input lines are in the output too, exactly once.
void Diff3LineList::debugLineCheck(const LineCount size, const e_SrcSelector srcSelector) const
{
    Diff3LineList::const_iterator it = begin();
    int i = 0;

    for(it = begin(); it != end(); ++it)
    {
        LineRef line;

        Q_ASSERT(srcSelector == e_SrcSelector::A || srcSelector == e_SrcSelector::B || srcSelector == e_SrcSelector::C);
        if(srcSelector == e_SrcSelector::A)
            line = it->getLineA();
        else if(srcSelector == e_SrcSelector::B)
            line = it->getLineB();
        else if(srcSelector == e_SrcSelector::C)
            line = it->getLineC();

        if(line.isValid())
        {
            if(line != i)
            {
                KMessageBox::error(nullptr, i18n(
                                          "Data loss error:\n"
                                          "If it is reproducible please contact the author.\n"),
                                   i18n("Severe Internal Error"));

                qCCritical(kdiffMain) << i18n("Severe Internal Error.") << " line != i for srcSelector=" << (int)srcSelector << "\n";
                ::exit(-1);
            }
            ++i;
        }
    }

    if(size != i)
    {
        KMessageBox::error(nullptr, i18n(
                                  "Data loss error:\n"
                                  "If it is reproducible please contact the author.\n"),
                           i18n("Severe Internal Error"));

        qCCritical(kdiffMain) << i18n("Severe Internal Error.: ") << size << " != " << i << "\n";
        ::exit(-1);
    }
}

void Diff3LineList::findHistoryRange(const QRegExp& historyStart, bool bThreeFiles,
                             Diff3LineList::const_iterator& iBegin, Diff3LineList::const_iterator& iEnd, int& idxBegin, int& idxEnd) const
{
    QString historyLead;
    // Search for start of history
    for(iBegin = begin(), idxBegin = 0; iBegin != end(); ++iBegin, ++idxBegin)
    {
        if(historyStart.exactMatch(iBegin->getString(e_SrcSelector::A)) &&
           historyStart.exactMatch(iBegin->getString(e_SrcSelector::B)) &&
           (!bThreeFiles || historyStart.exactMatch(iBegin->getString(e_SrcSelector::C))))
        {
            historyLead = Utils::calcHistoryLead(iBegin->getString(e_SrcSelector::A));
            break;
        }
    }
    // Search for end of history
    for(iEnd = iBegin, idxEnd = idxBegin; iEnd != end(); ++iEnd, ++idxEnd)
    {
        QString sA = iEnd->getString(e_SrcSelector::A);
        QString sB = iEnd->getString(e_SrcSelector::B);
        QString sC = iEnd->getString(e_SrcSelector::C);
        if(!((sA.isEmpty() || historyLead == Utils::calcHistoryLead(sA)) &&
             (sB.isEmpty() || historyLead == Utils::calcHistoryLead(sB)) &&
             (!bThreeFiles || sC.isEmpty() || historyLead == Utils::calcHistoryLead(sC))))
        {
            break; // End of the history
        }
    }
}
