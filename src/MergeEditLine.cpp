/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include "MergeEditLine.h"

#include "diff.h"
#include "LineRef.h"

#include <memory>
#include <optional>
#include <vector>

QString MergeEditLine::getString(const std::shared_ptr<LineDataVector> &pLineDataA, const std::shared_ptr<LineDataVector> &pLineDataB, const std::shared_ptr<LineDataVector> &pLineDataC) const
{
    //Triggered by resize event during early init. Ignore these calls.
    if((mSrc == e_SrcSelector::A && pLineDataA->empty()) || (mSrc == e_SrcSelector::B && pLineDataB->empty()) || (mSrc == e_SrcSelector::C && pLineDataC->empty()))
        return QString();

    if(isRemoved() || (!isModified() && mSrc == e_SrcSelector::None))
    {
        return QString();
    }

    if(!isModified())
    {
        std::optional<LineData> lineData;
        assert(mSrc == e_SrcSelector::A || mSrc == e_SrcSelector::B || mSrc == e_SrcSelector::C);

        if(mSrc == e_SrcSelector::A && m_id3l->getLineA().isValid())
            lineData = (*pLineDataA)[m_id3l->getLineA()];
        else if(mSrc == e_SrcSelector::B && m_id3l->getLineB().isValid())
            lineData = (*pLineDataB)[m_id3l->getLineB()];
        else if(mSrc == e_SrcSelector::C && m_id3l->getLineC().isValid())
            lineData = (*pLineDataC)[m_id3l->getLineC()];

        //Not an error.
        if(!lineData.has_value())
        {
            return QString();
        }

        return lineData->getLine();
    }

    return mStr;
}

bool MergeLine::isSameKind(const MergeLine &ml2) const
{
    if(bConflict && ml2.bConflict)
    {
        // Both lines have conflicts: If one is only a white space conflict and
        // the other one is a real conflict, then this line returns false.
        return mId3l->isEqualAC() == ml2.mId3l->isEqualAC() && mId3l->isEqualAB() == ml2.mId3l->isEqualAB();
    }
    else
        return (
            (!bConflict && !ml2.bConflict && bDelta && ml2.bDelta && srcSelect == ml2.srcSelect && (mergeDetails == ml2.mergeDetails || (mergeDetails != e_MergeDetails::eBCAddedAndEqual && ml2.mergeDetails != e_MergeDetails::eBCAddedAndEqual))) ||
            (!bDelta && !ml2.bDelta));
}

