/***************************************************************************
                         pdiff.cpp  -  Implementation for class KDiff3App
                         ---------------
    begin                : Mon M� 18 20:04:50 CET 2002
    copyright            : (C) 2002-2004 by Joachim Eibl
    email                : joachim.eibl@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "diff.h"
#include "directorymergewindow.h"

#include <iostream>
#include <algorithm>
#include <ctype.h>
#include <qaccel.h>

#include <kfiledialog.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kfontdialog.h>
#include <kstatusbar.h>
#include <kkeydialog.h>

#include <qclipboard.h>
#include <qscrollbar.h>
#include <qlayout.h>
#include <qsplitter.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qdir.h>
#include <qfile.h>
#include <qvbuttongroup.h>
#include <qradiobutton.h>
#include <qdragobject.h>
#include <assert.h>

#include "kdiff3.h"
#include "optiondialog.h"
#include "fileaccess.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "gnudiff_diff.h"

//using namespace std;

int g_tabSize = 8;
bool g_bIgnoreWhiteSpace = true;
bool g_bIgnoreTrivialMatches = true;


bool KDiff3App::runDiff( const LineData* p1, int size1, const LineData* p2, int size2, DiffList& diffList )
{
   ProgressProxy pp;
   static GnuDiff gnuDiff;  // All values are initialized with zeros.

   pp.setCurrent(0);

   diffList.clear();
   if ( p1[0].pLine==0 || p2[0].pLine==0 )
   {
      Diff d( 0,0,0);
      if ( p1[0].pLine==0 && p2[0].pLine==0 && size1 == size2 )
         d.nofEquals = size1;
      else 
      {
         d.diff1=size1;
         d.diff2=size2;
      }

      diffList.push_back(d);
   }
   else
   {
      GnuDiff::comparison comparisonInput;
      memset( &comparisonInput, 0, sizeof(comparisonInput) );
      comparisonInput.parent = 0;
      comparisonInput.file[0].buffer = (word*)p1[0].pLine;//ptr to buffer
      comparisonInput.file[0].buffered = (p1[size1-1].pLine-p1[0].pLine+p1[size1-1].size); // size of buffer
      comparisonInput.file[1].buffer = (word*)p2[0].pLine;//ptr to buffer
      comparisonInput.file[1].buffered = (p2[size2-1].pLine-p2[0].pLine+p2[size2-1].size); // size of buffer

      gnuDiff.ignore_white_space = GnuDiff::IGNORE_ALL_SPACE;  // I think nobody needs anything else ...
      gnuDiff.bIgnoreWhiteSpace = true;
      gnuDiff.bIgnoreNumbers    = m_pOptionDialog->m_bIgnoreNumbers;
      gnuDiff.minimal = m_pOptionDialog->m_bTryHard;
      gnuDiff.ignore_case = false;
      GnuDiff::change* script = gnuDiff.diff_2_files( &comparisonInput );

      int equalLinesAtStart =  comparisonInput.file[0].prefix_lines;
      int currentLine1 = 0;
      int currentLine2 = 0;
      GnuDiff::change* p=0;
      for (GnuDiff::change* e = script; e; e = p)
      {
         Diff d(0,0,0);
         d.nofEquals = e->line0 - currentLine1;
         assert( d.nofEquals == e->line1 - currentLine2 );
         d.diff1 = e->deleted;
         d.diff2 = e->inserted;
         currentLine1 += d.nofEquals + d.diff1;
         currentLine2 += d.nofEquals + d.diff2;
         diffList.push_back(d);

         p = e->link;
         free (e);
      }

      if ( diffList.empty() )
      {
         Diff d(0,0,0);
         d.nofEquals = min2(size1,size2);
         d.diff1 = size1 - d.nofEquals;
         d.diff2 = size2 - d.nofEquals;
         diffList.push_back(d);
/*         Diff d(0,0,0);
         d.nofEquals = equalLinesAtStart;
         if ( gnuDiff.files[0].missing_newline != gnuDiff.files[1].missing_newline )
         {
            d.diff1 = gnuDiff.files[0].missing_newline ? 0 : 1;
            d.diff2 = gnuDiff.files[1].missing_newline ? 0 : 1;
            ++d.nofEquals;
         }
         else if ( !gnuDiff.files[0].missing_newline )
         {
            ++d.nofEquals;
         }
         diffList.push_back(d);
*/
      }
      else
      {
         diffList.front().nofEquals += equalLinesAtStart;   
         currentLine1 += equalLinesAtStart;
         currentLine2 += equalLinesAtStart;

         int nofEquals = min2(size1-currentLine1,size2-currentLine2);
         if ( nofEquals==0 )
         {
            diffList.back().diff1 += size1-currentLine1;
            diffList.back().diff2 += size2-currentLine2;
         }
         else
         {
            Diff d( nofEquals,size1-currentLine1-nofEquals,size2-currentLine2-nofEquals);
            diffList.push_back(d);
         }

         /*
         if ( gnuDiff.files[0].missing_newline != gnuDiff.files[1].missing_newline )
         {
            diffList.back().diff1 += gnuDiff.files[0].missing_newline ? 0 : 1;
            diffList.back().diff2 += gnuDiff.files[1].missing_newline ? 0 : 1;
         }
         else if ( !gnuDiff.files[0].missing_newline )
         {
            ++ diffList.back().nofEquals;
         }
         */
      }
   }

#ifndef NDEBUG
   // Verify difflist
   {
      int l1=0;
      int l2=0;
      DiffList::iterator i;
      for( i = diffList.begin(); i!=diffList.end(); ++i )
      {
         l1+= i->nofEquals + i->diff1;
         l2+= i->nofEquals + i->diff2;
      }

      //if( l1!=p1-p1start || l2!=p2-p2start )
      if( l1!=size1 || l2!=size2 )
         assert( false );
   }
#endif

   pp.setCurrent(1.0);

   return true;
}


