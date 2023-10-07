/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef MERGEEDITLINE_H

#include "diff.h"
#include "LineRef.h"

#include <memory>

#include <QString>

using MergeEditLineList = std::list<class MergeEditLine>;

class MergeEditLine
{
  public:
    explicit MergeEditLine(const Diff3LineList::const_iterator& i, e_SrcSelector src = e_SrcSelector::None)
    {
        m_id3l = i;
        mSrc = src;
        mLineRemoved = false;
        mChanged = false;
    }

    void setConflict()
    {
        mSrc = e_SrcSelector::None;
        mLineRemoved = false;
        mChanged = false;
        mStr = QString();
    }
    Q_REQUIRED_RESULT inline bool isConflict() const { return mSrc == e_SrcSelector::None && !mLineRemoved && !mChanged; }

    void setRemoved(e_SrcSelector src = e_SrcSelector::None)
    {
        mSrc = src;
        mLineRemoved = true;
        mStr = QString();
        mChanged = (src == e_SrcSelector::None);
    }
    Q_REQUIRED_RESULT inline bool isRemoved() const { return mLineRemoved; }

    Q_REQUIRED_RESULT inline bool isEditableText() const { return !isConflict(); }

    void setString(const QString& s)
    {
        mStr = s;
        mLineRemoved = false;
        mSrc = e_SrcSelector::None;
        mChanged = true;
    }
    Q_REQUIRED_RESULT QString getString(const std::shared_ptr<LineDataVector>& pLineDataA, const std::shared_ptr<LineDataVector>& pLineDataB, const std::shared_ptr<LineDataVector>& pLineDataC) const;
    Q_REQUIRED_RESULT inline bool isModified() const { return mChanged; }

    void setSource(e_SrcSelector src, bool bLineRemoved)
    {
        mSrc = src;
        mLineRemoved = bLineRemoved;
        if(bLineRemoved && mSrc == e_SrcSelector::None)
            mChanged = true;
        else if(mSrc != e_SrcSelector::None)
        {
            mChanged = false;
            mStr = u8"";
        }
    }

    [[nodiscard]] inline e_SrcSelector src() const { return mSrc; }
    [[nodiscard]] inline Diff3LineList::const_iterator id3l() const { return m_id3l; }

  private:
    Diff3LineList::const_iterator m_id3l;
    e_SrcSelector mSrc; // 1, 2 or 3 for A, B or C respectively, or 0 when line is from neither source.
    QString mStr;       // String when modified by user or null-string when orig data is used.
    bool mLineRemoved;
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
    [[nodiscard]] inline const MergeEditLineList& list() const { return mMergeEditLineList; }
    [[nodiscard]] inline MergeEditLineList& list() { return mMergeEditLineList; }

    [[nodiscard]] inline e_SrcSelector source() const { return srcSelect; }

    [[nodiscard]] inline LineIndex getIndex() const { return d3lLineIdx; }
    [[nodiscard]] inline LineCount sourceRangeLength() const { return srcRangeLength; }

    [[nodiscard]] inline bool isConflict() const { return bConflict; }
    [[nodiscard]] inline bool isWhiteSpaceConflict() const { return bWhiteSpaceConflict; }
    [[nodiscard]] inline bool isDelta() const { return bDelta; }
    [[nodiscard]] inline bool hasModfiedText()
    {
        return std::any_of(list().cbegin(), list().cend(), [](const MergeEditLine& mel) { return mel.isModified(); });
    }

    [[nodiscard]] inline Diff3LineList::const_iterator id3l() const { return mId3l; }

    [[nodiscard]] inline e_MergeDetails details() const { return mergeDetails; }

    [[nodiscard]] inline LineCount lineCount() const { return SafeInt<qint32>(list().size()); }

    void split(MergeLine& ml2, qint32 d3lLineIdx2) // The caller must insert the ml2 after this ml in the m_mergeLineList
    {
        if(d3lLineIdx2 < d3lLineIdx || d3lLineIdx2 >= d3lLineIdx + srcRangeLength)
        {
            assert(false);
            return; //Error
        }
        ml2.mergeDetails = mergeDetails;
        ml2.bConflict = bConflict;
        ml2.bWhiteSpaceConflict = bWhiteSpaceConflict;
        ml2.bDelta = bDelta;
        ml2.srcSelect = srcSelect;

        ml2.d3lLineIdx = d3lLineIdx2;
        ml2.srcRangeLength = srcRangeLength - (d3lLineIdx2 - d3lLineIdx);
        srcRangeLength = d3lLineIdx2 - d3lLineIdx; // current MergeLine controls fewer lines
        ml2.mId3l = mId3l;
        for(qint32 i = 0; i < srcRangeLength; ++i)
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

    MergeLineListImp::iterator splitAtDiff3LineIdx(qint32 d3lLineIdx);
};

#endif
