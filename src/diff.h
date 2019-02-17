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

#ifndef DIFF_H
#define DIFF_H

#include <QPainter>
#include <QList>

#include "common.h"
#include "fileaccess.h"
#include "options.h"
#include "gnudiff_diff.h"
#include "SourceData.h"

enum e_MergeDetails
{
   eDefault,
   eNoChange,
   eBChanged,
   eCChanged,
   eBCChanged,         // conflict
   eBCChangedAndEqual, // possible conflict
   eBDeleted,
   eCDeleted,
   eBCDeleted,         // possible conflict

   eBChanged_CDeleted, // conflict
   eCChanged_BDeleted, // conflict
   eBAdded,
   eCAdded,
   eBCAdded,           // conflict
   eBCAddedAndEqual    // possible conflict
};

// Each range with matching elements is followed by a range with differences on either side.
// Then again range of matching elements should follow.
class Diff
{
  public:
    LineRef nofEquals;

    qint64 diff1;
    qint64 diff2;

    Diff(LineRef eq, qint64 d1, qint64 d2)
    {
        nofEquals = eq;
        diff1 = d1;
        diff2 = d2;
    }
};

typedef std::list<Diff> DiffList;

class LineData
{
  private:
    const QChar* pLine = nullptr;
    const QChar* pFirstNonWhiteChar = nullptr;
    int mSize = 0;
    bool bContainsPureComment = false;

  public:
    inline int size() const { return mSize; }
    inline void setSize(const int newSize) { mSize = newSize; }

    inline void setFirstNonWhiteChar(const QChar* firstNonWhiteChar) { pFirstNonWhiteChar = firstNonWhiteChar;}
    inline const QChar* getFirstNonWhiteChar() const { return pFirstNonWhiteChar; }

    inline const QChar* getLine() const { return pLine; }
    inline void setLine(const QChar* line) { pLine = line;}
    int width(int tabSize) const; // Calcs width considering tabs.
    //int occurrences;
    bool whiteLine() const { return pFirstNonWhiteChar - pLine == mSize; }

    bool isPureComment() const { return bContainsPureComment; }
    void setPureComment(const bool bPureComment) { bContainsPureComment = bPureComment; }
};

class Diff3LineList;
class Diff3LineVector;

class DiffBufferInfo
{
  public:
    const LineData* m_pLineDataA;
    const LineData* m_pLineDataB;
    const LineData* m_pLineDataC;
    LineRef m_sizeA;
    LineRef m_sizeB;
    LineRef m_sizeC;
    const Diff3LineList* m_pDiff3LineList;
    const Diff3LineVector* m_pDiff3LineVector;
    void init(Diff3LineList* d3ll, const Diff3LineVector* d3lv,
              const LineData* pldA, LineRef sizeA, const LineData* pldB, LineRef sizeB, const LineData* pldC, LineRef sizeC);
};

class Diff3Line
{
  private:
    LineRef lineA = -1;
    LineRef lineB = -1;
    LineRef lineC = -1;
  public:

    bool bAEqC = false; // These are true if equal or only white-space changes exist.
    bool bBEqC = false;
    bool bAEqB = false;

    bool bWhiteLineA  = false;
    bool bWhiteLineB  = false;
    bool bWhiteLineC  = false;

    DiffList* pFineAB = nullptr; // These are 0 only if completely equal or if either source doesn't exist.
    DiffList* pFineBC = nullptr;
    DiffList* pFineCA = nullptr;

    int linesNeededForDisplay = 1;    // Due to wordwrap
    int sumLinesNeededForDisplay = 0; // For fast conversion to m_diff3WrapLineVector

    DiffBufferInfo* m_pDiffBufferInfo = nullptr; // For convenience

    ~Diff3Line()
    {
        if(pFineAB != nullptr) delete pFineAB;
        if(pFineBC != nullptr) delete pFineBC;
        if(pFineCA != nullptr) delete pFineCA;
        pFineAB = nullptr;
        pFineBC = nullptr;
        pFineCA = nullptr;
    }

    LineRef getLineA() const { return lineA; }
    LineRef getLineB() const { return lineB; }
    LineRef getLineC() const { return lineC; }

    void setLineA(const LineRef& line) { lineA = line; }
    void setLineB(const LineRef& line) { lineB = line; }
    void setLineC(const LineRef& line) { lineC = line; }

    bool operator==(const Diff3Line& d3l) const
    {
        return lineA == d3l.lineA && lineB == d3l.lineB && lineC == d3l.lineC && bAEqB == d3l.bAEqB && bAEqC == d3l.bAEqC && bBEqC == d3l.bBEqC;
    }

