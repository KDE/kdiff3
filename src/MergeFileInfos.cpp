// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include "MergeFileInfos.h"

#include "DirectoryInfo.h"
#include "directorymergewindow.h"
#include "fileaccess.h"
#include "Logging.h"
#include "progress.h"

#include <map>
#include <vector>

#include <QString>

#include <KLocalizedString>

MergeFileInfos::MergeFileInfos() = default;

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

bool MergeFileInfos::conflictingFileTypes() const
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

    return gDirInfo->dirA().absoluteFilePath() + '/' + subPath();
}

QString MergeFileInfos::fullNameB() const
{
    if(existsInB())
        return getFileInfoB()->absoluteFilePath();

    return gDirInfo->dirB().absoluteFilePath() + '/' + subPath();
}

QString MergeFileInfos::fullNameC() const
{
    if(existsInC())
        return getFileInfoC()->absoluteFilePath();

    return gDirInfo->dirC().absoluteFilePath() + '/' + subPath();
}

void MergeFileInfos::sort(Qt::SortOrder order)
{
    std::sort(m_children.begin(), m_children.end(), MfiCompare(order));

    for(qint32 i = 0; i < m_children.count(); ++i)
        m_children[i]->sort(order);
}

QString MergeFileInfos::fullNameDest() const
{
    if(gDirInfo->destDir().prettyAbsPath() == gDirInfo->dirC().prettyAbsPath())
        return fullNameC();
    else if(gDirInfo->destDir().prettyAbsPath() == gDirInfo->dirB().prettyAbsPath())
        return fullNameB();
    else
        return gDirInfo->destDir().absoluteFilePath() + '/' + subPath();
}

bool MergeFileInfos::compareFilesAndCalcAges(QStringList& errors, QSharedPointer<Options> const &pOptions, DirectoryMergeWindow* pDMW)
{
    enum class FilesFound
    {
        aOnly,
        bAndA,
        all
    };

    std::map<QDateTime, FilesFound> dateMap;

    if(existsInA())
    {
        dateMap[getFileInfoA()->lastModified()] = FilesFound::aOnly;
    }
    if(existsInB())
    {
        dateMap[getFileInfoB()->lastModified()] = FilesFound::bAndA;
    }
    if(existsInC())
    {
        dateMap[getFileInfoC()->lastModified()] = FilesFound::all;
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
            Q_EMIT pDMW->startDiffMerge(errors,
                existsInA() ? getFileInfoA()->absoluteFilePath() : QString(""),
                existsInB() ? getFileInfoB()->absoluteFilePath() : QString(""),
                existsInC() ? getFileInfoC()->absoluteFilePath() : QString(""),
                "",
                "", "", "", &diffStatus());
            qint32 nofNonwhiteConflicts = diffStatus().getNonWhitespaceConflicts();

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

            //Limit size of error list in memory.
            if(errors.size() >= 30)
            {
                //Bail out something is very wrong.
                return false;
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
            if((m_bEqualAB && m_bEqualAC) || isDirB())
                m_bEqualBC = true;
            else
            {
                m_bEqualBC = fastFileComparison(*getFileInfoB(), *getFileInfoC(), bError, eqStatus, pOptions);
            }
        }
        if(bError)
        {
            //Limit size of error list in memory.
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

    assert(eNew == 0 && eMiddle == 1 && eOld == 2);

    // The map automatically sorts the keys.
    qint32 age = eNew;
    std::map<QDateTime, FilesFound>::reverse_iterator i;
    for(i = dateMap.rbegin(); i != dateMap.rend(); ++i)
    {
        FilesFound n = i->second;

        if(n == FilesFound::aOnly && getAgeA() == eNotThere)
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
        else if(n == FilesFound::bAndA && getAgeB() == eNotThere)
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
        else if(n == FilesFound::all && getAgeC() == eNotThere)
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
    bool& bError, QString& status, const QSharedPointer<const Options> &pOptions)
{
    ProgressScope pp;
    bool bEqual = false;

    status = "";
    bError = true;

    qCDebug(kdiffMergeFileInfo) << "Entering MergeFileInfos::fastFileComparison";
    if(fi1.isNormal() != fi2.isNormal())
    {
        qCDebug(kdiffMergeFileInfo) << "Have: \'" << fi2.fileName() << "\' , isNormal = " << fi2.isNormal();

        status = i18n("Unable to compare non-normal file with normal file.");
        return false;
    }

    if(!fi1.isNormal())
    {
        qCInfo(kdiffMergeFileInfo) << "Skipping not a normal file.";
        bError = false;
        return false;
    }

    if(!pOptions->m_bDmFollowFileLinks)
    {
        qCInfo(kdiffMergeFileInfo) << "Have: \'" << fi2.fileName() << "\' , isSymLink = " << fi2.isSymLink();
        /*
            "git difftool --dir-diff"
            sets up directory comparsions with symlinks to the current work tree being compared with real files.

            m_bAllowMismatch is a compatibility work around.
        */
        if(fi1.isSymLink() != fi2.isSymLink())
        {
            qCDebug(kdiffMergeFileInfo) << "Rejecting comparison of link to file. With the same relitive path.";
            status = i18n("Mix of links and normal files.");
            return bEqual;
        }
        else if(fi1.isSymLink() && fi2.isSymLink())
        {
            qCDebug(kdiffMergeFileInfo) << "Comparison of link to link. OK.";
            bError = false;
            bEqual = fi1.readLink() == fi2.readLink();
            status = i18n("Link: ");
            return bEqual;
        }
    }

    if(fi1.size() != fi2.size())
    {
        qCInfo(kdiffMergeFileInfo) << "Sizes differ.";
        bError = false;
        bEqual = false;
        status = i18n("Size. ");
        return bEqual;
    }
    else if(pOptions->m_bDmTrustSize)
    {
        qCInfo(kdiffMergeFileInfo) << "Same size. Trusting result.";

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
    qCInfo(kdiffMergeFileInfo) << "Comparing files...";
    ProgressProxy::setInformation(i18n("Comparing file..."), 0, false);
    typedef qint64 t_FileSize;
    t_FileSize fullSize = fi1.size();
    t_FileSize sizeLeft = fullSize;

    ProgressProxy::setMaxNofSteps(fullSize / buf1.size());

    while(sizeLeft > 0 && !ProgressProxy::wasCancelled())
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
        //ProgressProxy::setCurrent(double(fullSize-sizeLeft)/fullSize, false );
        ProgressProxy::step();
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

    vm.writeEntry("MergeOperation", (qint32)mfi.getOperation());
    vm.writeEntry("DirA", mfi.isDirA());
    vm.writeEntry("DirB", mfi.isDirB());
    vm.writeEntry("DirC", mfi.isDirC());
    vm.writeEntry("LinkA", mfi.isLinkA());
    vm.writeEntry("LinkB", mfi.isLinkB());
    vm.writeEntry("LinkC", mfi.isLinkC());
    vm.writeEntry("OperationComplete", !mfi.isOperationRunning());

    vm.writeEntry("AgeA", (qint32)mfi.getAgeA());
    vm.writeEntry("AgeB", (qint32)mfi.getAgeB());
    vm.writeEntry("AgeC", (qint32)mfi.getAgeC());
    vm.writeEntry("ConflictingAges", mfi.conflictingAges()); // Equal age but files are not!

    vm.save(ts);

    ts << "}\n";

    return ts;
}
