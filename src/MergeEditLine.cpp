/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "MergeEditLine.h"
#include "LineRef.h"
#include "diff.h"

QString MergeEditLine::getString(const std::shared_ptr<LineDataVector> &pLineDataA, const std::shared_ptr<LineDataVector> &pLineDataB, const std::shared_ptr<LineDataVector> &pLineDataC) const
{
    //Triggered by resize event during early init. Ignore these calls.
    if((m_src == e_SrcSelector::A && pLineDataA->empty()) || (m_src == e_SrcSelector::B && pLineDataB->empty()) || (m_src == e_SrcSelector::C && pLineDataC->empty()))
        return QString();

    if(isRemoved())
    {
        return QString();
    }

    if(!isModified())
    {
        e_SrcSelector src = m_src;
        if(src == e_SrcSelector::None)
        {
            return QString();
        }
        const Diff3Line &d3l = *m_id3l;
        const LineData *pld = nullptr;
        Q_ASSERT(src == e_SrcSelector::A || src == e_SrcSelector::B || src == e_SrcSelector::C);

        if(src == e_SrcSelector::A && d3l.getLineA().isValid())
            pld = &(*pLineDataA)[d3l.getLineA()];
        else if(src == e_SrcSelector::B && d3l.getLineB().isValid())
            pld = &(*pLineDataB)[d3l.getLineB()];
        else if(src == e_SrcSelector::C && d3l.getLineC().isValid())
            pld = &(*pLineDataC)[d3l.getLineC()];

        //Not an error.
        if(pld == nullptr)
        {
            return QString();
        }

        return pld->getLine();
    }
    else
    {
        return m_str;
    }
    return QString();
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
            Q_ASSERT(true);
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
        Q_ASSERT(true);
}
/*
    Build a new MergeLineList from scratch using a Diff3LineList.
*/
void MergeLineList::buildFromDiff3(const Diff3LineList &diff3List, bool isThreeway)
{
    LineIndex lineIdx = 0;
    Diff3LineList::const_iterator it;
    for(const Diff3Line &d: diff3List)
    {
        MergeLine ml;
        bool bLineRemoved;
        ml.mergeOneLine(d, bLineRemoved, !isThreeway);
        ml.dectectWhiteSpaceConflict(d, isThreeway);

        ml.d3lLineIdx = lineIdx;
        ml.bDelta = ml.srcSelect != e_SrcSelector::A;
        ml.mId3l = it;
        ml.srcRangeLength = 1;

        MergeLine *back = mList.empty() ? nullptr : &mList.back();

        bool bSame = back != nullptr && ml.isSameKind(*back);
        if(bSame)
        {
            ++back->srcRangeLength;
            if(back->isWhiteSpaceConflict() && !ml.isWhiteSpaceConflict())
                back->bWhiteSpaceConflict = false;
        }
        else
        {
            mList.push_back(ml);
        }

        if(!ml.isConflict())
        {
            MergeLine &tmpBack = mList.back();
            MergeEditLine mel(ml.id3l());
            mel.setSource(ml.srcSelect, bLineRemoved);
            tmpBack.mergeEditLineList.push_back(mel);
        }
        else if(back == nullptr || !back->isConflict() || !bSame)
        {
            MergeLine &tmpBack = mList.back();
            MergeEditLine mel(ml.id3l());
            mel.setConflict();
            tmpBack.mergeEditLineList.push_back(mel);
        }

        ++lineIdx;
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