void KDiff3App::init( bool bAuto, TotalDiffStatus* pTotalDiffStatus )
{
   ProgressProxy pp;
   // When doing a full analysis in the directory-comparison, then the statistics-results
   // will be stored in the given TotalDiffStatus. Otherwise it will be 0.
   bool bGUI = pTotalDiffStatus == 0;
   if (pTotalDiffStatus==0) 
      pTotalDiffStatus = &m_totalDiffStatus;

   bool bPreserveCarriageReturn = m_pOptionDialog->m_bPreserveCarriageReturn;

   bool bVisibleMergeResultWindow = ! m_outputFilename.isEmpty();
   if ( bVisibleMergeResultWindow && bGUI )
   {
      bPreserveCarriageReturn = false;
      
      QString msg;
      
      if ( !m_pOptionDialog->m_PreProcessorCmd.isEmpty() )
      {
         msg += "- " + i18n("PreprocessorCmd: ") + m_pOptionDialog->m_PreProcessorCmd + "\n";
      }
      if ( !msg.isEmpty() )
      {
         int result = KMessageBox::warningYesNo( this,
                               i18n("The following option(s) you selected might change data:\n") + msg + 
                               i18n("\nMost likely this is not wanted during a merge.\n"
                                    "Do you want to disable these settings or continue with these settings active?"),
                               i18n("Option Unsafe for Merging"), 
                               i18n("Use These Options During Merge"), i18n("Disable Unsafe Options")
                               );
                               
         if (result == KMessageBox::No )
         {
            m_pOptionDialog->m_PreProcessorCmd = "";
         }
      }
   }

   m_diff3LineList.clear();

   // Because of the progressdialog paintevents can occur, but data is invalid,
   // so painting must be suppressed.
   if (m_pDiffTextWindow1) m_pDiffTextWindow1->setPaintingAllowed( false );
   if (m_pDiffTextWindow2) m_pDiffTextWindow2->setPaintingAllowed( false );
   if (m_pDiffTextWindow3) m_pDiffTextWindow3->setPaintingAllowed( false );
   if (m_pOverview)        m_pOverview->setPaintingAllowed( false );
   if (m_pMergeResultWindow) m_pMergeResultWindow->setPaintingAllowed( false );


   if( m_sd3.isEmpty() )
      pp.setMaxNofSteps( 4 );  // Read 2 files, 1 comparison, 1 finediff
   else
      pp.setMaxNofSteps( 9 );  // Read 3 files, 3 comparisons, 3 finediffs

   // First get all input data.
   pp.setInformation(i18n("Loading A"));
   m_sd1.readAndPreprocess(m_pOptionDialog->m_pEncodingA);
   pp.step();

   pp.setInformation(i18n("Loading B"));
   m_sd2.readAndPreprocess(m_pOptionDialog->m_pEncodingB);
   pp.step();

   pTotalDiffStatus->reset();
   // Run the diff.
   if ( m_sd3.isEmpty() )
   {
      pTotalDiffStatus->bBinaryAEqB = m_sd1.isBinaryEqualWith( m_sd2 );
      pp.setInformation(i18n("Diff: A <-> B"));
      
      runDiff( m_sd1.getLineDataForDiff(), m_sd1.getSizeLines(), m_sd2.getLineDataForDiff(), m_sd2.getSizeLines(), m_diffList12 );

      pp.step();

      pp.setInformation(i18n("Linediff: A <-> B"));
      calcDiff3LineListUsingAB( &m_diffList12, m_diff3LineList );
      fineDiff( m_diff3LineList, 1, m_sd1.getLineDataForDisplay(), m_sd2.getLineDataForDisplay(), pTotalDiffStatus->bTextAEqB );
      if ( m_sd1.getSizeBytes()==0 ) pTotalDiffStatus->bTextAEqB=false;

      pp.step();
   }
   else
   {
      pp.setInformation(i18n("Loading C"));
      m_sd3.readAndPreprocess(m_pOptionDialog->m_pEncodingC);
      pp.step();

      pTotalDiffStatus->bBinaryAEqB = m_sd1.isBinaryEqualWith( m_sd2 );
      pTotalDiffStatus->bBinaryAEqC = m_sd1.isBinaryEqualWith( m_sd3 );
      pTotalDiffStatus->bBinaryBEqC = m_sd3.isBinaryEqualWith( m_sd2 );

      pp.setInformation(i18n("Diff: A <-> B"));
      runDiff( m_sd1.getLineDataForDiff(), m_sd1.getSizeLines(), m_sd2.getLineDataForDiff(), m_sd2.getSizeLines(), m_diffList12 );
      pp.step();
      pp.setInformation(i18n("Diff: B <-> C"));
      runDiff( m_sd2.getLineDataForDiff(), m_sd2.getSizeLines(), m_sd3.getLineDataForDiff(), m_sd3.getSizeLines(), m_diffList23 );
      pp.step();
      pp.setInformation(i18n("Diff: A <-> C"));
      runDiff( m_sd1.getLineDataForDiff(), m_sd1.getSizeLines(), m_sd3.getLineDataForDiff(), m_sd3.getSizeLines(), m_diffList13 );
      pp.step();

      calcDiff3LineListUsingAB( &m_diffList12, m_diff3LineList );
      calcDiff3LineListUsingAC( &m_diffList13, m_diff3LineList );
      calcDiff3LineListTrim( m_diff3LineList, m_sd1.getLineDataForDiff(), m_sd2.getLineDataForDiff(), m_sd3.getLineDataForDiff() );

      calcDiff3LineListUsingBC( &m_diffList23, m_diff3LineList );
      calcDiff3LineListTrim( m_diff3LineList, m_sd1.getLineDataForDiff(), m_sd2.getLineDataForDiff(), m_sd3.getLineDataForDiff() );
      debugLineCheck( m_diff3LineList, m_sd1.getSizeLines(), 1 );
      debugLineCheck( m_diff3LineList, m_sd2.getSizeLines(), 2 );
      debugLineCheck( m_diff3LineList, m_sd3.getSizeLines(), 3 );

      pp.setInformation(i18n("Linediff: A <-> B"));
      fineDiff( m_diff3LineList, 1, m_sd1.getLineDataForDisplay(), m_sd2.getLineDataForDisplay(), pTotalDiffStatus->bTextAEqB );
      pp.step();
      pp.setInformation(i18n("Linediff: B <-> C"));
      fineDiff( m_diff3LineList, 2, m_sd2.getLineDataForDisplay(), m_sd3.getLineDataForDisplay(), pTotalDiffStatus->bTextBEqC );
      pp.step();
      pp.setInformation(i18n("Linediff: A <-> C"));
      fineDiff( m_diff3LineList, 3, m_sd3.getLineDataForDisplay(), m_sd1.getLineDataForDisplay(), pTotalDiffStatus->bTextAEqC );
      pp.step();
      if ( m_sd1.getSizeBytes()==0 ) { pTotalDiffStatus->bTextAEqB=false;  pTotalDiffStatus->bTextAEqC=false; }
      if ( m_sd2.getSizeBytes()==0 ) { pTotalDiffStatus->bTextAEqB=false;  pTotalDiffStatus->bTextBEqC=false; }
   }
   calcWhiteDiff3Lines( m_diff3LineList, m_sd1.getLineDataForDiff(), m_sd2.getLineDataForDiff(), m_sd3.getLineDataForDiff() );
   calcDiff3LineVector( m_diff3LineList, m_diff3LineVector );

   // Calc needed lines for display
   m_neededLines = m_diff3LineList.size();

   m_pDirectoryMergeWindow->allowResizeEvents(false);
   initView();
   if ( !bGUI )
   {
      m_pMainWidget->hide();
   }
   m_pDirectoryMergeWindow->allowResizeEvents(true);

   m_bTripleDiff = ! m_sd3.isEmpty();

   if ( bGUI )
   {
      m_pDiffTextWindow1->init( m_sd1.getAliasName(),
         m_sd1.getLineDataForDisplay(), m_sd1.getSizeLines(), &m_diff3LineVector, 1, m_bTripleDiff );
      m_pDiffTextWindow2->init( m_sd2.getAliasName(),
         m_sd2.getLineDataForDisplay(), m_sd2.getSizeLines(), &m_diff3LineVector, 2, m_bTripleDiff );
      m_pDiffTextWindow3->init( m_sd3.getAliasName(),
         m_sd3.getLineDataForDisplay(), m_sd3.getSizeLines(), &m_diff3LineVector, 3, m_bTripleDiff );
   
      if (m_bTripleDiff) m_pDiffTextWindow3->show();
      else               m_pDiffTextWindow3->hide();
   }

   m_bOutputModified = bVisibleMergeResultWindow;

   m_pMergeResultWindow->init(
      m_sd1.getLineDataForDisplay(),
      m_sd2.getLineDataForDisplay(),
      m_bTripleDiff ? m_sd3.getLineDataForDisplay() : 0,
      &m_diff3LineList,
      pTotalDiffStatus,
      m_outputFilename.isEmpty() ? QString("unnamed.txt") : m_outputFilename
      );
   
   if ( !bGUI ) 
   {
      // We now have all needed information. The rest below is only for GUI-activation.
      m_sd1.reset();      
      m_sd2.reset();      
      m_sd3.reset();      
      return;
   }
      
   m_pOverview->init(&m_diff3LineList, m_bTripleDiff );
   m_pDiffVScrollBar->setValue( 0 );
   m_pHScrollBar->setValue( 0 );
   m_pMergeVScrollBar->setValue( 0 );

   m_pDiffTextWindow1->setPaintingAllowed( true );
   m_pDiffTextWindow2->setPaintingAllowed( true );
   m_pDiffTextWindow3->setPaintingAllowed( true );
   m_pOverview->setPaintingAllowed( true );
   m_pMergeResultWindow->setPaintingAllowed( true );


   if ( !bVisibleMergeResultWindow )
      m_pMergeWindowFrame->hide();
   else
      m_pMergeWindowFrame->show();

   // Calc max width for display
   m_maxWidth = max2( m_pDiffTextWindow1->getNofColumns(), m_pDiffTextWindow2->getNofColumns() );
   m_maxWidth = max2( m_maxWidth, m_pDiffTextWindow3->getNofColumns() );
   m_maxWidth += 5;

   // Try to create a meaningful but not too long caption
   if ( !isPart() )
   {
      // 1. If the filenames are equal then show only one filename
      QString caption;
      QString a1 = m_sd1.getAliasName();
      QString a2 = m_sd2.getAliasName();
      QString a3 = m_sd3.getAliasName();
      QString f1, f2, f3;
      int p1,p2,p3;
      if ( !a1.isEmpty() &&  (p1=a1.findRev('/'))>=0 )
         f1 = a1.mid( p1+1 );
      if ( !a2.isEmpty() &&  (p2=a2.findRev('/'))>=0 )
         f2 = a2.mid( p2+1 );
      if ( !a3.isEmpty() &&  (p3=a3.findRev('/'))>=0 )
         f3 = a3.mid( p3+1 );
      if ( !f1.isEmpty() ) 
      {
         if ( ( f2.isEmpty() && f3.isEmpty() ) || 
              (f2.isEmpty() && f1==f3) || ( f3.isEmpty() && f1==f2 ) || (f1==f2 && f1==f3)) 
            caption = ".../"+f1;
      }
      else if ( ! f2.isEmpty() ) 
      {
         if ( f3.isEmpty() || f2==f3 ) 
            caption = ".../"+f2;
      }
      else if ( ! f3.isEmpty() ) 
         caption = ".../"+f3;
         
      // 2. If the files don't have the same name then show all names
      if ( caption.isEmpty() && (!f1.isEmpty() || !f2.isEmpty() || !f3.isEmpty()) )
      {
         caption = ( f1.isEmpty()? QString("") : QString(".../")+f1 );
         caption += QString(caption.isEmpty() || f2.isEmpty() ? "" : " <-> ") + ( f2.isEmpty()? QString("") : QString(".../")+f2 );
         caption += QString(caption.isEmpty() || f3.isEmpty() ? "" : " <-> ") + ( f3.isEmpty()? QString("") : QString(".../")+f3 ) ;
      }      
      
      m_pKDiff3Shell->setCaption( caption.isEmpty() ? QString("KDiff3") : caption+QString(" - KDiff3"));
   }
   
   
   if ( bVisibleMergeResultWindow && !bAuto )
      m_pMergeResultWindow->showNrOfConflicts();
   else if ( !bAuto && 
      // Avoid showing this message during startup without parameters.
      !( m_sd1.getAliasName().isEmpty() && m_sd2.getAliasName().isEmpty() && m_sd3.getAliasName().isEmpty() ) &&
      ( m_sd1.isValid() && m_sd2.isValid() && m_sd3.isValid() )
      )
   {
      QString totalInfo;
      if ( pTotalDiffStatus->bBinaryAEqB && pTotalDiffStatus->bBinaryAEqC )
         totalInfo += i18n("All input files are binary equal.");
      else  if ( pTotalDiffStatus->bTextAEqB && pTotalDiffStatus->bTextAEqC )
         totalInfo += i18n("All input files contain the same text.");
      else {
         if    ( pTotalDiffStatus->bBinaryAEqB ) totalInfo += i18n("Files A and B are binary equal.\n");
         else if ( pTotalDiffStatus->bTextAEqB ) totalInfo += i18n("Files A and B have equal text. \n");
         if    ( pTotalDiffStatus->bBinaryAEqC ) totalInfo += i18n("Files A and C are binary equal.\n");
         else if ( pTotalDiffStatus->bTextAEqC ) totalInfo += i18n("Files A and C have equal text. \n");
         if    ( pTotalDiffStatus->bBinaryBEqC ) totalInfo += i18n("Files B and C are binary equal.\n");
         else if ( pTotalDiffStatus->bTextBEqC ) totalInfo += i18n("Files B and C have equal text. \n");
      }

      if ( !totalInfo.isEmpty() )
         KMessageBox::information( this, totalInfo );
   }

   if ( bVisibleMergeResultWindow && (!m_sd1.isText() || !m_sd2.isText() || !m_sd3.isText()) )
   {
      KMessageBox::information( this, i18n(
         "Some inputfiles don't seem to be pure textfiles.\n"
         "Note that the KDiff3-merge was not meant for binary data.\n"
         "Continue at your own risk.") );
/*      QDialog d(this);
      QVBoxLayout* l = new QVBoxLayout(&d);
      QVButtonGroup* bg = new QVButtonGroup(i18n("Some File(s) Don't Seem to be Text Files"),&d);
      l->addWidget(bg);
      new QRadioButton("A: " + m_sd1.getAliasName(), bg);
      new QRadioButton("B: " + m_sd2.getAliasName(), bg);
      if (!m_sd3.isEmpty()) new QRadioButton("C: " + m_sd3.getAliasName(), bg);
      bg->setButton(0);
      QHBoxLayout* hbl = new QHBoxLayout(l);
      QPushButton* pSaveButton = new QPushButton( i18n("Save Selection"), &d );
      connect( pSaveButton, SIGNAL(clicked()), &d, SLOT(accept()));
      hbl->addWidget( pSaveButton );
      QPushButton* pProceedButton = new QPushButton( i18n("Proceed with Text-Merge"), &d );
      connect( pProceedButton, SIGNAL(clicked()), &d, SLOT(reject()));
      hbl->addWidget( pProceedButton );

      int result = d.exec();
      if( result == QDialog::Accepted )
      {
         if      ( bg->find(0)->isOn() ) saveBuffer( m_sd1.m_pBuf, m_sd1.m_size, m_outputFilename );
         else if ( bg->find(1)->isOn() ) saveBuffer( m_sd2.m_pBuf, m_sd2.m_size, m_outputFilename );
         else                            saveBuffer( m_sd3.m_pBuf, m_sd3.m_size, m_outputFilename );
      }*/
   }

   QTimer::singleShot( 10, this, SLOT(slotAfterFirstPaint()) );
   
   if ( bVisibleMergeResultWindow && m_pMergeResultWindow )
   {
      m_pMergeResultWindow->setFocus();
   }
   else if(m_pDiffTextWindow1)
   {
      m_pDiffTextWindow1->setFocus();
   }
   
}


void KDiff3App::resizeDiffTextWindow(int newWidth, int newHeight)
{
   recalcWordWrap();

   m_DTWHeight = newHeight;
   m_pDiffVScrollBar->setRange(0, max2(0, m_neededLines+1 - newHeight) );
   m_pDiffVScrollBar->setPageStep( newHeight );
   m_pOverview->setRange( m_pDiffVScrollBar->value(), m_pDiffVScrollBar->pageStep() );

   // The second window has a somewhat inverse width

   m_pHScrollBar->setRange(0, max2(0, m_maxWidth - newWidth) );
   m_pHScrollBar->setPageStep( newWidth );
}

void KDiff3App::resizeMergeResultWindow()
{
   MergeResultWindow* p = m_pMergeResultWindow;
   m_pMergeVScrollBar->setRange(0, max2(0, p->getNofLines() - p->getNofVisibleLines()) );
   m_pMergeVScrollBar->setPageStep( p->getNofVisibleLines() );

   // The second window has a somewhat inverse width
//   m_pHScrollBar->setRange(0, max2(0, m_maxWidth - newWidth) );
//   m_pHScrollBar->setPageStep( newWidth );
}

