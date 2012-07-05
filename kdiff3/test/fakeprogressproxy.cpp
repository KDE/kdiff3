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

void ProgressProxy::setInformation( const QString& info, double dCurrent, bool bRedrawUpdate )
{
  /* Suppress warning about unused parameters */
  (void)info;
  (void)dCurrent;
  (void)bRedrawUpdate;
}

void ProgressProxy::setCurrent( double dCurrent, bool bRedrawUpdate )
{
  /* Suppress warning about unused parameters */
  (void)dCurrent;
  (void)bRedrawUpdate;
}

void ProgressProxy::step( bool bRedrawUpdate )
{
  /* Suppress warning about unused parameters */
  (void)bRedrawUpdate;
}

bool ProgressProxy::wasCancelled()
{
  return false;
}

void ProgressProxy::exitEventLoop()
{
}

void ProgressProxy::enterEventLoop( KJob* pJob, const QString& jobInfo )
{
  /* Suppress warning about unused parameters */
  (void)pJob;
  (void)jobInfo;
}

QDialog *ProgressProxy::getDialog()
{
  return NULL;
}

