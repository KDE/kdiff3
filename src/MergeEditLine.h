/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MERGEEDITLINE_H

#include "diff.h"

#include <QString>
#include <QVector>

using MergeEditLineList = std::list<class MergeEditLine>;

class MergeEditLine
{
  public:
    explicit MergeEditLine(const Diff3LineList::const_iterator& i, e_SrcSelector src = e_SrcSelector::None)
    {
        m_id3l = i;
        m_src = src;
        m_bLineRemoved = false;
        mChanged = false;
    }

    void setConflict()
    {
        m_src = e_SrcSelector::None;
        m_bLineRemoved = false;
        mChanged = false;
        m_str = QString();
    }
    Q_REQUIRED_RESULT bool isConflict() const { return m_src == e_SrcSelector::None && !m_bLineRemoved && !mChanged; }

    void setRemoved(e_SrcSelector src = e_SrcSelector::None)
    {
        m_src = src;
        m_bLineRemoved = true;
        m_str = QString();
        mChanged = (src == e_SrcSelector::None);
    }
    Q_REQUIRED_RESULT bool isRemoved() const { return m_bLineRemoved; }

    Q_REQUIRED_RESULT bool isEditableText() const { return !isConflict(); }

    void setString(const QString& s)
    {
        m_str = s;
        m_bLineRemoved = false;
        m_src = e_SrcSelector::None;
        mChanged = true;
    }
    Q_REQUIRED_RESULT QString getString(const std::shared_ptr<LineDataVector>& pLineDataA, const std::shared_ptr<LineDataVector>& pLineDataB, const std::shared_ptr<LineDataVector>& pLineDataC) const;
    Q_REQUIRED_RESULT bool isModified() const { return mChanged; }

    void setSource(e_SrcSelector src, bool bLineRemoved)
    {
        m_src = src;
        m_bLineRemoved = bLineRemoved;
        if(bLineRemoved && m_src == e_SrcSelector::None)
            mChanged = true;
        else if(m_src != e_SrcSelector::None)
        {
            mChanged = false;
            m_str = u8"";
        }
    }

    [[nodiscard]] e_SrcSelector src() const { return m_src; }
    [[nodiscard]] Diff3LineList::const_iterator id3l() const { return m_id3l; }

  private:
    Diff3LineList::const_iterator m_id3l;
    e_SrcSelector m_src; // 1, 2 or 3 for A, B or C respectively, or 0 when line is from neither source.
    QString m_str;       // String when modified by user or null-string when orig data is used.
    bool m_bLineRemoved;
    bool mChanged;
};


class MergeLine
{
  private:
    friend class MergeLineList;

    Diff3LineList::const_iterator mId3l;
    LineIndex d3lLineIdx = -1;    // Needed to show the correct window pos.
    LineCount srcRangeLength = 0; // how many src-lines have these properties
    e_MergeDetails mergeDetails = e_MergeDetails::eDefault;
    bool bConflict = false;
    bool bWhiteSpaceConflict = false;
    bool bDelta = false;
    e_SrcSelector srcSelect = e_SrcSelector::None;
    MergeEditLineList mMergeEditLineList;

  public:
    [[nodiscard]] inline MergeEditLineList list() const { return mMergeEditLineList; }

    [[nodiscard]] inline e_SrcSelector source() const { return srcSelect; }

    [[nodiscard]] inline LineIndex getIndex() const { return d3lLineIdx; }
    [[nodiscard]] inline LineCount sourceRangeLength() const { return srcRangeLength; }

    [[nodiscard]] inline bool isConflict() const { return bConflict; }
    [[nodiscard]] inline bool isWhiteSpaceConflict() const { return bWhiteSpaceConflict; }
    [[nodiscard]] inline bool isDelta() const { return bDelta; }

    [[nodiscard]] inline Diff3LineList::const_iterator id3l() const { return mId3l; }

    [[nodiscard]] inline e_MergeDetails details() const { return mergeDetails; }

    [[nodiscard]] inline LineRef lineCount() const { return (qint64)list().size(); }

    void split(MergeLine& ml2, int d3lLineIdx2) // The caller must insert the ml2 after this ml in the m_mergeLineList
    {
        if(d3lLineIdx2 < d3lLineIdx || d3lLineIdx2 >= d3lLineIdx + srcRangeLength)
            return; //Error
        ml2.mergeDetails = mergeDetails;
        ml2.bConflict = bConflict;
        ml2.bWhiteSpaceConflict = bWhiteSpaceConflict;
        ml2.bDelta = bDelta;
        ml2.srcSelect = srcSelect;

        ml2.d3lLineIdx = d3lLineIdx2;
        ml2.srcRangeLength = srcRangeLength - (d3lLineIdx2 - d3lLineIdx);
        srcRangeLength = d3lLineIdx2 - d3lLineIdx; // current MergeLine controls fewer lines
        ml2.mId3l = mId3l;
        for(int i = 0; i < srcRangeLength; ++i)
            ++ml2.mId3l;

        ml2.mMergeEditLineList.clear();
        // Search for best place to splice
        for(MergeEditLineList::iterator i = mMergeEditLineList.begin(); i != mMergeEditLineList.end(); ++i)
        {
            if(i->id3l() == ml2.mId3l)
            {
                ml2.mMergeEditLineList.splice(ml2.mMergeEditLineList.begin(), mMergeEditLineList, i, mMergeEditLineList.end());
                return;
            }
        }
        ml2.mMergeEditLineList.push_back(MergeEditLine(ml2.mId3l));
    }

    void join(MergeLine& ml2) // The caller must remove the ml2 from the m_mergeLineList after this call
    {
        srcRangeLength += ml2.srcRangeLength;
        ml2.mMergeEditLineList.clear();
        mMergeEditLineList.clear();
        mMergeEditLineList.push_back(MergeEditLine(mId3l)); // Create a simple conflict
        if(ml2.bConflict) bConflict = true;
        if(!ml2.bWhiteSpaceConflict) bWhiteSpaceConflict = false;
        if(ml2.bDelta) bDelta = true;
    }

    bool isSameKind(const MergeLine& ml2) const;

    void mergeOneLine(const Diff3Line& diffRec, bool& bLineRemoved, bool bTwoInputs);
    void dectectWhiteSpaceConflict(const Diff3Line& d, const bool isThreeWay);

    void removeEmptySource();
};

typedef std::list<MergeLine> MergeLineListImp;

class MergeLineList
{
  private:
    MergeLineListImp mImp;

  public:
    [[nodiscard]] inline const MergeLineListImp& list() const { return mImp; }
    [[nodiscard]] inline MergeLineListImp& list() { return mImp; }

    void buildFromDiff3(const Diff3LineList& diff3List, bool isThreeway);
    void updateDefaults(const e_SrcSelector defaultSelector, const bool bConflictsOnly, const bool bWhiteSpaceOnly);

    MergeLineListImp::iterator splitAtDiff3LineIdx(int d3lLineIdx);
};

#endif
