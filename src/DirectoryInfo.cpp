/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021-2021 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DirectoryInfo.h"

#include <memory>

//Intialize with a dummy default DirectoryInfo so we don't crash on first run.
QSharedPointer<DirectoryInfo>  gDirInfo = QSharedPointer<DirectoryInfo>::create();

bool DirectoryInfo::listDirA(const Options& options)
{
    return m_dirA.listDir(&m_dirListA,
                          options.m_bDmRecursiveDirs, options.m_bDmFindHidden,
                          options.m_DmFilePattern, options.m_DmFileAntiPattern,
                          options.m_DmDirAntiPattern, options.m_bDmFollowDirLinks,
                          options.m_bDmUseCvsIgnore);
}

bool DirectoryInfo::listDirB(const Options& options)
{
    return m_dirB.listDir(&m_dirListB,
                          options.m_bDmRecursiveDirs, options.m_bDmFindHidden,
                          options.m_DmFilePattern, options.m_DmFileAntiPattern,
                          options.m_DmDirAntiPattern, options.m_bDmFollowDirLinks,
                          options.m_bDmUseCvsIgnore);
}

bool DirectoryInfo::listDirC(const Options& options)
{
    return m_dirC.listDir(&m_dirListC,
                          options.m_bDmRecursiveDirs, options.m_bDmFindHidden,
                          options.m_DmFilePattern, options.m_DmFileAntiPattern,
                          options.m_DmDirAntiPattern, options.m_bDmFollowDirLinks,
                          options.m_bDmUseCvsIgnore);
}
