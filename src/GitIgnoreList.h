/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 David Hallas <david@davidhallas.dk>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef GIT_IGNORE_LIST_H
#define GIT_IGNORE_LIST_H

#include "IgnoreList.h"

#include <QRegularExpression>
#include <QString>

#include <map>
#include <vector>

class GitIgnoreList : public IgnoreList
{
  public:
    GitIgnoreList();
    ~GitIgnoreList() override;
    void enterDir(const QString& dir, const DirectoryList& directoryList) override;
    [[nodiscard]] bool matches(const QString& dir, const QString& text, bool bCaseSensitive) const override;

  private:
    [[nodiscard]] virtual QString readFile(const QString& fileName) const;
    void addEntries(const QString& dir, const QString& lines);

  private:
    mutable std::map<QString, std::vector<QRegularExpression>> m_patterns;
};

#endif