void KDiff3App::scrollDiffTextWindow( int deltaX, int deltaY )
{
   if ( deltaY!= 0 )
   {
      m_pDiffVScrollBar->setValue( m_pDiffVScrollBar->value() + deltaY );
      m_pOverview->setRange( m_pDiffVScrollBar->value(), m_pDiffVScrollBar->pageStep() );
   }
   if ( deltaX!= 0)
      m_pHScrollBar->setValue( m_pHScrollBar->value() + deltaX );
}

void KDiff3App::scrollMergeResultWindow( int deltaX, int deltaY )
{
   if ( deltaY!= 0 )
      m_pMergeVScrollBar->setValue( m_pMergeVScrollBar->value() + deltaY );
   if ( deltaX!= 0)
      m_pHScrollBar->setValue( m_pHScrollBar->value() + deltaX );
}

void KDiff3App::setDiff3Line( int line )
{
   m_pDiffVScrollBar->setValue( line );
}

void KDiff3App::sourceMask( int srcMask, int enabledMask )
{
   chooseA->setChecked( (srcMask & 1) != 0 );
   chooseB->setChecked( (srcMask & 2) != 0 );
   chooseC->setChecked( (srcMask & 4) != 0 );
   chooseA->setEnabled( (enabledMask & 1) != 0 );
   chooseB->setEnabled( (enabledMask & 2) != 0 );
   chooseC->setEnabled( (enabledMask & 4) != 0 );
}



// Function uses setMinSize( sizeHint ) before adding the widget.
// void addWidget(QBoxLayout* layout, QWidget* widget);
template <class W, class L>
void addWidget( L* layout, W* widget)
{
   QSize s = widget->sizeHint();
   widget->setMinimumSize( QSize(max2(s.width(),0),max2(s.height(),0) ) );
   layout->addWidget( widget );
}

void KDiff3App::initView()
{
   // set the main widget here
   QValueList<int> oldHeights;
   if ( m_pDirectoryMergeSplitter->isVisible() )
   {
      oldHeights = m_pMainSplitter->sizes();
   }

   if ( m_pMainWidget != 0 )
   {
      return;
      //delete m_pMainWidget;
   }
   m_pMainWidget = new QFrame(m_pMainSplitter);
   m_pMainWidget->setFrameStyle( QFrame::Panel | QFrame::Sunken );
   m_pMainWidget->setLineWidth(1);


   QVBoxLayout* pVLayout = new QVBoxLayout(m_pMainWidget);

   QSplitter* pVSplitter = new QSplitter( m_pMainWidget );
   pVSplitter->setOrientation( Vertical );
   pVLayout->addWidget( pVSplitter );

   QWidget* pDiffWindowFrame = new QFrame( pVSplitter );
   QHBoxLayout* pDiffHLayout = new QHBoxLayout( pDiffWindowFrame );

   m_pDiffWindowSplitter = new QSplitter( pDiffWindowFrame );
   m_pDiffWindowSplitter->setOrientation( m_pOptionDialog->m_bHorizDiffWindowSplitting ?  Horizontal : Vertical );
   pDiffHLayout->addWidget( m_pDiffWindowSplitter );

   m_pOverview = new Overview( pDiffWindowFrame, m_pOptionDialog );
   pDiffHLayout->addWidget(m_pOverview);
   connect( m_pOverview, SIGNAL(setLine(int)), this, SLOT(setDiff3Line(int)) );
   //connect( m_pOverview, SIGNAL(afterFirstPaint()), this, SLOT(slotAfterFirstPaint()));

   m_pDiffVScrollBar = new QScrollBar( Vertical, pDiffWindowFrame );
   pDiffHLayout->addWidget( m_pDiffVScrollBar );

   m_pDiffTextWindow1 = new DiffTextWindow( m_pDiffWindowSplitter, statusBar(), m_pOptionDialog );
   m_pDiffTextWindow2 = new DiffTextWindow( m_pDiffWindowSplitter, statusBar(), m_pOptionDialog );
   m_pDiffTextWindow3 = new DiffTextWindow( m_pDiffWindowSplitter, statusBar(), m_pOptionDialog );

   // Merge window
   m_pMergeWindowFrame = new QFrame( pVSplitter );
   QHBoxLayout* pMergeHLayout = new QHBoxLayout( m_pMergeWindowFrame );

   m_pMergeResultWindow = new MergeResultWindow( m_pMergeWindowFrame, m_pOptionDialog, statusBar() );
   pMergeHLayout->addWidget( m_pMergeResultWindow );

   m_pMergeVScrollBar = new QScrollBar( Vertical, m_pMergeWindowFrame );
   pMergeHLayout->addWidget( m_pMergeVScrollBar );

   autoAdvance->setEnabled(true);

   QValueList<int> sizes = pVSplitter->sizes();
   int total = sizes[0] + sizes[1];
   sizes[0]=total/2; sizes[1]=total/2;
   pVSplitter->setSizes( sizes );

   m_pMergeResultWindow->installEventFilter( this );       // for Cut/Copy/Paste-shortcuts

   QHBoxLayout* pHScrollBarLayout = new QHBoxLayout( pVLayout );
   m_pHScrollBar = new ReversibleScrollBar( Horizontal, m_pMainWidget, &m_pOptionDialog->m_bRightToLeftLanguage );
   pHScrollBarLayout->addWidget( m_pHScrollBar );
   m_pCornerWidget = new QWidget( m_pMainWidget );
   pHScrollBarLayout->addWidget( m_pCornerWidget );


   connect( m_pDiffVScrollBar, SIGNAL(valueChanged(int)), m_pOverview, SLOT(setFirstLine(int)));
   connect( m_pDiffVScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow1, SLOT(setFirstLine(int)));
   connect( m_pHScrollBar, SIGNAL(valueChanged2(int)), m_pDiffTextWindow1, SLOT(setFirstColumn(int)));
   connect( m_pDiffTextWindow1, SIGNAL(newSelection()), this, SLOT(slotSelectionStart()));
   connect( m_pDiffTextWindow1, SIGNAL(selectionEnd()), this, SLOT(slotSelectionEnd()));
   connect( m_pDiffTextWindow1, SIGNAL(scroll(int,int)), this, SLOT(scrollDiffTextWindow(int,int)));
   m_pDiffTextWindow1->installEventFilter( this );

   connect( m_pDiffVScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow2, SLOT(setFirstLine(int)));
   connect( m_pHScrollBar, SIGNAL(valueChanged2(int)), m_pDiffTextWindow2, SLOT(setFirstColumn(int)));
   connect( m_pDiffTextWindow2, SIGNAL(newSelection()), this, SLOT(slotSelectionStart()));
   connect( m_pDiffTextWindow2, SIGNAL(selectionEnd()), this, SLOT(slotSelectionEnd()));
   connect( m_pDiffTextWindow2, SIGNAL(scroll(int,int)), this, SLOT(scrollDiffTextWindow(int,int)));
   m_pDiffTextWindow2->installEventFilter( this );

   connect( m_pDiffVScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow3, SLOT(setFirstLine(int)));
   connect( m_pHScrollBar, SIGNAL(valueChanged2(int)), m_pDiffTextWindow3, SLOT(setFirstColumn(int)));
   connect( m_pDiffTextWindow3, SIGNAL(newSelection()), this, SLOT(slotSelectionStart()));
   connect( m_pDiffTextWindow3, SIGNAL(selectionEnd()), this, SLOT(slotSelectionEnd()));
   connect( m_pDiffTextWindow3, SIGNAL(scroll(int,int)), this, SLOT(scrollDiffTextWindow(int,int)));
   m_pDiffTextWindow3->installEventFilter( this );

   MergeResultWindow* p = m_pMergeResultWindow;
   connect( m_pMergeVScrollBar, SIGNAL(valueChanged(int)), p, SLOT(setFirstLine(int)));

   connect( m_pHScrollBar,      SIGNAL(valueChanged2(int)), p, SLOT(setFirstColumn(int)));
   connect( p, SIGNAL(scroll(int,int)), this, SLOT(scrollMergeResultWindow(int,int)));
   connect( p, SIGNAL(sourceMask(int,int)), this, SLOT(sourceMask(int,int)));
   connect( p, SIGNAL( resizeSignal() ),this, SLOT(resizeMergeResultWindow()));
   connect( p, SIGNAL( selectionEnd() ), this, SLOT( slotSelectionEnd() ) );
   connect( p, SIGNAL( newSelection() ), this, SLOT( slotSelectionStart() ) );
   connect( p, SIGNAL( modified() ), this, SLOT( slotOutputModified() ) );
   connect( p, SIGNAL( updateAvailabilities() ), this, SLOT( slotUpdateAvailabilities() ) );
   connect( p, SIGNAL( showPopupMenu(const QPoint&) ), this, SLOT(showPopupMenu(const QPoint&)));
   sourceMask(0,0);


   connect( p, SIGNAL(setFastSelectorRange(int,int)), m_pDiffTextWindow1, SLOT(setFastSelectorRange(int,int)));
   connect( p, SIGNAL(setFastSelectorRange(int,int)), m_pDiffTextWindow2, SLOT(setFastSelectorRange(int,int)));
   connect( p, SIGNAL(setFastSelectorRange(int,int)), m_pDiffTextWindow3, SLOT(setFastSelectorRange(int,int)));
   connect(m_pDiffTextWindow1, SIGNAL(setFastSelectorLine(int)), p, SLOT(slotSetFastSelectorLine(int)));
   connect(m_pDiffTextWindow2, SIGNAL(setFastSelectorLine(int)), p, SLOT(slotSetFastSelectorLine(int)));
   connect(m_pDiffTextWindow3, SIGNAL(setFastSelectorLine(int)), p, SLOT(slotSetFastSelectorLine(int)));
   connect(m_pDiffTextWindow1, SIGNAL(gotFocus()), p, SLOT(updateSourceMask()));
   connect(m_pDiffTextWindow2, SIGNAL(gotFocus()), p, SLOT(updateSourceMask()));
   connect(m_pDiffTextWindow3, SIGNAL(gotFocus()), p, SLOT(updateSourceMask()));
   connect(m_pDirectoryMergeInfo, SIGNAL(gotFocus()), p, SLOT(updateSourceMask()));

   connect( m_pDiffTextWindow1, SIGNAL( resizeSignal(int,int) ),this, SLOT(resizeDiffTextWindow(int,int)));
   // The following two connects cause the wordwrap to be recalced thrice, just to make sure. Better than forgetting one.
   connect( m_pDiffTextWindow2, SIGNAL( resizeSignal(int,int) ),this, SLOT(recalcWordWrap()));
   connect( m_pDiffTextWindow3, SIGNAL( resizeSignal(int,int) ),this, SLOT(recalcWordWrap()));

   m_pDiffTextWindow1->setFocus();
   m_pMainWidget->setMinimumSize(50,50);
   if ( m_pDirectoryMergeSplitter->isVisible() )
   {
      if (oldHeights.count() < 2)
         oldHeights.append(0);
      if (oldHeights[1]==0)    // Distribute the available space evenly between the two widgets.
      {
         oldHeights[1] = oldHeights[0]/2;
         oldHeights[0] -= oldHeights[1];
      }
      m_pMainSplitter->setSizes( oldHeights );
   }
   m_pCornerWidget->setFixedSize( m_pDiffVScrollBar->width(), m_pHScrollBar->height() );
   //show();
   m_pMainWidget->show();
   showWindowA->setChecked( true );
   showWindowB->setChecked( true );
   showWindowC->setChecked( true );
}

