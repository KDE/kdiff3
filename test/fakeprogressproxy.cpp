/*
  SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <assert.h>
#include "progress.h"

void ProgressDialog::delayedHide()
{
}

void ProgressDialog::slotAbort()
{
}

void ProgressDialog::reject()
{
}

void ProgressDialog::timerEvent(QTimerEvent*)
{
}

ProgressProxy::ProgressProxy()
{
}

ProgressProxy::~ProgressProxy()
{
}

void ProgressProxy::setInformation( const QString& info, bool bRedrawUpdate )
{
  /* Suppress warning about unused parameters */
  Q_UNUSED(info);
  Q_UNUSED(bRedrawUpdate);
}

void ProgressProxy::setInformation( const QString& info, int current, bool bRedrawUpdate )
{
  /* Suppress warning about unused parameters */
  Q_UNUSED(info);
  Q_UNUSED(current);
  Q_UNUSED(bRedrawUpdate);
}

void ProgressProxy::setCurrent( qint64 current, bool bRedrawUpdate )
{
  /* Suppress warning about unused parameters */
  Q_UNUSED(current);
  Q_UNUSED(bRedrawUpdate);
}

void ProgressProxy::step( bool bRedrawUpdate )
{
  /* Suppress warning about unused parameters */
  Q_UNUSED(bRedrawUpdate);
}

void ProgressProxy::setMaxNofSteps( qint64 dMaxNofSteps )
{
  /* Suppress warning about unused parameters */
  Q_UNUSED(dMaxNofSteps);
}



bool ProgressProxy::wasCancelled()
{
  return false;
}

void ProgressProxy::enterEventLoop( KJob* pJob, const QString& jobInfo )
{
  /* Suppress warning about unused parameters */
  Q_UNUSED(pJob);
  Q_UNUSED(jobInfo);
}

void ProgressProxy::exitEventLoop()
{
}

void ProgressDialog::recalc(bool bUpdate)
{
  /* Suppress warning about unused parameters */
  Q_UNUSED(bUpdate);
}

QDialog *ProgressProxy::getDialog()
{
  return NULL;
}

