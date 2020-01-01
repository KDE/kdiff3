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
#include "directorymergewindow.h"
#include "fileaccess.h"
#include "progress.h"

#include <QString>

#include <KLocalizedString>

QSharedPointer<DirectoryInfo> MergeFileInfos::m_dirInfo;

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

void MergeFileInfos::updateParents()
{
    MergeFileInfos* current = parent();
    while(current != nullptr)
    {
        bool bChange = false;
        if(!isEqualAB() && current->isEqualAB())
        {
            current->m_bEqualAB = false;
            bChange = true;
        }
        if(!isEqualAC() && current->isEqualAC())
        {
            current->m_bEqualAC = false;
            bChange = true;
        }
        if(!isEqualBC() && current->isEqualBC())
        {
            current->m_bEqualBC = false;
            bChange = true;
        }

        if(bChange)
            current->updateAge();
        else
            break;

        current = current->parent();
    }
}
/*
    Check for directories or links marked as not equal and mark them equal.
*/
void MergeFileInfos::updateDirectoryOrLink()
{
    bool bChange = false;
    if(!isEqualAB() && isDirA() == isDirB() && isLinkA() == isLinkB())
    {
        m_bEqualAB = true;
        bChange = true;
    }
    if(!isEqualBC() && isDirC() == isDirB() && isLinkC() == isLinkB())
    {
        m_bEqualBC = true;
        bChange = true;
    }
    if(!isEqualAC() && isDirA() == isDirC() && isLinkA() == isLinkC())
    {
        m_bEqualAC = true;
        bChange = true;
    }

    if(bChange)
        updateAge();
}

QString MergeFileInfos::subPath() const
{
    if(m_pFileInfoA != nullptr && m_pFileInfoA->exists())
        return m_pFileInfoA->fileRelPath();
    else if(m_pFileInfoB != nullptr && m_pFileInfoB->exists())
        return m_pFileInfoB->fileRelPath();
    else if(m_pFileInfoC != nullptr && m_pFileInfoC->exists())
        return m_pFileInfoC->fileRelPath();
    return QString("");
}

QString MergeFileInfos::fileName() const
{
    if(m_pFileInfoA != nullptr && m_pFileInfoA->exists())
        return m_pFileInfoA->fileName();
    else if(m_pFileInfoB != nullptr && m_pFileInfoB->exists())
        return m_pFileInfoB->fileName();
    else if(m_pFileInfoC != nullptr && m_pFileInfoC->exists())
        return m_pFileInfoC->fileName();
    return QString("");
}

bool MergeFileInfos::conflictingFileTypes()
{
    if((m_pFileInfoA != nullptr && !m_pFileInfoA->isNormal()) || (m_pFileInfoB != nullptr && !m_pFileInfoB->isNormal()) || (m_pFileInfoC != nullptr && !m_pFileInfoC->isNormal()))
        return true;
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

    if(isDirA() || isDirB() || isDirC())
    {
        if((existsInA() && !isDirA()) ||
           (existsInB() && !isDirB()) ||
           (existsInC() && !isDirC()))
        {
            return true;
        }
    }
    return false;
}

QString MergeFileInfos::fullNameA() const
{
    if(existsInA())
        return getFileInfoA()->absoluteFilePath();

    return m_dirInfo->dirA().absoluteFilePath() + '/' + subPath();
}

QString MergeFileInfos::fullNameB() const
{
    if(existsInB())
        return getFileInfoB()->absoluteFilePath();

    return m_dirInfo->dirB().absoluteFilePath() + '/' + subPath();
}

QString MergeFileInfos::fullNameC() const
{
    if(existsInC())
        return getFileInfoC()->absoluteFilePath();

    return m_dirInfo->dirC().absoluteFilePath() + '/' + subPath();
}

void MergeFileInfos::sort(Qt::SortOrder order)
{
    std::sort(m_children.begin(), m_children.end(), MfiCompare(order));

    for(int i = 0; i < m_children.count(); ++i)
        m_children[i]->sort(order);
}

