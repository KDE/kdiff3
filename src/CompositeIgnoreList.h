// clang-format off
/*
 * This file is part of KDiff3
 *
 * SPDX-FileCopyrightText: 2021-2021 David Hallas, david@davidhallas.dk
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

#ifndef COMPOSITEIGNORELIST_H
#define COMPOSITEIGNORELIST_H

#include "IgnoreList.h"
#include "DirectoryList.h"

#include <memory>
#include <vector>

#include <QString>

class CompositeIgnoreList : public IgnoreList
{
  public:
    ~CompositeIgnoreList() override = default;
    void enterDir(const QString& dir, const DirectoryList& directoryList) override;
    [[nodiscard]] bool matches(const QString& dir, const QString& text, bool bCaseSensitive) const override;
    void addIgnoreList(std::unique_ptr<IgnoreList> ignoreList);

  private:
    std::vector<std::unique_ptr<IgnoreList>> m_ignoreLists;
};

#endif
