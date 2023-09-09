// clang-format off
/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

#ifndef DIFF_H
#define DIFF_H

#include "common.h"
#include "LineRef.h"
#include "Logging.h"
#include "TypeUtils.h"

#include <list>
#include <memory>
#include <vector>

#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVector>

class LineData;

class Options;

using LineDataVector=std::vector<LineData>;

//e_SrcSelector must be sequential integers with no gaps between Min and Max.
enum class e_SrcSelector
{
    Min = -1,
    Invalid = -1,
    None = 0,
    A = 1,
    B = 2,
    C = 3,
    Max = C
};

inline e_SrcSelector nextSelector(e_SrcSelector selector)
{
    switch(selector)
    {
        case e_SrcSelector::None:
            return e_SrcSelector::A;
        case e_SrcSelector::A:
            return e_SrcSelector::B;
        case e_SrcSelector::B:
            return e_SrcSelector::C;
        default:
            return e_SrcSelector::Invalid;
    }
}

enum class e_MergeDetails
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

enum ChangeFlag
{
    NoChange = 0,
    AChanged = 0x1,
    BChanged = 0x2,
    Both = AChanged | BChanged
};

Q_DECLARE_FLAGS(ChangeFlags, ChangeFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(ChangeFlags);

// Each range with matching elements is followed by a range with differences on either side.
// Then again range of matching elements should follow.
class Diff
{
  private:
    LineCount nofEquals = 0;

    qint64 mDiff1 = 0;
    qint64 mDiff2 = 0;

  public:
    Diff() = default;//We use defualt initialization force compiler to generate a default constructor
    Diff(LineCount eq, const qint64 inDiff1, const qint64 inDiff2)
    {
        assert(eq >= 0);
        nofEquals = eq;
        mDiff1 = inDiff1;
        mDiff2 = inDiff2;
    }

    [[nodiscard]] bool isEmpty() const { return nofEquals == 0 && mDiff1 == 0 && mDiff2 == 0;}

    Q_REQUIRED_RESULT inline qint32 numberOfEquals() const { return nofEquals; };

    Q_REQUIRED_RESULT inline qint64 diff1() const { return mDiff1; };
    Q_REQUIRED_RESULT inline qint64 diff2() const { return mDiff2; };

    inline void setNumberOfEquals(const qint32 inNumOfEquals) { nofEquals = inNumOfEquals; }

    inline void adjustNumberOfEquals(const qint64 delta) { nofEquals += delta; }
    inline void adjustDiff1(const qint64 delta) { mDiff1 += delta; }
    inline void adjustDiff2(const qint64 delta) { mDiff2 += delta; }

    bool operator==(const Diff& b) const
    {
        return nofEquals == b.nofEquals && mDiff1 == b.mDiff1 && mDiff2 == b.mDiff2;
    };

    bool operator!=(const Diff& b) const
    {
        return !(*this == b);
    };
};

class DiffList: public std::list<Diff>
{
  public:
    using std::list<Diff>::list;
    void calcDiff(const QString& line1, const QString& line2, const qint32 maxSearchRange);
    void runDiff(const std::shared_ptr<LineDataVector> &p1, const size_t index1, LineRef size1, const std::shared_ptr<LineDataVector> &p2, const size_t index2, LineRef size2, const QSharedPointer<Options>& pOptions);
    void verify(const LineRef size1, const LineRef size2);
};

class LineData
{
  private:
    QSharedPointer<QString> mBuffer;
    QtSizeType mFirstNonWhiteChar = 0;
    //This tracks the offset with-in our unicode buffer not the file offset
    QtSizeType mOffset = 0;
    QtSizeType mSize = 0;
    bool bContainsPureComment = false;
    bool bSkipable = false;//TODO: Move me

  public:
    explicit LineData() = default; // needed for Qt internal reasons should not be used.
    inline LineData(const QSharedPointer<QString>& buffer, const QtSizeType inOffset, QtSizeType inSize = 0, QtSizeType inFirstNonWhiteChar = 0, bool inIsSkipable = false, const bool inIsPureComment = false)
    {
        mBuffer = buffer;
        mOffset = inOffset;
        mSize = inSize;
        bContainsPureComment = inIsPureComment;
        bSkipable = inIsSkipable;
        mFirstNonWhiteChar = inFirstNonWhiteChar;
    }
    Q_REQUIRED_RESULT inline QtSizeType size() const { return mSize; }
    Q_REQUIRED_RESULT inline qint32 getFirstNonWhiteChar() const { return mFirstNonWhiteChar; }

    /*
        QString::fromRawData allows us to create a light weight QString backed by the buffer memmory.
    */
    Q_REQUIRED_RESULT inline const QString getLine() const { return QString::fromRawData(mBuffer->data() + mOffset, mSize); }
    Q_REQUIRED_RESULT inline const QSharedPointer<QString>& getBuffer() const { return mBuffer; }

    Q_REQUIRED_RESULT inline QtSizeType getOffset() const { return mOffset; }
    Q_REQUIRED_RESULT qint32 width(qint32 tabSize) const; // Calcs width considering tabs.

    Q_REQUIRED_RESULT inline bool whiteLine() const { return mFirstNonWhiteChar == 0; }

    Q_REQUIRED_RESULT inline bool isPureComment() const { return bContainsPureComment; }
    inline void setPureComment(const bool bPureComment) { bContainsPureComment = bPureComment; }

    Q_REQUIRED_RESULT inline bool isSkipable() const { return bSkipable; }
    inline void setSkipable(const bool inSkipable) { bSkipable = inSkipable; }

    Q_REQUIRED_RESULT static bool equal(const LineData& l1, const LineData& l2);
};

class ManualDiffHelpList; // A list of corresponding ranges

class Diff3Line;
class Diff3LineList;

using Diff3LineVector = QVector<Diff3Line*>;

class DiffBufferInfo
{
  private:
    std::shared_ptr<LineDataVector> mLineDataA;
    std::shared_ptr<LineDataVector> mLineDataB;
    std::shared_ptr<LineDataVector> mLineDataC;
    const Diff3LineList* m_pDiff3LineList = nullptr;

  public:
    void init(Diff3LineList* d3ll,
              const std::shared_ptr<LineDataVector> &pldA, const std::shared_ptr<LineDataVector> &pldB, const std::shared_ptr<LineDataVector> &pldC);

    [[nodiscard]] inline std::shared_ptr<LineDataVector> getLineData(e_SrcSelector srcIndex) const
    {
        switch(srcIndex)
        {
            case e_SrcSelector::A:
                return mLineDataA;
            case e_SrcSelector::B:
                return mLineDataB;
            case e_SrcSelector::C:
                return mLineDataC;
            default:
                return nullptr;
        }
    }
};

enum class IgnoreFlag
{
    none = 0,
    ignoreWhiteSpace = 1 << 1,
    ignoreComments = 1 << 2,
};

Q_DECLARE_FLAGS(IgnoreFlags, IgnoreFlag);
Q_DECLARE_OPERATORS_FOR_FLAGS(IgnoreFlags);

class Diff3Line
{
  protected:
    friend class Diff3LineList;
    LineRef lineA;
    LineRef lineB;
    LineRef lineC;

    bool bAEqC = false; // These are true if equal or only white-space changes exist.
    bool bBEqC = false;
    bool bAEqB = false;

    bool bWhiteLineA = false;
    bool bWhiteLineB = false;
    bool bWhiteLineC = false;

    std::shared_ptr<DiffList> pFineAB; // These are NULL only if completely equal or if either source doesn't exist.
    std::shared_ptr<DiffList> pFineBC;
    std::shared_ptr<DiffList> pFineCA;

    qint32 mLinesNeededForDisplay = 1;    // Due to wordwrap
    qint32 mSumLinesNeededForDisplay = 0; // For fast conversion to m_diff3WrapLineVector
  public:
    static QSharedPointer<DiffBufferInfo> m_pDiffBufferInfo; // Needed by this class and only this but inited directly from KDiff3App::mainInit

    [[nodiscard]] inline bool hasFineDiffAB() const { return pFineAB != nullptr; }
    [[nodiscard]] inline bool hasFineDiffBC() const { return pFineBC != nullptr; }
    [[nodiscard]] inline bool hasFineDiffCA() const { return pFineCA != nullptr; }

    [[nodiscard]] inline LineRef getLineIndex(e_SrcSelector src) const
    {
        switch(src)
        {
            case e_SrcSelector::A:
                return getLineA();
            case e_SrcSelector::B:
                return getLineB();
            case e_SrcSelector::C:
                return getLineC();
            default:
                assert(false);
                return LineRef::invalid;
        }
    }

    Q_REQUIRED_RESULT LineRef getLineA() const { return lineA; }
    Q_REQUIRED_RESULT LineRef getLineB() const { return lineB; }
    Q_REQUIRED_RESULT LineRef getLineC() const { return lineC; }

    inline void setLineA(const LineRef& line) { lineA = line; }
    inline void setLineB(const LineRef& line) { lineB = line; }
    inline void setLineC(const LineRef& line) { lineC = line; }

    Q_REQUIRED_RESULT inline bool isEqualAB() const { return bAEqB; }
    Q_REQUIRED_RESULT inline bool isEqualAC() const { return bAEqC; }
    Q_REQUIRED_RESULT inline bool isEqualBC() const { return bBEqC; }

    Q_REQUIRED_RESULT inline bool isWhiteLine(e_SrcSelector src) const
    {
        assert(src == e_SrcSelector::A || src == e_SrcSelector::B || src == e_SrcSelector::C);

        switch(src)
        {
            case e_SrcSelector::A:
                return bWhiteLineA;
            case e_SrcSelector::B:
                return bWhiteLineB;
            case e_SrcSelector::C:
                return bWhiteLineC;
            default:
                //should never get here
                assert(false);
                return false;
        }
    }

    bool operator==(const Diff3Line& d3l) const
    {
        return lineA == d3l.lineA && lineB == d3l.lineB && lineC == d3l.lineC &&
               bAEqB == d3l.bAEqB && bAEqC == d3l.bAEqC && bBEqC == d3l.bBEqC &&
               bWhiteLineA == d3l.bWhiteLineA && bWhiteLineB == d3l.bWhiteLineB && bWhiteLineC == d3l.bWhiteLineC;
    }

    Q_REQUIRED_RESULT const LineData& getLineData(e_SrcSelector src) const
    {
        assert(m_pDiffBufferInfo != nullptr);
        assert(src == e_SrcSelector::A || src == e_SrcSelector::B || src == e_SrcSelector::C);
        //Use at() here not [] to avoid using really weird syntax
        if(src == e_SrcSelector::A && lineA.isValid()) return m_pDiffBufferInfo->getLineData(src)->at(lineA);
        if(src == e_SrcSelector::B && lineB.isValid()) return m_pDiffBufferInfo->getLineData(src)->at(lineB);
        return m_pDiffBufferInfo->getLineData(src)->at(lineC);
    }
    Q_REQUIRED_RESULT const QString getString(const e_SrcSelector src) const
    {
        const LineData& pld = getLineData(src);

        return pld.getLine();
    }
    Q_REQUIRED_RESULT LineRef getLineInFile(e_SrcSelector src) const
    {
        if(src == e_SrcSelector::A) return lineA;
        if(src == e_SrcSelector::B) return lineB;
        if(src == e_SrcSelector::C) return lineC;
        return LineRef();
    }

    Q_REQUIRED_RESULT inline qint32 sumLinesNeededForDisplay() const { return mSumLinesNeededForDisplay; }

    Q_REQUIRED_RESULT inline qint32 linesNeededForDisplay() const { return mLinesNeededForDisplay; }

    void setLinesNeeded(const qint32 lines) { mLinesNeededForDisplay = lines; }
    Q_REQUIRED_RESULT bool fineDiff(bool bTextsTotalEqual, const e_SrcSelector selector, const std::shared_ptr<LineDataVector> &v1, const std::shared_ptr<LineDataVector> &v2, const IgnoreFlags eIgnoreFlags);
    void getLineInfo(const e_SrcSelector winIdx, const bool isTriple, LineRef& lineIdx,
                     std::shared_ptr<DiffList>& pFineDiff1, std::shared_ptr<DiffList>& pFineDiff2, // return values
                     ChangeFlags& changed, ChangeFlags& changed2) const;

  private:
    void setFineDiff(const e_SrcSelector selector, std::shared_ptr<DiffList>& pDiffList)
    {
        assert(selector == e_SrcSelector::A || selector == e_SrcSelector::B || selector == e_SrcSelector::C);
        if(selector == e_SrcSelector::A)
        {
            pFineAB = pDiffList;
        }
        else if(selector == e_SrcSelector::B)
        {
            pFineBC = pDiffList;
        }
        else if(selector == e_SrcSelector::C)
        {
            pFineCA = pDiffList;
        }
    }
};

struct HistoryRange;

class Diff3LineList: public std::list<Diff3Line>
{
  public:
    using std::list<Diff3Line>::list;

    void findHistoryRange(const QRegularExpression& historyStart, bool bThreeFiles, HistoryRange& range) const;
    bool fineDiff(const e_SrcSelector selector, const std::shared_ptr<LineDataVector> &v1, const std::shared_ptr<LineDataVector> &v2, const IgnoreFlags eIgnoreFlags);
    void calcDiff3LineVector(Diff3LineVector& d3lv);
    void calcWhiteDiff3Lines(const std::shared_ptr<LineDataVector> &pldA, const std::shared_ptr<LineDataVector> &pldB, const std::shared_ptr<LineDataVector> &pldC, const bool bIgnoreComments);

    void calcDiff3LineListUsingAB(const DiffList* pDiffListAB);
    void calcDiff3LineListUsingAC(const DiffList* pDiffListAC);
    void calcDiff3LineListUsingBC(const DiffList* pDiffListBC);

    void correctManualDiffAlignment(ManualDiffHelpList* pManualDiffHelpList);

    void calcDiff3LineListTrim(const std::shared_ptr<LineDataVector> &pldA, const std::shared_ptr<LineDataVector> &pldB, const std::shared_ptr<LineDataVector> &pldC, ManualDiffHelpList* pManualDiffHelpList);

    LineCount recalcWordWrap(bool resetDisplayCount)
    {
        LineCount sumOfLines = 0;

        for(Diff3Line& d3l: *this)
        {
            if(resetDisplayCount)
                d3l.mLinesNeededForDisplay = 1;

            d3l.mSumLinesNeededForDisplay = sumOfLines;
            sumOfLines += d3l.linesNeededForDisplay();
        }

        return sumOfLines;
    }

    void debugLineCheck(const LineCount size, const e_SrcSelector srcSelector) const;

    void dump();

    [[nodiscard]] LineCount numberOfLines(bool bWordWrap) const
    {
        if(bWordWrap)
        {
            LineCount lines;

            lines = 0;
            Diff3LineList::const_iterator i;
            for(i = begin(); i != end(); ++i)
            {
                lines += i->linesNeededForDisplay();
            }

            return lines;
        }
        else
        {
            return SafeInt<LineCount>(size());
        }
    }
};

struct HistoryRange
{
    Diff3LineList::const_iterator start;
    Diff3LineList::const_iterator end;
    qint32 startIdx = -1;
    qint32 endIdx = -1;
};

struct Diff3WrapLine
{
    Diff3Line* pD3L = nullptr;
    qint32 diff3LineIndex = 0;
    qint32 wrapLineOffset = 0;
    qint32 wrapLineLength = 0;
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

    [[nodiscard]] inline qint32 getUnsolvedConflicts() const { return nofUnsolvedConflicts; }
    inline void setUnsolvedConflicts(const qint32 unsolved) { nofUnsolvedConflicts = unsolved; }

    [[nodiscard]] inline qint32 getSolvedConflicts() const { return nofSolvedConflicts; }
    inline void setSolvedConflicts(const qint32 solved) { nofSolvedConflicts = solved; }

    [[nodiscard]] inline qint32 getWhitespaceConflicts() const { return nofWhitespaceConflicts; }
    inline void setWhitespaceConflicts(const qint32 wintespace) { nofWhitespaceConflicts = wintespace; }

    [[nodiscard]] inline qint32 getNonWhitespaceConflicts() const { return getUnsolvedConflicts() + getSolvedConflicts() - getWhitespaceConflicts(); }

    [[nodiscard]] bool isBinaryEqualAC() const { return bBinaryAEqC; }
    [[nodiscard]] bool isBinaryEqualBC() const { return bBinaryBEqC; }
    [[nodiscard]] bool isBinaryEqualAB() const { return bBinaryAEqB; }

    void setBinaryEqualAC(const bool equal) { bBinaryAEqC = equal; }
    void setBinaryEqualBC(const bool equal) { bBinaryBEqC = equal; }
    void setBinaryEqualAB(const bool equal) { bBinaryAEqB = equal; }

    [[nodiscard]] bool isTextEqualAC() const { return bTextAEqC; }
    [[nodiscard]] bool isTextEqualBC() const { return bTextBEqC; }
    [[nodiscard]] bool isTextEqualAB() const { return bTextAEqB; }

    void setTextEqualAC(const bool equal) { bTextAEqC = equal; }
    void setTextEqualBC(const bool equal) { bTextBEqC = equal; }
    void setTextEqualAB(const bool equal) { bTextAEqB = equal; }

  private:
    bool bBinaryAEqC = false;
    bool bBinaryBEqC = false;
    bool bBinaryAEqB = false;

    bool bTextAEqC = false;
    bool bTextBEqC = false;
    bool bTextAEqB = false;
    qint32 nofUnsolvedConflicts = 0;
    qint32 nofSolvedConflicts = 0;
    qint32 nofWhitespaceConflicts = -1;
};

// Three corresponding ranges. (Minimum size of a valid range is one line.)
class ManualDiffHelpEntry
{
  private:
    LineRef lineA1;
    LineRef lineA2;
    LineRef lineB1;
    LineRef lineB2;
    LineRef lineC1;
    LineRef lineC2;

  public:
    ManualDiffHelpEntry() = default;
    ManualDiffHelpEntry(e_SrcSelector winIdx, LineRef firstLine, LineRef lastLine)
    {
        if(winIdx == e_SrcSelector::A)
        {
            lineA1 = firstLine;
            lineA2 = lastLine;
        }
        else if(winIdx == e_SrcSelector::B)
        {
            lineB1 = firstLine;
            lineB2 = lastLine;
        }
        else
        {
            lineC1 = firstLine;
            lineC2 = lastLine;
        }
    }

    LineRef& firstLine(e_SrcSelector winIdx)
    {
        return winIdx == e_SrcSelector::A ? lineA1 : (winIdx == e_SrcSelector::B ? lineB1 : lineC1);
    }
    LineRef& lastLine(e_SrcSelector winIdx)
    {
        return winIdx == e_SrcSelector::A ? lineA2 : (winIdx == e_SrcSelector::B ? lineB2 : lineC2);
    }
    bool isLineInRange(LineRef line, e_SrcSelector winIdx)
    {
        return line.isValid() && line >= firstLine(winIdx) && line <= lastLine(winIdx);
    }
    bool operator==(const ManualDiffHelpEntry& r) const
    {
        return lineA1 == r.lineA1 && lineB1 == r.lineB1 && lineC1 == r.lineC1 &&
               lineA2 == r.lineA2 && lineB2 == r.lineB2 && lineC2 == r.lineC2;
    }

    qint32 calcManualDiffFirstDiff3LineIdx(const Diff3LineVector& d3lv);

    void getRangeForUI(const e_SrcSelector winIdx, LineRef* rangeLine1, LineRef* rangeLine2) const
    {
        if(winIdx == e_SrcSelector::A)
        {
            *rangeLine1 = lineA1;
            *rangeLine2 = lineA2;
        }
        if(winIdx == e_SrcSelector::B)
        {
            *rangeLine1 = lineB1;
            *rangeLine2 = lineB2;
        }
        if(winIdx == e_SrcSelector::C)
        {
            *rangeLine1 = lineC1;
            *rangeLine2 = lineC2;
        }
    }

    [[nodiscard]] inline const LineRef& getLine1(const e_SrcSelector winIdx) const { return winIdx == e_SrcSelector::A ? lineA1 : winIdx == e_SrcSelector::B ? lineB1 : lineC1;}
    [[nodiscard]] inline const LineRef& getLine2(const e_SrcSelector winIdx) const { return winIdx == e_SrcSelector::A ? lineA2 : winIdx == e_SrcSelector::B ? lineB2 : lineC2;}
    [[nodiscard]] bool isValidMove(LineRef line1, LineRef line2, e_SrcSelector winIdx1, e_SrcSelector winIdx2) const;
};

// A list of corresponding ranges
class ManualDiffHelpList: public std::list<ManualDiffHelpEntry>
{
  public:
    Q_REQUIRED_RESULT bool isValidMove(LineRef line1, LineRef line2, e_SrcSelector winIdx1, e_SrcSelector winIdx2) const;
    void insertEntry(e_SrcSelector winIdx, LineRef firstLine, LineRef lastLine);

    void runDiff(const std::shared_ptr<LineDataVector> &p1, LineRef size1, const std::shared_ptr<LineDataVector> &p2, LineRef size2, DiffList& diffList,
                 e_SrcSelector winIdx1, e_SrcSelector winIdx2,
                 const QSharedPointer<Options>& pOptions);
};

/** Returns the number of equivalent spaces at position outPos.
*/
inline qint32 tabber(qint32 outPos, qint32 tabSize)
{
    return tabSize - (outPos % tabSize);
}

/** Returns a line number where the linerange [line, line+nofLines] can
    be displayed best. If it fits into the currently visible range then
    the returned value is the current firstLine.
*/
qint32 getBestFirstLine(qint32 line, qint32 nofLines, qint32 firstLine, qint32 visibleLines);

enum e_CoordType
{
    eFileCoords,
    eD3LLineCoords,
    eWrapCoords
};

QString calcHistorySortKey(const QString& keyOrder, QRegularExpressionMatch& matchedRegExpr, const QStringList& parenthesesGroupList);
bool findParenthesesGroups(const QString& s, QStringList& sl);
#endif
