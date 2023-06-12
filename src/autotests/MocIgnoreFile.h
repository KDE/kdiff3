// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2018-2021 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef MOCIGNOREFILE_H
#define MOCIGNOREFILE_H

#include "FileAccessJobHandlerMoc.h"
#include "../fileaccess.h"

#include <QString>

#include <list>

//typedef class MocIgnoreFile FileAccess;
//typedef std::list<FileAccess> t_DirectoryList;

class MocIgnoreFile: public FileAccess
{
  public:
    MocIgnoreFile()
    {
        mJobHandler.reset(new FileAccessJobHandlerMoc(this));
        /*
          FileAccess set file calls our overriden loadData to actually get file meta data.
          This way we can avoid making any actual FileSystem checks on the simulated file.
        */
        setFile(QUrl("/test/ui/.cvsignore"));
    }
    void setParent(MocIgnoreFile* parent) { m_pParent = (FileAccess*)parent;}
    void setFileName(const QString& name) { m_name = name; }

    void loadData() override
    {
        m_bValidData = true;
        m_bExists = true;
        m_bReadable = true;

        m_name = ".cvsignore";
        setUrl(QUrl("/test/ui/.cvsignore"));
    }
    void addPath([[maybe_unused]] const QString& txt, bool /*reinit*/ = true) override {}
    bool createLocalCopy() override { return true; }

    [[nodiscard]] qint64 size() const override { return 0; };

    [[nodiscard]] bool isFile() const override { return true; };
    [[nodiscard]] bool isDir() const override { return false; };
    [[nodiscard]] bool isSymLink() const override { return false; };
    [[nodiscard]] bool exists() const override { return m_bExists; };
    [[nodiscard]] bool isReadable() const override { return m_bReadable; };
    [[nodiscard]] bool isWritable() const override { return m_bWritable; };
    [[nodiscard]] bool isExecutable() const override { return m_bExecutable; };
    [[nodiscard]] bool isHidden() const override { return m_bHidden; };

    bool readFile(void*  /*pDestBuffer*/, qint64  /*maxLength*/) override { return true;};
    bool writeFile(const void* /*pSrcBuffer*/, qint64 /*length*/) override { return true; };

  private:
    QString mPath = "/test/ui/.cvsignore";
};

#endif // !FILEACCESS_H