    const LineData* getLineData(int src) const
    {
        Q_ASSERT(m_pDiffBufferInfo != nullptr);
        if(src == 1 && lineA >= 0) return &m_pDiffBufferInfo->m_pLineDataA[lineA];
        if(src == 2 && lineB >= 0) return &m_pDiffBufferInfo->m_pLineDataB[lineB];
        if(src == 3 && lineC >= 0) return &m_pDiffBufferInfo->m_pLineDataC[lineC];
        return nullptr;
    }
    QString getString(int src) const
    {
        const LineData* pld = getLineData(src);
        if(pld)
            return QString(pld->getLine(), pld->size());
        else
            return QString();
    }
    LineRef getLineInFile(int src) const
    {
        if(src == 1) return lineA;
        if(src == 2) return lineB;
        if(src == 3) return lineC;
        return -1;
    }

    bool fineDiff(const int selector, const LineData* v1, const LineData* v2);
    void mergeOneLine(e_MergeDetails& mergeDetails, bool& bConflict, bool& bLineRemoved, int& src, bool bTwoInputs) const;

  private:
    void setFineDiff(const int selector, DiffList* pDiffList)
    {
        Q_ASSERT(selector == 1 || selector == 2 || selector == 3);
        if(selector == 1)
        {
            if(pFineAB != nullptr)
                delete pFineAB;
            pFineAB = pDiffList;
        }
        else if(selector == 2)
        {
            if(pFineBC != nullptr)
                delete pFineBC;
            pFineBC = pDiffList;
        }
        else if(selector == 3)
        {
            if(pFineCA)
                delete pFineCA;
            pFineCA = pDiffList;
        }
    }
};

class Diff3LineList : public QList<Diff3Line>
{
  public:
    bool fineDiff(const int selector, const LineData* v1, const LineData* v2);
    void calcDiff3LineVector(Diff3LineVector& d3lv);
    void calcWhiteDiff3Lines(const LineData* pldA, const LineData* pldB, const LineData* pldC);
};

class Diff3LineVector : public QVector<Diff3Line*>
{
};

class Diff3WrapLine
{
  public:
    Diff3Line* pD3L;
    int diff3LineIndex;
    int wrapLineOffset;
    int wrapLineLength;
};

typedef QVector<Diff3WrapLine> Diff3WrapLineVector;

class TotalDiffStatus
{
  public:
    inline void reset()
    {
        bBinaryAEqC = false;
        bBinaryBEqC = false;
        bBinaryAEqB = false;
        bTextAEqC = false;
        bTextBEqC = false;
        bTextAEqB = false;
        nofUnsolvedConflicts = 0;
        nofSolvedConflicts = 0;
        nofWhitespaceConflicts = 0;
    }

    inline int getUnsolvedConflicts() const { return nofUnsolvedConflicts; }
    inline void setUnsolvedConflicts(const int unsolved) { nofUnsolvedConflicts = unsolved; }

    inline int getSolvedConflicts() const { return nofSolvedConflicts; }
    inline void setSolvedConflicts(const int solved) { nofSolvedConflicts = solved; }

    inline int getWhitespaceConflicts() const { return nofWhitespaceConflicts; }
    inline void setWhitespaceConflicts(const int wintespace) { nofWhitespaceConflicts = wintespace; }

    inline int getNonWhitespaceConflicts() { return getUnsolvedConflicts() + getSolvedConflicts() - getWhitespaceConflicts(); }

    bool isBinaryEqualAC() const { return bBinaryAEqC; }
    bool isBinaryEqualBC() const { return bBinaryBEqC; }
    bool isBinaryEqualAB() const { return bBinaryAEqB; }

    bool bBinaryAEqC = false;
    bool bBinaryBEqC = false;
    bool bBinaryAEqB = false;

    bool bTextAEqC = false;
    bool bTextBEqC = false;
    bool bTextAEqB = false;

  private:
    int nofUnsolvedConflicts = 0;
    int nofSolvedConflicts = 0;
    int nofWhitespaceConflicts = 0;
};

