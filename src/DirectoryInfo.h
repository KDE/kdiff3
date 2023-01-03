/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on
#ifndef DIRECTORYINFO_H
#define DIRECTORYINFO_H

#include "fileaccess.h"
#include "options.h"

class DirectoryInfo
{
  public:
    explicit DirectoryInfo() = default;

    DirectoryInfo(const FileAccess& dirA, const FileAccess& dirB, const FileAccess& dirC, const FileAccess& dirDest)
    {
        m_dirA = dirA;
        m_dirB = dirB;
        m_dirC = dirC;
        m_dirDest = dirDest;

        m_dirListA.clear();
        m_dirListB.clear();
        m_dirListC.clear();
    }

    inline const FileAccess& dirA() const { return m_dirA; }
    inline const FileAccess& dirB() const { return m_dirB; }
    inline const FileAccess& dirC() const { return m_dirC; }
    inline const FileAccess& destDir() const
    {
        if(m_dirDest.isValid())
            return m_dirDest;
        else
            return m_dirC.isValid() ? m_dirC : m_dirB;
    }

    inline bool allowSyncMode() { return !m_dirC.isValid() && !m_dirDest.isValid(); }

    bool listDirA(const QSharedPointer<const Options>& options);
    bool listDirB(const QSharedPointer<const Options>& options);
    bool listDirC(const QSharedPointer<const Options>& options);
    DirectoryList& getDirListA() { return m_dirListA; }
    DirectoryList& getDirListB() { return m_dirListB; }
    DirectoryList& getDirListC() { return m_dirListC; }

  private:
    bool listDir(FileAccess& fileAccess, DirectoryList& dirList, const QSharedPointer<const Options>& options);

    FileAccess m_dirA, m_dirB, m_dirC;

    DirectoryList m_dirListA;
    DirectoryList m_dirListB;
    DirectoryList m_dirListC;
    FileAccess m_dirDest;
};

extern QSharedPointer<DirectoryInfo> gDirInfo;

#endif
