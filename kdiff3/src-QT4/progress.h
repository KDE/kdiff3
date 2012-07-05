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
#include <list>

class KJob;
class QEventLoop;
class QLabel;
class QProgressBar;

class ProgressDialog : public QDialog
{
   Q_OBJECT
public:
   ProgressDialog( QWidget* pParent );

   void setStayHidden( bool bStayHidden );
   void setInformation( const QString& info, bool bRedrawUpdate=true );
   void setInformation( const QString& info, double dCurrent, bool bRedrawUpdate=true );
   void setCurrent( double dCurrent, bool bRedrawUpdate=true  );
   void step( bool bRedrawUpdate=true );
   void setMaxNofSteps( int dMaxNofSteps );
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
   void show();
   void hide();
   
   virtual void timerEvent(QTimerEvent*);
private:

   struct ProgressLevelData
   {
      ProgressLevelData()
      {
         m_dCurrent=0; m_maxNofSteps=1; m_dRangeMin=0; m_dRangeMax=1; 
         m_dSubRangeMin = 0; m_dSubRangeMax = 1;
      }
      double m_dCurrent;
      int    m_maxNofSteps;     // when step() is used.
      double m_dRangeMax;
      double m_dRangeMin;
      double m_dSubRangeMax;
      double m_dSubRangeMin;
   };
   std::list<ProgressLevelData> m_progressStack;
   
   int m_progressDelayTimer;
   std::list<QEventLoop*> m_eventLoopStack;

   QProgressBar* m_pProgressBar;
   QProgressBar* m_pSubProgressBar;
   QLabel* m_pInformation;
   QLabel* m_pSubInformation;
   QLabel* m_pSlowJobInfo;
   QPushButton* m_pAbortButton;
   void recalc(bool bRedrawUpdate);
   QTime m_t1;
   QTime m_t2;
   bool m_bWasCancelled;
   KJob* m_pJob;
   QString m_currentJobInfo;  // Needed if the job doesn't stop after a reasonable time.
   bool m_bStayHidden;
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
   void setInformation( const QString& info, double dCurrent, bool bRedrawUpdate=true );
   void setCurrent( double dCurrent, bool bRedrawUpdate=true  );
   void step( bool bRedrawUpdate=true );
   void setMaxNofSteps( int dMaxNofSteps );
   bool wasCancelled();
   void setRangeTransformation( double dMin, double dMax );
   void setSubRangeTransformation( double dMin, double dMax );

   static void exitEventLoop();
   static void enterEventLoop( KJob* pJob, const QString& jobInfo );
   static QDialog *getDialog();
private:
};

extern ProgressDialog* g_pProgressDialog;

#endif

