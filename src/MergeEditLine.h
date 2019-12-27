
/***************************************************************************
 *   Copyright (C) 2002-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MERGEEDITLINE_H
#include "diff.h"

#include <QString>
#include <QVector>

class MergeEditLine
{
  public:
    explicit MergeEditLine(const Diff3LineList::const_iterator& i, e_SrcSelector src = None)
    {
        m_id3l = i;
        m_src = src;
        m_bLineRemoved = false;
    }
    void setConflict()
    {
        m_src = None;
        m_bLineRemoved = false;
        m_str = QString();
    }
    bool isConflict() { return m_src == None && !m_bLineRemoved && m_str.isEmpty(); }
    void setRemoved(e_SrcSelector src = None)
    {
        m_src = src;
        m_bLineRemoved = true;
        m_str = QString();
    }
    bool isRemoved() const { return m_bLineRemoved; }
    bool isEditableText() { return !isConflict() && !isRemoved(); }
    void setString(const QString& s)
    {
        m_str = s;
        m_bLineRemoved = false;
        m_src = None;
    }
    QString getString(const QVector<LineData>* pLineDataA, const QVector<LineData>* pLineDataB, const QVector<LineData>* pLineDataC);
    bool isModified() { return !m_str.isEmpty() || (m_bLineRemoved && m_src == None); }

    void setSource(e_SrcSelector src, bool bLineRemoved)
    {
        m_src = src;
        m_bLineRemoved = bLineRemoved;
    }
    e_SrcSelector src() { return m_src; }
    Diff3LineList::const_iterator id3l() { return m_id3l; }
  private:
    Diff3LineList::const_iterator m_id3l;
    e_SrcSelector m_src; // 1, 2 or 3 for A, B or C respectively, or 0 when line is from neither source.
    QString m_str;       // String when modified by user or null-string when orig data is used.
    bool m_bLineRemoved;
};

class MergeEditLineList :public std::list<MergeEditLine>
{ // I want to know the size immediately!
  private:
    typedef std::list<MergeEditLine> BASE;

  public:
    typedef std::list<MergeEditLine>::iterator iterator;
    typedef std::list<MergeEditLine>::reverse_iterator reverse_iterator;
    typedef std::list<MergeEditLine>::const_iterator const_iterator;


    int size()
    {
        return (int)BASE::size();
    }
};

class MergeLine
{
  public:
    Diff3LineList::const_iterator id3l;
    LineIndex d3lLineIdx = -1;    // Needed to show the correct window pos.
    LineCount srcRangeLength = 0; // how many src-lines have this properties
    e_MergeDetails mergeDetails = eDefault;
    bool bConflict = false;
    bool bWhiteSpaceConflict = false;
    bool bDelta = false;
    e_SrcSelector srcSelect = None;
    MergeEditLineList mergeEditLineList;
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
        ml2.id3l = id3l;
        for(int i = 0; i < srcRangeLength; ++i)
            ++ml2.id3l;

        ml2.mergeEditLineList.clear();
        // Search for best place to splice
        for(MergeEditLineList::iterator i = mergeEditLineList.begin(); i != mergeEditLineList.end(); ++i)
        {
            if(i->id3l() == ml2.id3l)
            {
                ml2.mergeEditLineList.splice(ml2.mergeEditLineList.begin(), mergeEditLineList, i, mergeEditLineList.end());
                return;
            }
        }
        ml2.mergeEditLineList.push_back(MergeEditLine(ml2.id3l));
    }
    void join(MergeLine& ml2) // The caller must remove the ml2 from the m_mergeLineList after this call
    {
        srcRangeLength += ml2.srcRangeLength;
        ml2.mergeEditLineList.clear();
        mergeEditLineList.clear();
        mergeEditLineList.push_back(MergeEditLine(id3l)); // Create a simple conflict
        if(ml2.bConflict) bConflict = true;
        if(!ml2.bWhiteSpaceConflict) bWhiteSpaceConflict = false;
        if(ml2.bDelta) bDelta = true;
    }
};

typedef std::list<MergeLine> MergeLineList;

#endif
