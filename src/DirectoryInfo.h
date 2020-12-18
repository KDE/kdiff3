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

      inline FileAccess dirA() const { return m_dirA; }
      inline FileAccess dirB() const { return m_dirB; }
      inline FileAccess dirC() const { return m_dirC; }
      inline FileAccess destDir() const
      {
          if(m_dirDest.isValid())
              return m_dirDest;
          else
              return m_dirC.isValid() ? m_dirC : m_dirB;
      }

      inline bool allowSyncMode() { return !m_dirC.isValid() && !m_dirDest.isValid(); }

      inline bool listDirA(const Options& options)
      {
          return m_dirA.listDir(&m_dirListA,
                                        options.m_bDmRecursiveDirs, options.m_bDmFindHidden,
                                        options.m_DmFilePattern, options.m_DmFileAntiPattern,
                                        options.m_DmDirAntiPattern, options.m_bDmFollowDirLinks,
                                        options.m_bDmUseCvsIgnore);
      }

      inline bool listDirB(const Options& options)
      {
          return m_dirB.listDir(&m_dirListB,
                                        options.m_bDmRecursiveDirs, options.m_bDmFindHidden,
                                        options.m_DmFilePattern, options.m_DmFileAntiPattern,
                                        options.m_DmDirAntiPattern, options.m_bDmFollowDirLinks,
                                        options.m_bDmUseCvsIgnore);
      }

      inline bool listDirC(const Options& options)
      {
          return m_dirC.listDir(&m_dirListC,
                                        options.m_bDmRecursiveDirs, options.m_bDmFindHidden,
                                        options.m_DmFilePattern, options.m_DmFileAntiPattern,
                                        options.m_DmDirAntiPattern, options.m_bDmFollowDirLinks,
                                        options.m_bDmUseCvsIgnore);
      }

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

#endif