void KDiff3App::slotAfterFirstPaint()
{
   recalcWordWrap();
   int newHeight = m_pDiffTextWindow1->getNofVisibleLines();
   int newWidth  = m_pDiffTextWindow1->getNofVisibleColumns();
   m_DTWHeight = newHeight;
   m_pDiffVScrollBar->setRange(0, max2(0, m_neededLines+1 - newHeight) );
   m_pDiffVScrollBar->setPageStep( newHeight );
   m_pOverview->setRange( m_pDiffVScrollBar->value(), m_pDiffVScrollBar->pageStep() );

   // The second window has a somewhat inverse width
   m_pHScrollBar->setRange(0, max2(0, m_maxWidth - newWidth) );
   m_pHScrollBar->setPageStep( newWidth );

   m_pMergeResultWindow->slotGoTop();

   if (m_pCornerWidget)
      m_pCornerWidget->setFixedSize( m_pDiffVScrollBar->width(), m_pHScrollBar->height() );

   slotUpdateAvailabilities();
}

void KDiff3App::resizeEvent(QResizeEvent* e)
{
   QSplitter::resizeEvent(e);
   if (m_pCornerWidget)
      m_pCornerWidget->setFixedSize( m_pDiffVScrollBar->width(), m_pHScrollBar->height() );
}


bool KDiff3App::eventFilter( QObject* o, QEvent* e )
{
   if( o == m_pMergeResultWindow )
   {
      if ( e->type() == QEvent::KeyPress )
      {  // key press
         QKeyEvent *k = (QKeyEvent*)e;
         if (k->key()==Qt::Key_Insert &&  (k->state() & Qt::ControlButton)!=0 )
         {
            slotEditCopy();
            return true;
         }
         if (k->key()==Qt::Key_Insert &&  (k->state() & Qt::ShiftButton)!=0 )
         {
            slotEditPaste();
            return true;
         }
         if (k->key()==Qt::Key_Delete &&  (k->state() & Qt::ShiftButton)!=0 )
         {
            slotEditCut();
            return true;
         }
      }
      return QSplitter::eventFilter( o, e );    // standard event processing
   }

   if ( e->type() == QEvent::KeyPress )   // key press
   {
       QKeyEvent *k = (QKeyEvent*)e;

       bool bCtrl = (k->state() & Qt::ControlButton) != 0;
       if (k->key()==Qt::Key_Insert &&  bCtrl )
       {
          slotEditCopy();
          return true;
       }
       if (k->key()==Qt::Key_Insert &&  (k->state() & Qt::ShiftButton)!=0 )
       {
          slotEditPaste();
          return true;
       }
       int deltaX=0;
       int deltaY=0;
       int pageSize =  m_DTWHeight;
       switch( k->key() )
       {
       case Key_Down:   ++deltaY; break;
       case Key_Up:     --deltaY; break;
       case Key_PageDown: if (!bCtrl) deltaY+=pageSize; break;
       case Key_PageUp:   if (!bCtrl) deltaY-=pageSize; break;
       case Key_Left:     --deltaX;  break;
       case Key_Right:    ++deltaX;  break;
       case Key_Home: if ( bCtrl )  m_pDiffVScrollBar->setValue( 0 );
                      else          m_pHScrollBar->setValue( 0 );
                      break;
       case Key_End:  if ( bCtrl )  m_pDiffVScrollBar->setValue( m_pDiffVScrollBar->maxValue() );
                      else          m_pHScrollBar->setValue( m_pHScrollBar->maxValue() );
                      break;
       default: break;
       }

       scrollDiffTextWindow( deltaX, deltaY );

       return true;                        // eat event
   }
   else if (e->type() == QEvent::Wheel )   // wheel event
   {
       QWheelEvent *w = (QWheelEvent*)e;
       w->accept();

       int deltaX=0;

       int d=w->delta();
       int deltaY = -d/120 * QApplication::wheelScrollLines();

       scrollDiffTextWindow( deltaX, deltaY );
       return true;
   }
   else if (e->type() == QEvent::Drop )
   {
      QDropEvent* pDropEvent = static_cast<QDropEvent*>(e);
      pDropEvent->accept();

      if ( QUriDrag::canDecode(pDropEvent) )
      {
#ifdef KREPLACEMENTS_H
         QStringList stringList;
         QUriDrag::decodeLocalFiles( pDropEvent, stringList );
         if ( canContinue() && !stringList.isEmpty() )
         {
            raise();
            QString filename = stringList.first();
            if      ( o == m_pDiffTextWindow1 ) m_sd1.setFilename( filename );
            else if ( o == m_pDiffTextWindow2 ) m_sd2.setFilename( filename );
            else if ( o == m_pDiffTextWindow3 ) m_sd3.setFilename( filename );
            init();
         }
#else
         KURL::List urlList;
         KURLDrag::decode( pDropEvent, urlList );
         if ( canContinue() && !urlList.isEmpty() )
         {
            raise();
            FileAccess fa( urlList.first().url() );
            if      ( o == m_pDiffTextWindow1 ) m_sd1.setFileAccess( fa );
            else if ( o == m_pDiffTextWindow2 ) m_sd2.setFileAccess( fa );
            else if ( o == m_pDiffTextWindow3 ) m_sd3.setFileAccess( fa );
            init();
         }
#endif
      }
      else if ( QTextDrag::canDecode(pDropEvent) )
      {
         QString text;
         bool bDecodeSuccess = QTextDrag::decode( pDropEvent, text );
         if ( bDecodeSuccess && canContinue() )
         {
            raise();
            if      ( o == m_pDiffTextWindow1 ) m_sd1.setData(text);
            else if ( o == m_pDiffTextWindow2 ) m_sd2.setData(text);
            else if ( o == m_pDiffTextWindow3 ) m_sd3.setData(text);
            init();
         }
      }

      return true;
   }
   return QSplitter::eventFilter( o, e );    // standard event processing
}



OpenDialog::OpenDialog(
   QWidget* pParent, const QString& n1, const QString& n2, const QString& n3,
   bool bMerge, const QString& outputName, const char* slotConfigure, OptionDialog* pOptions )
: QDialog( pParent, "OpenDialog", true /*modal*/ )
{
   m_pOptions = pOptions;

   QVBoxLayout* v = new QVBoxLayout( this, 5 );
   QGridLayout* h = new QGridLayout( v, 5, 4, 5 );
   h->setColStretch( 1, 10 );

   QLabel* label  = new QLabel( i18n("A (Base):"), this );

   m_lineA = new QComboBox( true, this );
   m_lineA->insertStringList( m_pOptions->m_recentAFiles );
   m_lineA->setEditText( KURL(n1).prettyURL() );
   m_lineA->setMinimumSize( 200, m_lineA->size().height() );
   QPushButton * button = new QPushButton( i18n("File..."), this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectFileA() ) );
   QPushButton * button2 = new QPushButton( i18n("Dir..."), this );
   connect( button2, SIGNAL(clicked()), this, SLOT( selectDirA() ) );
   connect( m_lineA, SIGNAL(textChanged(const QString&)), this, SLOT(inputFilenameChanged() ) );

   h->addWidget( label,    0, 0 );
   h->addWidget( m_lineA,  0, 1 );
   h->addWidget( button,   0, 2 );
   h->addWidget( button2,  0, 3 );

   label    = new QLabel( "B:", this );
   m_lineB  = new QComboBox( true, this );
   m_lineB->insertStringList( m_pOptions->m_recentBFiles );
   m_lineB->setEditText( KURL(n2).prettyURL() );
   m_lineB->setMinimumSize( 200, m_lineB->size().height() );
   button   = new QPushButton( i18n("File..."), this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectFileB() ) );
   button2   = new QPushButton( i18n("Dir..."), this );
   connect( button2, SIGNAL(clicked()), this, SLOT( selectDirB() ) );
   connect( m_lineB, SIGNAL(textChanged(const QString&)), this, SLOT(inputFilenameChanged() ) );

   h->addWidget( label,     1, 0 );
   h->addWidget( m_lineB,   1, 1 );
   h->addWidget( button,    1, 2 );
   h->addWidget( button2,   1, 3 );

   label  = new QLabel( i18n("C (Optional):"), this );
   m_lineC= new QComboBox( true, this );
   m_lineC->insertStringList( m_pOptions->m_recentCFiles );
   m_lineC->setEditText( KURL(n3).prettyURL() );
   m_lineC->setMinimumSize( 200, m_lineC->size().height() );
   button = new QPushButton( i18n("File..."), this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectFileC() ) );
   button2   = new QPushButton( i18n("Dir..."), this );
   connect( button2, SIGNAL(clicked()), this, SLOT( selectDirC() ) );
   connect( m_lineC, SIGNAL(textChanged(const QString&)), this, SLOT(inputFilenameChanged() ) );

   h->addWidget( label,     2, 0 );
   h->addWidget( m_lineC,   2, 1 );
   h->addWidget( button,    2, 2 );
   h->addWidget( button2,   2, 3 );

   m_pMerge = new QCheckBox( i18n("Merge"), this );
   h->addWidget( m_pMerge, 3, 0 );

   label  = new QLabel( i18n("Output (optional):"), this );
   m_lineOut = new QComboBox( true, this );
   m_lineOut->insertStringList( m_pOptions->m_recentOutputFiles );
   m_lineOut->setEditText( KURL(outputName).prettyURL() );
   m_lineOut->setMinimumSize( 200, m_lineOut->size().height() );
   button = new QPushButton( i18n("File..."), this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectOutputName() ) );
   button2   = new QPushButton( i18n("Dir..."), this );
   connect( button2, SIGNAL(clicked()), this, SLOT( selectOutputDir() ) );
   connect( m_pMerge, SIGNAL(stateChanged(int)), this, SLOT(internalSlot(int)) );
   connect( this, SIGNAL(internalSignal(bool)), m_lineOut, SLOT(setEnabled(bool)) );
   connect( this, SIGNAL(internalSignal(bool)), button, SLOT(setEnabled(bool)) );
   connect( this, SIGNAL(internalSignal(bool)), button2, SLOT(setEnabled(bool)) );

   m_pMerge->setChecked( !bMerge );
   m_pMerge->setChecked( bMerge );
//   m_lineOutput->setEnabled( bMerge );

//   button->setEnabled( bMerge );

   h->addWidget( label,          4, 0 );
   h->addWidget( m_lineOut,      4, 1 );
   h->addWidget( button,         4, 2 );
   h->addWidget( button2,        4, 3 );

   h->addColSpacing( 1, 200 );

   QHBoxLayout* l = new QHBoxLayout( v, 5 );

   button = new QPushButton( i18n("Configure..."), this );
   connect( button, SIGNAL(clicked()), pParent, slotConfigure );
   l->addWidget( button, 1 );

   l->addStretch(1);

   button = new QPushButton( i18n("&OK"), this );
   button->setDefault( true );
   connect( button, SIGNAL(clicked()), this, SLOT( accept() ) );
   l->addWidget( button, 1 );

   button = new QPushButton( i18n("&Cancel"), this );
   connect( button, SIGNAL(clicked()), this, SLOT( reject() ) );
   l->addWidget( button,1 );

   QSize sh = sizeHint();
   setFixedHeight( sh.height() );
   m_bInputFileNameChanged = false;

#ifdef KREPLACEMENTS_H
   m_lineA->lineEdit()->installEventFilter( this );
   m_lineB->lineEdit()->installEventFilter( this );
   m_lineC->lineEdit()->installEventFilter( this );
   m_lineOut->lineEdit()->installEventFilter( this );
#endif
}

// Eventfilter: Only needed under Windows.
// Without this, files dropped in the line edit have URL-encoding.
// This eventfilter decodes the filenames as needed by KDiff3.
bool OpenDialog::eventFilter(QObject* o, QEvent* e)
{
   if (e->type()==QEvent::Drop)
   {
      QDropEvent* d = static_cast<QDropEvent*>(e);

      if ( !QUriDrag::canDecode( d ) ) {
         return false;
      }

      QStringList lst;
      QUriDrag::decodeLocalFiles( d, lst );

      if ( lst.count() > 0 )
      {
         static_cast<QLineEdit*>(o)->setText( lst[0] );
         static_cast<QLineEdit*>(o)->setFocus();
      }
       
      return true;
   }
   return false;
}