// Calculate the merge information for the given Diff3Line.
// Results will be stored in this and bLineRemoved.
void MergeLine::mergeOneLine(const Diff3Line &diffRec, bool &bLineRemoved, bool bTwoInputs)
{
    mergeDetails = e_MergeDetails::eDefault;
    bConflict = false;
    bLineRemoved = false;
    srcSelect = e_SrcSelector::None;

    if(bTwoInputs) // Only two input files
    {
        if(diffRec.getLineA().isValid() && diffRec.getLineB().isValid())
        {
            if(!diffRec.hasFineDiffAB())
            {
                mergeDetails = e_MergeDetails::eNoChange;
                srcSelect = e_SrcSelector::A;
            }
            else
            {
                mergeDetails = e_MergeDetails::eBChanged;
                bConflict = true;
            }
        }
        else
        {
            mergeDetails = e_MergeDetails::eBDeleted;
            bConflict = true;
        }
        return;
    }

    // A is base.
    if(diffRec.getLineA().isValid() && diffRec.getLineB().isValid() && diffRec.getLineC().isValid())
    {
        if(!diffRec.hasFineDiffAB() && !diffRec.hasFineDiffBC() && !diffRec.hasFineDiffCA())
        {
            mergeDetails = e_MergeDetails::eNoChange;
            srcSelect = e_SrcSelector::A;
        }
        else if(!diffRec.hasFineDiffAB() && diffRec.hasFineDiffBC() && diffRec.hasFineDiffCA())
        {
            mergeDetails = e_MergeDetails::eCChanged;
            srcSelect = e_SrcSelector::C;
        }
        else if(diffRec.hasFineDiffAB() && diffRec.hasFineDiffBC() && !diffRec.hasFineDiffCA())
        {
            mergeDetails = e_MergeDetails::eBChanged;
            srcSelect = e_SrcSelector::B;
        }
        else if(diffRec.hasFineDiffAB() && !diffRec.hasFineDiffBC() && diffRec.hasFineDiffCA())
        {
            mergeDetails = e_MergeDetails::eBCChangedAndEqual;
            srcSelect = e_SrcSelector::C;
        }
        else if(diffRec.hasFineDiffAB() && diffRec.hasFineDiffBC() && diffRec.hasFineDiffCA())
        {
            mergeDetails = e_MergeDetails::eBCChanged;
            bConflict = true;
        }
        else
        {
            assert(false);
        }
    }
    else if(diffRec.getLineA().isValid() && diffRec.getLineB().isValid() && !diffRec.getLineC().isValid())
    {
        if(diffRec.hasFineDiffAB())
        {
            mergeDetails = e_MergeDetails::eBChanged_CDeleted;
            bConflict = true;
        }
        else
        {
            mergeDetails = e_MergeDetails::eCDeleted;
            bLineRemoved = true;
            srcSelect = e_SrcSelector::C;
        }
    }
    else if(diffRec.getLineA().isValid() && !diffRec.getLineB().isValid() && diffRec.getLineC().isValid())
    {
        if(diffRec.hasFineDiffCA())
        {
            mergeDetails = e_MergeDetails::eCChanged_BDeleted;
            bConflict = true;
        }
        else
        {
            mergeDetails = e_MergeDetails::eBDeleted;
            bLineRemoved = true;
            srcSelect = e_SrcSelector::B;
        }
    }
    else if(!diffRec.getLineA().isValid() && diffRec.getLineB().isValid() && diffRec.getLineC().isValid())
    {
        if(diffRec.hasFineDiffBC())
        {
            mergeDetails = e_MergeDetails::eBCAdded;
            bConflict = true;
        }
        else // B==C
        {
            mergeDetails = e_MergeDetails::eBCAddedAndEqual;
            srcSelect = e_SrcSelector::C;
        }
    }
    else if(!diffRec.getLineA().isValid() && !diffRec.getLineB().isValid() && diffRec.getLineC().isValid())
    {
        mergeDetails = e_MergeDetails::eCAdded;
        srcSelect = e_SrcSelector::C;
    }
    else if(!diffRec.getLineA().isValid() && diffRec.getLineB().isValid() && !diffRec.getLineC().isValid())
    {
        mergeDetails = e_MergeDetails::eBAdded;
        srcSelect = e_SrcSelector::B;
    }
    else if(diffRec.getLineA().isValid() && !diffRec.getLineB().isValid() && !diffRec.getLineC().isValid())
    {
        mergeDetails = e_MergeDetails::eBCDeleted;
        bLineRemoved = true;
        srcSelect = e_SrcSelector::C;
    }
    else
        assert(false);
}
/*
    Build a new MergeLineList from scratch using a Diff3LineList.
*/
void MergeLineList::buildFromDiff3(const Diff3LineList &diff3List, bool isThreeway)
{
    LineIndex lineIdx = 0;
    for(auto it = diff3List.cbegin(); it != diff3List.cend(); ++it)
    {
        const Diff3Line &d = *it;
        MergeLine ml;
        bool bLineRemoved;

        ml.mergeOneLine(d, bLineRemoved, !isThreeway);
        ml.dectectWhiteSpaceConflict(d, isThreeway);

        ml.d3lLineIdx = lineIdx;
        ml.bDelta = ml.srcSelect != e_SrcSelector::A;
        ml.mId3l = it;
        ml.srcRangeLength = 1;

        MergeLine *lBack = mImp.empty() ? nullptr : &mImp.back();

        bool bSame = lBack != nullptr && ml.isSameKind(*lBack);
        if(bSame)
        {
            ++lBack->srcRangeLength;
            if(lBack->isWhiteSpaceConflict() && !ml.isWhiteSpaceConflict())
                lBack->bWhiteSpaceConflict = false;
        }
        else
        {
            mImp.push_back(ml);
        }

        if(!ml.isConflict())
        {
            MergeLine &tmpBack = mImp.back();
            MergeEditLine mel(ml.id3l());
            mel.setSource(ml.srcSelect, bLineRemoved);
            tmpBack.list().push_back(mel);
        }
        else if(lBack == nullptr || !lBack->isConflict() || !bSame)
        {
            MergeLine &tmpBack = mImp.back();
            MergeEditLine mel(ml.id3l());
            mel.setConflict();
            tmpBack.list().push_back(mel);
        }

        ++lineIdx;
    }
}
/*
    Changes default merge settings currently used when not in auto mode or if white space is being auto solved.
*/
void MergeLineList::updateDefaults(const e_SrcSelector defaultSelector, const bool bConflictsOnly, const bool bWhiteSpaceOnly)
{
    // Change all auto selections
    MergeLineListImp::iterator mlIt;
    for(mlIt = mImp.begin(); mlIt != mImp.end(); ++mlIt)
    {
        MergeLine &ml = *mlIt;
        bool bConflict = ml.list().empty() || ml.list().begin()->isConflict();
        if(ml.isDelta() && !(ml.hasModfiedText() && (bConflictsOnly || bWhiteSpaceOnly)) && (!bConflictsOnly || bConflict) && (!bWhiteSpaceOnly || ml.isWhiteSpaceConflict()))
        {
            ml.list().clear();
            if(defaultSelector == e_SrcSelector::Invalid)
            {
                MergeEditLine mel(ml.id3l());

                mel.setConflict();
                ml.bConflict = true;
                ml.list().push_back(mel);
            }
            else
            {
                Diff3LineList::const_iterator d3llit = ml.id3l();
                qint32 j;

                for(j = 0; j < ml.srcRangeLength; ++j)
                {
                    MergeEditLine mel(d3llit);
                    mel.setSource(defaultSelector, false);

                    LineRef srcLine = defaultSelector == e_SrcSelector::A ? d3llit->getLineA() : defaultSelector == e_SrcSelector::B ? d3llit->getLineB() :
                                                                                             defaultSelector == e_SrcSelector::C     ? d3llit->getLineC() :
                                                                                                                                       LineRef();
                    if(srcLine.isValid())
                    {
                        ml.list().push_back(mel);
                    }

                    ++d3llit;
                }

                if(ml.list().empty()) // Make a line nevertheless
                {
                    MergeEditLine mel(ml.id3l());
                    mel.setRemoved(defaultSelector);
                    ml.list().push_back(mel);
                }
            }
        }
    }
}