class ManualDiffHelpList; // A list of corresponding ranges
// Three corresponding ranges. (Minimum size of a valid range is one line.)
class ManualDiffHelpEntry
{
  private:
    LineRef lineA1 = -1;
    LineRef lineA2 = -1;
    LineRef lineB1 = -1;
    LineRef lineB2 = -1;
    LineRef lineC1 = -1;
    LineRef lineC2 = -1;
  public:
    LineRef& firstLine(int winIdx)
    {
        return winIdx == 1 ? lineA1 : (winIdx == 2 ? lineB1 : lineC1);
    }
    LineRef& lastLine(int winIdx)
    {
        return winIdx == 1 ? lineA2 : (winIdx == 2 ? lineB2 : lineC2);
    }
    bool isLineInRange(LineRef line, int winIdx)
    {
        return line >= 0 && line >= firstLine(winIdx) && line <= lastLine(winIdx);
    }
    bool operator==(const ManualDiffHelpEntry& r) const
    {
        return lineA1 == r.lineA1 && lineB1 == r.lineB1 && lineC1 == r.lineC1 &&
               lineA2 == r.lineA2 && lineB2 == r.lineB2 && lineC2 == r.lineC2;
    }

    int calcManualDiffFirstDiff3LineIdx(const Diff3LineVector& d3lv);

    void getRangeForUI(const int winIdx, int *rangeLine1, int *rangeLine2) const {
        if(winIdx == 1) {
            *rangeLine1 = lineA1;
            *rangeLine2 = lineA2;
        }
        if(winIdx == 2) {
            *rangeLine1 = lineB1;
            *rangeLine2 = lineB2;
        }
        if(winIdx == 3) {
            *rangeLine1 = lineC1;
            *rangeLine2 = lineC2;
        }
    }

    inline int getLine1(const int winIdx) const { return winIdx == 1 ? lineA1 : winIdx == 2 ? lineB1 : lineC1;}
    inline int getLine2(const int winIdx) const { return winIdx == 1 ? lineA2 : winIdx == 2 ? lineB2 : lineC2;}
    bool isValidMove(int line1, int line2, int winIdx1, int winIdx2) const;
};

// A list of corresponding ranges
class ManualDiffHelpList: public std::list<ManualDiffHelpEntry>
{
    public:
        bool isValidMove(int line1, int line2, int winIdx1, int winIdx2) const;
        void insertEntry(int winIdx, LineRef firstLine, LineRef lastLine);

        bool runDiff(const LineData* p1, LineRef size1, const LineData* p2, LineRef size2, DiffList& diffList,
                     int winIdx1, int winIdx2,
                     Options* pOptions);
};

void calcDiff3LineListUsingAB(
    const DiffList* pDiffListAB,
    Diff3LineList& d3ll);

void calcDiff3LineListUsingAC(
    const DiffList* pDiffListAC,
    Diff3LineList& d3ll);

void calcDiff3LineListUsingBC(
    const DiffList* pDiffListBC,
    Diff3LineList& d3ll);

void correctManualDiffAlignment(Diff3LineList& d3ll, ManualDiffHelpList* pManualDiffHelpList);

void calcDiff3LineListTrim(Diff3LineList& d3ll, const LineData* pldA, const LineData* pldB, const LineData* pldC, ManualDiffHelpList* pManualDiffHelpList);


// Helper class that swaps left and right for some commands.
class MyPainter : public QPainter
{
    int m_factor;
    int m_xOffset;
    int m_fontWidth;

  public:
    MyPainter(QPaintDevice* pd, bool bRTL, int width, int fontWidth)
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

bool fineDiff(
    Diff3LineList& diff3LineList,
    int selector,
    const LineData* v1,
    const LineData* v2);

bool equal(const LineData& l1, const LineData& l2, bool bStrict);

inline bool isWhite(QChar c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

/** Returns the number of equivalent spaces at position outPos.
*/
inline int tabber(int outPos, int tabSize)
{
    return tabSize - (outPos % tabSize);
}

/** Returns a line number where the linerange [line, line+nofLines] can
    be displayed best. If it fits into the currently visible range then
    the returned value is the current firstLine.
*/
int getBestFirstLine(int line, int nofLines, int firstLine, int visibleLines);

extern bool g_bIgnoreWhiteSpace;
extern bool g_bIgnoreTrivialMatches;
extern int g_bAutoSolve;

// Cursor conversions that consider g_tabSize.
int convertToPosInText(const QString& s, int posOnScreen, int tabSize);
int convertToPosOnScreen(const QString& s, int posInText, int tabSize);

enum e_CoordType
{
    eFileCoords,
    eD3LLineCoords,
    eWrapCoords
};

void calcTokenPos(const QString&, int posOnScreen, int& pos1, int& pos2, int tabSize);

QString calcHistorySortKey(const QString& keyOrder, QRegExp& matchedRegExpr, const QStringList& parenthesesGroupList);
bool findParenthesesGroups(const QString& s, QStringList& sl);
#endif
