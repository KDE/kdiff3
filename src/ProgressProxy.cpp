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

signals2::signal<void()> ProgressProxy::push;
signals2::signal<void(bool)> ProgressProxy::pop;
signals2::signal<void()> ProgressProxy::clearSig;

signals2::signal<void(KJob*, const QString&)> ProgressProxy::enterEventLoopSig;
signals2::signal<void()> ProgressProxy::exitEventLoopSig;

signals2::signal<void(qint64, bool)> ProgressProxy::setCurrentSig;
signals2::signal<void(qint64)> ProgressProxy::setMaxNofStepsSig;
signals2::signal<void(qint64)> ProgressProxy::addNofStepsSig;
signals2::signal<void(bool)> ProgressProxy::stepSig;

signals2::signal<void(double, double)> ProgressProxy::setRangeTransformationSig;
signals2::signal<void(double, double)> ProgressProxy::setSubRangeTransformationSig;

signals2::signal<bool(), find> ProgressProxy::wasCancelledSig;

signals2::signal<void(const QString&, bool)> ProgressProxy::setInformationSig;
signals2::signal<void(const QString&, int, bool)> ProgressProxy::setInfoAndStepSig;

ProgressProxy::ProgressProxy()
{
    push();
}

ProgressProxy::~ProgressProxy()
{
    pop(false);
}

void ProgressProxy::enterEventLoop(KJob* pJob, const QString& jobInfo)
{
    enterEventLoopSig(pJob, jobInfo);
}

void ProgressProxy::exitEventLoop()
{
    exitEventLoopSig();
}

void ProgressProxy::setInformation(const QString& info, bool bRedrawUpdate)
{
    setInformationSig(info, bRedrawUpdate);
}

void ProgressProxy::setInformation(const QString& info, int current, bool bRedrawUpdate)
{
    setInfoAndStepSig(info, current, bRedrawUpdate);
}

void ProgressProxy::setCurrent(qint64 current, bool bRedrawUpdate)
{
    setCurrentSig(current, bRedrawUpdate);
}

void ProgressProxy::step(bool bRedrawUpdate)
{
    stepSig(bRedrawUpdate);
}

void ProgressProxy::clear()
{
    clearSig();
}

void ProgressProxy::setMaxNofSteps(const qint64 maxNofSteps)
{
    setMaxNofStepsSig(maxNofSteps);
}

void ProgressProxy::addNofSteps(const qint64 nofSteps)
{
    addNofStepsSig(nofSteps);
}

bool ProgressProxy::wasCancelled()
{
    return wasCancelledSig();
}

void ProgressProxy::setRangeTransformation(double dMin, double dMax)
{
    setRangeTransformationSig(dMin, dMax);
}

void ProgressProxy::setSubRangeTransformation(double dMin, double dMax)
{
    setSubRangeTransformationSig(dMin, dMax);
}
