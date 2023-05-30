// clang-format off
/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */
// clang-format on

#ifndef SOURCEDATAMOC_H
#define SOURCEDATAMOC_H

#include "../SourceData.h"

#include <memory>

class SourceDataMoc: public SourceData
{
  private:
    std::unique_ptr<Options> defualtOptions = std::make_unique<Options>();

  public:
    SourceDataMoc() = default;

    [[nodiscard]] const std::unique_ptr<Options>& options() { return defualtOptions; }
};

#endif /* SOURCEDATAMOC_H */
