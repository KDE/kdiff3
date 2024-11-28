// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef PROGRESSPROXY_H
#define PROGRESSPROXY_H

#include "combiners.h"

#include <boost/signals2.hpp>

#include <QObject>
#include <QString>

class ProgressDialog;
class KJob;

namespace signals2 = boost::signals2;
// When using the ProgressScope you need not take care of the push and pop, except when explicit.

class ProgressScope
{
  public:
    ProgressScope();
    ~ProgressScope();
};

class ProgressProxy
{
  public:
    static void setInformation(const QString& info, bool bRedrawUpdate = true);
    static void setInformation(const QString& info, qint32 current, bool bRedrawUpdate = true);
    static void setCurrent(quint64 current, bool bRedrawUpdate = true);
    static void step(bool bRedrawUpdate = true);

    inline static signals2::signal<void()> startBackgroundTask;
    inline static signals2::signal<void()> endBackgroundTask;

    inline static signals2::signal<void()> push;
    inline static signals2::signal<void(bool)> pop;
    inline static signals2::signal<void()> clear;

    inline static signals2::signal<void(KJob*, const QString&)> enterEventLoop;
    inline static signals2::signal<void()> exitEventLoop;

    inline static signals2::signal<void(quint64, bool)> setCurrentSig;
    inline static signals2::signal<void(quint64)> setMaxNofSteps;
    inline static signals2::signal<void(quint64)> addNofSteps;
    inline static signals2::signal<void(bool)> stepSig;

    inline static signals2::signal<void(double, double)> setRangeTransformation;
    inline static signals2::signal<void(double, double)> setSubRangeTransformation;

    inline static signals2::signal<bool(), find> wasCancelled;

    inline static signals2::signal<void(const QString&, bool)> setInformationSig;
};

#endif /* PROGRESSPROXY_H */
