/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "MergeFileInfos.h"
#include "DirectoryInfo.h"
#include "fileaccess.h"

#include <QString>

MergeFileInfos::MergeFileInfos()
{
    m_bEqualAB = false;
    m_bEqualAC = false;
    m_bEqualBC = false;
    m_pParent = nullptr;
    m_bOperationComplete = false;
    m_bSimOpComplete = false;
    m_eMergeOperation = eNoOperation;
    m_eOpStatus = eOpStatusNone;
    m_ageA = eNotThere;
    m_ageB = eNotThere;
    m_ageC = eNotThere;
    m_bConflictingAges = false;
    m_pFileInfoA = nullptr;
    m_pFileInfoB = nullptr;
    m_pFileInfoC = nullptr;
    m_dirInfo.clear();
}

MergeFileInfos::~MergeFileInfos()
{
    m_children.clear();
}

//bool operator>( const MergeFileInfos& );
QString MergeFileInfos::subPath(void) const
{
    if(m_pFileInfoA && m_pFileInfoA->exists())
        return m_pFileInfoA->fileRelPath();
    else if(m_pFileInfoB && m_pFileInfoB->exists())
        return m_pFileInfoB->fileRelPath();
    else if(m_pFileInfoC && m_pFileInfoC->exists())
        return m_pFileInfoC->fileRelPath();
    return QString("");
}

QString MergeFileInfos::fileName(void) const
{
    if(m_pFileInfoA && m_pFileInfoA->exists())
        return m_pFileInfoA->fileName();
    else if(m_pFileInfoB && m_pFileInfoB->exists())
        return m_pFileInfoB->fileName();
    else if(m_pFileInfoC && m_pFileInfoC->exists())
        return m_pFileInfoC->fileName();
    return QString("");
}

bool MergeFileInfos::conflictingFileTypes(void)
{
    // Now check if file/dir-types fit.
    if(isLinkA() || isLinkB() || isLinkC())
    {
        if((existsInA() && !isLinkA()) ||
           (existsInB() && !isLinkB()) ||
           (existsInC() && !isLinkC()))
        {
            return true;
        }
    }

    if(dirA() || dirB() || dirC())
    {
        if((existsInA() && !dirA()) ||
           (existsInB() && !dirB()) ||
           (existsInC() && !dirC()))
        {
            return true;
        }
    }
    return false;
}

QString MergeFileInfos::fullNameA(void) const
{
    if(existsInA())
        return getFileInfoA()->absoluteFilePath();

    return m_dirInfo->dirA().absoluteFilePath() + "/" + subPath();
}

QString MergeFileInfos::fullNameB(void) const
{
    if(existsInB())
        return getFileInfoB()->absoluteFilePath();

    return m_dirInfo->dirB().absoluteFilePath() + "/" + subPath();
}

QString MergeFileInfos::fullNameC(void) const
{
    if(existsInC())
        return getFileInfoC()->absoluteFilePath();

    return m_dirInfo->dirC().absoluteFilePath() + "/" + subPath();
}

void MergeFileInfos::sort(Qt::SortOrder order)
{
    std::sort(m_children.begin(), m_children.end(), MfiCompare(order));

    for(int i = 0; i < m_children.count(); ++i)
        m_children[i]->sort(order);
}

QString MergeFileInfos::fullNameDest(void) const
{
    if(m_dirInfo->destDir().prettyAbsPath() == m_dirInfo->dirC().prettyAbsPath())
        return fullNameC();
    else if(m_dirInfo->destDir().prettyAbsPath() == m_dirInfo->dirB().prettyAbsPath())
        return fullNameB();
    else
        return m_dirInfo->destDir().absoluteFilePath() + "/" + subPath();
}
