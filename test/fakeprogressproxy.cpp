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
  (void)info;
  (void)bRedrawUpdate;
}

void ProgressProxy::setInformation( const QString& info, int current, bool bRedrawUpdate )
{
  /* Suppress warning about unused parameters */
  (void)info;
  (void)current;
  (void)bRedrawUpdate;
}

void ProgressProxy::setCurrent( int current, bool bRedrawUpdate )
{
  /* Suppress warning about unused parameters */
  (void)current;
  (void)bRedrawUpdate;
}

void ProgressProxy::step( bool bRedrawUpdate )
{
  /* Suppress warning about unused parameters */
  (void)bRedrawUpdate;
}

void ProgressProxy::setMaxNofSteps( int dMaxNofSteps )
{
  /* Suppress warning about unused parameters */
  (void)dMaxNofSteps;
}



bool ProgressProxy::wasCancelled()
{
  return false;
}

void ProgressProxy::enterEventLoop( KJob* pJob, const QString& jobInfo )
{
  /* Suppress warning about unused parameters */
  (void)pJob;
  (void)jobInfo;
}

void ProgressProxy::exitEventLoop()
{
}

void ProgressDialog::recalc(bool bUpdate)
{
  /* Suppress warning about unused parameters */
  (void)bUpdate;
}

QDialog *ProgressProxy::getDialog()
{
  return NULL;
}

