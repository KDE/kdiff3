/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef DIRECTORYINFO_H
#define DIRECTORYINFO_H

#include "fileaccess.h"
#include "options.h"

class DirectoryInfo
{
    public:
      explicit DirectoryInfo() = default;

      DirectoryInfo(FileAccess& dirA, FileAccess& dirB, FileAccess& dirC, FileAccess& dirDest)
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

    bool listDirA(const Options& options);
    bool listDirB(const Options& options);
    bool listDirC(const Options& options);

      t_DirectoryList& getDirListA() { return m_dirListA; }
      t_DirectoryList& getDirListB() { return m_dirListB; }
      t_DirectoryList& getDirListC() { return m_dirListC; }

    private:
      FileAccess m_dirA, m_dirB, m_dirC;

      t_DirectoryList m_dirListA;
      t_DirectoryList m_dirListB;
      t_DirectoryList m_dirListC;
      FileAccess m_dirDest;
};

extern QSharedPointer<DirectoryInfo> gDirInfo;

#endif
