/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef CVSIGNORELIST_H
#define CVSIGNORELIST_H

//#include "fileaccess.h"

#include "DirectoryList.h"
#include "IgnoreList.h"

#include <QString>
#include <QStringList>

#include <map>

struct CvsIgnorePatterns
{
    QStringList m_exactPatterns;
    QStringList m_startPatterns;
    QStringList m_endPatterns;
    QStringList m_generalPatterns;
};

class CvsIgnoreList : public IgnoreList
{
public:
    CvsIgnoreList();
    ~CvsIgnoreList() override;
    void enterDir(const QString& dir, const DirectoryList& directoryList) override;
    [[nodiscard]] bool matches(const QString& dir, const QString& text, bool bCaseSensitive) const override;

protected:
    bool ignoreExists(const DirectoryList& pDirList);

    void addEntriesFromString(const QString& dir, const QString& str);
    void addEntriesFromFile(const QString& dir, const QString& name);
    void addEntry(const QString& dir, const QString& pattern);

    std::map<QString, CvsIgnorePatterns> m_ignorePatterns;
private:
    friend class CvsIgnoreListTest;
    /*
        The name of the users global ignore can be changed separately in some cases in the future
        kdiff will handle this through a user settings.
        For now just return the same thing as gerIngoreName. That works
    */
    [[nodiscard]] inline virtual const QString getGlobalIgnoreName() const { return getIgnoreName(); }
    [[nodiscard]] inline const char* getVarName() const { return "CVSIGNORE"; }
    [[nodiscard]] inline const QString getIgnoreName() const { return QStringLiteral(".cvsignore"); }
};

#endif /* CVSIGNORELIST_H */