QString MergeFileInfos::fullNameDest() const
{
    if(m_dirInfo->destDir().prettyAbsPath() == m_dirInfo->dirC().prettyAbsPath())
        return fullNameC();
    else if(m_dirInfo->destDir().prettyAbsPath() == m_dirInfo->dirB().prettyAbsPath())
        return fullNameB();
    else
        return m_dirInfo->destDir().absoluteFilePath() + '/' + subPath();
}

bool MergeFileInfos::compareFilesAndCalcAges(QStringList& errors, QSharedPointer<Options> const pOptions, DirectoryMergeWindow* pDMW)
{
    std::map<QDateTime, int> dateMap;

    if(existsInA())
    {
        dateMap[getFileInfoA()->lastModified()] = 0;
    }
    if(existsInB())
    {
        dateMap[getFileInfoB()->lastModified()] = 1;
    }
    if(existsInC())
    {
        dateMap[getFileInfoC()->lastModified()] = 2;
    }

    if(pOptions->m_bDmFullAnalysis)
    {
        if((existsInA() && isDirA()) || (existsInB() && isDirB()) || (existsInC() && isDirC()))
        {
            // If any input is a directory, don't start any comparison.
            m_bEqualAB = existsInA() && existsInB();
            m_bEqualAC = existsInA() && existsInC();
            m_bEqualBC = existsInB() && existsInC();
        }
        else
        {
            Q_EMIT pDMW->startDiffMerge(
                existsInA() ? getFileInfoA()->absoluteFilePath() : QString(""),
                existsInB() ? getFileInfoB()->absoluteFilePath() : QString(""),
                existsInC() ? getFileInfoC()->absoluteFilePath() : QString(""),
                "",
                "", "", "", &diffStatus());
            int nofNonwhiteConflicts = diffStatus().getNonWhitespaceConflicts();

            if(pOptions->m_bDmWhiteSpaceEqual && nofNonwhiteConflicts == 0)
            {
                m_bEqualAB = existsInA() && existsInB();
                m_bEqualAC = existsInA() && existsInC();
                m_bEqualBC = existsInB() && existsInC();
            }
            else
            {
                m_bEqualAB = diffStatus().isBinaryEqualAB();
                m_bEqualBC = diffStatus().isBinaryEqualBC();
                m_bEqualAC = diffStatus().isBinaryEqualAC();
            }
        }
    }
    else
    {
        bool bError = false;
        QString eqStatus;
        if(existsInA() && existsInB())
        {
            if(isDirA())
                m_bEqualAB = true;
            else
                m_bEqualAB = fastFileComparison(*getFileInfoA(), *getFileInfoB(), bError, eqStatus, pOptions);
        }
        if(existsInA() && existsInC())
        {
            if(isDirA())
                m_bEqualAC = true;
            else
                m_bEqualAC = fastFileComparison(*getFileInfoA(), *getFileInfoC(), bError, eqStatus, pOptions);
        }
        if(existsInB() && existsInC())
        {
            if(m_bEqualAB && m_bEqualAC)
                m_bEqualBC = true;
            else
            {
                if(isDirB())
                    m_bEqualBC = true;
                else
                    m_bEqualBC = fastFileComparison(*getFileInfoB(), *getFileInfoC(), bError, eqStatus, pOptions);
            }
        }
        if(bError)
        {
            //Limit size of error list in memmory.
            if(errors.size() < 30)
                errors.append(eqStatus);
            return false;
        }
    }

    if(isLinkA() != isLinkB()) m_bEqualAB = false;
    if(isLinkA() != isLinkC()) m_bEqualAC = false;
    if(isLinkB() != isLinkC()) m_bEqualBC = false;

    if(isDirA() != isDirB()) m_bEqualAB = false;
    if(isDirA() != isDirC()) m_bEqualAC = false;
    if(isDirB() != isDirC()) m_bEqualBC = false;

    Q_ASSERT(eNew == 0 && eMiddle == 1 && eOld == 2);

    // The map automatically sorts the keys.
    int age = eNew;
    std::map<QDateTime, int>::reverse_iterator i;
    for(i = dateMap.rbegin(); i != dateMap.rend(); ++i)
    {
        int n = i->second;
        if(n == 0 && getAgeA() == eNotThere)
        {
            setAgeA((e_Age)age);
            ++age;
            if(m_bEqualAB)
            {
                setAgeB(getAgeA());
                ++age;
            }
            if(m_bEqualAC)
            {
                setAgeC(getAgeA());
                ++age;
            }
        }
        else if(n == 1 && getAgeB() == eNotThere)
        {
            setAgeB((e_Age)age);
            ++age;
            if(m_bEqualAB)
            {
                setAgeA(getAgeB());
                ++age;
            }
            if(m_bEqualBC)
            {
                setAgeC(getAgeB());
                ++age;
            }
        }
        else if(n == 2 && getAgeC() == eNotThere)
        {
            setAgeC((e_Age)age);
            ++age;
            if(m_bEqualAC)
            {
                setAgeA(getAgeC());
                ++age;
            }
            if(m_bEqualBC)
            {
                setAgeB(getAgeC());
                ++age;
            }
        }
    }

    // The checks below are necessary when the dates of the file are equal but the
    // files are not. One wouldn't expect this to happen, yet it happens sometimes.
    if(existsInC() && getAgeC() == eNotThere)
    {
        setAgeC((e_Age)age);
        ++age;
        m_bConflictingAges = true;
    }
    if(existsInB() && getAgeB() == eNotThere)
    {
        setAgeB((e_Age)age);
        ++age;
        m_bConflictingAges = true;
    }
    if(existsInA() && getAgeA() == eNotThere)
    {
        setAgeA((e_Age)age);
        ++age;
        m_bConflictingAges = true;
    }

    if(getAgeA() != eOld && getAgeB() != eOld && getAgeC() != eOld)
    {
        if(getAgeA() == eMiddle) setAgeA(eOld);
        if(getAgeB() == eMiddle) setAgeB(eOld);
        if(getAgeC() == eMiddle) setAgeC(eOld);
    }

    return true;
}

