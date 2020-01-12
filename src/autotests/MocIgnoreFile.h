// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Copyright (C) 2020 Michael Reeves <reeves.87@gmail.com>
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
#ifndef MOCIGNOREFILE_H
#define MOCIGNOREFILE_H

#include <QString>

#include <list>

typedef class MocIgnoreFile FileAccess;
typedef std::list<FileAccess> t_DirectoryList;

class MocIgnoreFile
{
  public:
    void mocSetPath(const QString& path) { mPath = path; }
    void mocSetIsLocal(bool local) { mLocal = local; }

    void addPath(const QString& txt) { Q_UNUSED(txt); }
    QString absoluteFilePath() const { return mPath; }

    bool createLocalCopy() { return true; }

    bool isLocal() const { return mLocal; }

    bool exists() const { return mExists; }
    QString getTempName() const { return mPath; }

    QString fileName(bool needTmp = false) const
    {
        Q_UNUSED(needTmp);
        return ".cvsignore";
    }
    
  private:
    QString mPath = "/test/ui/.cvsignore";
    bool mLocal = true, mExists = true;
};

#endif
