/***************************************************************************
 *   Copyright (C) 2003-2011 by Joachim Eibl                               *
 *   joachim.eibl at gmx.de                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "progress.h"

#include <QProgressBar>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QApplication>
#include <kio/job.h>

#include <klocale.h>

ProgressDialog* g_pProgressDialog=0;

ProgressDialog::ProgressDialog( QWidget* pParent )
: QDialog( pParent )
{
   setObjectName("ProgressDialog");
   m_bStayHidden = false;
   setModal(true);
   QVBoxLayout* layout = new QVBoxLayout(this);

   m_pInformation = new QLabel( " ", this );
   layout->addWidget( m_pInformation );

   m_pProgressBar = new QProgressBar();
   m_pProgressBar->setRange(0,1000);
   layout->addWidget( m_pProgressBar );

   m_pSubInformation = new QLabel( " ", this);
   layout->addWidget( m_pSubInformation );

   m_pSubProgressBar = new QProgressBar();
   m_pSubProgressBar->setRange(0,1000);
   layout->addWidget( m_pSubProgressBar );

   m_pSlowJobInfo = new QLabel( " ", this);
   layout->addWidget( m_pSlowJobInfo );

   QHBoxLayout* hlayout = new QHBoxLayout();
   layout->addLayout(hlayout);
   hlayout->addStretch(1);
   m_pAbortButton = new QPushButton( i18n("&Cancel"), this);
   hlayout->addWidget( m_pAbortButton );
   connect( m_pAbortButton, SIGNAL(clicked()), this, SLOT(slotAbort()) );

   m_progressDelayTimer = 0;
   resize( 400, 100 );
   m_t1.start();
   m_t2.start();
   m_bWasCancelled = false;
   m_pJob = 0;
}

void ProgressDialog::setStayHidden( bool bStayHidden )
{
   m_bStayHidden = bStayHidden;
}

void ProgressDialog::push()
{
   ProgressLevelData pld;
   if ( !m_progressStack.empty() )
   {
      pld.m_dRangeMax = m_progressStack.back().m_dSubRangeMax;
      pld.m_dRangeMin = m_progressStack.back().m_dSubRangeMin;
   }
   else
   {
      m_bWasCancelled = false;
      m_t1.restart();
      m_t2.restart();
      if ( !m_bStayHidden )
         show();
   }

   m_progressStack.push_back( pld );
}

void ProgressDialog::pop( bool bRedrawUpdate )
{
   if ( !m_progressStack.empty() )
   {
      m_progressStack.pop_back();
      if ( m_progressStack.empty() )
         hide();
      else
         recalc(bRedrawUpdate);
   }
}

void ProgressDialog::setInformation(const QString& info, double dCurrent, bool bRedrawUpdate )
{
   if ( m_progressStack.empty() )
      return;
   ProgressLevelData& pld = m_progressStack.back();
   pld.m_dCurrent = dCurrent;
   int level = m_progressStack.size();
   if ( level==1 )
   {
      m_pInformation->setText( info );
      m_pSubInformation->setText("");
   }
   else if ( level==2 )
   {
      m_pSubInformation->setText( info );
   }
   recalc(bRedrawUpdate);
}

void ProgressDialog::setInformation(const QString& info, bool bRedrawUpdate )
{
   if ( m_progressStack.empty() )
      return;
   //ProgressLevelData& pld = m_progressStack.back();
   int level = m_progressStack.size();
   if ( level==1 )
   {
      m_pInformation->setText( info );
      m_pSubInformation->setText( "" );
   }
   else if ( level==2 )
   {
      m_pSubInformation->setText( info );
   }
   recalc(bRedrawUpdate);
}

void ProgressDialog::setMaxNofSteps( int maxNofSteps )
{
   if ( m_progressStack.empty() )
      return;
   ProgressLevelData& pld = m_progressStack.back();
   pld.m_maxNofSteps = maxNofSteps;
   pld.m_dCurrent = 0;
}

void ProgressDialog::step( bool bRedrawUpdate )
{
   if ( m_progressStack.empty() )
      return;
   ProgressLevelData& pld = m_progressStack.back();
   pld.m_dCurrent += 1.0/pld.m_maxNofSteps;
   recalc(bRedrawUpdate);
}

void ProgressDialog::setCurrent( double dSubCurrent, bool bRedrawUpdate )
{
   if ( m_progressStack.empty() )
      return;
   ProgressLevelData& pld = m_progressStack.back();
   pld.m_dCurrent = dSubCurrent;
   recalc( bRedrawUpdate );
}

// The progressbar goes from 0 to 1 usually.
// By supplying a subrange transformation the subCurrent-values
// 0 to 1 will be transformed to dMin to dMax instead.
// Requirement: 0 < dMin < dMax < 1
void ProgressDialog::setRangeTransformation( double dMin, double dMax )
{
   if ( m_progressStack.empty() )
      return;
   ProgressLevelData& pld = m_progressStack.back();
   pld.m_dRangeMin = dMin;
   pld.m_dRangeMax = dMax;
   pld.m_dCurrent = 0;
}

void ProgressDialog::setSubRangeTransformation( double dMin, double dMax )
{
   if ( m_progressStack.empty() )
      return;
   ProgressLevelData& pld = m_progressStack.back();
   pld.m_dSubRangeMin = dMin;
   pld.m_dSubRangeMax = dMax;
}

void qt_enter_modal(QWidget*);
void qt_leave_modal(QWidget*);

void ProgressDialog::enterEventLoop( KJob* pJob, const QString& jobInfo )
{
   m_pJob = pJob;
   m_pSlowJobInfo->setText("");
   m_currentJobInfo = jobInfo;
   if ( m_progressDelayTimer )
      killTimer( m_progressDelayTimer );
   m_progressDelayTimer = startTimer( 3000 ); /* 3 s delay */

   // instead of using exec() the eventloop is entered and exited often without hiding/showing the window.
   //qt_enter_modal(this);
   QEventLoop* pEventLoop = new QEventLoop(this);
   m_eventLoopStack.push_back( pEventLoop );
   pEventLoop->exec(); // this function only returns after ProgressDialog::exitEventLoop() is called.
   delete pEventLoop;
   m_eventLoopStack.pop_back();
   //qt_leave_modal(this);
}

