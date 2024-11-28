// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include "ProgressProxy.h"

#include "combiners.h"

#include <boost/signals2.hpp>

#include <QDialog>
#include <QString>

/*
    Using boost allows ProgessProxy to be disconnected during auto testing.
    This prevents uneeded UI calls from being made in an evironment were they
    cann't function properly.
*/

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

void ProgressProxy::setCurrent(quint64 current, bool bRedrawUpdate)
{
    setCurrentSig(current, bRedrawUpdate);
}

void ProgressProxy::step(bool bRedrawUpdate)
{
    stepSig(bRedrawUpdate);
}
