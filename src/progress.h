/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl                               *
 *   joachim.eibl at gmx.de                                                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef PROGRESS_H
#define PROGRESS_H

#include <QDialog>
#include <QTime>
#include <QList>

class KJob;
class QEventLoop;
class QLabel;
class QProgressBar;
class QStatusBar;

class ProgressDialog : public QDialog
{
   Q_OBJECT
public:
   ProgressDialog( QWidget* pParent,QStatusBar* );

   void setStayHidden( bool bStayHidden );
   void setInformation( const QString& info, bool bRedrawUpdate=true );
   void setInformation( const QString& info, int current, bool bRedrawUpdate=true );
   void setCurrent( int current, bool bRedrawUpdate=true  );
   void step( bool bRedrawUpdate=true );
   void setMaxNofSteps( int dMaxNofSteps );
   void addNofSteps( int nofSteps );
   void push();
   void pop(bool bRedrawUpdate=true);

   // The progressbar goes from 0 to 1 usually.
   // By supplying a subrange transformation the subCurrent-values
   // 0 to 1 will be transformed to dMin to dMax instead.
   // Requirement: 0 < dMin < dMax < 1
   void setRangeTransformation( double dMin, double dMax );
   void setSubRangeTransformation( double dMin, double dMax );

   void exitEventLoop();
   void enterEventLoop( KJob* pJob, const QString& jobInfo );

   bool wasCancelled();
   enum e_CancelReason{eUserAbort,eResize};
   void cancel(e_CancelReason);
   e_CancelReason cancelReason();
   void clearCancelState();
   void show();
   void hide();
   void hideStatusBarWidget();
   void delayedHideStatusBarWidget();
   
   virtual void timerEvent(QTimerEvent*);
public slots:
   void recalc(bool bRedrawUpdate);
private:

   struct ProgressLevelData
   {
      ProgressLevelData()
      {
         m_current=0; m_maxNofSteps=1; m_dRangeMin=0; m_dRangeMax=1; 
         m_dSubRangeMin = 0; m_dSubRangeMax = 1;
      }
      QAtomicInt m_current;
      QAtomicInt m_maxNofSteps;     // when step() is used.
      double m_dRangeMax;
      double m_dRangeMin;
      double m_dSubRangeMax;
      double m_dSubRangeMin;
   };
   QList<ProgressLevelData> m_progressStack;
   
   int m_progressDelayTimer;
   int m_delayedHideTimer;
   int m_delayedHideStatusBarWidgetTimer;
   QList<QEventLoop*> m_eventLoopStack;

   QProgressBar* m_pProgressBar;
   QProgressBar* m_pSubProgressBar;
   QLabel* m_pInformation;
   QLabel* m_pSubInformation;
   QLabel* m_pSlowJobInfo;
   QPushButton* m_pAbortButton;
   QTime m_t1;
   QTime m_t2;
   bool m_bWasCancelled;
   e_CancelReason m_eCancelReason;
   KJob* m_pJob;
   QString m_currentJobInfo;  // Needed if the job doesn't stop after a reasonable time.
   bool m_bStayHidden;
   QThread* m_pGuiThread;
   QStatusBar* m_pStatusBar;  // status bar of main window (if exists)
   QWidget* m_pStatusBarWidget;
   QProgressBar* m_pStatusProgressBar;
   QPushButton* m_pStatusAbortButton;
protected:
   virtual void reject();
private slots:
   void delayedHide();
   void slotAbort();
};

// When using the ProgressProxy you need not take care of the push and pop, except when explicit.
class ProgressProxy: public QObject
{
   Q_OBJECT
public:
   ProgressProxy();
   ~ProgressProxy();
   
   void setInformation( const QString& info, bool bRedrawUpdate=true );
   void setInformation( const QString& info, int current, bool bRedrawUpdate=true );
   void setCurrent( int current, bool bRedrawUpdate=true  );
   void step( bool bRedrawUpdate=true );
   void setMaxNofSteps( int maxNofSteps );
   void addNofSteps( int nofSteps );
   bool wasCancelled();
   void setRangeTransformation( double dMin, double dMax );
   void setSubRangeTransformation( double dMin, double dMax );

   static void exitEventLoop();
   static void enterEventLoop( KJob* pJob, const QString& jobInfo );
   static QDialog *getDialog();
   static void recalc();
private:
};

extern ProgressDialog* g_pProgressDialog;

#endif