void OpenDialog::selectURL( QComboBox* pLine, bool bDir, int i, bool bSave )
{
   QString current = pLine->currentText();
   if (current.isEmpty() && i>3 ){  current = m_lineC->currentText(); }
   if (current.isEmpty()        ){  current = m_lineB->currentText(); }
   if (current.isEmpty()        ){  current = m_lineA->currentText(); }
   KURL newURL = bDir ? KFileDialog::getExistingURL( current, this)
                      : bSave ? KFileDialog::getSaveURL( current, 0, this)
                              : KFileDialog::getOpenURL( current, 0, this);
   if ( !newURL.isEmpty() )
   {
      pLine->setEditText( newURL.url() );
   }
   // newURL won't be modified if nothing was selected.
}

void OpenDialog::selectFileA()     {  selectURL( m_lineA,    false, 1, false );  }
void OpenDialog::selectFileB()     {  selectURL( m_lineB,    false, 2, false );  }
void OpenDialog::selectFileC()     {  selectURL( m_lineC,    false, 3, false );  }
void OpenDialog::selectOutputName(){  selectURL( m_lineOut,  false, 4, true );  }
void OpenDialog::selectDirA()      {  selectURL( m_lineA,    true,  1, false );  }
void OpenDialog::selectDirB()      {  selectURL( m_lineB,    true,  2, false );  }
void OpenDialog::selectDirC()      {  selectURL( m_lineC,    true,  3, false );  }
void OpenDialog::selectOutputDir() {  selectURL( m_lineOut,  true,  4, true );  }

void OpenDialog::internalSlot(int i)
{
   emit internalSignal(i!=0);
}

// Clear the output-filename when any input-filename changed, 
// because users forgot to change the output and accidently overwrote it with
// wrong data during a merge.
void OpenDialog::inputFilenameChanged()
{   
   if(!m_bInputFileNameChanged)
   {
      m_bInputFileNameChanged=true;
      m_lineOut->clearEdit();
   }
}

void OpenDialog::accept()
{
   unsigned int maxNofRecentFiles = 10;

   QString s = m_lineA->currentText();
   s = KURL::fromPathOrURL(s).prettyURL();
   QStringList* sl = &m_pOptions->m_recentAFiles;
   // If an item exist, remove it from the list and reinsert it at the beginning.
   sl->remove(s);
   if ( !s.isEmpty() ) sl->prepend( s );
   if (sl->count()>maxNofRecentFiles) sl->erase( sl->at(maxNofRecentFiles), sl->end() );

   s = m_lineB->currentText();
   s = KURL::fromPathOrURL(s).prettyURL();
   sl = &m_pOptions->m_recentBFiles;
   sl->remove(s);
   if ( !s.isEmpty() ) sl->prepend( s );
   if (sl->count()>maxNofRecentFiles) sl->erase( sl->at(maxNofRecentFiles), sl->end() );

   s = m_lineC->currentText();
   s = KURL::fromPathOrURL(s).prettyURL();
   sl = &m_pOptions->m_recentCFiles;
   sl->remove(s);
   if ( !s.isEmpty() ) sl->prepend( s );
   if (sl->count()>maxNofRecentFiles) sl->erase( sl->at(maxNofRecentFiles), sl->end() );

   s = m_lineOut->currentText();
   s = KURL::fromPathOrURL(s).prettyURL();
   sl = &m_pOptions->m_recentOutputFiles;
   sl->remove(s);
   if ( !s.isEmpty() ) sl->prepend( s );
   if (sl->count()>maxNofRecentFiles) sl->erase( sl->at(maxNofRecentFiles), sl->end() );

   QDialog::accept();
}

void KDiff3App::slotFileOpen()
{
   if ( !canContinue() ) return;

   if ( m_pDirectoryMergeWindow->isDirectoryMergeInProgress() )
   {
      int result = KMessageBox::warningYesNo(this,
         i18n("You are currently doing a directory merge. Are you sure, you want to abort?"),
         i18n("Warning"), i18n("Abort"), i18n("Continue Merging") );
      if ( result!=KMessageBox::Yes )
         return;
   }


   slotStatusMsg(i18n("Opening files..."));

   for(;;)
   {
      OpenDialog d(this,
         m_sd1.isFromBuffer() ? QString("") : m_sd1.getAliasName(),
         m_sd2.isFromBuffer() ? QString("") : m_sd2.getAliasName(),
         m_sd3.isFromBuffer() ? QString("") : m_sd3.getAliasName(),
         !m_outputFilename.isEmpty(),
         m_bDefaultFilename ? QString("") : m_outputFilename,
         SLOT(slotConfigure()), m_pOptionDialog );
      int status = d.exec();
      if ( status == QDialog::Accepted )
      {
         m_sd1.setFilename( d.m_lineA->currentText() );
         m_sd2.setFilename( d.m_lineB->currentText() );
         m_sd3.setFilename( d.m_lineC->currentText() );

         if( d.m_pMerge->isChecked() )
         {
            if ( d.m_lineOut->currentText().isEmpty() )
            {
               m_outputFilename = "unnamed.txt";
               m_bDefaultFilename = true;
            }
            else
            {
               m_outputFilename = d.m_lineOut->currentText();
               m_bDefaultFilename = false;
            }
         }
         else
            m_outputFilename = "";

         bool bSuccess = improveFilenames();
         if ( !bSuccess )
            continue;
            
         if ( m_bDirCompare )
         {
            m_pDirectoryMergeSplitter->show();
            if ( m_pMainWidget!=0 )
            {
               m_pMainWidget->hide();
            }
            break;
         }
         else
         {
            m_pDirectoryMergeSplitter->hide();
            init();

            if ( ! m_sd1.isEmpty() && !m_sd1.hasData()  ||
                 ! m_sd2.isEmpty() && !m_sd2.hasData()  ||
                 ! m_sd3.isEmpty() && !m_sd3.hasData() )
            {
               QString text( i18n("Opening of these files failed:") );
               text += "\n\n";
               if ( ! m_sd1.isEmpty() && !m_sd1.hasData() )
                  text += " - " + m_sd1.getAliasName() + "\n";
               if ( ! m_sd2.isEmpty() && !m_sd2.hasData() )
                  text += " - " + m_sd2.getAliasName() + "\n";
               if ( ! m_sd3.isEmpty() && !m_sd3.hasData() )
                  text += " - " + m_sd3.getAliasName() + "\n";

               KMessageBox::sorry( this, text, i18n("File open error") );
               continue;
            }
         }
      }
      break;
   }

   slotUpdateAvailabilities();
   slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotFileOpen2(QString fn1, QString fn2, QString fn3, QString ofn,
                              QString an1, QString an2, QString an3, TotalDiffStatus* pTotalDiffStatus )
{
   if ( !canContinue() ) return;

   if(fn1=="" && fn2=="" && fn3=="" && ofn=="" && m_pMainWidget!=0 )
   {
      m_pMainWidget->hide();
      return;
   }

   slotStatusMsg(i18n("Opening files..."));

   m_sd1.setFilename( fn1 );
   m_sd2.setFilename( fn2 );
   m_sd3.setFilename( fn3 );

   m_sd1.setAliasName( an1 );
   m_sd2.setAliasName( an2 );
   m_sd3.setAliasName( an3 );

   if ( ! ofn.isEmpty() )
   {
      m_outputFilename = ofn;
      m_bDefaultFilename = false;
   }
   else
   {
      m_outputFilename = "";
      m_bDefaultFilename = true;
   }

   bool bDirCompare = m_bDirCompare;
   improveFilenames();

   if( m_bDirCompare )
   {
   }
   else
   {
      m_bDirCompare = bDirCompare;  // Don't allow this to change here.
      init( false, pTotalDiffStatus );

      if ( pTotalDiffStatus!=0 )
         return;
      
      if ( ! m_sd1.isEmpty() && ! m_sd1.hasData()  ||
           ! m_sd2.isEmpty() && ! m_sd2.hasData()  ||
           ! m_sd3.isEmpty() && ! m_sd3.hasData() )
      {
         QString text( i18n("Opening of these files failed:") );
         text += "\n\n";
         if ( ! m_sd1.isEmpty() && !m_sd1.hasData() )
            text += " - " + m_sd1.getAliasName() + "\n";
         if ( ! m_sd2.isEmpty() && !m_sd2.hasData() )
            text += " - " + m_sd2.getAliasName() + "\n";
         if ( ! m_sd3.isEmpty() && !m_sd3.hasData() )
            text += " - " + m_sd3.getAliasName() + "\n";

         KMessageBox::sorry( this, text, i18n("File open error") );
      }
      else
      {
         if ( m_pDirectoryMergeWindow!=0 && m_pDirectoryMergeWindow->isVisible() && ! dirShowBoth->isChecked() )
         {
            slotDirViewToggle();
         }
      }
   }
   slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditCut()
{
   slotStatusMsg(i18n("Cutting selection..."));

   QString s;
   if ( m_pMergeResultWindow!=0 )
   {
      s = m_pMergeResultWindow->getSelection();
      m_pMergeResultWindow->deleteSelection();

      m_pMergeResultWindow->update();
   }

   if ( !s.isNull() )
   {
      QApplication::clipboard()->setText( s, QClipboard::Clipboard );
   }

   slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditCopy()
{
   slotStatusMsg(i18n("Copying selection to clipboard..."));
   QString s;
   if (               m_pDiffTextWindow1!=0 )   s = m_pDiffTextWindow1->getSelection();
   if ( s.isNull() && m_pDiffTextWindow2!=0 )   s = m_pDiffTextWindow2->getSelection();
   if ( s.isNull() && m_pDiffTextWindow3!=0 )   s = m_pDiffTextWindow3->getSelection();
   if ( s.isNull() && m_pMergeResultWindow!=0 ) s = m_pMergeResultWindow->getSelection();
   if ( !s.isNull() )
   {
      QApplication::clipboard()->setText( s, QClipboard::Clipboard );
   }

   slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditPaste()
{
   slotStatusMsg(i18n("Inserting clipboard contents..."));

   if ( m_pMergeResultWindow!=0 && m_pMergeResultWindow->isVisible() )
   {
      m_pMergeResultWindow->pasteClipboard(false);
   }
   else if ( canContinue() )
   {
      if ( m_pDiffTextWindow1->hasFocus() )
      {
         m_sd1.setData( QApplication::clipboard()->text(QClipboard::Clipboard) );
         init();
      }
      else if ( m_pDiffTextWindow2->hasFocus() )
      {
         m_sd2.setData( QApplication::clipboard()->text(QClipboard::Clipboard) );
         init();
      }
      else if ( m_pDiffTextWindow3->hasFocus() )
      {
         m_sd3.setData( QApplication::clipboard()->text(QClipboard::Clipboard) );
         init();
      }
   }

   slotStatusMsg(i18n("Ready."));
}


void KDiff3App::slotGoCurrent()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoCurrent();
}
void KDiff3App::slotGoTop()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoTop();
}
void KDiff3App::slotGoBottom()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoBottom();
}
void KDiff3App::slotGoPrevUnsolvedConflict()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoPrevUnsolvedConflict();
}
void KDiff3App::slotGoNextUnsolvedConflict()
{
   m_bTimerBlock = false;
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoNextUnsolvedConflict();
}
void KDiff3App::slotGoPrevConflict()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoPrevConflict();
}
void KDiff3App::slotGoNextConflict()
{
   m_bTimerBlock = false;
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoNextConflict();
}
void KDiff3App::slotGoPrevDelta()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoPrevDelta();
}
void KDiff3App::slotGoNextDelta()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoNextDelta();
}

