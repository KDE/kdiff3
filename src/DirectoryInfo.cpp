// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021-2021 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include "DirectoryInfo.h"

#include "CompositeIgnoreList.h"
#include "CvsIgnoreList.h"
#include "GitIgnoreList.h"
#include "options.h"

#include <memory>

//Intialize with a dummy default DirectoryInfo so we don't crash on first run.
QSharedPointer<DirectoryInfo>  gDirInfo = QSharedPointer<DirectoryInfo>::create();

bool DirectoryInfo::listDirA()
{
    return listDir(m_dirA, m_dirListA);
}

bool DirectoryInfo::listDirB()
{
    return listDir(m_dirB, m_dirListB);
}

bool DirectoryInfo::listDirC()
{
    return listDir(m_dirC, m_dirListC);
}

bool DirectoryInfo::listDir(FileAccess& fileAccess, DirectoryList& dirList)
{
    CompositeIgnoreList ignoreList;
    if(gOptions->m_bDmUseCvsIgnore)
    {
        ignoreList.addIgnoreList(std::make_unique<CvsIgnoreList>());
        ignoreList.addIgnoreList(std::make_unique<GitIgnoreList>());
    }
    return fileAccess.listDir(&dirList,
                              gOptions->m_bDmRecursiveDirs, gOptions->m_bDmFindHidden,
                              gOptions->m_DmFilePattern, gOptions->m_DmFileAntiPattern,
                              gOptions->m_DmDirAntiPattern, gOptions->m_bDmFollowDirLinks,
                              ignoreList);
}
