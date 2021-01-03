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

namespace KIO {
class Job;
}

class KJob;

class FileAccess;
class t_DirectoryList;

#ifndef AUTOTEST
class FileAccessJobHandler : public QObject
{
    Q_OBJECT
  public:
    explicit FileAccessJobHandler(FileAccess* pFileAccess);

    bool get(void* pDestBuffer, long maxLength);
    bool put(const void* pSrcBuffer, long maxLength, bool bOverwrite, bool bResume = false, int permissions = -1);
    bool stat(short detailLevel = 2, bool bWantToWrite = false);
    bool copyFile(const QString& dest);
    bool rename(const FileAccess& dest);
    bool listDir(t_DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
                 const QString& filePattern, const QString& fileAntiPattern,
                 const QString& dirAntiPattern, bool bFollowDirLinks, bool bUseCvsIgnore);
    bool mkDir(const QString& dirName);
    bool rmDir(const QString& dirName);
    bool removeFile(const QUrl& fileName);
    bool symLink(const QUrl& linkTarget, const QUrl& linkLocation);

  private:
    FileAccess* m_pFileAccess = nullptr;
    bool m_bSuccess = false;

    // Data needed during Job
    qint64 m_transferredBytes = 0;
    char* m_pTransferBuffer = nullptr; // Needed during get or put
    qint64 m_maxLength = 0;

    QString m_filePattern;
    QString m_fileAntiPattern;
    QString m_dirAntiPattern;
    t_DirectoryList* m_pDirList = nullptr;
    bool m_bFindHidden = false;
    bool m_bRecursive = false;
    bool m_bFollowDirLinks = false;

    bool scanLocalDirectory(const QString& dirName, t_DirectoryList* dirList);

  private Q_SLOTS:
    void slotJobEnded(KJob*);
    void slotStatResult(KJob*);
    void slotSimpleJobResult(KJob* pJob);
    void slotPutJobResult(KJob* pJob);

    void slotGetData(KJob*, const QByteArray&);
    void slotPutData(KIO::Job*, QByteArray&);

    void slotListDirProcessNewEntries(KIO::Job*, const KIO::UDSEntryList& l);
};

#else
class FileAccessJobHandler: public QObject
{
  public:
    explicit FileAccessJobHandler(FileAccess* pFileAccess){Q_UNUSED(pFileAccess)};

    bool get(void*  /*pDestBuffer*/, long  /*maxLength*/){return true;};
    bool put(const void*  /*pSrcBuffer*/, long  /*maxLength*/, bool  /*bOverwrite*/, bool  /*bResume*/ = false, int  /*permissions*/ = -1){return true;};
    bool stat(short  /*detailLevel*/ = 2, bool  /*bWantToWrite*/ = false){return true;};
    bool copyFile(const QString&  /*dest*/){return true;};
    bool rename(const FileAccess&  /*dest*/){return true;};
    bool listDir(t_DirectoryList*  /*pDirList*/, bool  /*bRecursive*/, bool  /*bFindHidden*/,
                 const QString&  /*filePattern*/, const QString&  /*fileAntiPattern*/,
                 const QString&  /*dirAntiPattern*/, bool  /*bFollowDirLinks*/, bool  /*bUseCvsIgnore*/){return true;};
    bool mkDir(const QString&  /*dirName*/){return true;};
    bool rmDir(const QString&  /*dirName*/){return true;};
    bool removeFile(const QUrl&  /*fileName*/){return true;};
    bool symLink(const QUrl&  /*linkTarget*/, const QUrl&  /*linkLocation*/) { return true;};
};
#endif

#endif /* FILEACCESSJOBHANDLER_H */
