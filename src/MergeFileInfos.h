// clang-format off
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

    [[nodiscard]] QString subPath() const;
    [[nodiscard]] QString fileName() const;

    [[nodiscard]] bool isDirA() const { return m_pFileInfoA != nullptr ? m_pFileInfoA->isDir() : false; }
    [[nodiscard]] bool isDirB() const { return m_pFileInfoB != nullptr ? m_pFileInfoB->isDir() : false; }
    [[nodiscard]] bool isDirC() const { return m_pFileInfoC != nullptr ? m_pFileInfoC->isDir() : false; }
    [[nodiscard]] bool hasDir() const { return isDirA() || isDirB() || isDirC(); }

    [[nodiscard]] bool isLinkA() const { return m_pFileInfoA != nullptr ? m_pFileInfoA->isSymLink() : false; }
    [[nodiscard]] bool isLinkB() const { return m_pFileInfoB != nullptr ? m_pFileInfoB->isSymLink() : false; }
    [[nodiscard]] bool isLinkC() const { return m_pFileInfoC != nullptr ? m_pFileInfoC->isSymLink() : false; }
    [[nodiscard]] bool hasLink() const { return isLinkA() || isLinkB() || isLinkC(); }

    [[nodiscard]] bool existsInA() const { return m_pFileInfoA != nullptr; }
    [[nodiscard]] bool existsInB() const { return m_pFileInfoB != nullptr; }
    [[nodiscard]] bool existsInC() const { return m_pFileInfoC != nullptr; }

    [[nodiscard]] bool conflictingFileTypes() const;

    void sort(Qt::SortOrder order);
    [[nodiscard]] MergeFileInfos* parent() const { return m_pParent; }
    void setParent(MergeFileInfos* inParent) { m_pParent = inParent; }
    [[nodiscard]] const QList<MergeFileInfos*>& children() const { return m_children; }
    void addChild(MergeFileInfos* child) { m_children.push_back(child); }
    void clear() { m_children.clear(); }

    [[nodiscard]] FileAccess* getFileInfoA() const { return m_pFileInfoA; }
    [[nodiscard]] FileAccess* getFileInfoB() const { return m_pFileInfoB; }
    [[nodiscard]] FileAccess* getFileInfoC() const { return m_pFileInfoC; }

    void setFileInfoA(FileAccess* newInfo) { m_pFileInfoA = newInfo; }
    void setFileInfoB(FileAccess* newInfo) { m_pFileInfoB = newInfo; }
    void setFileInfoC(FileAccess* newInfo) { m_pFileInfoC = newInfo; }

    [[nodiscard]] QString fullNameA() const;
    [[nodiscard]] QString fullNameB() const;
    [[nodiscard]] QString fullNameC() const;
    [[nodiscard]] QString fullNameDest() const;

    [[nodiscard]] QString getDirNameA() const { return gDirInfo->dirA().prettyAbsPath(); }
    [[nodiscard]] QString getDirNameB() const { return gDirInfo->dirB().prettyAbsPath(); }
    [[nodiscard]] QString getDirNameC() const { return gDirInfo->dirC().prettyAbsPath(); }
    [[nodiscard]] QString getDirNameDest() const { return gDirInfo->destDir().prettyAbsPath(); }

    [[nodiscard]] TotalDiffStatus& diffStatus() { return m_totalDiffStatus; }

    [[nodiscard]] e_MergeOperation getOperation() const { return m_eMergeOperation; }
    void setOperation(const e_MergeOperation op) { m_eMergeOperation = op; }

    [[nodiscard]] e_OperationStatus getOpStatus() const { return m_eOpStatus; }
    void setOpStatus(const e_OperationStatus eOpStatus) { m_eOpStatus = eOpStatus; }

    [[nodiscard]] e_Age getAgeA() const { return m_ageA; }
    [[nodiscard]] e_Age getAgeB() const { return m_ageB; }
    [[nodiscard]] e_Age getAgeC() const { return m_ageC; }

    [[nodiscard]] bool isEqualAB() const { return m_bEqualAB; }
    [[nodiscard]] bool isEqualAC() const { return m_bEqualAC; }
    [[nodiscard]] bool isEqualBC() const { return m_bEqualBC; }
    bool compareFilesAndCalcAges(QStringList& errors, DirectoryMergeWindow* pDMW);

    void updateAge();

    void updateParents();

    void updateDirectoryOrLink();
    void startSimOp() { m_bSimOpComplete = false; }
    [[nodiscard]] bool isSimOpRunning() const { return !m_bOperationComplete; }
    void endSimOp() { m_bSimOpComplete = true; }

    void startOperation() { m_bOperationComplete = false; };
    [[nodiscard]] bool isOperationRunning() const { return !m_bOperationComplete; }
    void endOperation() { m_bOperationComplete = true; };

    [[nodiscard]] static bool isThreeWay()
    {
        if(gDirInfo == nullptr) return false;
        return gDirInfo->dirC().isValid();
    }
    [[nodiscard]] bool existsEveryWhere() const { return existsInA() && existsInB() && (existsInC() || !isThreeWay()); }

    [[nodiscard]] qint32 existsCount() const { return (existsInA() ? 1 : 0) + (existsInB() ? 1 : 0) + (existsInC() ? 1 : 0); }

    [[nodiscard]] bool onlyInA() const { return existsInA() && !existsInB() && !existsInC(); }
    [[nodiscard]] bool onlyInB() const { return !existsInA() && existsInB() && !existsInC(); }
    [[nodiscard]] bool onlyInC() const { return !existsInA() && !existsInB() && existsInC(); }

    [[nodiscard]] bool conflictingAges() const { return m_bConflictingAges; }

  private:
    [[nodiscard]] e_Age nextAgeValue(e_Age age)
    {
        switch(age)
        {
            case eNew:
                return eMiddle;
            case eMiddle:
                return eOld;
            case eOld:
                return eNotThere;
            case eNotThere:
                return eAgeEnd;
            case eAgeEnd:
                assert(false);
        }
        return age;
    }

    bool fastFileComparison(FileAccess& fi1, FileAccess& fi2, bool& bError, QString& status);
    void setAgeA(const e_Age inAge) { m_ageA = inAge; }
    void setAgeB(const e_Age inAge) { m_ageB = inAge; }
    void setAgeC(const e_Age inAge) { m_ageC = inAge; }

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
