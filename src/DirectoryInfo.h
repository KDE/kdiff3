/**
 * Copyright (C) 2018 Michael Reeves
 * 
 * This file is part of KDiff3.
 * 
 * KDiff3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 * 
 * KDiff3 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with KDiff3.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef DIRECTORYINFO_H
#define DIRECTORYINFO_H

#include "fileaccess.h"

class DirectoryInfo
{
    public:
      DirectoryInfo(
              FileAccess& dirA,
              FileAccess& dirB,
              FileAccess& dirC,
              FileAccess& dirDest)
      {
          m_dirA = dirA;
          m_dirB = dirB;
          m_dirC = dirC;
          m_dirDest = dirDest;
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

    private:
      FileAccess m_dirA, m_dirB, m_dirC;
      FileAccess m_dirDest;
};

#endif
