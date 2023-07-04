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
// When using the ProgressProxy you need not take care of the push and pop, except when explicit.
class ProgressProxy
{
  public:
    ProgressProxy();
    ~ProgressProxy();

    static void setInformation(const QString& info, bool bRedrawUpdate = true);
    static void setInformation(const QString& info, int current, bool bRedrawUpdate = true);
    static void setCurrent(quint64 current, bool bRedrawUpdate = true);
    static void step(bool bRedrawUpdate = true);

    static signals2::signal<void()> startBackgroundTask;
    static signals2::signal<void()> endBackgroundTask;

    static signals2::signal<void()> push;
    static signals2::signal<void(bool)> pop;
    static signals2::signal<void()> clear;

    static signals2::signal<void(KJob*, const QString&)> enterEventLoop;
    static signals2::signal<void()> exitEventLoop;

    static signals2::signal<void(quint64, bool)> setCurrentSig;
    static signals2::signal<void(quint64)> setMaxNofSteps;
    static signals2::signal<void(quint64)> addNofSteps;
    static signals2::signal<void(bool)> stepSig;

    static signals2::signal<void(double, double)> setRangeTransformation;
    static signals2::signal<void(double, double)> setSubRangeTransformation;

    static signals2::signal<bool(), find> wasCancelled;

    static signals2::signal<void(const QString&, bool)> setInformationSig;
};

#endif /* PROGRESSPROXY_H */
