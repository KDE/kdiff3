/**
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2021 Michael Reeves <reeves.87@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef SOURCEDATAMOC_H
#define SOURCEDATAMOC_H

#include "../SourceData.h"

class SourceDataMoc: public SourceData
{
  private:
    QSharedPointer<Options> defualtOptions = QSharedPointer<Options>::create();

  public:
    SourceDataMoc()
    {
        setOptions(defualtOptions);
    }

    [[nodiscard]] const QSharedPointer<Options>& options() { return defualtOptions; }
};

#endif /* SOURCEDATAMOC_H */
