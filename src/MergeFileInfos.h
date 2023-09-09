/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on
#ifndef MERGEFILEINFO_H
#define MERGEFILEINFO_H

#include "DirectoryInfo.h"
#include "diff.h"
#include "fileaccess.h"

#include <QSharedPointer>
#include <QString>

enum e_MergeOperation
{
    eTitleId,
    eNoOperation,
    // Operations in sync mode (with only two directories):
    eCopyAToB,
    eCopyBToA,
    eDeleteA,
    eDeleteB,
    eDeleteAB,
    eMergeToA,
    eMergeToB,
    eMergeToAB,

    // Operations in merge mode (with two or three directories)
    eCopyAToDest,
    eCopyBToDest,
    eCopyCToDest,
    eDeleteFromDest,
    eMergeABCToDest,
    eMergeABToDest,
    eConflictingFileTypes, // Error
    eChangedAndDeleted,    // Error
    eConflictingAges       // Equal age but files are not!
};

enum e_Age
{
    eNew,
    eMiddle,
    eOld,
    eNotThere,
    eAgeEnd
};

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

class DirectoryMergeWindow;

class MergeFileInfos
{
  public:
    MergeFileInfos();
    ~MergeFileInfos();

    Q_REQUIRED_RESULT QString subPath() const;
    Q_REQUIRED_RESULT QString fileName() const;

    Q_REQUIRED_RESULT bool isDirA() const { return m_pFileInfoA != nullptr ? m_pFileInfoA->isDir() : false; }
    Q_REQUIRED_RESULT bool isDirB() const { return m_pFileInfoB != nullptr ? m_pFileInfoB->isDir() : false; }
    Q_REQUIRED_RESULT bool isDirC() const { return m_pFileInfoC != nullptr ? m_pFileInfoC->isDir() : false; }
    Q_REQUIRED_RESULT bool hasDir() const { return isDirA() || isDirB() || isDirC(); }

    Q_REQUIRED_RESULT bool isLinkA() const { return m_pFileInfoA != nullptr ? m_pFileInfoA->isSymLink() : false; }
    Q_REQUIRED_RESULT bool isLinkB() const { return m_pFileInfoB != nullptr ? m_pFileInfoB->isSymLink() : false; }
    Q_REQUIRED_RESULT bool isLinkC() const { return m_pFileInfoC != nullptr ? m_pFileInfoC->isSymLink() : false; }
    Q_REQUIRED_RESULT bool hasLink() const { return isLinkA() || isLinkB() || isLinkC(); }

    Q_REQUIRED_RESULT bool existsInA() const { return m_pFileInfoA != nullptr; }
    Q_REQUIRED_RESULT bool existsInB() const { return m_pFileInfoB != nullptr; }
    Q_REQUIRED_RESULT bool existsInC() const { return m_pFileInfoC != nullptr; }

    Q_REQUIRED_RESULT bool conflictingFileTypes() const;

    void sort(Qt::SortOrder order);
    Q_REQUIRED_RESULT inline MergeFileInfos* parent() const { return m_pParent; }
    inline void setParent(MergeFileInfos* inParent) { m_pParent = inParent; }
    Q_REQUIRED_RESULT inline const QList<MergeFileInfos*>& children() const { return m_children; }
    inline void addChild(MergeFileInfos* child) { m_children.push_back(child); }
    inline void clear() { m_children.clear(); }

    Q_REQUIRED_RESULT FileAccess* getFileInfoA() const { return m_pFileInfoA; }
    Q_REQUIRED_RESULT FileAccess* getFileInfoB() const { return m_pFileInfoB; }
    Q_REQUIRED_RESULT FileAccess* getFileInfoC() const { return m_pFileInfoC; }

    void setFileInfoA(FileAccess* newInfo) { m_pFileInfoA = newInfo; }
    void setFileInfoB(FileAccess* newInfo) { m_pFileInfoB = newInfo; }
    void setFileInfoC(FileAccess* newInfo) { m_pFileInfoC = newInfo; }

    Q_REQUIRED_RESULT QString fullNameA() const;
    Q_REQUIRED_RESULT QString fullNameB() const;
    Q_REQUIRED_RESULT QString fullNameC() const;
    Q_REQUIRED_RESULT QString fullNameDest() const;

    Q_REQUIRED_RESULT inline QString getDirNameA() const { return gDirInfo->dirA().prettyAbsPath(); }
    Q_REQUIRED_RESULT inline QString getDirNameB() const { return gDirInfo->dirB().prettyAbsPath(); }
    Q_REQUIRED_RESULT inline QString getDirNameC() const { return gDirInfo->dirC().prettyAbsPath(); }
    Q_REQUIRED_RESULT inline QString getDirNameDest() const { return gDirInfo->destDir().prettyAbsPath(); }

