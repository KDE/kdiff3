/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef FILEACCESSJOBHANDLERMOC_H
#define FILEACCESSJOBHANDLERMOC_H

#include "../FileAccessJobHandler.h"

class FileAccessJobHandlerMoc: public FileAccessJobHandler
{
  public:
    using FileAccessJobHandler::FileAccessJobHandler;

    FileAccessJobHandler* copy(FileAccess* inFileAccess) override { return new FileAccessJobHandlerMoc(inFileAccess);}
    bool get(void*  /*pDestBuffer*/, long  /*maxLength*/) override {return true;};
    bool put(const void*  /*pSrcBuffer*/, long  /*maxLength*/, bool  /*bOverwrite*/, bool  /*bResume*/ = false, qint32  /*permissions*/ = -1) override {return true;};
    bool stat(bool  /*bWantToWrite*/ = false) override {return true;};
    bool copyFile(const QString&  /*dest*/) override {return true;};
    bool rename(const FileAccess&  /*dest*/) override {return true;};
    bool listDir(DirectoryList*  /*pDirList*/, bool  /*bRecursive*/, bool  /*bFindHidden*/,
                 const QString&  /*filePattern*/, const QString&  /*fileAntiPattern*/,
                 const QString&  /*dirAntiPattern*/, bool  /*bFollowDirLinks*/, IgnoreList& /*ignoreList*/) override {return true;};
    bool removeFile(const QUrl&  /*fileName*/) override {return true;};
    bool symLink(const QUrl&  /*linkTarget*/, const QUrl&  /*linkLocation*/) override { return true;};

  protected:
    bool mkDirImp(const QString&  /*dirName*/) override {return true;};
    bool rmDirImp(const QString&  /*dirName*/) override {return true;};
};

#endif /* FILEACCESSJOBHANDLERMOC_H */
