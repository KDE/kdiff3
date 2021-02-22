/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021-2021 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DirectoryInfo.h"
#include "CompositeIgnoreList.h"
#include "CvsIgnoreList.h"

bool DirectoryInfo::listDirA(const Options& options)
{
    return listDir(m_dirA, m_dirListA, options);
}

bool DirectoryInfo::listDirB(const Options& options)
{
    return listDir(m_dirB, m_dirListB, options);
}

bool DirectoryInfo::listDirC(const Options& options)
{
    return listDir(m_dirC, m_dirListC, options);
}

bool DirectoryInfo::listDir(FileAccess& fileAccess, DirectoryList& dirList, const Options& options)
{
    CompositeIgnoreList ignoreList;
    if (options.m_bDmUseCvsIgnore)
    {
        ignoreList.addIgnoreList(std::make_unique<CvsIgnoreList>());
    }
    return fileAccess.listDir(&dirList,
                              options.m_bDmRecursiveDirs, options.m_bDmFindHidden,
                              options.m_DmFilePattern, options.m_DmFileAntiPattern,
                              options.m_DmDirAntiPattern, options.m_bDmFollowDirLinks,
                              ignoreList);
}
