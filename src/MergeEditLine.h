// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef MERGEEDITLINE_H
#define MERGEEDITLINE_H

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
    [[nodiscard]] bool isConflict() const { return mSrc == e_SrcSelector::None && !mLineRemoved && !mChanged; }

    void setRemoved(e_SrcSelector src = e_SrcSelector::None)
    {
        mSrc = src;
        mLineRemoved = true;
        mStr = QString();
        mChanged = (src == e_SrcSelector::None);
    }
    [[nodiscard]] bool isRemoved() const { return mLineRemoved; }

    [[nodiscard]] bool isEditableText() const { return !isConflict(); }

    void setString(const QString& s)
    {
        mStr = s;
        mLineRemoved = false;
        mSrc = e_SrcSelector::None;
        mChanged = true;
    }
    [[nodiscard]] QString getString(const std::shared_ptr<const LineDataVector>& pLineDataA, const std::shared_ptr<const LineDataVector>& pLineDataB, const std::shared_ptr<const LineDataVector>& pLineDataC) const;
    [[nodiscard]] bool isModified() const { return mChanged; }

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

    [[nodiscard]] e_SrcSelector src() const { return mSrc; }
    [[nodiscard]] Diff3LineList::const_iterator id3l() const { return m_id3l; }

  private:
    Diff3LineList::const_iterator m_id3l;
    e_SrcSelector mSrc; // 1, 2 or 3 for A, B or C respectively, or 0 when line is from neither source.
    QString mStr;       // String when modified by user or null-string when orig data is used.
    bool mLineRemoved;
    bool mChanged;
};

class MergeBlock
{
  private:
    friend class MergeBlockList;

    Diff3LineList::const_iterator mId3l;
    LineType d3lLineIdx = -1;    // Needed to show the correct window pos.
    LineType srcRangeLength = 0; // how many src-lines have these properties
    e_MergeDetails mergeDetails = e_MergeDetails::eDefault;
    bool bConflict = false;
    bool bWhiteSpaceConflict = false;
    bool bDelta = false;
    e_SrcSelector srcSelect = e_SrcSelector::None;
    MergeEditLineList mMergeEditLineList;

  public:
    [[nodiscard]] const MergeEditLineList& list() const { return mMergeEditLineList; }
    [[nodiscard]] MergeEditLineList& list() { return mMergeEditLineList; }

    [[nodiscard]] e_SrcSelector source() const { return srcSelect; }

    [[nodiscard]] LineType getIndex() const { return d3lLineIdx; }
    [[nodiscard]] LineType sourceRangeLength() const { return srcRangeLength; }

    [[nodiscard]] bool isConflict() const { return bConflict; }
    [[nodiscard]] bool isWhiteSpaceConflict() const { return bWhiteSpaceConflict; }
    [[nodiscard]] bool isDelta() const { return bDelta; }
    [[nodiscard]] bool hasModfiedText()
    {
        return std::any_of(list().cbegin(), list().cend(), [](const MergeEditLine& mel) { return mel.isModified(); });
    }

    [[nodiscard]] Diff3LineList::const_iterator id3l() const { return mId3l; }

    [[nodiscard]] e_MergeDetails details() const { return mergeDetails; }

    [[nodiscard]] LineType lineCount() const { return SafeInt<qint32>(list().size()); }

    void split(MergeBlock& mb2, LineType d3lLineIdx2) // The caller must insert the mb2 after this mb in the m_mergeLineList
    {
        if(d3lLineIdx2 < d3lLineIdx || d3lLineIdx2 >= d3lLineIdx + srcRangeLength)
        {
            assert(false);
            return; //Error
        }
        mb2.mergeDetails = mergeDetails;
        mb2.bConflict = bConflict;
        mb2.bWhiteSpaceConflict = bWhiteSpaceConflict;
        mb2.bDelta = bDelta;
        mb2.srcSelect = srcSelect;

        mb2.d3lLineIdx = d3lLineIdx2;
        mb2.srcRangeLength = srcRangeLength - (d3lLineIdx2 - d3lLineIdx);
        srcRangeLength = d3lLineIdx2 - d3lLineIdx; // current MergeBlock controls fewer lines
        mb2.mId3l = mId3l;
        for(LineType i = 0; i < srcRangeLength; ++i)
            ++mb2.mId3l;

        mb2.mMergeEditLineList.clear();
        // Search for best place to splice
        for(MergeEditLineList::iterator i = mMergeEditLineList.begin(); i != mMergeEditLineList.end(); ++i)
        {
            if(i->id3l() == mb2.mId3l)
            {
                mb2.mMergeEditLineList.splice(mb2.mMergeEditLineList.begin(), mMergeEditLineList, i, mMergeEditLineList.end());
                return;
            }
        }
        mb2.mMergeEditLineList.push_back(MergeEditLine(mb2.mId3l));
    }

    void join(MergeBlock& mb2) // The caller must remove the ml2 from the m_mergeLineList after this call
    {
        srcRangeLength += mb2.srcRangeLength;
        mb2.mMergeEditLineList.clear();
        mMergeEditLineList.clear();
        mMergeEditLineList.push_back(MergeEditLine(mId3l)); // Create a simple conflict
        if(mb2.bConflict) bConflict = true;
        if(!mb2.bWhiteSpaceConflict) bWhiteSpaceConflict = false;
        if(mb2.bDelta) bDelta = true;
    }

    bool isSameKind(const MergeBlock& mb2) const;

    void mergeOneLine(const Diff3Line& diffRec, bool& bLineRemoved, bool bTwoInputs);
    void dectectWhiteSpaceConflict(const Diff3Line& d, const bool isThreeWay);

    void removeEmptySource();
};

/*
    Note std::list has no virtual destructor.

    This is a non-issue for classes that are not ussed in a generic(i.e. polymorphic) context.
*/
class MergeBlockList: public std::list<MergeBlock>
{
  public:
    using std::list<MergeBlock>::list;

    void buildFromDiff3(const Diff3LineList& diff3List, bool isThreeway);
    void updateDefaults(const e_SrcSelector defaultSelector, const bool bConflictsOnly, const bool bWhiteSpaceOnly);

    MergeBlockList::iterator splitAtDiff3LineIdx(qint32 d3lLineIdx);
};

#endif
