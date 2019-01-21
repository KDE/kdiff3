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
#ifndef MERGEFILEINFO_H
#define MERGEFILEINFO_H

#include "fileaccess.h"
#include "diff.h"
#include "DirectoryInfo.h"

#include <QString>
#include <QSharedPointer>
//class DirectoryInfo;

enum e_MergeOperation
{
   eTitleId,
   eNoOperation,
   // Operations in sync mode (with only two directories):
   eCopyAToB, eCopyBToA, eDeleteA, eDeleteB, eDeleteAB, eMergeToA, eMergeToB, eMergeToAB,

   // Operations in merge mode (with two or three directories)
   eCopyAToDest, eCopyBToDest, eCopyCToDest, eDeleteFromDest, eMergeABCToDest,
   eMergeABToDest,
   eConflictingFileTypes, // Error
   eChangedAndDeleted,    // Error
   eConflictingAges       // Equal age but files are not!
};

enum e_Age { eNew, eMiddle, eOld, eNotThere, eAgeEnd };

enum e_OperationStatus
{
    eOpStatusNone,
    eOpStatusDone,
    eOpStatusError,
    eOpStatusSkipped,
    eOpStatusNotSaved,
    eOpStatusInProgress,
    eOpStatusToDo
};

class MergeFileInfos
{
  public:
    MergeFileInfos();
    ~MergeFileInfos();

    //bool operator>( const MergeFileInfos& );
    QString subPath() const;
    QString fileName() const;

    bool isDirA() const { return m_pFileInfoA ? m_pFileInfoA->isDir() : false; }
    bool isDirB() const { return m_pFileInfoB ? m_pFileInfoB->isDir() : false; }
    bool isDirC() const { return m_pFileInfoC ? m_pFileInfoC->isDir() : false; }
    bool isLinkA() const { return m_pFileInfoA ? m_pFileInfoA->isSymLink() : false; }
    bool isLinkB() const { return m_pFileInfoB ? m_pFileInfoB->isSymLink() : false; }
    bool isLinkC() const { return m_pFileInfoC ? m_pFileInfoC->isSymLink() : false; }
    bool existsInA() const { return m_pFileInfoA != nullptr; }
    bool existsInB() const { return m_pFileInfoB != nullptr; }
    bool existsInC() const { return m_pFileInfoC != nullptr; }

    bool conflictingFileTypes();

    void sort(Qt::SortOrder order);
    inline MergeFileInfos* parent() const { return m_pParent; }
    inline void setParent(MergeFileInfos* inParent) { m_pParent = inParent; }
    inline const QList<MergeFileInfos*>& children() const { return m_children; }
    inline void addChild(MergeFileInfos* child) { m_children.push_back(child); }
    inline void clear() { m_children.clear(); }

    FileAccess* getFileInfoA() const { return m_pFileInfoA; }
    FileAccess* getFileInfoB() const { return m_pFileInfoB; }
    FileAccess* getFileInfoC() const { return m_pFileInfoC; }

    void setFileInfoA(FileAccess* newInfo) { m_pFileInfoA = newInfo; }
    void setFileInfoB(FileAccess* newInfo) { m_pFileInfoB = newInfo; }
    void setFileInfoC(FileAccess* newInfo) { m_pFileInfoC = newInfo; }

    QString fullNameA() const;
    QString fullNameB() const;
    QString fullNameC() const;
    QString fullNameDest() const;

    inline QSharedPointer<DirectoryInfo> getDirectoryInfo() const { return m_dirInfo; }
    void setDirectoryInfo(const QSharedPointer<DirectoryInfo> &dirInfo) { m_dirInfo = dirInfo; }

    inline QString getDirNameA() const { return getDirectoryInfo()->dirA().prettyAbsPath(); }
    inline QString getDirNameB() const { return getDirectoryInfo()->dirB().prettyAbsPath(); }
    inline QString getDirNameC() const { return getDirectoryInfo()->dirC().prettyAbsPath(); }
    inline QString getDirNameDest() const { return getDirectoryInfo()->destDir().prettyAbsPath(); }

    inline const QSharedPointer<TotalDiffStatus>& diffStatus() const { return m_totalDiffStatus; }

    inline e_MergeOperation getOperation() const { return m_eMergeOperation; }
    inline void setOperation(const e_MergeOperation op) { m_eMergeOperation = op; }

    inline e_OperationStatus getOpStatus() const { return  m_eOpStatus; }
    inline void setOpStatus(const e_OperationStatus eOpStatus){ m_eOpStatus = eOpStatus; }
  private:
    MergeFileInfos* m_pParent;
    QList<MergeFileInfos*> m_children;

    FileAccess* m_pFileInfoA;
    FileAccess* m_pFileInfoB;
    FileAccess* m_pFileInfoC;

    QSharedPointer<DirectoryInfo> m_dirInfo;

    QSharedPointer<TotalDiffStatus> m_totalDiffStatus;

    e_MergeOperation m_eMergeOperation;
    e_OperationStatus m_eOpStatus;
  public:
    e_Age m_ageA;
    e_Age m_ageB;
    e_Age m_ageC;

    bool m_bOperationComplete;
    bool m_bSimOpComplete;

    bool m_bEqualAB;
    bool m_bEqualAC;
    bool m_bEqualBC;
    bool m_bConflictingAges; // Equal age but files are not!
};

class MfiCompare
{
    Qt::SortOrder mOrder;

  public:
    explicit MfiCompare(Qt::SortOrder order)
    {
        mOrder = order;
    }
    bool operator()(MergeFileInfos* pMFI1, MergeFileInfos* pMFI2)
    {
        bool bDir1 = pMFI1->isDirA() || pMFI1->isDirB() || pMFI1->isDirC();
        bool bDir2 = pMFI2->isDirA() || pMFI2->isDirB() || pMFI2->isDirC();
        if(bDir1 == bDir2) {
            if(mOrder == Qt::AscendingOrder)
            {
                return pMFI1->fileName().compare(pMFI2->fileName(), Qt::CaseInsensitive) < 0;
            }
            else
            {
                return pMFI1->fileName().compare(pMFI2->fileName(), Qt::CaseInsensitive) > 0;
            }
        }
        else
            return bDir1;
    }
};

#endif // !MERGEFILEINFO_H