bool MergeFileInfos::fastFileComparison(
    FileAccess& fi1, FileAccess& fi2,
    bool& bError, QString& status, QSharedPointer<Options> const pOptions)
{
    ProgressProxy pp;
    bool bEqual = false;

    status = "";
    bError = true;

    if(fi1.isNormal() != fi2.isNormal())
    {
        status = i18n("Unable to compare non-normal file with normal file.");
        return false;
    }

    if(!fi1.isNormal())
    {
        bError = false;
        return false;
    }

    if(!pOptions->m_bDmFollowFileLinks)
    {
        if(fi1.isSymLink() != fi2.isSymLink())
        {
            status = i18n("Mix of links and normal files.");
            return bEqual;
        }
        else if(fi1.isSymLink() && fi2.isSymLink())
        {
            bError = false;
            bEqual = fi1.readLink() == fi2.readLink();
            status = i18n("Link: ");
            return bEqual;
        }
    }

    if(fi1.size() != fi2.size())
    {
        bError = false;
        bEqual = false;
        status = i18n("Size. ");
        return bEqual;
    }
    else if(pOptions->m_bDmTrustSize)
    {
        bEqual = true;
        bError = false;
        return bEqual;
    }

    if(pOptions->m_bDmTrustDate)
    {
        bEqual = (fi1.lastModified() == fi2.lastModified() && fi1.size() == fi2.size());
        bError = false;
        status = i18n("Date & Size: ");
        return bEqual;
    }

    if(pOptions->m_bDmTrustDateFallbackToBinary)
    {
        bEqual = (fi1.lastModified() == fi2.lastModified() && fi1.size() == fi2.size());
        if(bEqual)
        {
            bError = false;
            status = i18n("Date & Size: ");
            return bEqual;
        }
    }

    std::vector<char> buf1(100000);
    std::vector<char> buf2(buf1.size());

    if(!fi1.open(QIODevice::ReadOnly))
    {
        status = fi1.errorString();
        return bEqual;
    }

    if(!fi2.open(QIODevice::ReadOnly))
    {
        fi1.close();
        status = fi2.errorString();
        return bEqual;
    }

    pp.setInformation(i18n("Comparing file..."), 0, false);
    typedef qint64 t_FileSize;
    t_FileSize fullSize = fi1.size();
    t_FileSize sizeLeft = fullSize;

    pp.setMaxNofSteps(fullSize / buf1.size());

    while(sizeLeft > 0 && !pp.wasCancelled())
    {
        qint64 len = std::min(sizeLeft, (t_FileSize)buf1.size());
        if(len != fi1.read(&buf1[0], len))
        {
            status = fi1.errorString();
            fi1.close();
            fi2.close();
            return bEqual;
        }

        if(len != fi2.read(&buf2[0], len))
        {
            status = fi2.errorString();
            fi1.close();
            fi2.close();
            return bEqual;
        }

        if(memcmp(&buf1[0], &buf2[0], len) != 0)
        {
            bError = false;
            fi1.close();
            fi2.close();
            return bEqual;
        }
        sizeLeft -= len;
        //pp.setCurrent(double(fullSize-sizeLeft)/fullSize, false );
        pp.step();
    }
    fi1.close();
    fi2.close();

    // If the program really arrives here, then the files are really equal.
    bError = false;
    bEqual = true;
    return bEqual;
}

