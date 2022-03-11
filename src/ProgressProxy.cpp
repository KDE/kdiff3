/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "progress.h"

#include <QDialog>
#include <QString>

#ifndef AUTOTEST
ProgressProxy::ProgressProxy()
{
    g_pProgressDialog->push();
}
#else
ProgressProxy::ProgressProxy() = default;
#endif

#ifndef AUTOTEST
ProgressProxy::~ProgressProxy()
{
    g_pProgressDialog->pop(false);
}

#else
ProgressProxy::~ProgressProxy() = default;
#endif

void ProgressProxy::enterEventLoop(KJob* pJob, const QString& jobInfo)
{
#ifndef AUTOTEST
    g_pProgressDialog->enterEventLoop(pJob, jobInfo);
#else
    Q_UNUSED(pJob);
    Q_UNUSED(jobInfo);
#endif
}

void ProgressProxy::exitEventLoop()
{
#ifndef AUTOTEST
    g_pProgressDialog->exitEventLoop();
#endif
}

QDialog* ProgressProxy::getDialog()
{
    return g_pProgressDialog;
}

void ProgressProxy::setInformation(const QString& info, bool bRedrawUpdate)
{
#ifndef AUTOTEST
    g_pProgressDialog->setInformation(info, bRedrawUpdate);
#else
    Q_UNUSED(info);
    Q_UNUSED(bRedrawUpdate);
#endif
}

void ProgressProxy::setInformation(const QString& info, int current, bool bRedrawUpdate)
{
#ifndef AUTOTEST
    g_pProgressDialog->setInformation(info, current, bRedrawUpdate);
#else
    Q_UNUSED(info);
    Q_UNUSED(bRedrawUpdate);
    Q_UNUSED(current);
#endif
}

void ProgressProxy::setCurrent(qint64 current, bool bRedrawUpdate)
{
#ifndef AUTOTEST
    g_pProgressDialog->setCurrent(current, bRedrawUpdate);
#else
    Q_UNUSED(bRedrawUpdate);
    Q_UNUSED(current);
#endif
}

void ProgressProxy::step(bool bRedrawUpdate)
{
#ifndef AUTOTEST
    g_pProgressDialog->step(bRedrawUpdate);
#else
    Q_UNUSED(bRedrawUpdate);
#endif
}

void ProgressProxy::clear()
{
#ifndef AUTOTEST
    g_pProgressDialog->clear();
#endif
}

void ProgressProxy::setMaxNofSteps(const qint64 maxNofSteps)
{
#ifndef AUTOTEST
    g_pProgressDialog->setMaxNofSteps(maxNofSteps);
#else
    Q_UNUSED(maxNofSteps);
#endif
}

void ProgressProxy::addNofSteps(const qint64 nofSteps)
{
#ifndef AUTOTEST
    g_pProgressDialog->addNofSteps(nofSteps);
#else
    Q_UNUSED(nofSteps)
#endif
}

bool ProgressProxy::wasCancelled()
{
#ifndef AUTOTEST
    return g_pProgressDialog->wasCancelled();
#else
    return false;
#endif
}

void ProgressProxy::setRangeTransformation(double dMin, double dMax)
{
#ifndef AUTOTEST
    g_pProgressDialog->setRangeTransformation(dMin, dMax);
#else
    Q_UNUSED(dMin);
    Q_UNUSED(dMax);
#endif
}

void ProgressProxy::setSubRangeTransformation(double dMin, double dMax)
{
#ifndef AUTOTEST
    g_pProgressDialog->setSubRangeTransformation(dMin, dMax);
#else
    Q_UNUSED(dMin);
    Q_UNUSED(dMax);
#endif
}

void ProgressProxy::recalc()
{
#ifndef AUTOTEST
    g_pProgressDialog->recalc(true);
#endif
}