void KDiff3App::choose( int choice )
{
   if (!m_bTimerBlock )
   {
      if ( m_pDirectoryMergeWindow && m_pDirectoryMergeWindow->hasFocus() )
      {
         if (choice==A) m_pDirectoryMergeWindow->slotCurrentChooseA();
         if (choice==B) m_pDirectoryMergeWindow->slotCurrentChooseB();
         if (choice==C) m_pDirectoryMergeWindow->slotCurrentChooseC();
         
         chooseA->setChecked(false);
         chooseB->setChecked(false);
         chooseC->setChecked(false);
      }
      else if ( m_pMergeResultWindow )
      {
         m_pMergeResultWindow->choose( choice );
         if ( autoAdvance->isChecked() )
         {
            m_bTimerBlock = true;
            QTimer::singleShot( m_pOptionDialog->m_autoAdvanceDelay, this, SLOT( slotGoNextUnsolvedConflict() ) );
         }
      }
   }
}

void KDiff3App::slotChooseA() { choose( A ); }
void KDiff3App::slotChooseB() { choose( B ); }
void KDiff3App::slotChooseC() { choose( C ); }

// bConflictsOnly automatically choose for conflicts only (true) or for everywhere
static void mergeChooseGlobal( MergeResultWindow* pMRW, int selector, bool bConflictsOnly, bool bWhiteSpaceOnly )
{
   if ( pMRW )
   {
      pMRW->chooseGlobal(selector, bConflictsOnly, bWhiteSpaceOnly );
   }
}

void KDiff3App::slotChooseAEverywhere() {  mergeChooseGlobal( m_pMergeResultWindow, A, false, false ); }
void KDiff3App::slotChooseBEverywhere() {  mergeChooseGlobal( m_pMergeResultWindow, B, false, false ); }
void KDiff3App::slotChooseCEverywhere() {  mergeChooseGlobal( m_pMergeResultWindow, C, false, false ); }
void KDiff3App::slotChooseAForUnsolvedConflicts() {  mergeChooseGlobal( m_pMergeResultWindow, A, true, false ); }
void KDiff3App::slotChooseBForUnsolvedConflicts() {  mergeChooseGlobal( m_pMergeResultWindow, B, true, false ); }
void KDiff3App::slotChooseCForUnsolvedConflicts() {  mergeChooseGlobal( m_pMergeResultWindow, C, true, false ); }
void KDiff3App::slotChooseAForUnsolvedWhiteSpaceConflicts() {  mergeChooseGlobal( m_pMergeResultWindow, A, true, true ); }
void KDiff3App::slotChooseBForUnsolvedWhiteSpaceConflicts() {  mergeChooseGlobal( m_pMergeResultWindow, B, true, true ); }
void KDiff3App::slotChooseCForUnsolvedWhiteSpaceConflicts() {  mergeChooseGlobal( m_pMergeResultWindow, C, true, true ); }


void KDiff3App::slotAutoSolve()
{
   if (m_pMergeResultWindow )
   {
      m_pMergeResultWindow->slotAutoSolve();
      // m_pMergeWindowFrame->show(); incompatible with bPreserveCarriageReturn
      m_pMergeResultWindow->showNrOfConflicts();
      slotUpdateAvailabilities();
   }
}

void KDiff3App::slotUnsolve()
{
   if (m_pMergeResultWindow )
   {
      m_pMergeResultWindow->slotUnsolve();
   }
}


void KDiff3App::slotConfigure()
{
   m_pOptionDialog->setState();
   m_pOptionDialog->incInitialSize ( QSize(0,40) );
   m_pOptionDialog->exec();
   slotRefresh();
}

void KDiff3App::slotConfigureKeys()
{
    KKeyDialog::configure(actionCollection(), this);
}

void KDiff3App::slotRefresh()
{
   g_tabSize = m_pOptionDialog->m_tabSize;
   if (m_pDiffTextWindow1!=0)
   {
      m_pDiffTextWindow1->setFont(m_pOptionDialog->m_font);
      m_pDiffTextWindow1->update();
   }
   if (m_pDiffTextWindow2!=0)
   {
      m_pDiffTextWindow2->setFont(m_pOptionDialog->m_font);
      m_pDiffTextWindow2->update();
   }
   if (m_pDiffTextWindow3!=0)
   {
      m_pDiffTextWindow3->setFont(m_pOptionDialog->m_font);
      m_pDiffTextWindow3->update();
   }
   if (m_pMergeResultWindow!=0)
   {
      m_pMergeResultWindow->setFont(m_pOptionDialog->m_font);
      m_pMergeResultWindow->update();
   }
   if (m_pHScrollBar!=0)
   {
      m_pHScrollBar->setAgain();
   }
   if ( m_pDiffWindowSplitter!=0 )
   {
       m_pDiffWindowSplitter->setOrientation( m_pOptionDialog->m_bHorizDiffWindowSplitting ? Horizontal : Vertical );
   }
}

void KDiff3App::slotSelectionStart()
{
   //editCopy->setEnabled( false );
   //editCut->setEnabled( false );

   const QObject* s = sender();
   if (m_pDiffTextWindow1 && s!=m_pDiffTextWindow1)   m_pDiffTextWindow1->resetSelection();
   if (m_pDiffTextWindow2 && s!=m_pDiffTextWindow2)   m_pDiffTextWindow2->resetSelection();
   if (m_pDiffTextWindow3 && s!=m_pDiffTextWindow3)   m_pDiffTextWindow3->resetSelection();
   if (m_pMergeResultWindow && s!=m_pMergeResultWindow) m_pMergeResultWindow->resetSelection();
}

void KDiff3App::slotSelectionEnd()
{
   //const QObject* s = sender();
   //editCopy->setEnabled(true);
   //editCut->setEnabled( s==m_pMergeResultWindow );
   if ( m_pOptionDialog->m_bAutoCopySelection )
   {
      slotEditCopy();
   }
   else
   {
       QClipboard *clipBoard = QApplication::clipboard();

       if (clipBoard->supportsSelection ())
       {           
           QString s;
           if (               m_pDiffTextWindow1!=0 )   s = m_pDiffTextWindow1->getSelection();
           if ( s.isNull() && m_pDiffTextWindow2!=0 )   s = m_pDiffTextWindow2->getSelection();
           if ( s.isNull() && m_pDiffTextWindow3!=0 )   s = m_pDiffTextWindow3->getSelection();
           if ( s.isNull() && m_pMergeResultWindow!=0 ) s = m_pMergeResultWindow->getSelection();
           if ( !s.isNull() )
           {
               clipBoard->setText( s, QClipboard::Selection );
           }
       }
   }
}

void KDiff3App::slotClipboardChanged()
{
   QString s = QApplication::clipboard()->text();
   //editPaste->setEnabled(!s.isEmpty());
}

void KDiff3App::slotOutputModified()
{
   if ( !m_bOutputModified )
   {
      m_bOutputModified=true;
      slotUpdateAvailabilities();
   }
}

void KDiff3App::slotAutoAdvanceToggled()
{
   m_pOptionDialog->m_bAutoAdvance = autoAdvance->isChecked();
}

void KDiff3App::slotWordWrapToggled()
{
   m_pOptionDialog->m_bWordWrap = wordWrap->isChecked();
   recalcWordWrap();
}

void KDiff3App::recalcWordWrap()
{
   int firstD3LIdx = 0;
   if( m_pDiffTextWindow1 ) 
      firstD3LIdx = m_pDiffTextWindow1->convertLineToDiff3LineIdx( m_pDiffTextWindow1->getFirstLine() );

   // Convert selection to D3L-coords (converting back happens in DiffTextWindow::recalcWordWrap()
   if ( m_pDiffTextWindow1 )
      m_pDiffTextWindow1->convertSelectionToD3LCoords();
   if ( m_pDiffTextWindow2 )
      m_pDiffTextWindow2->convertSelectionToD3LCoords();
   if ( m_pDiffTextWindow3 )
      m_pDiffTextWindow3->convertSelectionToD3LCoords();
      
      
   if ( !m_diff3LineList.empty() && m_pOptionDialog->m_bWordWrap )
   {      
      Diff3LineList::iterator i;
      int sumOfLines=0;
      for ( i=m_diff3LineList.begin(); i!=m_diff3LineList.end(); ++i )
      {
         Diff3Line& d3l = *i;
         d3l.linesNeededForDisplay = 1;
         d3l.sumLinesNeededForDisplay = sumOfLines;
         sumOfLines += d3l.linesNeededForDisplay;
      }
      
      // Let every window calc how many lines will be needed.
      if ( m_pDiffTextWindow1 )
         m_pDiffTextWindow1->recalcWordWrap(true,0);
      if ( m_pDiffTextWindow2 )
         m_pDiffTextWindow2->recalcWordWrap(true,0);
      if ( m_pDiffTextWindow3 )
         m_pDiffTextWindow3->recalcWordWrap(true,0);
      
      sumOfLines=0;
      for ( i=m_diff3LineList.begin(); i!=m_diff3LineList.end(); ++i )
      {
         Diff3Line& d3l = *i;
         d3l.sumLinesNeededForDisplay = sumOfLines;
         sumOfLines += d3l.linesNeededForDisplay;
      }
      
      // Finish the initialisation:
      if ( m_pDiffTextWindow1 )
         m_pDiffTextWindow1->recalcWordWrap(true,sumOfLines);
      if ( m_pDiffTextWindow2 )
         m_pDiffTextWindow2->recalcWordWrap(true,sumOfLines);
      if ( m_pDiffTextWindow3 )
         m_pDiffTextWindow3->recalcWordWrap(true,sumOfLines);

      m_neededLines = sumOfLines;
   }
   else
   {
      m_neededLines = m_diff3LineVector.size();
      if ( m_pDiffTextWindow1 )
         m_pDiffTextWindow1->recalcWordWrap(false,0);
      if ( m_pDiffTextWindow2 )
         m_pDiffTextWindow2->recalcWordWrap(false,0);
      if ( m_pDiffTextWindow3 )
         m_pDiffTextWindow3->recalcWordWrap(false,0);
   }
   m_pOverview->slotRedraw();
   if ( m_pDiffTextWindow1 )
   {
      m_pDiffTextWindow1->setFirstLine( m_pDiffTextWindow1->convertDiff3LineIdxToLine( firstD3LIdx ) );
      m_pDiffTextWindow1->update();
   }
   if ( m_pDiffTextWindow2 )
   {
      m_pDiffTextWindow2->setFirstLine( m_pDiffTextWindow2->convertDiff3LineIdxToLine( firstD3LIdx ) );
      m_pDiffTextWindow2->update();
   }
   if ( m_pDiffTextWindow3 )
   {
      m_pDiffTextWindow3->setFirstLine( m_pDiffTextWindow3->convertDiff3LineIdxToLine( firstD3LIdx ) );
      m_pDiffTextWindow3->update();
   }

   m_pDiffVScrollBar->setRange(0, max2(0, m_neededLines+1 - m_DTWHeight) );
   if ( m_pDiffTextWindow1 )
   {
      m_pDiffVScrollBar->setValue( m_pDiffTextWindow1->convertDiff3LineIdxToLine( firstD3LIdx ) );
      
      m_maxWidth = max3( m_pDiffTextWindow1->getNofColumns(), 
                         m_pDiffTextWindow2->getNofColumns(),
                         m_pDiffTextWindow3->getNofColumns() ) + (m_pOptionDialog->m_bWordWrap ? 0 : 5);
                         
      m_pHScrollBar->setRange(0, max2( 0, m_maxWidth - m_pDiffTextWindow1->getNofVisibleColumns() ) );
      m_pHScrollBar->setPageStep( m_pDiffTextWindow1->getNofVisibleColumns() );
      m_pHScrollBar->setValue(0);
   }
}

