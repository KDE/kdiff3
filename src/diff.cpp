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

#include "diff.h"

#include "Utils.h"
#include "fileaccess.h"
#include "gnudiff_diff.h"
#include "options.h"
#include "progress.h"

#include <cstdlib>
#include <ctype.h>
#include <map>

#include <qglobal.h>
#include <KLocalizedString>
#include <KMessageBox>

int LineData::width(int tabSize) const
{
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

// The bStrict flag is true during the test where a nonmatching area ends.
// Then the equal()-function requires that the match has more than 2 nonwhite characters.
// This is to avoid matches on trivial lines (e.g. with white space only).
// This choice is good for C/C++.
bool equal(const LineData& l1, const LineData& l2, bool bStrict)
{
    if(l1.getLine() == nullptr || l2.getLine() == nullptr) return false;

    if(bStrict && g_bIgnoreTrivialMatches)
        return false;

    // Ignore white space diff
    const QChar* p1 = l1.getLine();
    const QChar* p1End = p1 + l1.size();

    const QChar* p2 = l2.getLine();
    const QChar* p2End = p2 + l2.size();

    if(g_bIgnoreWhiteSpace)
    {
        int nonWhite = 0;
        for(;;)
        {
            while(isWhite(*p1) && p1 != p1End) ++p1;
            while(isWhite(*p2) && p2 != p2End) ++p2;

            if(p1 == p1End && p2 == p2End)
            {
                if(bStrict && g_bIgnoreTrivialMatches)
                { // Then equality is not enough
                    return nonWhite > 2;
                }
                else // equality is enough
                    return true;
            }
            else if(p1 == p1End || p2 == p2End)
                return false;

            if(*p1 != *p2)
                return false;
            ++p1;
            ++p2;
            ++nonWhite;
        }
    }
    else
    {
        return (l1.size() == l2.size() && memcmp(p1, p2, l1.size()) == 0);
    }
}

// First step
void calcDiff3LineListUsingAB(
    const DiffList* pDiffListAB,
    Diff3LineList& d3ll)
{
    // First make d3ll for AB (from pDiffListAB)

    DiffList::const_iterator i = pDiffListAB->begin();
    int lineA = 0;
    int lineB = 0;
    Diff d(0, 0, 0);

    for(;;)
    {
        if(d.nofEquals == 0 && d.diff1 == 0 && d.diff2 == 0)
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
        if(d.nofEquals > 0)
        {
            d3l.bAEqB = true;
            d3l.setLineA(lineA);
            d3l.setLineB(lineB);
            --d.nofEquals;
            ++lineA;
            ++lineB;
        }
        else if(d.diff1 > 0 && d.diff2 > 0)
        {
            d3l.setLineA(lineA);
            d3l.setLineB(lineB);
            --d.diff1;
            --d.diff2;
            ++lineA;
            ++lineB;
        }
        else if(d.diff1 > 0)
        {
            d3l.setLineA(lineA);
            --d.diff1;
            ++lineA;
        }
        else if(d.diff2 > 0)
        {
            d3l.setLineB(lineB);
            --d.diff2;
            ++lineB;
        }

        Q_ASSERT(d.nofEquals >= 0);

        d3ll.push_back(d3l);
    }
}

// Second step
void calcDiff3LineListUsingAC(
    const DiffList* pDiffListAC,
    Diff3LineList& d3ll)
{
    ////////////////
    // Now insert data from C using pDiffListAC

    DiffList::const_iterator i = pDiffListAC->begin();
    Diff3LineList::iterator i3 = d3ll.begin();
    int lineA = 0;
    int lineC = 0;
    Diff d(0, 0, 0);

    for(;;)
    {
        if(d.nofEquals == 0 && d.diff1 == 0 && d.diff2 == 0)
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
        if(d.nofEquals > 0)
        {
            // Find the corresponding lineA
            while((*i3).getLineA() != lineA)
                ++i3;

            (*i3).setLineC(lineC);
            (*i3).bAEqC = true;
            (*i3).bBEqC = (*i3).bAEqB;

            --d.nofEquals;
            ++lineA;
            ++lineC;
            ++i3;
        }
        else if(d.diff1 > 0 && d.diff2 > 0)
        {
            d3l.setLineC(lineC);
            d3ll.insert(i3, d3l);
            --d.diff1;
            --d.diff2;
            ++lineA;
            ++lineC;
        }
        else if(d.diff1 > 0)
        {
            --d.diff1;
            ++lineA;
        }
        else if(d.diff2 > 0)
        {
            d3l.setLineC(lineC);
            d3ll.insert(i3, d3l);
            --d.diff2;
            ++lineC;
        }
    }
}

// Third step
void calcDiff3LineListUsingBC(
    const DiffList* pDiffListBC,
    Diff3LineList& d3ll)
{
    ////////////////
    // Now improve the position of data from C using pDiffListBC
    // If a line from C equals a line from A then it is in the
    // same Diff3Line already.
    // If a line from C equals a line from B but not A, this
    // information will be used here.

    DiffList::const_iterator i = pDiffListBC->begin();
    Diff3LineList::iterator i3b = d3ll.begin();
    Diff3LineList::iterator i3c = d3ll.begin();
    int lineB = 0;
    int lineC = 0;
    Diff d(0, 0, 0);

    for(;;)
    {
        if(d.nofEquals == 0 && d.diff1 == 0 && d.diff2 == 0)
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
        if(d.nofEquals > 0)
        {
            // Find the corresponding lineB and lineC
            while(i3b != d3ll.end() && (*i3b).getLineB() != lineB)
                ++i3b;

            while(i3c != d3ll.end() && (*i3c).getLineC() != lineC)
                ++i3c;

            Q_ASSERT(i3b != d3ll.end());
            Q_ASSERT(i3c != d3ll.end());

            if(i3b == i3c)
            {
                Q_ASSERT((*i3b).getLineC() == lineC);
                (*i3b).bBEqC = true;
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
                    Q_ASSERT(i3b1 != d3ll.end() || i3c1 != d3ll.end());
                    if(i3c1 != d3ll.end()) ++i3c1;
                    if(i3b1 != d3ll.end()) ++i3b1;
                }

                if(i3c1 == i3b && !(*i3b).bAEqB) // i3c before i3b
                {
                    Diff3LineList::iterator i3 = i3c;
                    int nofDisturbingLines = 0;
                    while(i3 != i3b && i3 != d3ll.end())
                    {
                        if((*i3).getLineB() != -1)
                            ++nofDisturbingLines;
                        ++i3;
                    }

                    if(nofDisturbingLines > 0) //&& nofDisturbingLines < d.nofEquals*d.nofEquals+4 )
                    {
                        Diff3LineList::iterator i3_last_equal_A = d3ll.end();

                        i3 = i3c;
                        while(i3 != i3b)
                        {
                            if(i3->bAEqB)
                            {
                                i3_last_equal_A = i3;
                            }
                            ++i3;
                        }

                        /* If i3_last_equal_A isn't still set to d3ll.end(), then
                   * we've found a line in A that is equal to one in B
                   * somewhere between i3c and i3b
                   */
                        bool before_or_on_equal_line_in_A = (i3_last_equal_A != d3ll.end());

                        // Move the disturbing lines up, out of sight.
                        i3 = i3c;
                        while(i3 != i3b)
                        {
                            if((*i3).getLineB() != -1 ||
                               (before_or_on_equal_line_in_A && i3->getLineA() != -1))
                            {
                                d3l.setLineB((*i3).getLineB());
                                (*i3).setLineB(-1);

                                // Move A along if it matched B
                                if(before_or_on_equal_line_in_A)
                                {
                                    d3l.setLineA(i3->getLineA());
                                    d3l.bAEqB = i3->bAEqB;
                                    i3->setLineA(-1);
                                    i3->bAEqC = false;
                                }

                                (*i3).bAEqB = false;
                                (*i3).bBEqC = false;
                                d3ll.insert(i3c, d3l);
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
                        (*i3b).setLineB(-1); // This might leave an empty line: removed later.
                        (*i3b).bAEqB = false;
                        (*i3b).bBEqC = false;
                        (*i3c).setLineB(lineB);
                        (*i3c).bBEqC = true;
                        (*i3c).bAEqB = (*i3c).bAEqC;
                    }
                }
                else if(i3b1 == i3c && !(*i3c).bAEqC)
                {
                    Diff3LineList::iterator i3 = i3b;
                    int nofDisturbingLines = 0;
                    while(i3 != i3c && i3 != d3ll.end())
                    {
                        if((*i3).getLineC() != -1)
                            ++nofDisturbingLines;
                        ++i3;
                    }

                    if(nofDisturbingLines > 0) //&& nofDisturbingLines < d.nofEquals*d.nofEquals+4 )
                    {
                        Diff3LineList::iterator i3_last_equal_A = d3ll.end();

                        i3 = i3b;
                        while(i3 != i3c)
                        {
                            if(i3->bAEqC)
                            {
                                i3_last_equal_A = i3;
                            }
                            ++i3;
                        }

                        /* If i3_last_equal_A isn't still set to d3ll.end(), then
                   * we've found a line in A that is equal to one in C
                   * somewhere between i3b and i3c
                   */
                        bool before_or_on_equal_line_in_A = (i3_last_equal_A != d3ll.end());

                        // Move the disturbing lines up.
                        i3 = i3b;
                        while(i3 != i3c)
                        {
                            if((*i3).getLineC() != -1 ||
                               (before_or_on_equal_line_in_A && i3->getLineA() != -1))
                            {
                                d3l.setLineC((*i3).getLineC());
                                (*i3).setLineC(-1);

                                // Move A along if it matched C
                                if(before_or_on_equal_line_in_A)
                                {
                                    d3l.setLineA(i3->getLineA());
                                    d3l.bAEqC = i3->bAEqC;
                                    i3->setLineA(-1);
                                    i3->bAEqB = false;
                                }

                                (*i3).bAEqC = false;
                                (*i3).bBEqC = false;
                                d3ll.insert(i3b, d3l);
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
                        (*i3c).setLineC(-1); // This might leave an empty line: removed later.
                        (*i3c).bAEqC = false;
                        (*i3c).bBEqC = false;
                        (*i3b).setLineC(lineC);
                        (*i3b).bBEqC = true;
                        (*i3b).bAEqC = (*i3b).bAEqB;
                    }
                }
            }

            --d.nofEquals;
            ++lineB;
            ++lineC;
            ++i3b;
            ++i3c;
        }
        else if(d.diff1 > 0)
        {
            Diff3LineList::iterator i3 = i3b;
            while((*i3).getLineB() != lineB)
                ++i3;
            if(i3 != i3b && !(*i3).bAEqB)
            {
                // Take B from this line and move it up as far as possible
                d3l.setLineB(lineB);
                d3ll.insert(i3b, d3l);
                (*i3).setLineB(-1);
            }
            else
            {
                i3b = i3;
            }
            --d.diff1;
            ++lineB;
            ++i3b;

            if(d.diff2 > 0)
            {
                --d.diff2;
                ++lineC;
            }
        }
        else if(d.diff2 > 0)
        {
            --d.diff2;
            ++lineC;
        }
    }
    /*
   Diff3LineList::iterator it = d3ll.begin();
   int li=0;
   for( ; it!=d3ll.end(); ++it, ++li )
   {
      printf( "%4d %4d %4d %4d  A%c=B A%c=C B%c=C\n",
         li, (*it).getLineA(), (*it).getLineB(), (*it).getLineC(),
         (*it).bAEqB ? '=' : '!', (*it).bAEqC ? '=' : '!', (*it).bBEqC ? '=' : '!' );
   }
   printf("\n");*/
}

// Test if the move would pass a barrier. Return true if not.
bool ManualDiffHelpList::isValidMove(int line1, int line2, int winIdx1, int winIdx2) const
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

bool ManualDiffHelpEntry::isValidMove(int line1, int line2, int winIdx1, int winIdx2) const
{
    // Barrier
    int l1 = winIdx1 == 1 ? lineA1 : winIdx1 == 2 ? lineB1 : lineC1;
    int l2 = winIdx2 == 1 ? lineA1 : winIdx2 == 2 ? lineB1 : lineC1;

    if(l1 >= 0 && l2 >= 0)
    {
        if((line1 >= l1 && line2 < l2) || (line1 < l1 && line2 >= l2))
            return false;
        l1 = winIdx1 == 1 ? lineA2 : winIdx1 == 2 ? lineB2 : lineC2;
        l2 = winIdx2 == 1 ? lineA2 : winIdx2 == 2 ? lineB2 : lineC2;
        ++l1;
        ++l2;
        if((line1 >= l1 && line2 < l2) || (line1 < l1 && line2 >= l2))
            return false;
    }

    return true;
}

static bool runDiff(const LineData* p1, LineRef size1, const LineData* p2, LineRef size2, DiffList& diffList,
                    Options* pOptions)
{
    ProgressProxy pp;
    static GnuDiff gnuDiff; // All values are initialized with zeros.

    pp.setCurrent(0);

    diffList.clear();
    if(p1 == nullptr || p1[0].getLine() == nullptr || p2 == nullptr || p2[0].getLine() == nullptr || size1 == 0 || size2 == 0)
    {
        Diff d(0, 0, 0);
        if(p1 != nullptr && p2 != nullptr && p1[0].getLine() == nullptr && p2[0].getLine() == nullptr && size1 == size2)
            d.nofEquals = size1;
        else
        {
            d.diff1 = size1;
            d.diff2 = size2;
        }

        diffList.push_back(d);
    }
    else
    {
        GnuDiff::comparison comparisonInput;
        memset(&comparisonInput, 0, sizeof(comparisonInput));
        comparisonInput.parent = nullptr;
        comparisonInput.file[0].buffer = p1[0].getLine();                                                //ptr to buffer
        comparisonInput.file[0].buffered = (p1[size1 - 1].getLine() - p1[0].getLine() + p1[size1 - 1].size()); // size of buffer
        comparisonInput.file[1].buffer = p2[0].getLine();                                                //ptr to buffer
        comparisonInput.file[1].buffered = (p2[size2 - 1].getLine() - p2[0].getLine() + p2[size2 - 1].size()); // size of buffer

        gnuDiff.ignore_white_space = GnuDiff::IGNORE_ALL_SPACE; // I think nobody needs anything else ...
        gnuDiff.bIgnoreWhiteSpace = true;
        gnuDiff.bIgnoreNumbers = pOptions->m_bIgnoreNumbers;
        gnuDiff.minimal = pOptions->m_bTryHard;
        gnuDiff.ignore_case = false;
        GnuDiff::change* script = gnuDiff.diff_2_files(&comparisonInput);

        LineRef equalLinesAtStart = comparisonInput.file[0].prefix_lines;
        LineRef currentLine1 = 0;
        LineRef currentLine2 = 0;
        GnuDiff::change* p = nullptr;
        for(GnuDiff::change* e = script; e; e = p)
        {
            Diff d(0, 0, 0);
            d.nofEquals = e->line0 - currentLine1;
            Q_ASSERT(d.nofEquals == e->line1 - currentLine2);
            d.diff1 = e->deleted;
            d.diff2 = e->inserted;
            currentLine1 += d.nofEquals + d.diff1;
            currentLine2 += d.nofEquals + d.diff2;
            diffList.push_back(d);

            p = e->link;
            free(e);
        }

        if(diffList.empty())
        {
            Diff d(0, 0, 0);
            d.nofEquals = std::min(size1, size2);
            d.diff1 = size1 - d.nofEquals;
            d.diff2 = size2 - d.nofEquals;
            diffList.push_back(d);
        }
        else
        {
            diffList.front().nofEquals += equalLinesAtStart;
            currentLine1 += equalLinesAtStart;
            currentLine2 += equalLinesAtStart;

            LineRef nofEquals = std::min(size1 - currentLine1, size2 - currentLine2);
            if(nofEquals == 0)
            {
                diffList.back().diff1 += size1 - currentLine1;
                diffList.back().diff2 += size2 - currentLine2;
            }
            else
            {
                Diff d(nofEquals, size1 - currentLine1 - nofEquals, size2 - currentLine2 - nofEquals);
                diffList.push_back(d);
            }
        }
    }

    // Verify difflist
    {
        LineRef l1 = 0;
        LineRef l2 = 0;
        DiffList::iterator i;
        for(i = diffList.begin(); i != diffList.end(); ++i)
        {
            l1 += i->nofEquals + i->diff1;
            l2 += i->nofEquals + i->diff2;
        }

        Q_ASSERT(l1 == size1 && l2 == size2);
    }

    pp.setCurrent(1);

    return true;
}

bool ManualDiffHelpList::runDiff(const LineData* p1, LineRef size1, const LineData* p2, LineRef size2, DiffList& diffList,
             int winIdx1, int winIdx2,
             Options* pOptions)
{
    diffList.clear();
    DiffList diffList2;

    int l1begin = 0;
    int l2begin = 0;
    ManualDiffHelpList::const_iterator i;
    for(i = begin(); i != end(); ++i)
    {
        const ManualDiffHelpEntry& mdhe = *i;

        int l1end = mdhe.getLine1(winIdx1);
        int l2end = mdhe.getLine1(winIdx2);

        if(l1end >= 0 && l2end >= 0)
        {
            ::runDiff(p1 + l1begin, l1end - l1begin, p2 + l2begin, l2end - l2begin, diffList2, pOptions);
            diffList.splice(diffList.end(), diffList2);
            l1begin = l1end;
            l2begin = l2end;

            l1end = mdhe.getLine2(winIdx1);
            l2end = mdhe.getLine2(winIdx2);

            if(l1end >= 0 && l2end >= 0)
            {
                ++l1end; // point to line after last selected line
                ++l2end;
                ::runDiff(p1 + l1begin, l1end - l1begin, p2 + l2begin, l2end - l2begin, diffList2, pOptions);
                diffList.splice(diffList.end(), diffList2);
                l1begin = l1end;
                l2begin = l2end;
            }
        }
    }
    ::runDiff(p1 + l1begin, size1 - l1begin, p2 + l2begin, size2 - l2begin, diffList2, pOptions);
    diffList.splice(diffList.end(), diffList2);
    return true;
}

void correctManualDiffAlignment(Diff3LineList& d3ll, ManualDiffHelpList* pManualDiffHelpList)
{
    if(pManualDiffHelpList->empty())
        return;

    // If a line appears unaligned in comparison to the manual alignment, correct this.

    ManualDiffHelpList::iterator iMDHL;
    for(iMDHL = pManualDiffHelpList->begin(); iMDHL != pManualDiffHelpList->end(); ++iMDHL)
    {
        Diff3LineList::iterator i3 = d3ll.begin();
        int missingWinIdx = 0;
        int alignedSum = (iMDHL->getLine1(1) < 0 ? 0 : 1) + (iMDHL->getLine1(2) < 0 ? 0 : 1) + (iMDHL->getLine1(3) < 0 ? 0 : 1);
        if(alignedSum == 2)
        {
            // If only A & B are aligned then let C rather be aligned with A
            // If only A & C are aligned then let B rather be aligned with A
            // If only B & C are aligned then let A rather be aligned with B
            missingWinIdx = iMDHL->getLine1(1) < 0 ? 1 : (iMDHL->getLine1(2) < 0 ? 2 : 3);
        }
        else if(alignedSum <= 1)
        {
            return;
        }

        // At the first aligned line, move up the two other lines into new d3ls until the second input is aligned
        // Then move up the third input until all three lines are aligned.
        int wi = 0;
        for(; i3 != d3ll.end(); ++i3)
        {
            for(wi = 1; wi <= 3; ++wi)
            {
                if(i3->getLineInFile(wi) >= 0 && iMDHL->firstLine(wi) == i3->getLineInFile(wi))
                    break;
            }
            if(wi <= 3)
                break;
        }

        if(wi >= 1 && wi <= 3)
        {
            // Found manual alignment for one source
            Diff3LineList::iterator iDest = i3;

            // Move lines up until the next firstLine is found. Omit wi from move and search.
            int wi2 = 0;
            for(; i3 != d3ll.end(); ++i3)
            {
                for(wi2 = 1; wi2 <= 3; ++wi2)
                {
                    if(wi != wi2 && i3->getLineInFile(wi2) >= 0 && iMDHL->firstLine(wi2) == i3->getLineInFile(wi2))
                        break;
                }
                if(wi2 > 3)
                { // Not yet found
                    // Move both others up
                    Diff3Line d3l;
                    // Move both up
                    if(wi == 1) // Move B and C up
                    {
                        d3l.bBEqC = i3->bBEqC;
                        d3l.setLineB(i3->getLineB());
                        d3l.setLineC(i3->getLineC());
                        i3->setLineB(-1);
                        i3->setLineC(-1);
                    }
                    if(wi == 2) // Move A and C up
                    {
                        d3l.bAEqC = i3->bAEqC;
                        d3l.setLineA(i3->getLineA());
                        d3l.setLineC(i3->getLineC());
                        i3->setLineA(-1);
                        i3->setLineC(-1);
                    }
                    if(wi == 3) // Move A and B up
                    {
                        d3l.bAEqB = i3->bAEqB;
                        d3l.setLineA(i3->getLineA());
                        d3l.setLineB(i3->getLineB());
                        i3->setLineA(-1);
                        i3->setLineB(-1);
                    }
                    i3->bAEqB = false;
                    i3->bAEqC = false;
                    i3->bBEqC = false;
                    d3ll.insert(iDest, d3l);
                }
                else
                {
                    // align the found line with the line we already have here
                    if(i3 != iDest)
                    {
                        if(wi2 == 1)
                        {
                            iDest->setLineA(i3->getLineA());
                            i3->setLineA(-1);
                            i3->bAEqB = false;
                            i3->bAEqC = false;
                        }
                        else if(wi2 == 2)
                        {
                            iDest->setLineB(i3->getLineB());
                            i3->setLineB(-1);
                            i3->bAEqB = false;
                            i3->bBEqC = false;
                        }
                        else if(wi2 == 3)
                        {
                            iDest->setLineC(i3->getLineC());
                            i3->setLineC(-1);
                            i3->bBEqC = false;
                            i3->bAEqC = false;
                        }
                    }

                    if(missingWinIdx != 0)
                    {
                        for(; i3 != d3ll.end(); ++i3)
                        {
                            int wi3 = missingWinIdx;
                            if(i3->getLineInFile(wi3) >= 0)
                            {
                                // not found, move the line before iDest
                                Diff3Line d3l;
                                if(wi3 == 1)
                                {
                                    if(i3->bAEqB) // Stop moving lines up if one equal is found.
                                        break;
                                    d3l.setLineA(i3->getLineA());
                                    i3->setLineA(-1);
                                    i3->bAEqB = false;
                                    i3->bAEqC = false;
                                }
                                if(wi3 == 2)
                                {
                                    if(i3->bAEqB)
                                        break;
                                    d3l.setLineB(i3->getLineB());
                                    i3->setLineB(-1);
                                    i3->bAEqB = false;
                                    i3->bBEqC = false;
                                }
                                if(wi3 == 3)
                                {
                                    if(i3->bAEqC)
                                        break;
                                    d3l.setLineC(i3->getLineC());
                                    i3->setLineC(-1);
                                    i3->bAEqC = false;
                                    i3->bBEqC = false;
                                }
                                d3ll.insert(iDest, d3l);
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
void calcDiff3LineListTrim(
    Diff3LineList& d3ll, const LineData* pldA, const LineData* pldB, const LineData* pldC, ManualDiffHelpList* pManualDiffHelpList)
{
    const Diff3Line d3l_empty;
    d3ll.removeAll(d3l_empty);

    Diff3LineList::iterator i3 = d3ll.begin();
    Diff3LineList::iterator i3A = d3ll.begin();
    Diff3LineList::iterator i3B = d3ll.begin();
    Diff3LineList::iterator i3C = d3ll.begin();

    int line = 0;  // diff3line counters
    int lineA = 0; //
    int lineB = 0;
    int lineC = 0;

    ManualDiffHelpList::iterator iMDHL = pManualDiffHelpList->begin();
    // The iterator i3 and the variable line look ahead.
    // The iterators i3A, i3B, i3C and corresponding lineA, lineB and lineC stop at empty lines, if found.
    // If possible, then the texts from the look ahead will be moved back to the empty places.

    for(; i3 != d3ll.end(); ++i3, ++line)
    {
        if(iMDHL != pManualDiffHelpList->end())
        {
            if((i3->getLineA() >= 0 && i3->getLineA() == iMDHL->getLine1(1)) ||
               (i3->getLineB() >= 0 && i3->getLineB() == iMDHL->getLine1(2)) ||
               (i3->getLineC() >= 0 && i3->getLineC() == iMDHL->getLine1(3)))
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

        if(line > lineA && (*i3).getLineA() != -1 && (*i3A).getLineB() != -1 && (*i3A).bBEqC &&
           ::equal(pldA[(*i3).getLineA()], pldB[(*i3A).getLineB()], false) &&
           pManualDiffHelpList->isValidMove((*i3).getLineA(), (*i3A).getLineB(), 1, 2) &&
           pManualDiffHelpList->isValidMove((*i3).getLineA(), (*i3A).getLineC(), 1, 3))
        {
            // Empty space for A. A matches B and C in the empty line. Move it up.
            (*i3A).setLineA((*i3).getLineA());
            (*i3A).bAEqB = true;
            (*i3A).bAEqC = true;

            (*i3).setLineA(-1);
            (*i3).bAEqB = false;
            (*i3).bAEqC = false;
            ++i3A;
            ++lineA;
        }

        if(line > lineB && (*i3).getLineB() != -1 && (*i3B).getLineA() != -1 && (*i3B).bAEqC &&
           ::equal(pldB[(*i3).getLineB()], pldA[(*i3B).getLineA()], false) &&
           pManualDiffHelpList->isValidMove((*i3).getLineB(), (*i3B).getLineA(), 2, 1) &&
           pManualDiffHelpList->isValidMove((*i3).getLineB(), (*i3B).getLineC(), 2, 3))
        {
            // Empty space for B. B matches A and C in the empty line. Move it up.
            (*i3B).setLineB((*i3).getLineB());
            (*i3B).bAEqB = true;
            (*i3B).bBEqC = true;
            (*i3).setLineB(-1);
            (*i3).bAEqB = false;
            (*i3).bBEqC = false;
            ++i3B;
            ++lineB;
        }

        if(line > lineC && (*i3).getLineC() != -1 && (*i3C).getLineA() != -1 && (*i3C).bAEqB &&
           ::equal(pldC[(*i3).getLineC()], pldA[(*i3C).getLineA()], false) &&
           pManualDiffHelpList->isValidMove((*i3).getLineC(), (*i3C).getLineA(), 3, 1) &&
           pManualDiffHelpList->isValidMove((*i3).getLineC(), (*i3C).getLineB(), 3, 2))
        {
            // Empty space for C. C matches A and B in the empty line. Move it up.
            (*i3C).setLineC((*i3).getLineC());
            (*i3C).bAEqC = true;
            (*i3C).bBEqC = true;
            (*i3).setLineC(-1);
            (*i3).bAEqC = false;
            (*i3).bBEqC = false;
            ++i3C;
            ++lineC;
        }

        if(line > lineA && (*i3).getLineA() != -1 && !(*i3).bAEqB && !(*i3).bAEqC &&
           pManualDiffHelpList->isValidMove((*i3).getLineA(), (*i3A).getLineB(), 1, 2) &&
           pManualDiffHelpList->isValidMove((*i3).getLineA(), (*i3A).getLineC(), 1, 3)) {
            // Empty space for A. A doesn't match B or C. Move it up.
            (*i3A).setLineA((*i3).getLineA());
            (*i3).setLineA(-1);

            if(i3A->getLineB() != -1 && ::equal(pldA[i3A->getLineA()], pldB[i3A->getLineB()], false))
            {
                i3A->bAEqB = true;
            }
            if((i3A->bAEqB && i3A->bBEqC) ||
               (i3A->getLineC() != -1 && ::equal(pldA[i3A->getLineA()], pldC[i3A->getLineC()], false)))
            {
                i3A->bAEqC = true;
            }

            ++i3A;
            ++lineA;
        }

        if(line > lineB && (*i3).getLineB() != -1 && !(*i3).bAEqB && !(*i3).bBEqC &&
           pManualDiffHelpList->isValidMove((*i3).getLineB(), (*i3B).getLineA(), 2, 1) &&
           pManualDiffHelpList->isValidMove((*i3).getLineB(), (*i3B).getLineC(), 2, 3))
        {
            // Empty space for B. B matches neither A nor C. Move B up.
            (*i3B).setLineB((*i3).getLineB());
            (*i3).setLineB(-1);

            if(i3B->getLineA() != -1 && ::equal(pldA[i3B->getLineA()], pldB[i3B->getLineB()], false))
            {
                i3B->bAEqB = true;
            }
            if((i3B->bAEqB && i3B->bAEqC) ||
               (i3B->getLineC() != -1 && ::equal(pldB[i3B->getLineB()], pldC[i3B->getLineC()], false)))
            {
                i3B->bBEqC = true;
            }

            ++i3B;
            ++lineB;
        }

        if(line > lineC && (*i3).getLineC() != -1 && !(*i3).bAEqC && !(*i3).bBEqC &&
           pManualDiffHelpList->isValidMove( (*i3).getLineC(), (*i3C).getLineA(), 3, 1) &&
           pManualDiffHelpList->isValidMove( (*i3).getLineC(), (*i3C).getLineB(), 3, 2))
        {
            // Empty space for C. C matches neither A nor B. Move C up.
            (*i3C).setLineC((*i3).getLineC());
            (*i3).setLineC(-1);

            if(i3C->getLineA() != -1 && ::equal(pldA[i3C->getLineA()], pldC[i3C->getLineC()], false))
            {
                i3C->bAEqC = true;
            }
            if((i3C->bAEqC && i3C->bAEqB) ||
               (i3C->getLineB() != -1 && ::equal(pldB[i3C->getLineB()], pldC[i3C->getLineC()], false)))
            {
                i3C->bBEqC = true;
            }

            ++i3C;
            ++lineC;
        }

        if(line > lineA && line > lineB && (*i3).getLineA() != -1 && (*i3).bAEqB && !(*i3).bAEqC)
        {
            // Empty space for A and B. A matches B, but not C. Move A & B up.
            Diff3LineList::iterator i = lineA > lineB ? i3A : i3B;
            int l = lineA > lineB ? lineA : lineB;

            if(pManualDiffHelpList->isValidMove( i->getLineC(), (*i3).getLineA(), 3, 1) &&
               pManualDiffHelpList->isValidMove( i->getLineC(), (*i3).getLineB(), 3, 2))
            {
                (*i).setLineA((*i3).getLineA());
                (*i).setLineB((*i3).getLineB());
                (*i).bAEqB = true;

                if(i->getLineC() != -1 && ::equal(pldA[i->getLineA()], pldC[i->getLineC()], false))
                {
                    (*i).bAEqC = true;
                    (*i).bBEqC = true;
                }

                (*i3).setLineA(-1);
                (*i3).setLineB(-1);
                (*i3).bAEqB = false;
                i3A = i;
                i3B = i;
                ++i3A;
                ++i3B;
                lineA = l + 1;
                lineB = l + 1;
            }
        }
        else if(line > lineA && line > lineC && (*i3).getLineA() != -1 && (*i3).bAEqC && !(*i3).bAEqB)
        {
            // Empty space for A and C. A matches C, but not B. Move A & C up.
            Diff3LineList::iterator i = lineA > lineC ? i3A : i3C;
            int l = lineA > lineC ? lineA : lineC;

            if(pManualDiffHelpList->isValidMove(i->getLineB(), (*i3).getLineA(), 2, 1) &&
               pManualDiffHelpList->isValidMove(i->getLineB(), (*i3).getLineC(), 2, 3))
            {
                (*i).setLineA((*i3).getLineA());
                (*i).setLineC((*i3).getLineC());
                (*i).bAEqC = true;

                if(i->getLineB() != -1 && ::equal(pldA[i->getLineA()], pldB[i->getLineB()], false))
                {
                    (*i).bAEqB = true;
                    (*i).bBEqC = true;
                }

                (*i3).setLineA(-1);
                (*i3).setLineC(-1);
                (*i3).bAEqC = false;
                i3A = i;
                i3C = i;
                ++i3A;
                ++i3C;
                lineA = l + 1;
                lineC = l + 1;
            }
        }
        else if(line > lineB && line > lineC && (*i3).getLineB() != -1 && (*i3).bBEqC && !(*i3).bAEqC)
        {
            // Empty space for B and C. B matches C, but not A. Move B & C up.
            Diff3LineList::iterator i = lineB > lineC ? i3B : i3C;
            int l = lineB > lineC ? lineB : lineC;

            if(pManualDiffHelpList->isValidMove( i->getLineA(), (*i3).getLineB(), 1, 2) &&
               pManualDiffHelpList->isValidMove( i->getLineA(), (*i3).getLineC(), 1, 3))
            {
                (*i).setLineB((*i3).getLineB());
                (*i).setLineC((*i3).getLineC());
                (*i).bBEqC = true;

                if(i->getLineA() != -1 && ::equal(pldA[i->getLineA()], pldB[i->getLineB()], false))
                {
                    (*i).bAEqB = true;
                    (*i).bAEqC = true;
                }

                (*i3).setLineB(-1);
                (*i3).setLineC(-1);
                (*i3).bBEqC = false;
                i3B = i;
                i3C = i;
                ++i3B;
                ++i3C;
                lineB = l + 1;
                lineC = l + 1;
            }
        }

        if((*i3).getLineA() != -1)
        {
            lineA = line + 1;
            i3A = i3;
            ++i3A;
        }
        if((*i3).getLineB() != -1)
        {
            lineB = line + 1;
            i3B = i3;
            ++i3B;
        }
        if((*i3).getLineC() != -1)
        {
            lineC = line + 1;
            i3C = i3;
            ++i3C;
        }
    }

    d3ll.removeAll(d3l_empty);

    /*

   Diff3LineList::iterator it = d3ll.begin();
   int li=0;
   for( ; it!=d3ll.end(); ++it, ++li )
   {
      printf( "%4d %4d %4d %4d  A%c=B A%c=C B%c=C\n",
         li, (*it).getLineA(), (*it).getLineB(), (*it).getLineC(),
         (*it).bAEqB ? '=' : '!', (*it).bAEqC ? '=' : '!', (*it).bBEqC ? '=' : '!' );

   }
*/
}

void DiffBufferInfo::init(Diff3LineList* pD3ll, const Diff3LineVector* pD3lv,
                          const LineData* pldA, LineRef sizeA, const LineData* pldB, LineRef sizeB, const LineData* pldC, LineRef sizeC)
{
    m_pDiff3LineList = pD3ll;
    m_pDiff3LineVector = pD3lv;
    m_pLineDataA = pldA;
    m_pLineDataB = pldB;
    m_pLineDataC = pldC;
    m_sizeA = sizeA;
    m_sizeB = sizeB;
    m_sizeC = sizeC;
    Diff3LineList::iterator i3 = pD3ll->begin();
    for(; i3 != pD3ll->end(); ++i3)
    {
        i3->m_pDiffBufferInfo = this;
    }
}

void Diff3LineList::calcWhiteDiff3Lines(
    const LineData* pldA, const LineData* pldB, const LineData* pldC)
{
    Diff3LineList::iterator i3;

    for(i3=begin(); i3 != end(); ++i3)
    {
        i3->bWhiteLineA = ((*i3).getLineA() == -1 || pldA == nullptr || pldA[(*i3).getLineA()].whiteLine() || pldA[(*i3).getLineA()].isPureComment());
        i3->bWhiteLineB = ((*i3).getLineB() == -1 || pldB == nullptr || pldB[(*i3).getLineB()].whiteLine() || pldB[(*i3).getLineB()].isPureComment());
        i3->bWhiteLineC = ((*i3).getLineC() == -1 || pldC == nullptr || pldC[(*i3).getLineC()].whiteLine() || pldC[(*i3).getLineC()].isPureComment());
    }
}

inline bool equal(QChar c1, QChar c2, bool /*bStrict*/)
{
    // If bStrict then white space doesn't match

    //if ( bStrict &&  ( c1==' ' || c1=='\t' ) )
    //   return false;

    return c1 == c2;
}

// My own diff-invention:
template <class T>
void calcDiff(const T* p1, LineRef size1, const T* p2, LineRef size2, DiffList& diffList, int match, int maxSearchRange)
{
    diffList.clear();

    const T* p1start = p1;
    const T* p2start = p2;
    const T* p1end = p1 + size1;
    const T* p2end = p2 + size2;
    for(;;)
    {
        int nofEquals = 0;
        while(p1 != p1end && p2 != p2end && equal(*p1, *p2, false))
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
                else if(equal(p2[i2], p1[i1], true) &&
                        (match == 1 || abs(i1 - i2) < 3 || (&p2[i2 + 1] == p2end && &p1[i1 + 1] == p1end) ||
                         (&p2[i2 + 1] != p2end && &p1[i1 + 1] != p1end && equal(p2[i2 + 1], p1[i1 + 1], false))))
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
        while(bestI1 >= 1 && bestI2 >= 1 && equal(p1[bestI1 - 1], p2[bestI2 - 1], false))
        {
            --bestI1;
            --bestI2;
        }

        bool bEndReached = false;
        if(bBestValid)
        {
            // continue somehow
            Diff d(nofEquals, bestI1, bestI2);
            diffList.push_back(d);

            p1 += bestI1;
            p2 += bestI2;
        }
        else
        {
            // Nothing else to match.
            Diff d(nofEquals, p1end - p1, p2end - p2);
            diffList.push_back(d);

            bEndReached = true; //break;
        }

        // Sometimes the algorithm that chooses the first match unfortunately chooses
        // a match where later actually equal parts don't match anymore.
        // A different match could be achieved, if we start at the end.
        // Do it, if it would be a better match.
        int nofUnmatched = 0;
        const T* pu1 = p1 - 1;
        const T* pu2 = p2 - 1;
        while(pu1 >= p1start && pu2 >= p2start && equal(*pu1, *pu2, false))
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
                if(d.diff1 > 0 && d.diff2 > 0)
                {
                    --d.diff1;
                    --d.diff2;
                    --nofUnmatched;
                }
                else if(d.nofEquals > 0)
                {
                    --d.nofEquals;
                    --nofUnmatched;
                }

                if(d.nofEquals == 0 && (d.diff1 == 0 || d.diff2 == 0) && nofUnmatched > 0)
                {
                    if(diffList.empty())
                        break;
                    d.nofEquals += diffList.back().nofEquals;
                    d.diff1 += diffList.back().diff1;
                    d.diff2 += diffList.back().diff2;
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

    // Verify difflist
    {
        LineRef l1 = 0;
        LineRef l2 = 0;
        DiffList::iterator i;
        for(i = diffList.begin(); i != diffList.end(); ++i)
        {
            l1 += i->nofEquals + i->diff1;
            l2 += i->nofEquals + i->diff2;
        }

        Q_ASSERT(l1 == size1 && l2 == size2);
    }
}

bool Diff3Line::fineDiff(const int selector, const LineData* v1, const LineData* v2)
{
    bool bTextsTotalEqual = true;
    LineRef k1 = 0;
    LineRef k2 = 0;
    int maxSearchLength = 500;

    Q_ASSERT(selector == 1 || selector == 2 || selector == 3);

    if(selector == 1)
    {
        k1 = getLineA();
        k2 = getLineB();
    }
    else if(selector == 2)
    {
        k1 = getLineB();
        k2 = getLineC();
    }
    else if(selector == 3)
    {
        k1 = getLineC();
        k2 = getLineA();
    }

    if((k1 == -1 && k2 != -1) || (k1 != -1 && k2 == -1)) bTextsTotalEqual = false;
    if(k1 != -1 && k2 != -1)
    {
        if(v1[k1].size() != v2[k2].size() || memcmp(v1[k1].getLine(), v2[k2].getLine(), v1[k1].size() << 1) != 0)
        {
            bTextsTotalEqual = false;
            DiffList* pDiffList = new DiffList;
            calcDiff(v1[k1].getLine(), v1[k1].size(), v2[k2].getLine(), v2[k2].size(), *pDiffList, 2, maxSearchLength);

            // Optimize the diff list.
            DiffList::iterator dli;
            bool bUsefulFineDiff = false;
            for(dli = pDiffList->begin(); dli != pDiffList->end(); ++dli)
            {
                if(dli->nofEquals >= 4)
                {
                    bUsefulFineDiff = true;
                    break;
                }
            }

            for(dli = pDiffList->begin(); dli != pDiffList->end(); ++dli)
            {
                if(dli->nofEquals < 4 && (dli->diff1 > 0 || dli->diff2 > 0) && !(bUsefulFineDiff && dli == pDiffList->begin()))
                {
                    dli->diff1 += dli->nofEquals;
                    dli->diff2 += dli->nofEquals;
                    dli->nofEquals = 0;
                }
            }

            setFineDiff(selector, pDiffList);
        }

        if((v1[k1].isPureComment() || v1[k1].whiteLine()) && (v2[k2].isPureComment() || v2[k2].whiteLine()))
        {
            if(selector == 1)
            {
                bAEqB = true;
            }
            else if(selector == 2)
            {
                bBEqC = true;
            }
            else if(selector == 3)
            {
                bAEqC = true;
            }
        }
    }

    return bTextsTotalEqual;
}

void Diff3Line::getLineInfo(const int winIdx, const bool isTriple, int& lineIdx,
    DiffList*& pFineDiff1, DiffList*& pFineDiff2, // return values
    int& changed, int& changed2) const
{
    changed = 0;
    changed2 = 0;
    bool bAEqualB = this->bAEqB || (bWhiteLineA && bWhiteLineB);
    bool bAEqualC = this->bAEqC || (bWhiteLineA && bWhiteLineC);
    bool bBEqualC = this->bBEqC || (bWhiteLineB && bWhiteLineC);

    Q_ASSERT(winIdx >= 1 && winIdx <= 3);
    if(winIdx == 1) {
        lineIdx = getLineA();
        pFineDiff1 = pFineAB;
        pFineDiff2 = pFineCA;
        changed |= ((getLineB() == -1) != (lineIdx == -1) ? 1 : 0) +
                   ((getLineC() == -1) != (lineIdx == -1) && isTriple ? 2 : 0);
        changed2 |= (bAEqualB ? 0 : 1) + (bAEqualC || !isTriple ? 0 : 2);
    }
    else if(winIdx == 2)
    {
        lineIdx = getLineB();
        pFineDiff1 = pFineBC;
        pFineDiff2 = pFineAB;
        changed |= ((getLineC() == -1) != (lineIdx == -1) && isTriple ? 1 : 0) +
                   ((getLineA() == -1) != (lineIdx == -1) ? 2 : 0);
        changed2 |= (bBEqualC || !isTriple ? 0 : 1) + (bAEqualB ? 0 : 2);
    }
    else if(winIdx == 3)
    {
        lineIdx = getLineC();
        pFineDiff1 = pFineCA;
        pFineDiff2 = pFineBC;
        changed |= ((getLineA() == -1) != (lineIdx == -1) ? 1 : 0) +
                   ((getLineB() == -1) != (lineIdx == -1) ? 2 : 0);
        changed2 |= (bAEqualC ? 0 : 1) + (bBEqualC ? 0 : 2);
    }
}

bool Diff3LineList::fineDiff(const int selector, const LineData* v1, const LineData* v2)
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
        bTextsTotalEqual = i->fineDiff(selector, v1, v2);
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
    Q_ASSERT(j == (int)d3lv.size());
}
