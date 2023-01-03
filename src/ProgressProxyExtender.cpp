/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include "ProgressProxyExtender.h"

#include <QString>

void ProgressProxyExtender::slotListDirInfoMessage(KJob*, const QString& msg)
{
    setInformation(msg, 0);
}

void ProgressProxyExtender::slotPercent(KJob*, unsigned long percent)
{
    setCurrent(percent);
}


