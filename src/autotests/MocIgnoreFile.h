/*
 * KDiff3 - Text Diff And Merge Tool
 * 
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
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