void MergeFileInfos::updateAge()
{
    if(isDirA() || isDirB() || isDirC())
    {
        setAgeA(eNotThere);
        setAgeB(eNotThere);
        setAgeC(eNotThere);
        e_Age age = eNew;
        if(existsInC())
        {
            setAgeC(age);
            if(m_bEqualAC) setAgeA(age);
            if(m_bEqualBC) setAgeB(age);
            age = eMiddle;
        }
        if(existsInB() && getAgeB() == eNotThere)
        {
            setAgeB(age);
            if(m_bEqualAB) setAgeA(age);
            age = eOld;
        }
        if(existsInA() && getAgeA() == eNotThere)
        {
            setAgeA(age);
        }
        if(getAgeA() != eOld && getAgeB() != eOld && getAgeC() != eOld)
        {
            if(getAgeA() == eMiddle) setAgeA(eOld);
            if(getAgeB() == eMiddle) setAgeB(eOld);
            if(getAgeC() == eMiddle) setAgeC(eOld);
        }
    }
}

QTextStream& operator<<(QTextStream& ts, MergeFileInfos& mfi)
{
    ts << "{\n";
    ValueMap vm;
    vm.writeEntry("SubPath", mfi.subPath());
    vm.writeEntry("ExistsInA", mfi.existsInA());
    vm.writeEntry("ExistsInB", mfi.existsInB());
    vm.writeEntry("ExistsInC", mfi.existsInC());
    vm.writeEntry("EqualAB", mfi.isEqualAB());
    vm.writeEntry("EqualAC", mfi.isEqualAC());
    vm.writeEntry("EqualBC", mfi.isEqualBC());

    vm.writeEntry("MergeOperation", (int)mfi.getOperation());
    vm.writeEntry("DirA", mfi.isDirA());
    vm.writeEntry("DirB", mfi.isDirB());
    vm.writeEntry("DirC", mfi.isDirC());
    vm.writeEntry("LinkA", mfi.isLinkA());
    vm.writeEntry("LinkB", mfi.isLinkB());
    vm.writeEntry("LinkC", mfi.isLinkC());
    vm.writeEntry("OperationComplete", !mfi.isOperationRunning());

    vm.writeEntry("AgeA", (int)mfi.getAgeA());
    vm.writeEntry("AgeB", (int)mfi.getAgeB());
    vm.writeEntry("AgeC", (int)mfi.getAgeC());
    vm.writeEntry("ConflictingAges", mfi.conflictingAges()); // Equal age but files are not!

    vm.save(ts);

    ts << "}\n";

    return ts;
}