    Q_REQUIRED_RESULT inline TotalDiffStatus& diffStatus() { return m_totalDiffStatus; }

    Q_REQUIRED_RESULT inline e_MergeOperation getOperation() const { return m_eMergeOperation; }
    inline void setOperation(const e_MergeOperation op) { m_eMergeOperation = op; }

    Q_REQUIRED_RESULT inline e_OperationStatus getOpStatus() const { return m_eOpStatus; }
    inline void setOpStatus(const e_OperationStatus eOpStatus) { m_eOpStatus = eOpStatus; }

    Q_REQUIRED_RESULT inline e_Age getAgeA() const { return m_ageA; }
    Q_REQUIRED_RESULT inline e_Age getAgeB() const { return m_ageB; }
    Q_REQUIRED_RESULT inline e_Age getAgeC() const { return m_ageC; }

    Q_REQUIRED_RESULT inline bool isEqualAB() const { return m_bEqualAB; }
    Q_REQUIRED_RESULT inline bool isEqualAC() const { return m_bEqualAC; }
    Q_REQUIRED_RESULT inline bool isEqualBC() const { return m_bEqualBC; }
    bool compareFilesAndCalcAges(QStringList& errors, QSharedPointer<Options> const& pOptions, DirectoryMergeWindow* pDMW);

    void updateAge();

    void updateParents();

    void updateDirectoryOrLink();
    inline void startSimOp() { m_bSimOpComplete = false; }
    Q_REQUIRED_RESULT inline bool isSimOpRunning() const { return !m_bOperationComplete; }
    inline void endSimOp() { m_bSimOpComplete = true; }

    inline void startOperation() { m_bOperationComplete = false; };
    Q_REQUIRED_RESULT inline bool isOperationRunning() const { return !m_bOperationComplete; }
    inline void endOperation() { m_bOperationComplete = true; };

    Q_REQUIRED_RESULT inline bool isThreeWay() const
    {
        if(gDirInfo == nullptr) return false;
        return gDirInfo->dirC().isValid();
    }
    Q_REQUIRED_RESULT inline bool existsEveryWhere() const { return existsInA() && existsInB() && (existsInC() || !isThreeWay()); }

    Q_REQUIRED_RESULT inline qint32 existsCount() const { return (existsInA() ? 1 : 0) + (existsInB() ? 1 : 0) + (existsInC() ? 1 : 0); }

    Q_REQUIRED_RESULT inline bool onlyInA() const { return existsInA() && !existsInB() && !existsInC(); }
    Q_REQUIRED_RESULT inline bool onlyInB() const { return !existsInA() && existsInB() && !existsInC(); }
    Q_REQUIRED_RESULT inline bool onlyInC() const { return !existsInA() && !existsInB() && existsInC(); }

    Q_REQUIRED_RESULT bool conflictingAges() const { return m_bConflictingAges; }

  private:
    bool fastFileComparison(FileAccess& fi1, FileAccess& fi2, bool& bError, QString& status, const QSharedPointer<const Options>& pOptions);
    inline void setAgeA(const e_Age inAge) { m_ageA = inAge; }
    inline void setAgeB(const e_Age inAge) { m_ageB = inAge; }
    inline void setAgeC(const e_Age inAge) { m_ageC = inAge; }

    MergeFileInfos* m_pParent = nullptr;
    QList<MergeFileInfos*> m_children;

    FileAccess* m_pFileInfoA = nullptr;
    FileAccess* m_pFileInfoB = nullptr;
    FileAccess* m_pFileInfoC = nullptr;

    TotalDiffStatus m_totalDiffStatus;

    e_MergeOperation m_eMergeOperation = eNoOperation;
    e_OperationStatus m_eOpStatus = eOpStatusNone;
    e_Age m_ageA = eNotThere;
    e_Age m_ageB = eNotThere;
    e_Age m_ageC = eNotThere;

    bool m_bOperationComplete = false;
    bool m_bSimOpComplete = false;

    bool m_bEqualAB = false;
    bool m_bEqualAC = false;
    bool m_bEqualBC = false;
    bool m_bConflictingAges = false; // Equal age but files are not!
};

QTextStream& operator<<(QTextStream& ts, MergeFileInfos& mfi);

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
        if(bDir1 == bDir2)
        {
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