void KDiff3App::slotShowWhiteSpaceToggled()
{
   m_pOptionDialog->m_bShowWhiteSpaceCharacters = showWhiteSpaceCharacters->isChecked();
   m_pOptionDialog->m_bShowWhiteSpace = showWhiteSpace->isChecked();
   showWhiteSpaceCharacters->setEnabled( showWhiteSpace->isChecked() );
   if ( m_pDiffTextWindow1!=0 )
      m_pDiffTextWindow1->update();
   if ( m_pDiffTextWindow2!=0 )
      m_pDiffTextWindow2->update();
   if ( m_pDiffTextWindow3!=0 )
      m_pDiffTextWindow3->update();
   if ( m_pOverview!=0 )
      m_pOverview->slotRedraw();
}

void KDiff3App::slotShowLineNumbersToggled()
{
   m_pOptionDialog->m_bShowLineNumbers = showLineNumbers->isChecked();
   if ( m_pDiffTextWindow1!=0 )
      m_pDiffTextWindow1->update();
   if ( m_pDiffTextWindow2!=0 )
      m_pDiffTextWindow2->update();
   if ( m_pDiffTextWindow3!=0 )
      m_pDiffTextWindow3->update();
}

/// Return true for success, else false
bool KDiff3App::improveFilenames()
{
   m_bDirCompare = false;
   
   FileAccess f1(m_sd1.getFilename());
   FileAccess f2(m_sd2.getFilename());
   FileAccess f3(m_sd3.getFilename());
   FileAccess f4(m_outputFilename);

   if ( f1.isFile()  &&  f1.exists() )
   {
      if ( f2.isDir() )
      {
         f2.addPath( f1.fileName() );
         if ( f2.isFile() && f2.exists() )
            m_sd2.setFileAccess( f2 );
      }
      if ( f3.isDir() )
      {
         f3.addPath( f1.fileName() );
         if ( f3.isFile() && f3.exists() )
            m_sd3.setFileAccess( f3 );
      }
      if ( f4.isDir() )
      {
         f4.addPath( f1.fileName() );
         if ( f4.isFile() && f4.exists() )
            m_outputFilename = f4.absFilePath();
      }
   }
   else if ( f1.isDir() )
   {
      m_bDirCompare = true;
      FileAccess destDir;
      if (!m_bDefaultFilename) destDir = f4;
      m_pDirectoryMergeSplitter->show();
      if (m_pMainWidget!=0) m_pMainWidget->hide();
      
      bool bSuccess = m_pDirectoryMergeWindow->init(
         f1, f2, f3,
         destDir,  // Destdirname
         !m_outputFilename.isEmpty()
         );
   
      m_bDirCompare = true;  // This seems redundant but it might have beed reset during full analysis.
      
      if (bSuccess)
      {
         m_sd1.reset();
         if (m_pDiffTextWindow1!=0) m_pDiffTextWindow1->init(0,0,0,0,1,false);
         m_sd2.reset();
         if (m_pDiffTextWindow2!=0) m_pDiffTextWindow2->init(0,0,0,0,2,false);
         m_sd3.reset();
         if (m_pDiffTextWindow3!=0) m_pDiffTextWindow3->init(0,0,0,0,3,false);
      }
      slotUpdateAvailabilities();
      return bSuccess;
   }
   return true;
}

void KDiff3App::slotReload()
{
   if ( !canContinue() ) return;

   init();
}

bool KDiff3App::canContinue()
{
   // First test if anything must be saved.
   if(m_bOutputModified)
   {
      int result = KMessageBox::warningYesNoCancel(this,
         i18n("The merge result hasn't been saved."),
         i18n("Warning"), i18n("Save && Continue"), i18n("Continue Without Saving") );
      if ( result==KMessageBox::Cancel )
         return false;
      else if ( result==KMessageBox::Yes )
      {
         slotFileSave();
         if ( m_bOutputModified )
         {
            KMessageBox::sorry(this, i18n("Saving the merge result failed."), i18n("Warning") );
            return false;
         }
      }
   }

   m_bOutputModified = false;
   return true;
}

void KDiff3App::slotCheckIfCanContinue( bool* pbContinue )
{
   if (pbContinue!=0) *pbContinue = canContinue();
}


void KDiff3App::slotDirShowBoth()
{
   if( dirShowBoth->isChecked() )
   {
      if ( m_bDirCompare )
         m_pDirectoryMergeSplitter->show();
      else
         m_pDirectoryMergeSplitter->hide();

      if ( m_pMainWidget!=0 )
         m_pMainWidget->show();
   }
   else
   {
      if ( m_pMainWidget!=0 )
      {
         m_pMainWidget->show();
         m_pDirectoryMergeSplitter->hide();
      }
      else if ( m_bDirCompare )
      {
         m_pDirectoryMergeSplitter->show();
      }
   }

   slotUpdateAvailabilities();
}


void KDiff3App::slotDirViewToggle()
{
   if ( m_bDirCompare )
   {
      if( ! m_pDirectoryMergeSplitter->isVisible() )
      {
         m_pDirectoryMergeSplitter->show();
         if (m_pMainWidget!=0)
            m_pMainWidget->hide();
      }
      else
      {
         if (m_pMainWidget!=0)
         {
            m_pDirectoryMergeSplitter->hide();
            m_pMainWidget->show();
         }
      }
   }
   slotUpdateAvailabilities();
}

void KDiff3App::slotShowWindowAToggled()
{
   if ( m_pDiffTextWindow1!=0 )
   {
      if ( showWindowA->isChecked() ) m_pDiffTextWindow1->show();
      else                            m_pDiffTextWindow1->hide();
      slotUpdateAvailabilities();
   }
}

void KDiff3App::slotShowWindowBToggled()
{
   if ( m_pDiffTextWindow2!=0 )
   {
      if ( showWindowB->isChecked() ) m_pDiffTextWindow2->show();
      else                            m_pDiffTextWindow2->hide();
      slotUpdateAvailabilities();
   }
}

void KDiff3App::slotShowWindowCToggled()
{
   if ( m_pDiffTextWindow3!=0 )
   {
      if ( showWindowC->isChecked() ) m_pDiffTextWindow3->show();
      else                            m_pDiffTextWindow3->hide();
      slotUpdateAvailabilities();
   }
}

void KDiff3App::slotEditFind()
{
   m_pFindDialog->currentLine = 0;
   m_pFindDialog->currentPos = 0;
   m_pFindDialog->currentWindow = 1;
   
   if ( QDialog::Accepted == m_pFindDialog->exec() )
   {
      slotEditFindNext();
   }
}

void KDiff3App::slotEditFindNext()
{
   QString s = m_pFindDialog->m_pSearchString->text();
   if ( s.isEmpty() )
   {
      slotEditFind();
      return;
   }

   bool bDirDown = true;
   bool bCaseSensitive = m_pFindDialog->m_pCaseSensitive->isChecked();

   int d3vLine = m_pFindDialog->currentLine;
   int posInLine = m_pFindDialog->currentPos;
   int l=0;
   int p=0;
   if ( m_pFindDialog->currentWindow == 1 )
   {
      if ( m_pFindDialog->m_pSearchInA->isChecked() && m_pDiffTextWindow1!=0  &&
           m_pDiffTextWindow1->findString( s, d3vLine, posInLine, bDirDown, bCaseSensitive ) )
      {
         m_pDiffTextWindow1->setSelection( d3vLine, posInLine, d3vLine, posInLine+s.length(), l, p );
         m_pDiffVScrollBar->setValue(l-m_pDiffVScrollBar->pageStep()/2);
         m_pHScrollBar->setValue( max2( 0, p+(int)s.length()-m_pHScrollBar->pageStep()) );
         m_pFindDialog->currentLine = d3vLine;
         m_pFindDialog->currentPos = posInLine + 1;
         return;
      }
      m_pFindDialog->currentWindow = 2;
      m_pFindDialog->currentLine = 0;
      m_pFindDialog->currentPos = 0;
   }

   d3vLine = m_pFindDialog->currentLine;
   posInLine = m_pFindDialog->currentPos;
   if ( m_pFindDialog->currentWindow == 2 )
   {
      if ( m_pFindDialog->m_pSearchInB->isChecked() && m_pDiffTextWindow2!=0 &&
           m_pDiffTextWindow2->findString( s, d3vLine, posInLine, bDirDown, bCaseSensitive ) )
      {
         m_pDiffTextWindow2->setSelection( d3vLine, posInLine, d3vLine, posInLine+s.length(),l,p );
         m_pDiffVScrollBar->setValue(l-m_pDiffVScrollBar->pageStep()/2);
         m_pHScrollBar->setValue( max2( 0, p+(int)s.length()-m_pHScrollBar->pageStep()) );
         m_pFindDialog->currentLine = d3vLine;
         m_pFindDialog->currentPos = posInLine + 1;
         return;
      }
      m_pFindDialog->currentWindow = 3;
      m_pFindDialog->currentLine = 0;
      m_pFindDialog->currentPos = 0;
   }

   d3vLine = m_pFindDialog->currentLine;
   posInLine = m_pFindDialog->currentPos;
   if ( m_pFindDialog->currentWindow == 3 )
   {
      if ( m_pFindDialog->m_pSearchInC->isChecked() && m_pDiffTextWindow3!=0 &&
           m_pDiffTextWindow3->findString( s, d3vLine, posInLine, bDirDown, bCaseSensitive ) )
      {
         m_pDiffTextWindow3->setSelection( d3vLine, posInLine, d3vLine, posInLine+s.length(),l,p );
         m_pDiffVScrollBar->setValue(l-m_pDiffVScrollBar->pageStep()/2);
         m_pHScrollBar->setValue( max2( 0, p+(int)s.length()-m_pHScrollBar->pageStep()) );
         m_pFindDialog->currentLine = d3vLine;
         m_pFindDialog->currentPos = posInLine + 1;
         return;
      }
      m_pFindDialog->currentWindow = 4;
      m_pFindDialog->currentLine = 0;
      m_pFindDialog->currentPos = 0;
   }  

   d3vLine = m_pFindDialog->currentLine;
   posInLine = m_pFindDialog->currentPos;
   if ( m_pFindDialog->currentWindow == 4 )
   {
      if ( m_pFindDialog->m_pSearchInOutput->isChecked() && m_pMergeResultWindow!=0 && m_pMergeResultWindow->isVisible() &&
           m_pMergeResultWindow->findString( s, d3vLine, posInLine, bDirDown, bCaseSensitive ) )
      {
         m_pMergeResultWindow->setSelection( d3vLine, posInLine, d3vLine, posInLine+s.length() );
         m_pMergeVScrollBar->setValue(d3vLine - m_pMergeVScrollBar->pageStep()/2);
         m_pHScrollBar->setValue( max2( 0, posInLine+(int)s.length()-m_pHScrollBar->pageStep()) );
         m_pFindDialog->currentLine = d3vLine;
         m_pFindDialog->currentPos = posInLine + 1;
         return;
      }
      m_pFindDialog->currentWindow = 5;
      m_pFindDialog->currentLine = 0;
      m_pFindDialog->currentPos = 0;
   }

   KMessageBox::information(this,i18n("Search complete."),i18n("Search Complete"));
   m_pFindDialog->currentWindow = 1;
   m_pFindDialog->currentLine = 0;
   m_pFindDialog->currentPos = 0;
}

