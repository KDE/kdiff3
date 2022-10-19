/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2019-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef LOGGING_H
#define LOGGING_H
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(kdiffMain);

Q_DECLARE_LOGGING_CATEGORY(kdiffDiffTextWindow);
Q_DECLARE_LOGGING_CATEGORY(kdiffFileAccess);
Q_DECLARE_LOGGING_CATEGORY(kdiffCore); //very noisey shows internal state information for kdiffs core.
Q_DECLARE_LOGGING_CATEGORY(kdiffGitIgnoreList);

Q_DECLARE_LOGGING_CATEGORY(kdiffMergeFileInfo);

#endif // !LOGGING_H
