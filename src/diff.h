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

#include "common.h"
#include "fileaccess.h"
#include "options.h"
#include "gnudiff_diff.h"
#include "SourceData.h"
#include "Logging.h"

#include <QList>

//enum must be sequential with no gaps to allow loop interiation of values
enum e_SrcSelector
{
   Min = -1,
   Invalid=-1,
   None=0,
   A = 1,
   B = 2,
   C = 3,
   Max=C
};

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
    LineCount nofEquals;

    qint64 diff1;
    qint64 diff2;

    Diff(LineCount eq, qint64 d1, qint64 d2)
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

    static bool equal(const LineData& l1, const LineData& l2, bool bStrict);
};

class Diff3LineList;
class Diff3LineVector;

class DiffBufferInfo
{
  public:
    const LineData* m_pLineDataA;
    const LineData* m_pLineDataB;
    const LineData* m_pLineDataC;
    LineCount m_sizeA;
    LineCount m_sizeB;
    LineCount m_sizeC;
    const Diff3LineList* m_pDiff3LineList;
    const Diff3LineVector* m_pDiff3LineVector;
    void init(Diff3LineList* d3ll, const Diff3LineVector* d3lv,
              const LineData* pldA, LineCount sizeA, const LineData* pldB, LineCount sizeB, const LineData* pldC, LineCount sizeC);
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

    inline void setLineA(const LineRef& line) { lineA = line; }
    inline void setLineB(const LineRef& line) { lineB = line; }
    inline void setLineC(const LineRef& line) { lineC = line; }

    inline bool isEqualAB() const { return bAEqB; }
    inline bool isEqualAC() const { return bAEqC; }
    inline bool isEqualBC() const { return bBEqC; }

    bool operator==(const Diff3Line& d3l) const
    {
        return lineA == d3l.lineA && lineB == d3l.lineB && lineC == d3l.lineC && bAEqB == d3l.bAEqB && bAEqC == d3l.bAEqC && bBEqC == d3l.bBEqC;
    }

    const LineData* getLineData(e_SrcSelector src) const
    {
        Q_ASSERT(m_pDiffBufferInfo != nullptr);
        if(src == A && lineA >= 0) return &m_pDiffBufferInfo->m_pLineDataA[lineA];
        if(src == B && lineB >= 0) return &m_pDiffBufferInfo->m_pLineDataB[lineB];
        if(src == C && lineC >= 0) return &m_pDiffBufferInfo->m_pLineDataC[lineC];
        return nullptr;
    }
    QString getString(const e_SrcSelector src) const
    {
        const LineData* pld = getLineData(src);
        if(pld)
            return QString(pld->getLine(), pld->size());
        else
            return QString();
    }
    LineRef getLineInFile(e_SrcSelector src) const
    {
        if(src == A) return lineA;
        if(src == B) return lineB;
        if(src == C) return lineC;
        return -1;
    }

    bool fineDiff(bool bTextsTotalEqual, const e_SrcSelector selector, const LineData* v1, const LineData* v2);
    void mergeOneLine(e_MergeDetails& mergeDetails, bool& bConflict, bool& bLineRemoved, e_SrcSelector& src, bool bTwoInputs) const;

    void getLineInfo(const e_SrcSelector winIdx, const bool isTriple, int& lineIdx,
        DiffList*& pFineDiff1, DiffList*& pFineDiff2, // return values
        int& changed, int& changed2) const;

  private:
    void setFineDiff(const e_SrcSelector selector, DiffList* pDiffList)
    {
        Q_ASSERT(selector == A || selector == B || selector == C);
        if(selector == A)
        {
            if(pFineAB != nullptr)
                delete pFineAB;
            pFineAB = pDiffList;
        }
        else if(selector == B)
        {
            if(pFineBC != nullptr)
                delete pFineBC;
            pFineBC = pDiffList;
        }
        else if(selector == C)
        {
            if(pFineCA)
                delete pFineCA;
            pFineCA = pDiffList;
        }
    }
};

class Diff3LineList : public std::list<Diff3Line>
{
  public:
    bool fineDiff(const e_SrcSelector selector, const LineData* v1, const LineData* v2);
    void calcDiff3LineVector(Diff3LineVector& d3lv);
    void calcWhiteDiff3Lines(const LineData* pldA, const LineData* pldB, const LineData* pldC);
    //TODO: Add safety guards to prevent list from getting too large. Same problem as with QLinkedList.
    int size() const {
        if(std::list<Diff3Line>::size() > std::numeric_limits<int>::max())
        {
            qCDebug(kdeMain) << "Diff3Line: List too large. size=" << std::list<Diff3Line>::size();
            Q_ASSERT(false);//Unsupported size
            return 0;
        }
        return (int)std::list<Diff3Line>::size();
    }//safe for small files same limit as exited with QLinkedList. This should ultimatly be removed.
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
    LineRef& firstLine(e_SrcSelector winIdx)
    {
        return winIdx == A ? lineA1 : (winIdx == B ? lineB1 : lineC1);
    }
    LineRef& lastLine(e_SrcSelector winIdx)
    {
        return winIdx == A ? lineA2 : (winIdx == B ? lineB2 : lineC2);
    }
    bool isLineInRange(LineRef line, e_SrcSelector winIdx)
    {
        return line >= 0 && line >= firstLine(winIdx) && line <= lastLine(winIdx);
    }
    bool operator==(const ManualDiffHelpEntry& r) const
    {
        return lineA1 == r.lineA1 && lineB1 == r.lineB1 && lineC1 == r.lineC1 &&
               lineA2 == r.lineA2 && lineB2 == r.lineB2 && lineC2 == r.lineC2;
    }

    int calcManualDiffFirstDiff3LineIdx(const Diff3LineVector& d3lv);

    void getRangeForUI(const e_SrcSelector winIdx, int *rangeLine1, int *rangeLine2) const {
        if(winIdx == A) {
            *rangeLine1 = lineA1;
            *rangeLine2 = lineA2;
        }
        if(winIdx == B) {
            *rangeLine1 = lineB1;
            *rangeLine2 = lineB2;
        }
        if(winIdx == C) {
            *rangeLine1 = lineC1;
            *rangeLine2 = lineC2;
        }
    }

    inline int getLine1(const e_SrcSelector winIdx) const { return winIdx == A ? lineA1 : winIdx == B ? lineB1 : lineC1;}
    inline int getLine2(const e_SrcSelector winIdx) const { return winIdx == A ? lineA2 : winIdx == B ? lineB2 : lineC2;}
    bool isValidMove(int line1, int line2, e_SrcSelector winIdx1, e_SrcSelector winIdx2) const;
};

// A list of corresponding ranges
class ManualDiffHelpList: public std::list<ManualDiffHelpEntry>
{
    public:
        bool isValidMove(int line1, int line2, e_SrcSelector winIdx1, e_SrcSelector winIdx2) const;
        void insertEntry(e_SrcSelector winIdx, LineRef firstLine, LineRef lastLine);

        bool runDiff(const LineData* p1, LineRef size1, const LineData* p2, LineRef size2, DiffList& diffList,
                     e_SrcSelector winIdx1, e_SrcSelector winIdx2,
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



bool fineDiff(
    Diff3LineList& diff3LineList,
    int selector,
    const LineData* v1,
    const LineData* v2);

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