void ProgressDialog::exitEventLoop()
{
   if ( m_progressDelayTimer )
      killTimer( m_progressDelayTimer );
   m_progressDelayTimer = 0;
   m_pJob = 0;
   if (!m_eventLoopStack.empty())
      m_eventLoopStack.back()->exit();
}

void ProgressDialog::recalc( bool bUpdate )
{
   if ( m_progressDelayTimer )
      killTimer( m_progressDelayTimer );
   m_progressDelayTimer = startTimer( 3000 ); /* 3 s delay */

   int level = m_progressStack.size();
   if( ( bUpdate && level==1) || m_t1.elapsed()>200 )
   {
      if (m_progressStack.empty() )
      {
         m_pProgressBar->setValue( 0 );
         m_pSubProgressBar->setValue( 0 );
      }
      else
      {
         std::list<ProgressLevelData>::iterator i = m_progressStack.begin();
         m_pProgressBar->setValue( int( 1000.0 * ( i->m_dCurrent * (i->m_dRangeMax - i->m_dRangeMin) + i->m_dRangeMin ) ) );
         ++i;
         if ( i!=m_progressStack.end() )
            m_pSubProgressBar->setValue( int( 1000.0 * ( i->m_dCurrent * (i->m_dRangeMax - i->m_dRangeMin) + i->m_dRangeMin ) ) );
         else
            m_pSubProgressBar->setValue( int( 1000.0 * m_progressStack.front().m_dSubRangeMin ) );
      }

      if ( !m_bStayHidden && !isVisible() )
         show();
      qApp->processEvents();
      m_t1.restart();
   }
}


#include <QTimer>
void ProgressDialog::show()
{
   if ( m_progressDelayTimer )
      killTimer( m_progressDelayTimer );
   m_progressDelayTimer = 0;
   if ( !isVisible() && (parentWidget()==0 || parentWidget()->isVisible()) )
   {
      QDialog::show();
   }
}

void ProgressDialog::hide()
{
   if ( m_progressDelayTimer )
      killTimer( m_progressDelayTimer );
   m_progressDelayTimer = 0;
   // Calling QDialog::hide() directly doesn't always work. (?)
   QTimer::singleShot( 100, this, SLOT(delayedHide()) );
}

void ProgressDialog::delayedHide()
{
   if (m_pJob!=0)
   {
      m_pJob->kill( KJob::Quietly );
      m_pJob = 0;
   }
   QDialog::hide();
   m_pInformation->setText( "" );

   //m_progressStack.clear();

   m_pProgressBar->setValue( 0 );
   m_pSubProgressBar->setValue( 0 );
   m_pSubInformation->setText("");
   m_pSlowJobInfo->setText("");
}

void ProgressDialog::reject()
{
   m_bWasCancelled = true;
   QDialog::reject();
}

void ProgressDialog::slotAbort()
{
   reject();
}

bool ProgressDialog::wasCancelled()
{
   if( m_t2.elapsed()>100 )
   {
      qApp->processEvents();
      m_t2.restart();
   }
   return m_bWasCancelled;
}


void ProgressDialog::timerEvent(QTimerEvent*)
{
   if( !isVisible() )
   {
      show();
   }
   m_pSlowJobInfo->setText( m_currentJobInfo );
}


ProgressProxy::ProgressProxy()
{
   g_pProgressDialog->push();
}

ProgressProxy::~ProgressProxy()
{
   g_pProgressDialog->pop(false);
}

void ProgressProxy::enterEventLoop( KJob* pJob, const QString& jobInfo )
{
  g_pProgressDialog->enterEventLoop(pJob, jobInfo);
}

void ProgressProxy::exitEventLoop()
{
  g_pProgressDialog->exitEventLoop();
}

QDialog *ProgressProxy::getDialog()
{
  return g_pProgressDialog;
}

void ProgressProxy::setInformation( const QString& info, bool bRedrawUpdate )
{
   g_pProgressDialog->setInformation( info, bRedrawUpdate );
}

void ProgressProxy::setInformation( const QString& info, double dCurrent, bool bRedrawUpdate )
{
   g_pProgressDialog->setInformation( info, dCurrent, bRedrawUpdate );
}

void ProgressProxy::setCurrent( double dCurrent, bool bRedrawUpdate  )
{
   g_pProgressDialog->setCurrent( dCurrent, bRedrawUpdate );
}

void ProgressProxy::step( bool bRedrawUpdate )
{
   g_pProgressDialog->step( bRedrawUpdate );
}

void ProgressProxy::setMaxNofSteps( int maxNofSteps )
{
   g_pProgressDialog->setMaxNofSteps( maxNofSteps );
}

bool ProgressProxy::wasCancelled()
{
   return g_pProgressDialog->wasCancelled();
}

void ProgressProxy::setRangeTransformation( double dMin, double dMax )
{
   g_pProgressDialog->setRangeTransformation( dMin, dMax );
}

void ProgressProxy::setSubRangeTransformation( double dMin, double dMax )
{
   g_pProgressDialog->setSubRangeTransformation( dMin, dMax );
}