void MergeLine::dectectWhiteSpaceConflict(const Diff3Line &d, const bool isThreeWay)
{
    // Automatic solving for only whitespace changes.
    if(isConflict() &&
       ((!isThreeWay && (d.isEqualAB() || (d.isWhiteLine(e_SrcSelector::A) && d.isWhiteLine(e_SrcSelector::B)))) ||
        (isThreeWay && ((d.isEqualAB() && d.isEqualAC()) || (d.isWhiteLine(e_SrcSelector::A) && d.isWhiteLine(e_SrcSelector::B) && d.isWhiteLine(e_SrcSelector::C))))))
    {
        bWhiteSpaceConflict = true;
    }
}
// Remove all lines that are empty, because no src lines are there.
void MergeLine::removeEmptySource()
{
    LineRef oldSrcLine;
    e_SrcSelector oldSrc = e_SrcSelector::Invalid;
    MergeEditLineList::iterator melIt;
    for(melIt = list().begin(); melIt != list().end();)
    {
        MergeEditLine& mel = *melIt;
        e_SrcSelector melsrc = mel.src();

        LineRef srcLine = mel.isRemoved() ? LineRef() : melsrc == e_SrcSelector::A ? mel.id3l()->getLineA() : melsrc == e_SrcSelector::B ? mel.id3l()->getLineB() : melsrc == e_SrcSelector::C ? mel.id3l()->getLineC() : LineRef();

        // At least one line remains because oldSrc != melsrc for first line in list
        // Other empty lines will be removed
        if(!srcLine.isValid() && !oldSrcLine.isValid() && oldSrc == melsrc)
            melIt = list().erase(melIt);
        else
            ++melIt;

        oldSrcLine = srcLine;
        oldSrc = melsrc;
    }
}

// Returns the iterator to the MergeLine after the split
MergeLineListImp::iterator MergeLineList::splitAtDiff3LineIdx(qint32 d3lLineIdx)
{
    MergeLineListImp::iterator i;
    for(i = mImp.begin(); i != mImp.end(); ++i)
    {
        if(i->getIndex() == d3lLineIdx)
        {
            // No split needed, this is the beginning of a MergeLine
            return i;
        }
        else if(i->getIndex() > d3lLineIdx)
        {
            // The split must be in the previous MergeLine
            --i;
            MergeLine& ml = *i;
            MergeLine newML;
            ml.split(newML, d3lLineIdx);
            ++i;
            return mImp.insert(i, newML);
        }
    }
    // The split must be in the previous MergeLine
    --i;
    MergeLine& ml = *i;
    MergeLine newML;
    ml.split(newML, d3lLineIdx);
    ++i;
    return mImp.insert(i, newML);
}
