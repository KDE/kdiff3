/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef FILEACCESSJOBHANDLER_H
#define FILEACCESSJOBHANDLER_H

#include "fileaccess.h"

#include <QObject>
#include <QString>

#ifndef AUTOTEST
#include <KIO/UDSEntry>
#endif

namespace KIO {
class Job;
}

class KJob;

class FileAccessJobHandler: public QObject
{
  public:
    FileAccessJobHandler(FileAccess* pFileAccess)
    {
        mFileAccess = pFileAccess;
    }

    virtual FileAccessJobHandler* copy(FileAccess* fileAccess) = 0;
    //This exists soley to allow FileAccess to be no-except movable
    void setFileAccess(FileAccess* pFileAccess) {  mFileAccess = pFileAccess; }
    virtual bool get(void* pDestBuffer, long maxLength) = 0;
    virtual bool put(const void* pSrcBuffer, long maxLength, bool bOverwrite, bool bResume = false, qint32 permissions = -1) = 0;
    virtual bool stat(bool bWantToWrite = false) = 0;
    virtual bool copyFile(const QString& dest) = 0;
    virtual bool rename(const FileAccess& dest) = 0;
    virtual bool listDir(DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
                 const QString& filePattern, const QString& fileAntiPattern,
                 const QString& dirAntiPattern, bool bFollowDirLinks, IgnoreList& ignoreList) = 0;
    virtual bool removeFile(const QUrl& fileName) = 0;
    virtual bool symLink(const QUrl& linkTarget, const QUrl& linkLocation) = 0;

  protected:
    FileAccess* mFileAccess = nullptr;
    bool m_bSuccess = false;

    // Data needed during Job
    qint64 m_transferredBytes = 0;
    char* m_pTransferBuffer = nullptr; // Needed during get or put
    qint64 m_maxLength = 0;

    QString m_filePattern;
    QString m_fileAntiPattern;
    QString m_dirAntiPattern;
    DirectoryList* m_pDirList = nullptr;
    bool m_bFindHidden = false;
    bool m_bRecursive = false;
    bool m_bFollowDirLinks = false;

  private:
    virtual bool mkDirImp(const QString& dirName) = 0;
    virtual bool rmDirImp(const QString& dirName) = 0;
};

#endif /* FILEACCESSJOBHANDLER_H */