void KDiff3App::slotMergeCurrentFile()
{
   if ( m_bDirCompare  &&  m_pDirectoryMergeWindow->isVisible()  &&  m_pDirectoryMergeWindow->isFileSelected() )
   {
      m_pDirectoryMergeWindow->mergeCurrentFile();
   }
   else if ( m_pMainWidget != 0 && m_pMainWidget->isVisible() )
   {
      if ( !canContinue() ) return;
      if ( m_outputFilename.isEmpty() )
      {
         if ( !m_sd3.isEmpty() && !m_sd3.isFromBuffer() )
         {
            m_outputFilename =  m_sd3.getFilename();
         }
         else if ( !m_sd2.isEmpty() && !m_sd2.isFromBuffer() )
         {
            m_outputFilename =  m_sd2.getFilename();
         }
         else if ( !m_sd1.isEmpty() && !m_sd1.isFromBuffer() )
         {
            m_outputFilename =  m_sd1.getFilename();
         }
         else
         {
            m_outputFilename = "unnamed.txt";
            m_bDefaultFilename = true;
         }
      }
      init();
   }
}

void KDiff3App::slotWinFocusNext()
{
   QWidget* focus = qApp->focusWidget();
   if ( focus == m_pDirectoryMergeWindow && m_pDirectoryMergeWindow->isVisible() && ! dirShowBoth->isChecked() )
   {
      slotDirViewToggle();
   }

   std::list<QWidget*> visibleWidgetList;
   if ( m_pDiffTextWindow1 && m_pDiffTextWindow1->isVisible() ) visibleWidgetList.push_back(m_pDiffTextWindow1);
   if ( m_pDiffTextWindow2 && m_pDiffTextWindow2->isVisible() ) visibleWidgetList.push_back(m_pDiffTextWindow2);
   if ( m_pDiffTextWindow3 && m_pDiffTextWindow3->isVisible() ) visibleWidgetList.push_back(m_pDiffTextWindow3);
   if ( m_pMergeResultWindow && m_pMergeResultWindow->isVisible() ) visibleWidgetList.push_back(m_pMergeResultWindow);
   if ( m_bDirCompare /*m_pDirectoryMergeWindow->isVisible()*/ ) visibleWidgetList.push_back(m_pDirectoryMergeWindow);
   //if ( m_pDirectoryMergeInfo->isVisible() ) visibleWidgetList.push_back(m_pDirectoryMergeInfo->getInfoList());

   std::list<QWidget*>::iterator i = std::find( visibleWidgetList.begin(),  visibleWidgetList.end(), focus);
   ++i;
   if ( i==visibleWidgetList.end() ) 
      i = visibleWidgetList.begin();
   if ( i!=visibleWidgetList.end() )
   {
      if ( *i == m_pDirectoryMergeWindow  && ! dirShowBoth->isChecked() )
      {
         slotDirViewToggle();
      }
      (*i)->setFocus();
   }
}

void KDiff3App::slotWinFocusPrev()
{
   QWidget* focus = qApp->focusWidget();
   if ( focus == m_pDirectoryMergeWindow  && m_pDirectoryMergeWindow->isVisible() && ! dirShowBoth->isChecked() )
   {
      slotDirViewToggle();
   }

   std::list<QWidget*> visibleWidgetList;
   if ( m_pDiffTextWindow1 && m_pDiffTextWindow1->isVisible() ) visibleWidgetList.push_back(m_pDiffTextWindow1);
   if ( m_pDiffTextWindow2 && m_pDiffTextWindow2->isVisible() ) visibleWidgetList.push_back(m_pDiffTextWindow2);
   if ( m_pDiffTextWindow3 && m_pDiffTextWindow3->isVisible() ) visibleWidgetList.push_back(m_pDiffTextWindow3);
   if ( m_pMergeResultWindow && m_pMergeResultWindow->isVisible() ) visibleWidgetList.push_back(m_pMergeResultWindow);
   if (m_bDirCompare /* m_pDirectoryMergeWindow->isVisible() */ ) visibleWidgetList.push_back(m_pDirectoryMergeWindow);
   //if ( m_pDirectoryMergeInfo->isVisible() ) visibleWidgetList.push_back(m_pDirectoryMergeInfo->getInfoList());

   std::list<QWidget*>::iterator i = std::find( visibleWidgetList.begin(),  visibleWidgetList.end(), focus);
   --i;
   if ( i==visibleWidgetList.end() ) --i;
   if ( i!=visibleWidgetList.end() )
   {
      if ( *i == m_pDirectoryMergeWindow  && ! dirShowBoth->isChecked() )
      {
         slotDirViewToggle();
      }
      (*i)->setFocus();
   }
}

void KDiff3App::slotWinToggleSplitterOrientation()
{
   if ( m_pDiffWindowSplitter!=0 )
   {
      m_pDiffWindowSplitter->setOrientation(
         m_pDiffWindowSplitter->orientation()==Vertical ? Horizontal : Vertical
         );

      m_pOptionDialog->m_bHorizDiffWindowSplitting = m_pDiffWindowSplitter->orientation()==Horizontal;
   }
}

void KDiff3App::slotOverviewNormal()
{   
   m_pOverview->setOverviewMode( Overview::eOMNormal );
   slotUpdateAvailabilities();
}

void KDiff3App::slotOverviewAB()
{
   m_pOverview->setOverviewMode( Overview::eOMAvsB );
   slotUpdateAvailabilities();
}

void KDiff3App::slotOverviewAC()
{
   m_pOverview->setOverviewMode( Overview::eOMAvsC );
   slotUpdateAvailabilities();
}

void KDiff3App::slotOverviewBC()
{
   m_pOverview->setOverviewMode( Overview::eOMBvsC );
   slotUpdateAvailabilities();
}
   
void KDiff3App::slotUpdateAvailabilities()
{
   bool bTextDataAvailable = ( m_sd1.hasData() || m_sd2.hasData() || m_sd3.hasData() );
   
   if( dirShowBoth->isChecked() )
   {
      if ( m_bDirCompare )
         m_pDirectoryMergeSplitter->show();
      else
         m_pDirectoryMergeSplitter->hide();

      if ( m_pMainWidget!=0 && !m_pMainWidget->isVisible() &&
           bTextDataAvailable && !m_pDirectoryMergeWindow->isScanning()
         )
         m_pMainWidget->show();
   }
   

   bool bDiffWindowVisible = m_pMainWidget != 0 && m_pMainWidget->isVisible();
   bool bMergeEditorVisible = m_pMergeWindowFrame !=0  &&  m_pMergeWindowFrame->isVisible();

   m_pDirectoryMergeWindow->updateAvailabilities( m_bDirCompare, bDiffWindowVisible, chooseA, chooseB, chooseC );

   dirShowBoth->setEnabled( m_bDirCompare );
   dirViewToggle->setEnabled(
      m_bDirCompare &&
      (!m_pDirectoryMergeSplitter->isVisible()  &&  m_pMainWidget!=0 && m_pMainWidget->isVisible() ||
        m_pDirectoryMergeSplitter->isVisible()  &&  m_pMainWidget!=0 && !m_pMainWidget->isVisible() && bTextDataAvailable )
      );
      
   bool bDirWindowHasFocus = m_pDirectoryMergeSplitter->isVisible() && m_pDirectoryMergeWindow->hasFocus();

   showWhiteSpaceCharacters->setEnabled( bDiffWindowVisible );
   autoAdvance->setEnabled( bMergeEditorVisible );
   autoSolve->setEnabled( bMergeEditorVisible  &&  m_bTripleDiff );
   unsolve->setEnabled( bMergeEditorVisible );
   if ( !bDirWindowHasFocus )
   {
      chooseA->setEnabled( bMergeEditorVisible );
      chooseB->setEnabled( bMergeEditorVisible );
      chooseC->setEnabled( bMergeEditorVisible  &&  m_bTripleDiff );
   }
   chooseAEverywhere->setEnabled( bMergeEditorVisible );
   chooseBEverywhere->setEnabled( bMergeEditorVisible );
   chooseCEverywhere->setEnabled( bMergeEditorVisible  &&  m_bTripleDiff );
   chooseAForUnsolvedConflicts->setEnabled( bMergeEditorVisible );
   chooseBForUnsolvedConflicts->setEnabled( bMergeEditorVisible );
   chooseCForUnsolvedConflicts->setEnabled( bMergeEditorVisible  &&  m_bTripleDiff );
   chooseAForUnsolvedWhiteSpaceConflicts->setEnabled( bMergeEditorVisible );
   chooseBForUnsolvedWhiteSpaceConflicts->setEnabled( bMergeEditorVisible );
   chooseCForUnsolvedWhiteSpaceConflicts->setEnabled( bMergeEditorVisible  &&  m_bTripleDiff );
   showWindowA->setEnabled( bDiffWindowVisible && ( m_pDiffTextWindow2->isVisible() || m_pDiffTextWindow3->isVisible() ) );
   showWindowB->setEnabled( bDiffWindowVisible && ( m_pDiffTextWindow1->isVisible() || m_pDiffTextWindow3->isVisible() ));
   showWindowC->setEnabled( bDiffWindowVisible &&  m_bTripleDiff && ( m_pDiffTextWindow1->isVisible() || m_pDiffTextWindow2->isVisible() ) );
   editFind->setEnabled( bDiffWindowVisible );
   editFindNext->setEnabled( bDiffWindowVisible );
   m_pFindDialog->m_pSearchInC->setEnabled( m_bTripleDiff );
   m_pFindDialog->m_pSearchInOutput->setEnabled( bMergeEditorVisible );

   bool bSavable = bMergeEditorVisible && m_pMergeResultWindow->getNrOfUnsolvedConflicts()==0;
   fileSave->setEnabled( m_bOutputModified && bSavable );
   fileSaveAs->setEnabled( bSavable );

   goTop->setEnabled( bDiffWindowVisible &&  m_pMergeResultWindow->isDeltaAboveCurrent() );
   goBottom->setEnabled( bDiffWindowVisible &&  m_pMergeResultWindow->isDeltaBelowCurrent() );
   goCurrent->setEnabled( bDiffWindowVisible );
   goPrevUnsolvedConflict->setEnabled( bMergeEditorVisible &&  m_pMergeResultWindow->isUnsolvedConflictAboveCurrent() );
   goNextUnsolvedConflict->setEnabled( bMergeEditorVisible &&  m_pMergeResultWindow->isUnsolvedConflictBelowCurrent() );
   goPrevConflict->setEnabled( bDiffWindowVisible &&  m_pMergeResultWindow->isConflictAboveCurrent() );
   goNextConflict->setEnabled( bDiffWindowVisible &&  m_pMergeResultWindow->isConflictBelowCurrent() );
   goPrevDelta->setEnabled( bDiffWindowVisible &&  m_pMergeResultWindow->isDeltaAboveCurrent() );
   goNextDelta->setEnabled( bDiffWindowVisible &&  m_pMergeResultWindow->isDeltaBelowCurrent() );
   
   overviewModeNormal->setEnabled( m_bTripleDiff && bDiffWindowVisible );
   overviewModeAB->setEnabled( m_bTripleDiff && bDiffWindowVisible );
   overviewModeAC->setEnabled( m_bTripleDiff && bDiffWindowVisible );
   overviewModeBC->setEnabled( m_bTripleDiff && bDiffWindowVisible );
   Overview::e_OverviewMode overviewMode = m_pOverview==0 ? Overview::eOMNormal : m_pOverview->getOverviewMode();
   overviewModeNormal->setChecked( overviewMode == Overview::eOMNormal );
   overviewModeAB->setChecked( overviewMode == Overview::eOMAvsB );
   overviewModeAC->setChecked( overviewMode == Overview::eOMAvsC );
   overviewModeBC->setChecked( overviewMode == Overview::eOMBvsC );

   winToggleSplitOrientation->setEnabled( bDiffWindowVisible && m_pDiffWindowSplitter!=0 );
}
