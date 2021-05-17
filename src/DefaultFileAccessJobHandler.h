/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef DEFAULTFILEACCESSJOBHANDLER_H
#define DEFAULTFILEACCESSJOBHANDLER_H

#include "FileAccessJobHandler.h"

#include "DirectoryList.h"         // for DirectoryList

#include <QByteArray>
#include <QString>

#include <KIO/UDSEntry>

namespace KIO {
class Job;
}

class KJob;

class DefaultFileAccessJobHandler: public FileAccessJobHandler
{
    Q_OBJECT
  public:
    using FileAccessJobHandler::FileAccessJobHandler;

    FileAccessJobHandler* copy(FileAccess* inFileAccess) override { return new DefaultFileAccessJobHandler(inFileAccess);}

    bool get(void* pDestBuffer, long maxLength) override;
    bool put(const void* pSrcBuffer, long maxLength, bool bOverwrite, bool bResume = false, int permissions = -1) override;
    bool stat(bool bWantToWrite = false) override;
    bool copyFile(const QString& dest) override;
    bool rename(const FileAccess& dest) override;
    bool listDir(DirectoryList* pDirList, bool bRecursive, bool bFindHidden,
                 const QString& filePattern, const QString& fileAntiPattern,
                 const QString& dirAntiPattern, bool bFollowDirLinks, IgnoreList& ignoreList) override;

    static bool mkDir(const QString& dirName) {return DefaultFileAccessJobHandler(nullptr).mkDirImp(dirName);}
    static bool rmDir(const QString& dirName) {return DefaultFileAccessJobHandler(nullptr).rmDirImp(dirName);}

    bool removeFile(const QUrl& fileName) override;
    bool symLink(const QUrl& linkTarget, const QUrl& linkLocation) override;

  private:
    bool mkDirImp(const QString& dirName) override;
    bool rmDirImp(const QString& dirName) override;

    bool scanLocalDirectory(const QString& dirName, DirectoryList* dirList);

  private Q_SLOTS:
    void slotJobEnded(KJob*);
    void slotStatResult(KJob*);
    void slotSimpleJobResult(KJob* pJob);
    void slotPutJobResult(KJob* pJob);

    void slotGetData(KJob*, const QByteArray&);
    void slotPutData(KIO::Job*, QByteArray&);

    void slotListDirProcessNewEntries(KIO::Job*, const KIO::UDSEntryList& l);
};



#endif /* FILEACCESSJOBHANDLER_H */
