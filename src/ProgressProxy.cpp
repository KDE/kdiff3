
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ProgressProxy.h"

#include "combiners.h"

#include <boost/signals2.hpp>

#include <QDialog>
#include <QString>

namespace signals2 = boost::signals2;

signals2::signal<void()> ProgressProxy::startBackgroundTask;
signals2::signal<void()> ProgressProxy::endBackgroundTask;

signals2::signal<void()> ProgressProxy::push;
signals2::signal<void(bool)> ProgressProxy::pop;
signals2::signal<void()> ProgressProxy::clear;

signals2::signal<void(KJob*, const QString&)> ProgressProxy::enterEventLoop;
signals2::signal<void()> ProgressProxy::exitEventLoop;

signals2::signal<void(qint64, bool)> ProgressProxy::setCurrentSig;
signals2::signal<void(qint64)> ProgressProxy::setMaxNofSteps;
signals2::signal<void(qint64)> ProgressProxy::addNofSteps;
signals2::signal<void(bool)> ProgressProxy::stepSig;

signals2::signal<void(double, double)> ProgressProxy::setRangeTransformation;
signals2::signal<void(double, double)> ProgressProxy::setSubRangeTransformation;

signals2::signal<bool(), find> ProgressProxy::wasCancelled;

signals2::signal<void(const QString&, bool)> ProgressProxy::setInformationSig;

ProgressScope::ProgressScope()
{
    ProgressProxy::push();
}

ProgressScope::~ProgressScope()
{
    ProgressProxy::pop(false);
}

void ProgressProxy::setInformation(const QString& info, bool bRedrawUpdate)
{
    setInformationSig(info, bRedrawUpdate);
}

void ProgressProxy::setInformation(const QString& info, qint32 current, bool bRedrawUpdate)
{
    setCurrentSig(current, false);
    setInformationSig(info, bRedrawUpdate);
}

void ProgressProxy::setCurrent(qint64 current, bool bRedrawUpdate)
{
    setCurrentSig(current, bRedrawUpdate);
}

void ProgressProxy::step(bool bRedrawUpdate)
{
    stepSig(bRedrawUpdate);
}
