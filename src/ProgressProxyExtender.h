/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef PROGRESSPROXYEXTENDER_H
#define PROGRESSPROXYEXTENDER_H

#include "ProgressProxy.h"
#include <QString>

class KJob;

class ProgressProxyExtender: public QObject, public ProgressProxy
{
  Q_OBJECT
public:
  ProgressProxyExtender() { setMaxNofSteps(100); }
  ~ProgressProxyExtender() override = default;
public Q_SLOTS:
  void slotListDirInfoMessage( KJob*, const QString& msg );
  void slotPercent( KJob*, unsigned long percent );
};
#endif
