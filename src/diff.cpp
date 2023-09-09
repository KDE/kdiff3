// clang-format off
/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

#include "diff.h"
#include <QtGlobal>

#include "gnudiff_diff.h"
#include "Logging.h"
#include "options.h"
#include "ProgressProxy.h"
#include "Utils.h"

#include <algorithm>           // for min
#include <cstdlib>
#include <ctype.h>
#include <memory>
#include <optional>
#include <utility>             // for swap

#ifndef AUTOTEST
#include <KLocalizedString>
#include <KMessageBox>
#endif

#include <QRegularExpression>
#include <QSharedPointer>
#include <QTextStream>

constexpr bool g_bIgnoreWhiteSpace = true;

QSharedPointer<DiffBufferInfo> Diff3Line::m_pDiffBufferInfo = QSharedPointer<DiffBufferInfo>::create();

qint32 LineData::width(qint32 tabSize) const
{
    const QString pLine = getLine();
    qint32 w = 0;
    qint32 j = 0;
    for(QtSizeType i = 0; i < size(); ++i)
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

class StringIter {
    QString::const_iterator ptr;
    QString::const_iterator end;

public:
    StringIter(QString const& s)
        : ptr(s.begin())
        , end(s.end())
    {}

    bool hasPeek() const {
        return ptr < end;
    }

    std::optional<QChar> tryPeek() const {
        if (ptr < end) {
            return *ptr;
        } else {
            return {};
        }
    }

    void next() {
        assert(ptr < end);
        ptr++;
    }
};

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
        StringIter p1{line1};
        StringIter p2{line2};

        for(; ; p1.next(), p2.next())
        {
            // Advance to the next non-whitespace character or EOL.
            while (p1.hasPeek() && isspace(p1.tryPeek()->toLatin1())) {
                p1.next();
            }
            while (p2.hasPeek() && isspace(p2.tryPeek()->toLatin1())) {
                p2.next();
            }

            // If the two strings differ outside of whitespace, or either ends earlier, return false.
            if(p1.tryPeek() != p2.tryPeek())
                return false;

            // If both strings end together, return true.
            if (!p1.hasPeek() && !p2.hasPeek()) {
                return true;
            }

            // Test remaining characters.
        }
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
    LineRef lineA = 0;
    LineRef lineB = 0;
    Diff d;

    qCInfo(kdiffMain) << "Enter: calcDiff3LineListUsingAB";
    while(i != pDiffListAB->end())
    {
        d = *i;
        ++i;

        while(d.numberOfEquals() > 0)
        {
            Diff3Line d3l;

            d3l.bAEqB = true;
            d3l.setLineA(lineA);
            d3l.setLineB(lineB);
            d.adjustNumberOfEquals(-1);
            ++lineA;
            ++lineB;

            qCDebug(kdiffCore) << "lineA = " << d3l.getLineA() << ", lineB = " << d3l.getLineB() ;
            push_back(d3l);
        }

        while(d.diff1() > 0 && d.diff2() > 0)
        {
            Diff3Line d3l;

            d3l.setLineA(lineA);
            d3l.setLineB(lineB);
            d.adjustDiff1(-1);
            d.adjustDiff2(-1);
            ++lineA;
            ++lineB;

            qCDebug(kdiffCore) << "lineA = " << d3l.getLineA() << ", lineB = " << d3l.getLineB() ;
            push_back(d3l);
        }

        while(d.diff1() > 0)
        {
            Diff3Line d3l;

            d3l.setLineA(lineA);
            d.adjustDiff1(-1);
            ++lineA;

            qCDebug(kdiffCore) << "lineA = " << d3l.getLineA() << ", lineB = " << d3l.getLineB() ;
            push_back(d3l);
        }

        while(d.diff2() > 0)
        {
            Diff3Line d3l;

            d3l.setLineB(lineB);
            d.adjustDiff2(-1);
            ++lineB;

            qCDebug(kdiffCore) << "lineA = " << d3l.getLineA() << ", lineB = " << d3l.getLineB() ;
            push_back(d3l);
        }
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
    LineRef lineA = 0;
    LineRef lineC = 0;
    Diff d;

    while(i != pDiffListAC->end())
    {
        d = *i;
        ++i;

        assert(d.diff1() <= TYPE_MAX(LineRef::LineType) && d.diff2() <= TYPE_MAX(LineRef::LineType));

        while(d.numberOfEquals() > 0)
        {
            Diff3Line d3l;

            // Find the corresponding lineA
            while(i3->getLineA() != lineA && i3 != end())
                ++i3;
            assert(i3 != end());

            i3->setLineC(lineC);
            i3->bAEqC = true;
            i3->bBEqC = i3->isEqualAB();

            d.adjustNumberOfEquals(-1);
            ++lineA;
            ++lineC;
            ++i3;
        }
        assert(i3 != end() || (d.diff1() == 0 && d.diff2() == 0));

        while(d.diff1() > 0 && d.diff2() > 0)
        {
            Diff3Line d3l;

            d3l.setLineC(lineC);
            insert(i3, d3l);
            d.adjustDiff1(-1);
            d.adjustDiff2(-1);
            ++lineA;
            ++lineC;
        }

        while(d.diff1() > 0)
        {
            d.adjustDiff1(-1);
            ++lineA;
        }

        while(d.diff2() > 0)
        {
            Diff3Line d3l;

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
    LineRef lineB = 0;
    LineRef lineC = 0;
    Diff d;

    while(i == pDiffListBC->end())
    {
        d = *i;
        ++i;

        while(d.numberOfEquals() > 0)
        {
            Diff3Line d3l;
            // Find the corresponding lineB and lineC
            while(i3b != end() && i3b->getLineB() != lineB)
                ++i3b;

            while(i3c != end() && i3c->getLineC() != lineC)
                ++i3c;

            assert(i3b != end());
            assert(i3c != end());

            if(i3b == i3c)
            {
                assert(i3b->getLineC() == lineC);
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
                    assert(i3b1 != end() || i3c1 != end());
                    if(i3c1 != end()) ++i3c1;
                    if(i3b1 != end()) ++i3b1;
                }

                if(i3c1 == i3b && !i3b->isEqualAB()) // i3c before i3b
                {
                    Diff3LineList::iterator i3 = i3c;
                    qint32 nofDisturbingLines = 0;
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
                    qint32 nofDisturbingLines = 0;
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

        while(d.diff1() > 0)
        {
            Diff3Line d3l;
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

        while(d.diff2() > 0)
        {
            d.adjustDiff2(-1);
            ++lineC;
        }
    }
    /*
   Diff3LineList::iterator it = d3ll.begin();
   qint32 li=0;
   for( ; it!=d3ll.end(); ++it, ++li )
   {
      printf( "%4d %4d %4d %4d  A%c=B A%c=C B%c=C\n",
         li, it->getLineA(), it->getLineB(), it->getLineC(),
         it->isEqualAB() ? '=' : '!', it->isEqualAC() ? '=' : '!', it->isEqualBC() ? '=' : '!' );
   }
   printf("\n");*/
}

// Test if the move would pass a barrier. Return true if not.
bool ManualDiffHelpList::isValidMove(LineRef line1, LineRef line2, e_SrcSelector winIdx1, e_SrcSelector winIdx2) const
{
    if(line1.isValid() && line2.isValid())
    {
        for(const ManualDiffHelpEntry& mdhe: *this)
        {
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

    ManualDiffHelpEntry mdhe(winIdx, firstLine, lastLine);

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

bool ManualDiffHelpEntry::isValidMove(LineRef line1, LineRef line2, e_SrcSelector winIdx1, e_SrcSelector winIdx2) const
{
    // Barrier
    LineRef l1 = winIdx1 == e_SrcSelector::A ? lineA1 : winIdx1 == e_SrcSelector::B ? lineB1 : lineC1;
    LineRef l2 = winIdx2 == e_SrcSelector::A ? lineA1 : winIdx2 == e_SrcSelector::B ? lineB1 : lineC1;

    if(l1.isValid() && l2.isValid())
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

qint32 ManualDiffHelpEntry::calcManualDiffFirstDiff3LineIdx(const Diff3LineVector& d3lv)
{
    QtSizeType i;
    for(i = 0; i < d3lv.size(); ++i)
    {
        const Diff3Line* d3l = d3lv[i];
        if((lineA1.isValid() && lineA1 == d3l->getLineA()) ||
           (lineB1.isValid() && lineB1 == d3l->getLineB()) ||
           (lineC1.isValid() && lineC1 == d3l->getLineC()))
            return SafeInt<qint32>(i);
    }
    return -1;
}

void DiffList::runDiff(const std::shared_ptr<LineDataVector> &p1, const size_t index1, LineRef size1, const std::shared_ptr<LineDataVector> &p2, const size_t index2, LineRef size2,
                    const QSharedPointer<Options> &pOptions)
{
    ProgressScope pp;
    static GnuDiff gnuDiff; // All values are initialized with zeros.

    ProgressProxy::setCurrent(0);

    clear();
    if(p1->empty() || (*p1)[index1].getBuffer() == nullptr || p2->empty() || (*p2)[index2].getBuffer() == nullptr || size1 == 0 || size2 == 0)
    {
        if(!p1->empty() && !p2->empty() && (*p1)[index1].getBuffer() == nullptr && (*p2)[index2].getBuffer() == nullptr && size1 == size2)
            push_back(Diff(size1, 0, 0));
        else
        {
            push_back(Diff(0, size1, size2));
        }
    }
    else
    {
        assert((size_t)size1 < p1->size() && (size_t)size2 < p2->size());

        GnuDiff::comparison comparisonInput;
        memset(&comparisonInput, 0, sizeof(comparisonInput));
        comparisonInput.parent = nullptr;
        comparisonInput.file[0].buffer = (*p1)[index1].getBuffer()->unicode() + (*p1)[index1].getOffset();                                         //ptr to buffer
        comparisonInput.file[0].buffered = ((*p1)[index1 + size1 - 1].getOffset() + (*p1)[index1 + size1 - 1].size() - (*p1)[index1].getOffset()); // size of buffer
        comparisonInput.file[1].buffer = (*p2)[index2].getBuffer()->unicode() + (*p2)[index2].getOffset();                                         //ptr to buffer
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
            assert(d.numberOfEquals() == e->line1 - currentLine2);

            currentLine1 += (LineRef)(d.numberOfEquals() + d.diff1());
            currentLine2 += (LineRef)(d.numberOfEquals() + d.diff2());
            assert(currentLine1 <= size1 && currentLine2 <= size2);
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

    verify(size1, size2);

    ProgressProxy::setCurrent(1);
}

// Verify difflist
void DiffList::verify(const LineRef size1, const LineRef size2)
{
#ifndef NDEBUG
    LineRef l1 = 0;
    LineRef l2 = 0;

    for(const Diff& curDiff: *this)
    {
        assert(curDiff.numberOfEquals() >= 0);
        assert(curDiff.diff1() <= TYPE_MAX(LineRef::LineType) && curDiff.diff2() <= TYPE_MAX(LineRef::LineType));
        l1 += curDiff.numberOfEquals() + LineRef(curDiff.diff1());
        l2 += curDiff.numberOfEquals() + LineRef(curDiff.diff2());
    }

    assert(l1 == size1 && l2 == size2);
#else
    Q_UNUSED(size1); Q_UNUSED(size2);
#endif
}

void ManualDiffHelpList::runDiff(const std::shared_ptr<LineDataVector> &p1, LineRef size1, const std::shared_ptr<LineDataVector> &p2, LineRef size2, DiffList& diffList,
                                 e_SrcSelector winIdx1, e_SrcSelector winIdx2,
                                 const QSharedPointer<Options> &pOptions)
{
    diffList.clear();
    DiffList diffList2;

    qint32 l1begin = 0;
    qint32 l2begin = 0;
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
        qint32 alignedSum = (!iMDHL->getLine1(e_SrcSelector::A).isValid() ? 0 : 1) + (!iMDHL->getLine1(e_SrcSelector::B).isValid() ? 0 : 1) + (!iMDHL->getLine1(e_SrcSelector::C).isValid() ? 0 : 1);
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
    const std::shared_ptr<LineDataVector> &pldA, const std::shared_ptr<LineDataVector> &pldB, const std::shared_ptr<LineDataVector> &pldC, ManualDiffHelpList* pManualDiffHelpList)
{
    const Diff3Line d3l_empty;
    remove(d3l_empty);

    Diff3LineList::iterator lookAhead = begin();
    Diff3LineList::iterator i3A = begin();
    Diff3LineList::iterator i3B = begin();
    Diff3LineList::iterator i3C = begin();

    qint32 line = 0;  // diff3line counters
    qint32 lineA = 0; //
    qint32 lineB = 0;
    qint32 lineC = 0;

    ManualDiffHelpList::iterator iMDHL = pManualDiffHelpList->begin();
    // The iterator lookAhead is the variable line look ahead.
    // The iterators i3A, i3B, i3C and corresponding lineA, lineB and lineC stop at empty lines, if found.
    // If possible, then the texts from the look ahead will be moved back to the empty places.

    for(; lookAhead != end(); ++lookAhead, ++line)
    {
        if(iMDHL != pManualDiffHelpList->end())
        {
            if((lookAhead->getLineA().isValid() && lookAhead->getLineA() == iMDHL->getLine1(e_SrcSelector::A)) ||
               (lookAhead->getLineB().isValid() && lookAhead->getLineB() == iMDHL->getLine1(e_SrcSelector::B)) ||
               (lookAhead->getLineC().isValid() && lookAhead->getLineC() == iMDHL->getLine1(e_SrcSelector::C)))
            {
                i3A = lookAhead;
                i3B = lookAhead;
                i3C = lookAhead;
                lineA = line;
                lineB = line;
                lineC = line;
                ++iMDHL;
            }
        }

        if(line > lineA && lookAhead->getLineA().isValid() && i3A->getLineB().isValid() && i3A->isEqualBC() &&
           LineData::equal((*pldA)[lookAhead->getLineA()], (*pldB)[i3A->getLineB()]) &&
           pManualDiffHelpList->isValidMove(lookAhead->getLineA(), i3A->getLineB(), e_SrcSelector::A, e_SrcSelector::B) &&
           pManualDiffHelpList->isValidMove(lookAhead->getLineA(), i3A->getLineC(), e_SrcSelector::A, e_SrcSelector::C))
        {
            // Empty space for A. A matches B and C in the empty line. Move it up.
            i3A->setLineA(lookAhead->getLineA());
            i3A->bAEqB = true;
            i3A->bAEqC = true;

            lookAhead->setLineA(LineRef::invalid);
            lookAhead->bAEqB = false;
            lookAhead->bAEqC = false;
            ++i3A;
            ++lineA;
        }

        if(line > lineB && lookAhead->getLineB().isValid() && i3B->getLineA().isValid() && i3B->isEqualAC() &&
           LineData::equal((*pldB)[lookAhead->getLineB()], (*pldA)[i3B->getLineA()]) &&
           pManualDiffHelpList->isValidMove(lookAhead->getLineB(), i3B->getLineA(), e_SrcSelector::B, e_SrcSelector::A) &&
           pManualDiffHelpList->isValidMove(lookAhead->getLineB(), i3B->getLineC(), e_SrcSelector::B, e_SrcSelector::C))
        {
            // Empty space for B. B matches A and C in the empty line. Move it up.
            i3B->setLineB(lookAhead->getLineB());
            i3B->bAEqB = true;
            i3B->bBEqC = true;
            lookAhead->setLineB(LineRef::invalid);
            lookAhead->bAEqB = false;
            lookAhead->bBEqC = false;
            ++i3B;
            ++lineB;
        }

        if(line > lineC && lookAhead->getLineC().isValid() && i3C->getLineA().isValid() && i3C->isEqualAB() &&
           LineData::equal((*pldC)[lookAhead->getLineC()], (*pldA)[i3C->getLineA()]) &&
           pManualDiffHelpList->isValidMove(lookAhead->getLineC(), i3C->getLineA(), e_SrcSelector::C, e_SrcSelector::A) &&
           pManualDiffHelpList->isValidMove(lookAhead->getLineC(), i3C->getLineB(), e_SrcSelector::C, e_SrcSelector::B))
        {
            // Empty space for C. C matches A and B in the empty line. Move it up.
            i3C->setLineC(lookAhead->getLineC());
            i3C->bAEqC = true;
            i3C->bBEqC = true;
            lookAhead->setLineC(LineRef::invalid);
            lookAhead->bAEqC = false;
            lookAhead->bBEqC = false;
            ++i3C;
            ++lineC;
        }

        if(line > lineA && lookAhead->getLineA().isValid() && !lookAhead->isEqualAB() && !lookAhead->isEqualAC() &&
           pManualDiffHelpList->isValidMove(lookAhead->getLineA(), i3A->getLineB(), e_SrcSelector::A, e_SrcSelector::B) &&
           pManualDiffHelpList->isValidMove(lookAhead->getLineA(), i3A->getLineC(), e_SrcSelector::A, e_SrcSelector::C))
        {
            // Empty space for A. A doesn't match B or C. Move it up.
            i3A->setLineA(lookAhead->getLineA());
            lookAhead->setLineA(LineRef::invalid);

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

        if(line > lineB && lookAhead->getLineB().isValid() && !lookAhead->isEqualAB() && !lookAhead->isEqualBC() &&
           pManualDiffHelpList->isValidMove(lookAhead->getLineB(), i3B->getLineA(), e_SrcSelector::B, e_SrcSelector::A) &&
           pManualDiffHelpList->isValidMove(lookAhead->getLineB(), i3B->getLineC(), e_SrcSelector::B, e_SrcSelector::C))
        {
            // Empty space for B. B matches neither A nor C. Move B up.
            i3B->setLineB(lookAhead->getLineB());
            lookAhead->setLineB(LineRef::invalid);

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

        if(line > lineC && lookAhead->getLineC().isValid() && !lookAhead->isEqualAC() && !lookAhead->isEqualBC() &&
           pManualDiffHelpList->isValidMove(lookAhead->getLineC(), i3C->getLineA(), e_SrcSelector::C, e_SrcSelector::A) &&
           pManualDiffHelpList->isValidMove(lookAhead->getLineC(), i3C->getLineB(), e_SrcSelector::C, e_SrcSelector::B))
        {
            // Empty space for C. C matches neither A nor B. Move C up.
            i3C->setLineC(lookAhead->getLineC());
            lookAhead->setLineC(LineRef::invalid);

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

        if(line > lineA && line > lineB && lookAhead->getLineA().isValid() && lookAhead->isEqualAB() && !lookAhead->isEqualAC())
        {
            // Empty space for A and B. A matches B, but not C. Move A & B up.
            Diff3LineList::iterator i = lineA > lineB ? i3A : i3B;
            qint32 l = lineA > lineB ? lineA : lineB;

            if(pManualDiffHelpList->isValidMove(i->getLineC(), lookAhead->getLineA(), e_SrcSelector::C, e_SrcSelector::A) &&
               pManualDiffHelpList->isValidMove(i->getLineC(), lookAhead->getLineB(), e_SrcSelector::C, e_SrcSelector::B))
            {
                i->setLineA(lookAhead->getLineA());
                i->setLineB(lookAhead->getLineB());
                i->bAEqB = true;

                if(i->getLineC().isValid() && LineData::equal((*pldA)[i->getLineA()], (*pldC)[i->getLineC()]))
                {
                    i->bAEqC = true;
                    i->bBEqC = true;
                }

                lookAhead->setLineA(LineRef::invalid);
                lookAhead->setLineB(LineRef::invalid);
                lookAhead->bAEqB = false;
                i3A = i;
                i3B = i;
                ++i3A;
                ++i3B;
                lineA = l + 1;
                lineB = l + 1;
            }
        }
        else if(line > lineA && line > lineC && lookAhead->getLineA().isValid() && lookAhead->isEqualAC() && !lookAhead->isEqualAB())
        {
            // Empty space for A and C. A matches C, but not B. Move A & C up.
            Diff3LineList::iterator i = lineA > lineC ? i3A : i3C;
            qint32 l = lineA > lineC ? lineA : lineC;

            if(pManualDiffHelpList->isValidMove(i->getLineB(), lookAhead->getLineA(), e_SrcSelector::B, e_SrcSelector::A) &&
               pManualDiffHelpList->isValidMove(i->getLineB(), lookAhead->getLineC(), e_SrcSelector::B, e_SrcSelector::C))
            {
                i->setLineA(lookAhead->getLineA());
                i->setLineC(lookAhead->getLineC());
                i->bAEqC = true;

                if(i->getLineB().isValid() && LineData::equal((*pldA)[i->getLineA()], (*pldB)[i->getLineB()]))
                {
                    i->bAEqB = true;
                    i->bBEqC = true;
                }

                lookAhead->setLineA(LineRef::invalid);
                lookAhead->setLineC(LineRef::invalid);
                lookAhead->bAEqC = false;
                i3A = i;
                i3C = i;
                ++i3A;
                ++i3C;
                lineA = l + 1;
                lineC = l + 1;
            }
        }
        else if(line > lineB && line > lineC && lookAhead->getLineB().isValid() && lookAhead->isEqualBC() && !lookAhead->isEqualAC())
        {
            // Empty space for B and C. B matches C, but not A. Move B & C up.
            Diff3LineList::iterator i = lineB > lineC ? i3B : i3C;
            qint32 l = lineB > lineC ? lineB : lineC;
            if(pManualDiffHelpList->isValidMove(i->getLineA(), lookAhead->getLineB(), e_SrcSelector::A, e_SrcSelector::B) &&
               pManualDiffHelpList->isValidMove(i->getLineA(), lookAhead->getLineC(), e_SrcSelector::A, e_SrcSelector::C))
            {
                i->setLineB(lookAhead->getLineB());
                i->setLineC(lookAhead->getLineC());
                i->bBEqC = true;

                if(i->getLineA().isValid() && LineData::equal((*pldA)[i->getLineA()], (*pldB)[i->getLineB()]))
                {
                    i->bAEqB = true;
                    i->bAEqC = true;
                }

                lookAhead->setLineB(LineRef::invalid);
                lookAhead->setLineC(LineRef::invalid);
                lookAhead->bBEqC = false;
                i3B = i;
                i3C = i;
                ++i3B;
                ++i3C;
                lineB = l + 1;
                lineC = l + 1;
            }
        }

        if(lookAhead->getLineA().isValid())
        {
            lineA = line + 1;
            i3A = lookAhead;
            ++i3A;
        }
        if(lookAhead->getLineB().isValid())
        {
            lineB = line + 1;
            i3B = lookAhead;
            ++i3B;
        }
        if(lookAhead->getLineC().isValid())
        {
            lineC = line + 1;
            i3C = lookAhead;
            ++i3C;
        }
    }

    remove(d3l_empty);

    /*

   Diff3LineList::iterator it = d3ll.begin();
   qint32 li=0;
   for( ; it!=d3ll.end(); ++it, ++li )
   {
      printf( "%4d %4d %4d %4d  A%c=B A%c=C B%c=C\n",
         li, it->getLineA(), it->getLineB(), it->getLineC(),
         it->isEqualAB() ? '=' : '!', it->isEqualAC() ? '=' : '!', it->isEqualBC() ? '=' : '!' );

   }
*/
}

void DiffBufferInfo::init(Diff3LineList* pD3ll,
                          const std::shared_ptr<LineDataVector> &pldA, const std::shared_ptr<LineDataVector> &pldB, const std::shared_ptr<LineDataVector> &pldC)
{
    m_pDiff3LineList = pD3ll;
    mLineDataA = pldA;
    mLineDataB = pldB;
    mLineDataC = pldC;
}

void Diff3LineList::calcWhiteDiff3Lines(
    const std::shared_ptr<LineDataVector> &pldA, const std::shared_ptr<LineDataVector> &pldB, const std::shared_ptr<LineDataVector> &pldC, const bool bIgnoreComments)
{
    for(Diff3Line& diff3Line: *this)
    {
        diff3Line.bWhiteLineA = (!diff3Line.getLineA().isValid() || (*pldA)[diff3Line.getLineA()].whiteLine() || (bIgnoreComments && (*pldA)[diff3Line.getLineA()].isPureComment()));
        diff3Line.bWhiteLineB = (!diff3Line.getLineB().isValid() || (*pldB)[diff3Line.getLineB()].whiteLine() || (bIgnoreComments && (*pldB)[diff3Line.getLineB()].isPureComment()));
        diff3Line.bWhiteLineC = (!diff3Line.getLineC().isValid() || (*pldC)[diff3Line.getLineC()].whiteLine() || (bIgnoreComments && (*pldC)[diff3Line.getLineC()].isPureComment()));
    }
}

// My own diff-invention:
/*
    Builds DiffList for scratch. Automaticly clears all previous data in list.
*/
void DiffList::calcDiff(const QString& line1, const QString& line2, const qint32 maxSearchRange)
{
    clear();

    const QChar* p1end = line1.constData() + line1.size();
    const QChar* p2end = line2.constData() + line2.size();

    QString::const_iterator p1 = line1.cbegin(), p2 = line2.cbegin();

    /*
        This loop should never reach the exit condition specified here. However it must have a hard wired
        stopping point to prevent runaway allocation if something unexpected happens.
        diffList is therefor hard capped at aprox 50 MB in size.
     */
    for(; size() * sizeof(Diff) + sizeof(DiffList) < (50 << 20);)
    {
        qint32 nofEquals = 0;
        while(p1 != line1.cend() && p2 != line2.cend() && *p1 == *p2)
        {
            ++p1;
            ++p2;
            ++nofEquals;
        }

        bool bBestValid = false;
        qint32 bestI1 = 0;
        qint32 bestI2 = 0;
        qint32 i1 = 0;
        qint32 i2 = 0;

        for(i1 = 0;; ++i1)
        {
            if(&p1[i1] == p1end || (bBestValid && i1 >= bestI1 + bestI2))
            {
                break;
            }
            for(i2 = 0; i2 < maxSearchRange; ++i2)
            {
                if (&p2[i2] == p2end || (bBestValid && i1 + i2 >= bestI1 + bestI2))
                {
                    break;
                }
                else if(p2[i2] == p1[i1] &&
                        (abs(i1 - i2) < 3 || (&p2[i2 + 1] == p2end && &p1[i1 + 1] == p1end) ||
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
            //Bail this should never happen. Not a nice exit but acts as a back stop against harder to detect infine looping.
            if(i1 == TYPE_MAX(qint32))
            {
                assert(false);
                abort();
                return;
            }
        }

        bool bEndReached = false;
        if(bBestValid)
        {
            // The match was found using the strict search. Go back if there are non-strict
            // matches.
            while(bestI1 >= 1 && bestI2 >= 1 && p1[bestI1 - 1] == p2[bestI2 - 1])
            {
                --bestI1;
                --bestI2;
            }
            // continue somehow
            Diff d(nofEquals, bestI1, bestI2);
            assert(nofEquals + bestI1 + bestI2 != 0);
            push_back(d);

            p1 += bestI1;
            p2 += bestI2;
        }
        else
        {
            // NOTE: Very consitantly entered with p1end == p1 and p2end == p2
            // Nothing else to match.
            Diff d(nofEquals, p1end - p1, p2end - p2);
            push_back(d);

            bEndReached = true; //break;
        }

        // Sometimes the algorithm that chooses the first match unfortunately chooses
        // a match where later actually equal parts don't match anymore.
        // A different match could be achieved, if we start at the end.
        // Do it, if it would be a better match.
        qint32 nofUnmatched = 0;
        QString::const_iterator pu1 = p1 - 1;
        QString::const_iterator pu2 = p2 - 1;

        while(pu1 >= line1.begin() && pu2 >= line2.begin() && *pu1 == *pu2)
        {
            ++nofUnmatched;
            --pu1;
            --pu2;
        }

        if(nofUnmatched > 0)
        {
            // We want to go backwards the nofUnmatched elements and redo
            // the matching
            Diff d = back();
            const Diff origBack = d;
            pop_back();

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
                    if(empty())
                        break;
                    d.adjustNumberOfEquals(back().numberOfEquals());
                    d.adjustDiff1(back().diff1());
                    d.adjustDiff2(back().diff2());
                    pop_back();
                    bEndReached = false;
                }
            }

            if(bEndReached)
                push_back(origBack);
            else
            {

                p1 = pu1 + 1 + nofUnmatched;
                p2 = pu2 + 1 + nofUnmatched;
                push_back(d);
            }
        }
        if(bEndReached)
            break;
    }

    assert(size() * sizeof(Diff) + sizeof(DiffList) <= (50 << 20));

#ifndef NDEBUG
    // Verify difflist
    {

        qint32 l1 = 0;
        qint32 l2 = 0;

        for(const Diff& theDiff: *this)
        {
            l1 += (theDiff.numberOfEquals() + theDiff.diff1());
            l2 += (theDiff.numberOfEquals() + theDiff.diff2());
        }

        assert(l1 == line1.size() && l2 == line2.size());
    }
#endif // !NDEBUG
}

bool Diff3Line::fineDiff(bool inBTextsTotalEqual, const e_SrcSelector selector, const std::shared_ptr<LineDataVector> &v1, const std::shared_ptr<LineDataVector> &v2, const IgnoreFlags eIgnoreFlags)
{
    LineRef k1 = 0;
    LineRef k2 = 0;
    qint32 maxSearchLength = 500;
    bool bTextsTotalEqual = inBTextsTotalEqual;
    bool bIgnoreComments = eIgnoreFlags & IgnoreFlag::ignoreComments;
    bool bIgnoreWhiteSpace = eIgnoreFlags & IgnoreFlag::ignoreWhiteSpace;

    assert(selector == e_SrcSelector::A || selector == e_SrcSelector::B || selector == e_SrcSelector::C);

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
            auto pDiffList = std::make_shared<DiffList>();
            pDiffList->calcDiff((*v1)[k1].getLine(), (*v2)[k2].getLine(), maxSearchLength);

            // Optimize the diff list.
            DiffList::iterator dli;
            bool bUsefulFineDiff = false;
            for(const Diff& diff: *pDiffList)
            {
                if(diff.numberOfEquals() >= 4)
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
                            std::shared_ptr<DiffList>& pFineDiff1, std::shared_ptr<DiffList>& pFineDiff2, // return values
                            ChangeFlags& changed, ChangeFlags& changed2) const
{
    changed = NoChange;
    changed2 = NoChange;
    bool bAEqualB = this->isEqualAB() || (bWhiteLineA && bWhiteLineB);
    bool bAEqualC = this->isEqualAC() || (bWhiteLineA && bWhiteLineC);
    bool bBEqualC = this->isEqualBC() || (bWhiteLineB && bWhiteLineC);

    assert(winIdx >= e_SrcSelector::A && winIdx <= e_SrcSelector::C);
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

bool Diff3LineList::fineDiff(const e_SrcSelector selector, const std::shared_ptr<LineDataVector> &v1, const std::shared_ptr<LineDataVector> &v2, const IgnoreFlags eIgnoreFlags)
{
    // Finetuning: Diff each line with deltas
    ProgressScope pp;
    bool bTextsTotalEqual = true;
    size_t listSize = size();
    ProgressProxy::setMaxNofSteps(listSize);

    for(Diff3Line &diff: *this)
    {
        bTextsTotalEqual = diff.fineDiff(bTextsTotalEqual, selector, v1, v2, eIgnoreFlags);
        ProgressProxy::step();
    }
    return bTextsTotalEqual;
}

// Convert the list to a vector of pointers
void Diff3LineList::calcDiff3LineVector(Diff3LineVector& d3lv)
{
    d3lv.resize(SafeInt<QtSizeType>(size()));
    Diff3LineList::iterator i;
    QtSizeType j = 0;
    for(i = begin(); i != end(); ++i, ++j)
    {
        d3lv[j] = &(*i);
    }
    assert(j == d3lv.size());
}

// Just make sure that all input lines are in the output too, exactly once.
void Diff3LineList::debugLineCheck(const LineCount size, const e_SrcSelector srcSelector) const
{
    //FIXME:Does not work m_pOptions->m_bDiff3AlignBC is set.
    LineCount i = 0;

    for(const Diff3Line &entry: *this)
    {
        LineRef line;

        assert(srcSelector == e_SrcSelector::A || srcSelector == e_SrcSelector::B || srcSelector == e_SrcSelector::C);
        if(srcSelector == e_SrcSelector::A)
            line = entry.getLineA();
        else if(srcSelector == e_SrcSelector::B)
            line = entry.getLineB();
        else if(srcSelector == e_SrcSelector::C)
            line = entry.getLineC();

        if(line.isValid())
        {
            if(line != i)
            {
                #ifndef AUTOTEST
                KMessageBox::error(nullptr, i18n("Data loss error:\n"
                                                 "If it is reproducible please contact the author.\n"),
                                   i18n("Severe Internal Error"));
                #endif

                qCCritical(kdiffMain) << "Severe Internal Error." << " line != i for srcSelector=" << (qint32)srcSelector << "\n";
                ::exit(-1);
            }
            ++i;
        }
    }

    if(size != i)
    {
        #ifndef AUTOTEST
        KMessageBox::error(nullptr, i18n(
                                  "Data loss error:\n"
                                  "If it is reproducible please contact the author.\n"),
                           i18n("Severe Internal Error"));
        #endif

        qCCritical(kdiffMain) << "Severe Internal Error.: " << size << " != " << i << "\n";
        ::exit(-1);
    }
}

void Diff3LineList::findHistoryRange(const QRegularExpression& historyStart, bool bThreeFiles, HistoryRange& range) const
{
    QString historyLead;
    // Search for start of history
    for(range.start = begin(), range.startIdx = 0; range.start != end(); ++range.start, ++range.startIdx)
    {
        if(historyStart.match(range.start->getString(e_SrcSelector::A)).hasMatch() &&
           historyStart.match(range.start->getString(e_SrcSelector::B)).hasMatch() &&
           (!bThreeFiles || historyStart.match(range.start->getString(e_SrcSelector::C)).hasMatch()))
        {
            historyLead = Utils::calcHistoryLead(range.start->getString(e_SrcSelector::A));
            break;
        }
    }
    // Search for end of history
    for(range.end = range.start, range.endIdx = range.startIdx; range.end != end(); ++range.end, ++range.endIdx)
    {
        QString sA = range.end->getString(e_SrcSelector::A);
        QString sB = range.end->getString(e_SrcSelector::B);
        QString sC = range.end->getString(e_SrcSelector::C);
        if(!((sA.isEmpty() || historyLead == Utils::calcHistoryLead(sA)) &&
             (sB.isEmpty() || historyLead == Utils::calcHistoryLead(sB)) &&
             (!bThreeFiles || sC.isEmpty() || historyLead == Utils::calcHistoryLead(sC))))
        {
            break; // End of the history
        }
    }
}

void Diff3LineList::dump()
{
    QTextStream out(stdout);
    quint32 i = 1;
    out << "---begin----\n";
    for(const Diff3Line &diffRec : *this)
    {
        out << "line = " << i<< "\n";
        out << "lineA = " << diffRec.getLineA()<< "\n";
        out << "lineB = " << diffRec.getLineB()<< "\n";
        out << "lineC = " << diffRec.getLineC()<< "\n";

        out << "isEqualAB = " << diffRec.isEqualAB()<< "\n";
        out << "isEqualAC = " << diffRec.isEqualAC()<< "\n";
        out << "isEqualBC = " << diffRec.isEqualBC()<< "\n";
        ++i;
    }

    out << "---end----\n";
}
