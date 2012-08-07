/***************************************************************************
                          mergeresultwindow.cpp  -  description
                             -------------------
    begin                : Sun Apr 14 2002
    copyright            : (C) 2002-2007 by Joachim Eibl
    email                : joachim.eibl at gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "mergeresultwindow.h"
#include "options.h"

#include <QPainter>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QCursor>
#include <QStatusBar>
#include <QRegExp>
#include <QTextStream>
#include <QKeyEvent>
#include <QMouseEvent>

#include <QLineEdit>
#include <QTextCodec>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QUrl>
//Added by qt3to4:
#include <QTimerEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QPixmap>
#include <QFocusEvent>
#include <QEvent>
#include <QInputEvent>
#include <QDropEvent>
#include <QPaintEvent>
#include <QTextStream>

#include <klocale.h>
#include <kmessagebox.h>

int g_bAutoSolve = true;

#undef leftInfoWidth
#define leftInfoWidth 3

MergeResultWindow::MergeResultWindow(
   QWidget* pParent,
   Options* pOptions,
   QStatusBar* pStatusBar
   )
: QWidget( pParent )
{
   setObjectName( "MergeResultWindow" );
   setFocusPolicy( Qt::ClickFocus );

   m_firstLine = 0;
   m_firstColumn = 0;
   m_nofColumns = 0;
   m_nofLines = 0;
   m_totalSize = 0;
   m_bMyUpdate = false;
   m_bInsertMode = true;
   m_scrollDeltaX = 0;
   m_scrollDeltaY = 0;
   m_bModified = false;
   m_eOverviewMode=Overview::eOMNormal;

   m_pldA = 0;
   m_pldB = 0;
   m_pldC = 0;
   m_sizeA = 0;
   m_sizeB = 0;
   m_sizeC = 0;

   m_pDiff3LineList = 0;
   m_pTotalDiffStatus = 0;
   m_pStatusBar = pStatusBar;

   m_pOptions = pOptions;
   m_bPaintingAllowed = false;
   m_delayedDrawTimer = 0;

   m_cursorXPos=0;
   m_cursorOldXPos=0;
   m_cursorYPos=0;
   m_bCursorOn = true;
   m_bCursorUpdate = false;
   connect( &m_cursorTimer, SIGNAL(timeout()), this, SLOT( slotCursorUpdate() ) );
   m_cursorTimer.setSingleShot(true);
   m_cursorTimer.start( 500 /*ms*/ );
   m_selection.reset();

   setMinimumSize( QSize(20,20) );
   setFont( m_pOptions->m_font );
}

void MergeResultWindow::init(
   const LineData* pLineDataA, int sizeA,
   const LineData* pLineDataB, int sizeB,
   const LineData* pLineDataC, int sizeC,
   const Diff3LineList* pDiff3LineList,
   TotalDiffStatus* pTotalDiffStatus
   )
{
   m_firstLine = 0;
   m_firstColumn = 0;
   m_nofColumns = 0;
   m_nofLines = 0;
   m_bMyUpdate = false;
   m_bInsertMode = true;
   m_scrollDeltaX = 0;
   m_scrollDeltaY = 0;
   setModified( false );
   
   m_pldA = pLineDataA;
   m_pldB = pLineDataB;
   m_pldC = pLineDataC;
   m_sizeA = sizeA;
   m_sizeB = sizeB;
   m_sizeC = sizeC;

   m_pDiff3LineList = pDiff3LineList;
   m_pTotalDiffStatus = pTotalDiffStatus;
   
   m_selection.reset();
   m_cursorXPos=0;
   m_cursorOldXPos=0;
   m_cursorYPos=0;

   merge( g_bAutoSolve, -1 );
   g_bAutoSolve = true;
   update();
   updateSourceMask();

   int wsc;
   int nofUnsolved = getNrOfUnsolvedConflicts(&wsc);
   if (m_pStatusBar)
      m_pStatusBar->showMessage( i18n("Number of remaining unsolved conflicts: %1 (of which %2 are whitespace)"
         ,nofUnsolved,wsc) );
}

void MergeResultWindow::reset()
{
   m_pDiff3LineList = 0;
   m_pTotalDiffStatus = 0;
   m_pldA = 0;
   m_pldB = 0;
   m_pldC = 0;
}

// Calculate the merge information for the given Diff3Line.
// Results will be stored in mergeDetails, bConflict, bLineRemoved and src.
void mergeOneLine(
   const Diff3Line& d, e_MergeDetails& mergeDetails, bool& bConflict,
   bool& bLineRemoved, int& src, bool bTwoInputs
   )
{
   mergeDetails = eDefault;
   bConflict = false;
   bLineRemoved = false;
   src = 0;

   if ( bTwoInputs )   // Only two input files
   {
      if ( d.lineA!=-1 && d.lineB!=-1 )
      {
         if ( d.pFineAB == 0 )
         {
            mergeDetails = eNoChange;           src = A;
         }
         else
         {
            mergeDetails = eBChanged;           bConflict = true;
         }
      }
      else
      {
         if ( d.lineA!=-1 && d.lineB==-1 )
         {
            mergeDetails = eBDeleted;   bConflict = true;
         }
         else if ( d.lineA==-1 && d.lineB!=-1 )
         {
            mergeDetails = eBDeleted;   bConflict = true;
         }
      }
      return;
   }

   // A is base.
   if ( d.lineA!=-1 && d.lineB!=-1 && d.lineC!=-1 )
   {
      if ( d.pFineAB == 0  &&  d.pFineBC == 0 &&  d.pFineCA == 0)
      {
         mergeDetails = eNoChange;           src = A;
      }
      else if( d.pFineAB == 0  &&  d.pFineBC != 0  &&  d.pFineCA != 0 )
      {
         mergeDetails = eCChanged;           src = C;
      }
      else if( d.pFineAB != 0  &&  d.pFineBC != 0  &&  d.pFineCA == 0 )
      {
         mergeDetails = eBChanged;           src = B;
      }
      else if( d.pFineAB != 0  &&  d.pFineBC == 0  &&  d.pFineCA != 0 )
      {
         mergeDetails = eBCChangedAndEqual;  src = C;
      }
      else if( d.pFineAB != 0  &&  d.pFineBC != 0  &&  d.pFineCA != 0 )
      {
         mergeDetails = eBCChanged;           bConflict = true;
      }
      else
         assert(false);
   }
   else if ( d.lineA!=-1 && d.lineB!=-1 && d.lineC==-1 )
   {
      if( d.pFineAB != 0 )
      {
         mergeDetails = eBChanged_CDeleted;   bConflict = true;
      }
      else
      {
         mergeDetails = eCDeleted;            bLineRemoved = true;        src = C;
      }
   }
   else if ( d.lineA!=-1 && d.lineB==-1 && d.lineC!=-1 )
   {
      if( d.pFineCA != 0 )
      {
         mergeDetails = eCChanged_BDeleted;   bConflict = true;
      }
      else
      {
         mergeDetails = eBDeleted;            bLineRemoved = true;        src = B;
      }
   }
   else if ( d.lineA==-1 && d.lineB!=-1 && d.lineC!=-1 )
   {
      if( d.pFineBC != 0 )
      {
         mergeDetails = eBCAdded;             bConflict = true;
      }
      else // B==C
      {
         mergeDetails = eBCAddedAndEqual;     src = C;
      }
   }
   else if ( d.lineA==-1 && d.lineB==-1 && d.lineC!= -1 )
   {
      mergeDetails = eCAdded;                 src = C;
   }
   else if ( d.lineA==-1 && d.lineB!=-1 && d.lineC== -1 )
   {
      mergeDetails = eBAdded;                 src = B;
   }
   else if ( d.lineA!=-1 && d.lineB==-1 && d.lineC==-1 )
   {
      mergeDetails = eBCDeleted;              bLineRemoved = true;     src = C;
   }
   else
      assert(false);
}

bool MergeResultWindow::sameKindCheck( const MergeLine& ml1, const MergeLine& ml2 )
{
   if ( ml1.bConflict && ml2.bConflict )
   {
      // Both lines have conflicts: If one is only a white space conflict and
      // the other one is a real conflict, then this line returns false.
      return ml1.id3l->bAEqC == ml2.id3l->bAEqC && ml1.id3l->bAEqB == ml2.id3l->bAEqB;
   }
   else
      return (
         ( !ml1.bConflict && !ml2.bConflict && ml1.bDelta && ml2.bDelta && ml1.srcSelect == ml2.srcSelect 
         && (ml1.mergeDetails==ml2.mergeDetails || (ml1.mergeDetails!=eBCAddedAndEqual && ml2.mergeDetails!=eBCAddedAndEqual) ) )
         ||
         (!ml1.bDelta && !ml2.bDelta)
         );
}

void MergeResultWindow::merge(bool bAutoSolve, int defaultSelector, bool bConflictsOnly, bool bWhiteSpaceOnly )
{
   if ( !bConflictsOnly )
   {
      if(m_bModified)
      {
         int result = KMessageBox::warningYesNo(this,
            i18n("The output has been modified.\n"
               "If you continue your changes will be lost."),
            i18n("Warning"), 
            KStandardGuiItem::cont(),
            KStandardGuiItem::cancel());
         if ( result==KMessageBox::No )
            return;
      }

      m_mergeLineList.clear();
      m_totalSize = 0;
      int lineIdx = 0;
      Diff3LineList::const_iterator it;
      for( it=m_pDiff3LineList->begin(); it!=m_pDiff3LineList->end(); ++it, ++lineIdx )
      {
         const Diff3Line& d = *it;

         MergeLine ml;
         bool bLineRemoved;
         mergeOneLine( d, ml.mergeDetails, ml.bConflict, bLineRemoved, ml.srcSelect, m_pldC==0 );

         // Automatic solving for only whitespace changes.
         if ( ml.bConflict &&
              ( (m_pldC==0 && (d.bAEqB || (d.bWhiteLineA && d.bWhiteLineB)))  ||
                (m_pldC!=0 && ((d.bAEqB && d.bAEqC) || (d.bWhiteLineA && d.bWhiteLineB && d.bWhiteLineC) ) ) ) )
         {
            ml.bWhiteSpaceConflict = true;
         }

         ml.d3lLineIdx   = lineIdx;
         ml.bDelta       = ml.srcSelect != A;
         ml.id3l         = it;
         ml.srcRangeLength = 1;

         MergeLine* back = m_mergeLineList.empty() ? 0 : &m_mergeLineList.back();

         bool bSame = back!=0 && sameKindCheck( ml, *back );
         if( bSame )
         {
            ++back->srcRangeLength;
            if ( back->bWhiteSpaceConflict && !ml.bWhiteSpaceConflict )
               back->bWhiteSpaceConflict = false;
         }
         else
         {
            ml.mergeEditLineList.setTotalSizePtr(&m_totalSize);
            m_mergeLineList.push_back( ml );
         }

         if ( ! ml.bConflict )
         {
            MergeLine& tmpBack = m_mergeLineList.back();
            MergeEditLine mel(ml.id3l);
            mel.setSource( ml.srcSelect, bLineRemoved );
            tmpBack.mergeEditLineList.push_back(mel);
         }
         else if ( back==0  || ! back->bConflict || !bSame )
         {
            MergeLine& tmpBack = m_mergeLineList.back();
            MergeEditLine mel(ml.id3l);
            mel.setConflict();
            tmpBack.mergeEditLineList.push_back(mel);
         }
      }
   }

   bool bSolveWhiteSpaceConflicts = false;
   if ( bAutoSolve ) // when true, then the other params are not used and we can change them here. (see all invocations of merge())
   {
      if ( m_pldC==0 && m_pOptions->m_whiteSpace2FileMergeDefault != 0 )  // Only two inputs
      {
         defaultSelector = m_pOptions->m_whiteSpace2FileMergeDefault;
         bWhiteSpaceOnly = true;
         bSolveWhiteSpaceConflicts = true;
      }
      else if ( m_pldC!=0 && m_pOptions->m_whiteSpace3FileMergeDefault != 0 )
      {
         defaultSelector = m_pOptions->m_whiteSpace3FileMergeDefault;
         bWhiteSpaceOnly = true;
         bSolveWhiteSpaceConflicts = true;
      }
   }

   if ( !bAutoSolve || bSolveWhiteSpaceConflicts )
   {
      // Change all auto selections
      MergeLineList::iterator mlIt;
      for( mlIt=m_mergeLineList.begin(); mlIt!=m_mergeLineList.end(); ++mlIt )
      {
         MergeLine& ml = *mlIt;
         bool bConflict = ml.mergeEditLineList.empty() || ml.mergeEditLineList.begin()->isConflict();
         if ( ml.bDelta && ( !bConflictsOnly || bConflict ) && (!bWhiteSpaceOnly || ml.bWhiteSpaceConflict ))
         {
            ml.mergeEditLineList.clear();
            if ( defaultSelector==-1 && ml.bDelta )
            {
               MergeEditLine mel(ml.id3l);;
               mel.setConflict();
               ml.bConflict = true;
               ml.mergeEditLineList.push_back(mel);
            }
            else
            {
               Diff3LineList::const_iterator d3llit=ml.id3l;
               int j;

               for( j=0; j<ml.srcRangeLength; ++j )
               {
                  MergeEditLine mel(d3llit);
                  mel.setSource( defaultSelector, false );

                  int srcLine = defaultSelector==1 ? d3llit->lineA :
                                defaultSelector==2 ? d3llit->lineB :
                                defaultSelector==3 ? d3llit->lineC : -1;

                  if ( srcLine != -1 )
                  {
                     ml.mergeEditLineList.push_back(mel);
                  }

                  ++d3llit;
               }

               if ( ml.mergeEditLineList.empty() ) // Make a line nevertheless
               {
                  MergeEditLine mel(ml.id3l);
                  mel.setRemoved( defaultSelector );
                  ml.mergeEditLineList.push_back(mel);
               }
            }
         }
      }
   }

   MergeLineList::iterator mlIt;
   for( mlIt=m_mergeLineList.begin(); mlIt!=m_mergeLineList.end(); ++mlIt )
   {
      MergeLine& ml = *mlIt;
      // Remove all lines that are empty, because no src lines are there.

      int oldSrcLine = -1;
      int oldSrc = -1;
      MergeEditLineList::iterator melIt;
      for( melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); )
      {
         MergeEditLine& mel = *melIt;
         int melsrc = mel.src();

         int srcLine = mel.isRemoved() ? -1 :
                       melsrc==1 ? mel.id3l()->lineA :
                       melsrc==2 ? mel.id3l()->lineB :
                       melsrc==3 ? mel.id3l()->lineC : -1;

         // At least one line remains because oldSrc != melsrc for first line in list
         // Other empty lines will be removed
         if ( srcLine == -1 && oldSrcLine==-1 && oldSrc == melsrc )
            melIt = ml.mergeEditLineList.erase( melIt );
         else
            ++melIt;

         oldSrcLine = srcLine;
         oldSrc = melsrc;
      }
   }

   if ( bAutoSolve && !bConflictsOnly )
   {
      if ( m_pOptions->m_bRunHistoryAutoMergeOnMergeStart )
         slotMergeHistory();
      if ( m_pOptions->m_bRunRegExpAutoMergeOnMergeStart )
         slotRegExpAutoMerge();
      if ( m_pldC != 0 && ! doRelevantChangesExist() )
         emit noRelevantChangesDetected();
   }

   int nrOfSolvedConflicts = 0;
   int nrOfUnsolvedConflicts = 0;
   int nrOfWhiteSpaceConflicts = 0;

   MergeLineList::iterator i;
   for ( i = m_mergeLineList.begin();  i!=m_mergeLineList.end(); ++i )
   {
      if ( i->bConflict )
         ++nrOfUnsolvedConflicts;
      else if ( i->bDelta )
         ++nrOfSolvedConflicts;

      if ( i->bWhiteSpaceConflict )
         ++nrOfWhiteSpaceConflicts;
   }

   m_pTotalDiffStatus->nofUnsolvedConflicts = nrOfUnsolvedConflicts;
   m_pTotalDiffStatus->nofSolvedConflicts = nrOfSolvedConflicts;
   m_pTotalDiffStatus->nofWhitespaceConflicts = nrOfWhiteSpaceConflicts;


   m_cursorXPos=0;
   m_cursorOldXPos=0;
   m_cursorYPos=0;
   //m_firstLine = 0; // Must not set line/column without scrolling there
   //m_firstColumn = 0;

   setModified(false);

   m_currentMergeLineIt = m_mergeLineList.begin();
   slotGoTop();

   updateAvailabilities();
   update();
}

void MergeResultWindow::setFirstLine(int firstLine)
{
   m_firstLine = max2(0,firstLine);
   update();
}

void MergeResultWindow::setFirstColumn(int firstCol)
{
   m_firstColumn = max2(0,firstCol);
   update();
}

int MergeResultWindow::getNofColumns()
{
   return m_nofColumns;
}

int MergeResultWindow::getNofLines()
{
   return m_totalSize;
}

int MergeResultWindow::getNofVisibleColumns()
{
   QFontMetrics fm = fontMetrics();
   return width()/fm.width('W')-4;
}

int MergeResultWindow::getNofVisibleLines()
{
   QFontMetrics fm = fontMetrics();
   return (height()-3)/fm.height()-2;
}

void MergeResultWindow::resizeEvent( QResizeEvent* e )
{
   QWidget::resizeEvent(e);
   emit resizeSignal();
}

Overview::e_OverviewMode MergeResultWindow::getOverviewMode()
{
   return m_eOverviewMode;
}

void MergeResultWindow::setOverviewMode( Overview::e_OverviewMode eOverviewMode )
{
   m_eOverviewMode = eOverviewMode;
}

// Check whether we should ignore current delta when moving to next/previous delta
bool MergeResultWindow::checkOverviewIgnore(MergeLineList::iterator &i)
{
   if (m_eOverviewMode == Overview::eOMNormal) return false;
   if (m_eOverviewMode == Overview::eOMAvsB)
      return i->mergeDetails == eCAdded || i->mergeDetails == eCDeleted || i->mergeDetails == eCChanged;
   if (m_eOverviewMode == Overview::eOMAvsC)
      return i->mergeDetails == eBAdded || i->mergeDetails == eBDeleted || i->mergeDetails == eBChanged;
   if (m_eOverviewMode == Overview::eOMBvsC)
      return i->mergeDetails == eBCAddedAndEqual || i->mergeDetails == eBCDeleted || i->mergeDetails == eBCChangedAndEqual;
   return false;
}

// Go to prev/next delta/conflict or first/last delta.
void MergeResultWindow::go( e_Direction eDir, e_EndPoint eEndPoint )
{
   assert( eDir==eUp || eDir==eDown );
   MergeLineList::iterator i = m_currentMergeLineIt;
   bool bSkipWhiteConflicts = ! m_pOptions->m_bShowWhiteSpace;
   if( eEndPoint==eEnd )
   {
      if (eDir==eUp) i = m_mergeLineList.begin();     // first mergeline
      else           i = --m_mergeLineList.end();     // last mergeline

      while ( isItAtEnd(eDir==eUp, i) && ! i->bDelta )
      {
         if ( eDir==eUp )  ++i;                       // search downwards
         else              --i;                       // search upwards
      }
   }
   else if ( eEndPoint == eDelta  &&  isItAtEnd(eDir!=eUp, i) )
   {
      do
      {
         if ( eDir==eUp )  --i;
         else              ++i;
      }
      while ( isItAtEnd(eDir!=eUp, i) && ( i->bDelta == false || checkOverviewIgnore(i) || (bSkipWhiteConflicts && i->bWhiteSpaceConflict) ) );
   }
   else if ( eEndPoint == eConflict  &&  isItAtEnd(eDir!=eUp, i) )
   {
      do
      {
         if ( eDir==eUp )  --i;
         else              ++i;
      }
      while ( isItAtEnd(eDir!=eUp, i) && (i->bConflict == false || (bSkipWhiteConflicts && i->bWhiteSpaceConflict) ) );
   }
   else if ( isItAtEnd(eDir!=eUp, i)  &&  eEndPoint == eUnsolvedConflict )
   {
      do
      {
         if ( eDir==eUp )  --i;
         else              ++i;
      }
      while ( isItAtEnd(eDir!=eUp, i) && ! i->mergeEditLineList.begin()->isConflict() );
   }

   if ( isVisible() )
      setFocus();

   setFastSelector( i );
}

bool MergeResultWindow::isDeltaAboveCurrent()
{
   bool bSkipWhiteConflicts = ! m_pOptions->m_bShowWhiteSpace;
   if (m_mergeLineList.empty()) return false;
   MergeLineList::iterator i = m_currentMergeLineIt;
   if (i == m_mergeLineList.begin()) return false;
   do
   {
      --i;
      if ( i->bDelta && !checkOverviewIgnore(i) && !( bSkipWhiteConflicts && i->bWhiteSpaceConflict ) ) return true;
   }
   while (i!=m_mergeLineList.begin());

   return false;
}

bool MergeResultWindow::isDeltaBelowCurrent()
{
   bool bSkipWhiteConflicts = ! m_pOptions->m_bShowWhiteSpace;
   if (m_mergeLineList.empty()) return false;

   MergeLineList::iterator i = m_currentMergeLineIt;
   if (i!=m_mergeLineList.end())
   {
      ++i;
      for( ; i!=m_mergeLineList.end(); ++i )
      {
         if ( i->bDelta && !checkOverviewIgnore(i) && !( bSkipWhiteConflicts && i->bWhiteSpaceConflict ) ) return true;
      }
   }
   return false;
}

bool MergeResultWindow::isConflictAboveCurrent()
{
   if (m_mergeLineList.empty()) return false;
   MergeLineList::iterator i = m_currentMergeLineIt;
   if (i == m_mergeLineList.begin()) return false;

   bool bSkipWhiteConflicts = ! m_pOptions->m_bShowWhiteSpace;

   do
   {
      --i;
      if ( i->bConflict && !(bSkipWhiteConflicts && i->bWhiteSpaceConflict) ) return true;
   } 
   while (i!=m_mergeLineList.begin());
   
   return false;
}

bool MergeResultWindow::isConflictBelowCurrent()
{
   MergeLineList::iterator i = m_currentMergeLineIt;
   if (m_mergeLineList.empty()) return false;

   bool bSkipWhiteConflicts = ! m_pOptions->m_bShowWhiteSpace;

   if (i!=m_mergeLineList.end())
   {
      ++i;
      for( ; i!=m_mergeLineList.end(); ++i )
      {
         if ( i->bConflict && !(bSkipWhiteConflicts && i->bWhiteSpaceConflict) ) return true;
      }
   }
   return false;
}

bool MergeResultWindow::isUnsolvedConflictAtCurrent()
{
   if (m_mergeLineList.empty()) return false;
   MergeLineList::iterator i = m_currentMergeLineIt;
   return i->mergeEditLineList.begin()->isConflict();
}

bool MergeResultWindow::isUnsolvedConflictAboveCurrent()
{
   if (m_mergeLineList.empty()) return false;
   MergeLineList::iterator i = m_currentMergeLineIt;
   if (i == m_mergeLineList.begin()) return false;
   
   do
   {
      --i;
      if ( i->mergeEditLineList.begin()->isConflict() ) return true;
   } 
   while (i!=m_mergeLineList.begin());
   
   return false;
}

bool MergeResultWindow::isUnsolvedConflictBelowCurrent()
{
   MergeLineList::iterator i = m_currentMergeLineIt;
   if (m_mergeLineList.empty()) return false;
   
   if (i!=m_mergeLineList.end())
   {
      ++i;
      for( ; i!=m_mergeLineList.end(); ++i )
      {
         if ( i->mergeEditLineList.begin()->isConflict() ) return true;
      }
   }
   return false;
}

void MergeResultWindow::slotGoTop()
{
   go( eUp, eEnd );
}

void MergeResultWindow::slotGoCurrent()
{
   setFastSelector( m_currentMergeLineIt );
}

void MergeResultWindow::slotGoBottom()
{
   go( eDown, eEnd );
}

void MergeResultWindow::slotGoPrevDelta()
{
   go( eUp, eDelta );
}

void MergeResultWindow::slotGoNextDelta()
{
   go( eDown, eDelta );
}

void MergeResultWindow::slotGoPrevConflict()
{
   go( eUp, eConflict );
}

void MergeResultWindow::slotGoNextConflict()
{
   go( eDown, eConflict );
}

void MergeResultWindow::slotGoPrevUnsolvedConflict()
{
   go( eUp, eUnsolvedConflict );
}

void MergeResultWindow::slotGoNextUnsolvedConflict()
{
   go( eDown, eUnsolvedConflict );
}

/** The line is given as a index in the Diff3LineList.
    The function calculates the corresponding iterator. */
void MergeResultWindow::slotSetFastSelectorLine( int line )
{
   MergeLineList::iterator i;
   for ( i = m_mergeLineList.begin();  i!=m_mergeLineList.end(); ++i )
   {
      if ( line>=i->d3lLineIdx  && line < i->d3lLineIdx + i->srcRangeLength )
      {
         //if ( i->bDelta )
         {
            setFastSelector( i );
         }
         break;
      }
   }
}

int MergeResultWindow::getNrOfUnsolvedConflicts( int* pNrOfWhiteSpaceConflicts )
{
   int nrOfUnsolvedConflicts = 0;
   if (pNrOfWhiteSpaceConflicts!=0)
      *pNrOfWhiteSpaceConflicts = 0;

   MergeLineList::iterator mlIt = m_mergeLineList.begin();
   for(mlIt = m_mergeLineList.begin();mlIt!=m_mergeLineList.end(); ++mlIt)
   {
      MergeLine& ml = *mlIt;
      MergeEditLineList::iterator melIt = ml.mergeEditLineList.begin();
      if ( melIt->isConflict() )
      {
         ++nrOfUnsolvedConflicts;
         if ( ml.bWhiteSpaceConflict &&  pNrOfWhiteSpaceConflicts!=0 )
            ++ *pNrOfWhiteSpaceConflicts;
      }
   }

   return nrOfUnsolvedConflicts;
}

void MergeResultWindow::showNrOfConflicts()
{
   if (!m_pOptions->m_bShowInfoDialogs)
      return;
   int nrOfConflicts = 0;
   MergeLineList::iterator i;
   for ( i = m_mergeLineList.begin();  i!=m_mergeLineList.end(); ++i )
   {
      if ( i->bConflict || i->bDelta )
         ++nrOfConflicts;
   }
   QString totalInfo;
   if ( m_pTotalDiffStatus->bBinaryAEqB && m_pTotalDiffStatus->bBinaryAEqC )
      totalInfo += i18n("All input files are binary equal.");
   else  if ( m_pTotalDiffStatus->bTextAEqB && m_pTotalDiffStatus->bTextAEqC )
      totalInfo += i18n("All input files contain the same text.");
   else {
      if    ( m_pTotalDiffStatus->bBinaryAEqB ) totalInfo += i18n("Files %1 and %2 are binary equal.\n",QString("A"),QString("B"));
      else if ( m_pTotalDiffStatus->bTextAEqB ) totalInfo += i18n("Files %1 and %2 have equal text.\n",QString("A"),QString("B"));
      if    ( m_pTotalDiffStatus->bBinaryAEqC ) totalInfo += i18n("Files %1 and %2 are binary equal.\n",QString("A"),QString("C"));
      else if ( m_pTotalDiffStatus->bTextAEqC ) totalInfo += i18n("Files %1 and %2 have equal text.\n",QString("A"),QString("C"));
      if    ( m_pTotalDiffStatus->bBinaryBEqC ) totalInfo += i18n("Files %1 and %2 are binary equal.\n",QString("B"),QString("C"));
      else if ( m_pTotalDiffStatus->bTextBEqC ) totalInfo += i18n("Files %1 and %2 have equal text.\n",QString("B"),QString("C"));
   }

   int nrOfUnsolvedConflicts = getNrOfUnsolvedConflicts();

   KMessageBox::information( this,
      i18n("Total number of conflicts: ") + QString::number(nrOfConflicts) +
      i18n("\nNr of automatically solved conflicts: ") + QString::number(nrOfConflicts-nrOfUnsolvedConflicts) +
      i18n("\nNr of unsolved conflicts: ") + QString::number(nrOfUnsolvedConflicts) +
      "\n"+totalInfo,
      i18n("Conflicts")
      );
}

void MergeResultWindow::setFastSelector(MergeLineList::iterator i)
{
   if ( i==m_mergeLineList.end() )
      return;
   m_currentMergeLineIt = i;
   emit setFastSelectorRange( i->d3lLineIdx, i->srcRangeLength );

   int line1 = 0;

   MergeLineList::iterator mlIt = m_mergeLineList.begin();
   for(mlIt = m_mergeLineList.begin();mlIt!=m_mergeLineList.end(); ++mlIt)
   {
      if(mlIt==m_currentMergeLineIt)
         break;
      line1 += mlIt->mergeEditLineList.size();
   }

   int nofLines = m_currentMergeLineIt->mergeEditLineList.size();
   int newFirstLine = getBestFirstLine( line1, nofLines, m_firstLine, getNofVisibleLines() );
   if ( newFirstLine != m_firstLine )
   {
      scroll( 0, newFirstLine - m_firstLine );
   }

   if ( m_selection.isEmpty() )
   {
      m_cursorXPos = 0;
      m_cursorOldXPos = 0;
      m_cursorYPos = line1;
   }

   update();
   updateSourceMask();
   emit updateAvailabilities();
}

void MergeResultWindow::choose( int selector )
{
   if ( m_currentMergeLineIt==m_mergeLineList.end() )
      return;

   setModified();

   // First find range for which this change works.
   MergeLine& ml = *m_currentMergeLineIt;

   MergeEditLineList::iterator melIt;

   // Now check if selector is active for this range already.
   bool bActive = false;

   // Remove unneeded lines in the range.
   for( melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); )
   {
      MergeEditLine& mel = *melIt;
      if ( mel.src()==selector )
         bActive = true;

      if ( mel.src()==selector || !mel.isEditableText() || mel.isModified() )
         melIt = ml.mergeEditLineList.erase( melIt );
      else
         ++melIt;
   }

   if ( !bActive )  // Selected source wasn't active.
   {     // Append the lines from selected source here at rangeEnd.
      Diff3LineList::const_iterator d3llit=ml.id3l;
      int j;

      for( j=0; j<ml.srcRangeLength; ++j )
      {
         MergeEditLine mel(d3llit);
         mel.setSource( selector, false );
         ml.mergeEditLineList.push_back(mel);

         ++d3llit;
      }
   }

   if ( ! ml.mergeEditLineList.empty() )
   {
      // Remove all lines that are empty, because no src lines are there.
      for( melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); )
      {
         MergeEditLine& mel = *melIt;

         int srcLine = mel.src()==1 ? mel.id3l()->lineA :
                       mel.src()==2 ? mel.id3l()->lineB :
                       mel.src()==3 ? mel.id3l()->lineC : -1;

         if ( srcLine == -1 )
            melIt = ml.mergeEditLineList.erase( melIt );
         else
            ++melIt;
      }
   }

   if ( ml.mergeEditLineList.empty() )
   {
      // Insert a dummy line:
      MergeEditLine mel(ml.id3l);

      if ( bActive )  mel.setConflict();         // All src entries deleted => conflict
      else            mel.setRemoved(selector);  // No lines in corresponding src found.

      ml.mergeEditLineList.push_back(mel);
   }

   if ( m_cursorYPos >= m_totalSize )
   {
      m_cursorYPos = m_totalSize-1;
      m_cursorXPos = 0;
   }

   update();
   updateSourceMask();
   emit updateAvailabilities();
   int wsc;
   int nofUnsolved = getNrOfUnsolvedConflicts(&wsc);
   m_pStatusBar->showMessage( i18n("Number of remaining unsolved conflicts: %1 (of which %2 are whitespace)"
         ,nofUnsolved,wsc) );
}

// bConflictsOnly: automatically choose for conflicts only (true) or for everywhere (false)
void MergeResultWindow::chooseGlobal(int selector, bool bConflictsOnly, bool bWhiteSpaceOnly )
{
   resetSelection();

   merge( false, selector, bConflictsOnly, bWhiteSpaceOnly );
   setModified( true );
   update();
   int wsc;
   int nofUnsolved = getNrOfUnsolvedConflicts(&wsc);
   m_pStatusBar->showMessage( i18n("Number of remaining unsolved conflicts: %1 (of which %2 are whitespace)"
         ,nofUnsolved,wsc) );
}

void MergeResultWindow::slotAutoSolve()
{
   resetSelection();
   merge( true, -1 );
   setModified( true );
   update();
   int wsc;
   int nofUnsolved = getNrOfUnsolvedConflicts(&wsc);
   m_pStatusBar->showMessage( i18n("Number of remaining unsolved conflicts: %1 (of which %2 are whitespace)"
         ,nofUnsolved,wsc) );
}

void MergeResultWindow::slotUnsolve()
{
   resetSelection();
   merge( false, -1 );
   setModified( true );
   update();
   int wsc;
   int nofUnsolved = getNrOfUnsolvedConflicts(&wsc);
   m_pStatusBar->showMessage( i18n("Number of remaining unsolved conflicts: %1 (of which %2 are whitespace)"
         ,nofUnsolved,wsc) );
}

static QString calcHistoryLead(const QString& s )
{
   // Return the start of the line until the first white char after the first non white char.
   int i;
   for( i=0; i<s.length(); ++i )
   {
      if (s[i]!=' ' && s[i]!='\t')
      {
         for( ; i<s.length(); ++i )
         {
            if (s[i]==' ' || s[i]=='\t')
            {
               return s.left(i);
            }
         }
         return s;  // Very unlikely
      }
   }
   return "";  // Must be an empty string, not a null string.
}

static void findHistoryRange( const QRegExp& historyStart, bool bThreeFiles, const Diff3LineList* pD3LList, 
                             Diff3LineList::const_iterator& iBegin, Diff3LineList::const_iterator& iEnd, int& idxBegin, int& idxEnd )
{
   QString historyLead;
   // Search for start of history
   for( iBegin = pD3LList->begin(), idxBegin=0; iBegin!=pD3LList->end(); ++iBegin, ++idxBegin )
   {
      if ( historyStart.exactMatch( iBegin->getString(A) ) && 
           historyStart.exactMatch( iBegin->getString(B) ) && 
           ( !bThreeFiles || historyStart.exactMatch( iBegin->getString(C) ) ) )
      {
         historyLead = calcHistoryLead( iBegin->getString(A) );
         break;
      }
   }
   // Search for end of history
   for( iEnd = iBegin, idxEnd = idxBegin; iEnd!=pD3LList->end(); ++iEnd, ++idxEnd )
   {
      QString sA = iEnd->getString(A);
      QString sB = iEnd->getString(B);
      QString sC = iEnd->getString(C);
      if ( ! ((sA.isNull() || historyLead == calcHistoryLead(sA) ) &&
              (sB.isNull() || historyLead == calcHistoryLead(sB) ) &&
           (!bThreeFiles || sC.isNull() || historyLead == calcHistoryLead(sC) )
         ))
      {
         break; // End of the history
      }
   }
}

bool findParenthesesGroups( const QString& s, QStringList& sl )
{
   sl.clear();
   int i=0;
   std::list<int> startPosStack;
   int length = s.length();
   for( i=0; i<length; ++i )
   {
      if ( s[i]=='\\' && i+1<length && ( s[i+1]=='\\' || s[i+1]=='(' || s[i+1]==')' ) )
      {
         ++i;
         continue;
      }
      if ( s[i]=='(' )
      {
         startPosStack.push_back(i);
      }
      else if ( s[i]==')' )
      {
         if (startPosStack.empty())
            return false; // Parentheses don't match
         int startPos = startPosStack.back();
         startPosStack.pop_back();
         sl.push_back( s.mid( startPos+1, i-startPos-1 ) );
      }
   }
   return startPosStack.empty(); // false if parentheses don't match
}

QString calcHistorySortKey( const QString& keyOrder, QRegExp& matchedRegExpr, const QStringList& parenthesesGroupList )
{
   QStringList keyOrderList = keyOrder.split(',');
   QString key;
   for ( QStringList::iterator keyIt = keyOrderList.begin(); keyIt!=keyOrderList.end(); ++keyIt )
   {
      if ( (*keyIt).isEmpty() )
         continue;
      bool bOk=false;
      int groupIdx = (*keyIt).toInt(&bOk);
      if (!bOk || groupIdx<0 || groupIdx >(int)parenthesesGroupList.size() )
         continue;
      QString s = matchedRegExpr.cap( groupIdx );
      if ( groupIdx == 0 )
      {
         key += s + " ";
         continue;
      }

      QString groupRegExp = parenthesesGroupList[groupIdx-1];
      if( groupRegExp.indexOf('|')<0 || groupRegExp.indexOf('(')>=0 )
      {
         bool bOk = false;
         int i = s.toInt( &bOk );
         if ( bOk && i>=0 && i<10000 )
            s.sprintf("%04d", i);  // This should help for correct sorting of numbers.
         key += s + " ";
      }
      else
      {
         // Assume that the groupRegExp consists of something like "Jan|Feb|Mar|Apr"
         // s is the string that managed to match.
         // Now we want to know at which position it occurred. e.g. Jan=0, Feb=1, Mar=2, etc.
         QStringList sl = groupRegExp.split( '|' );
         int idx = sl.indexOf( s );
         if (idx<0)
         {
            // Didn't match
         }
         else
         {
            QString sIdx;
            sIdx.sprintf("%02d", idx+1 ); // Up to 99 words in the groupRegExp (more than 12 aren't expected)
            key += sIdx + " ";
         }
      }
   }
   return key;
}

void MergeResultWindow::collectHistoryInformation(
   int src, Diff3LineList::const_iterator iHistoryBegin, Diff3LineList::const_iterator iHistoryEnd,
   HistoryMap& historyMap,
   std::list< HistoryMap::iterator >& hitList  // list of iterators
   )
{
   std::list< HistoryMap::iterator >::iterator itHitListFront = hitList.begin();
   Diff3LineList::const_iterator id3l = iHistoryBegin;
   QString historyLead;
   {
      const LineData* pld = id3l->getLineData(src);
      QString s( pld->pLine, pld->size );
      historyLead = calcHistoryLead(s);
   }
   QRegExp historyStart( m_pOptions->m_historyStartRegExp );
   if ( id3l == iHistoryEnd )
      return;
   ++id3l; // Skip line with "$Log ... $"
   QRegExp newHistoryEntry( m_pOptions->m_historyEntryStartRegExp );
   QStringList parenthesesGroups;
   findParenthesesGroups( m_pOptions->m_historyEntryStartRegExp, parenthesesGroups );
   QString key;
   MergeEditLineList melList;
   bool bPrevLineIsEmpty = true;
   bool bUseRegExp = !m_pOptions->m_historyEntryStartRegExp.isEmpty();
   for(; id3l != iHistoryEnd; ++id3l )
   {
      const LineData* pld = id3l->getLineData(src);
      if ( !pld ) continue;
      QString s( pld->pLine, pld->size );
      if (historyLead.isNull()) historyLead = calcHistoryLead(s);
      QString sLine = s.mid(historyLead.length());
      if ( ( !bUseRegExp && !sLine.trimmed().isEmpty() && bPrevLineIsEmpty )
           || (bUseRegExp && newHistoryEntry.exactMatch( sLine ) )
         )
      {
         if ( !key.isEmpty() && !melList.empty() )
         {
            // Only insert new HistoryMapEntry if key not found; in either case p.first is a valid iterator to element key.
            std::pair<HistoryMap::iterator, bool> p = historyMap.insert(HistoryMap::value_type(key,HistoryMapEntry()));
            HistoryMapEntry& hme = p.first->second;
            if ( src==A ) hme.mellA = melList;
            if ( src==B ) hme.mellB = melList;
            if ( src==C ) hme.mellC = melList;
            if ( p.second ) // Not in list yet?
            {
               hitList.insert( itHitListFront, p.first );
            }
         }

         if ( ! bUseRegExp )
            key = sLine;
         else
            key = calcHistorySortKey(m_pOptions->m_historyEntryStartSortKeyOrder,newHistoryEntry,parenthesesGroups);

         melList.clear();
         melList.push_back( MergeEditLine(id3l,src) );
      }
      else if ( ! historyStart.exactMatch( s ) )
      {
         melList.push_back( MergeEditLine(id3l,src) );
      }

      bPrevLineIsEmpty = sLine.trimmed().isEmpty();
   }
   if ( !key.isEmpty() )
   {
      // Only insert new HistoryMapEntry if key not found; in either case p.first is a valid iterator to element key.
      std::pair<HistoryMap::iterator, bool> p = historyMap.insert(HistoryMap::value_type(key,HistoryMapEntry()));
      HistoryMapEntry& hme = p.first->second;
      if ( src==A ) hme.mellA = melList;
      if ( src==B ) hme.mellB = melList;
      if ( src==C ) hme.mellC = melList;
      if ( p.second ) // Not in list yet?
      {
         hitList.insert( itHitListFront, p.first );
      }
   }
   // End of the history
}

MergeResultWindow::MergeEditLineList& MergeResultWindow::HistoryMapEntry::choice( bool bThreeInputs )
{
   if ( !bThreeInputs )
      return mellA.empty() ? mellB : mellA;
   else
   {
      if ( mellA.empty() )
         return mellC.empty() ? mellB : mellC;       // A doesn't exist, return one that exists
      else if ( ! mellB.empty() && ! mellC.empty() )
      {                                              // A, B and C exist
         return mellA;
      }
      else
         return mellB.empty() ? mellB : mellC;       // A exists, return the one that doesn't exist
   }
}

bool MergeResultWindow::HistoryMapEntry::staysInPlace( bool bThreeInputs, Diff3LineList::const_iterator& iHistoryEnd )
{
   // The entry should stay in place if the decision made by the automerger is correct.
   Diff3LineList::const_iterator& iHistoryLast = iHistoryEnd;
   --iHistoryLast;
   if ( !bThreeInputs )
   {
      if ( !mellA.empty() && !mellB.empty() && mellA.begin()->id3l()==mellB.begin()->id3l() && 
           mellA.back().id3l() == iHistoryLast && mellB.back().id3l() == iHistoryLast )
      {
         iHistoryEnd = mellA.begin()->id3l();
         return true;
      }
      else
      {
         return false;
      }
   }
   else
   {
      if ( !mellA.empty() && !mellB.empty() && !mellC.empty() 
           && mellA.begin()->id3l()==mellB.begin()->id3l() && mellA.begin()->id3l()==mellC.begin()->id3l()
           && mellA.back().id3l() == iHistoryLast && mellB.back().id3l() == iHistoryLast && mellC.back().id3l() == iHistoryLast )
      {
         iHistoryEnd = mellA.begin()->id3l();
         return true;
      }
      else
      {
         return false;
      }
   }
}

void MergeResultWindow::slotMergeHistory()
{
   Diff3LineList::const_iterator iD3LHistoryBegin;
   Diff3LineList::const_iterator iD3LHistoryEnd;
   int d3lHistoryBeginLineIdx = -1;
   int d3lHistoryEndLineIdx = -1;

   // Search for history start, history end in the diff3LineList
   findHistoryRange( QRegExp(m_pOptions->m_historyStartRegExp), m_pldC!=0, m_pDiff3LineList, iD3LHistoryBegin, iD3LHistoryEnd, d3lHistoryBeginLineIdx, d3lHistoryEndLineIdx );

   if (  iD3LHistoryBegin != m_pDiff3LineList->end() )
   {
      // Now collect the historyMap information
      HistoryMap historyMap;
      std::list< HistoryMap::iterator > hitList;
      if (m_pldC==0)
      {
         collectHistoryInformation( A, iD3LHistoryBegin, iD3LHistoryEnd, historyMap, hitList );
         collectHistoryInformation( B, iD3LHistoryBegin, iD3LHistoryEnd, historyMap, hitList );
      }
      else
      {
         collectHistoryInformation( A, iD3LHistoryBegin, iD3LHistoryEnd, historyMap, hitList );
         collectHistoryInformation( B, iD3LHistoryBegin, iD3LHistoryEnd, historyMap, hitList );
         collectHistoryInformation( C, iD3LHistoryBegin, iD3LHistoryEnd, historyMap, hitList );
      }

      Diff3LineList::const_iterator iD3LHistoryOrigEnd = iD3LHistoryEnd;

      bool bHistoryMergeSorting = m_pOptions->m_bHistoryMergeSorting  && ! m_pOptions->m_historyEntryStartSortKeyOrder.isEmpty() && 
                                  ! m_pOptions->m_historyEntryStartRegExp.isEmpty();

      if ( m_pOptions->m_maxNofHistoryEntries==-1 )
      {
         // Remove parts from the historyMap and hitList that stay in place
         if ( bHistoryMergeSorting )
         {
            while ( ! historyMap.empty() )
            {
               HistoryMap::iterator hMapIt = historyMap.begin();
               if( hMapIt->second.staysInPlace( m_pldC!=0, iD3LHistoryEnd ) )
                  historyMap.erase(hMapIt);
               else
                  break;
            }
         }
         else
         {
            while ( ! hitList.empty() )
            {
               HistoryMap::iterator hMapIt = hitList.back();
               if( hMapIt->second.staysInPlace( m_pldC!=0, iD3LHistoryEnd ) )
                  hitList.pop_back();
               else
                  break;
            }
         }
         while (iD3LHistoryOrigEnd != iD3LHistoryEnd)
         {
            --iD3LHistoryOrigEnd;
            --d3lHistoryEndLineIdx;
         }
      }

      MergeLineList::iterator iMLLStart = splitAtDiff3LineIdx(d3lHistoryBeginLineIdx);
      MergeLineList::iterator iMLLEnd   = splitAtDiff3LineIdx(d3lHistoryEndLineIdx);
      // Now join all MergeLines in the history
      MergeLineList::iterator i = iMLLStart;
      if ( i != iMLLEnd )
      {
         ++i;
         while ( i!=iMLLEnd )
         {
            iMLLStart->join(*i);
            i = m_mergeLineList.erase( i );
         }
      }
      iMLLStart->mergeEditLineList.clear();
      // Now insert the complete history into the first MergeLine of the history
      iMLLStart->mergeEditLineList.push_back( MergeEditLine( iD3LHistoryBegin, m_pldC == 0 ? B : C ) );
      QString lead = calcHistoryLead( iD3LHistoryBegin->getString(A) );
      MergeEditLine mel( m_pDiff3LineList->end() );
      mel.setString( lead );
      iMLLStart->mergeEditLineList.push_back(mel);

      int historyCount = 0;
      if ( bHistoryMergeSorting )
      {
         // Create a sorted history
         HistoryMap::reverse_iterator hmit;
         for ( hmit = historyMap.rbegin(); hmit != historyMap.rend(); ++hmit )
         {
            if ( historyCount==m_pOptions->m_maxNofHistoryEntries )
               break;
            ++historyCount;
            HistoryMapEntry& hme = hmit->second;
            MergeEditLineList& mell = hme.choice(m_pldC!=0);
            if (!mell.empty())
               iMLLStart->mergeEditLineList.splice( iMLLStart->mergeEditLineList.end(), mell, mell.begin(), mell.end() );
         }
      }
      else
      {
         // Create history in order of appearance
         std::list< HistoryMap::iterator >::iterator hlit;
         for ( hlit = hitList.begin(); hlit != hitList.end(); ++hlit )
         {
            if ( historyCount==m_pOptions->m_maxNofHistoryEntries )
               break;
            ++historyCount;
            HistoryMapEntry& hme = (*hlit)->second;
            MergeEditLineList& mell = hme.choice(m_pldC!=0);
            if (!mell.empty())
               iMLLStart->mergeEditLineList.splice( iMLLStart->mergeEditLineList.end(), mell, mell.begin(), mell.end() );
         }
         // If the end of start is empty and the first line at the end is empty remove the last line of start
         if ( !iMLLStart->mergeEditLineList.empty() && !iMLLEnd->mergeEditLineList.empty() )
         {
            QString lastLineOfStart = iMLLStart->mergeEditLineList.back().getString(this);
            QString firstLineOfEnd = iMLLEnd->mergeEditLineList.front().getString(this);
            if ( lastLineOfStart.mid(lead.length()).trimmed().isEmpty() && firstLineOfEnd.mid(lead.length()).trimmed().isEmpty() )
               iMLLStart->mergeEditLineList.pop_back();
         }
      }
      setFastSelector( iMLLStart );
      update();
   }
}

void MergeResultWindow::slotRegExpAutoMerge()
{
   if ( m_pOptions->m_autoMergeRegExp.isEmpty() )
      return;

   QRegExp vcsKeywords( m_pOptions->m_autoMergeRegExp );
   MergeLineList::iterator i;
   for ( i=m_mergeLineList.begin(); i!=m_mergeLineList.end(); ++i )
   {
      if (i->bConflict )
      {
         Diff3LineList::const_iterator id3l = i->id3l;
         if ( vcsKeywords.exactMatch( id3l->getString(A) ) && 
              vcsKeywords.exactMatch( id3l->getString(B) ) &&
              (m_pldC==0 || vcsKeywords.exactMatch( id3l->getString(C) )))
         {
            MergeEditLine& mel = *i->mergeEditLineList.begin();
            mel.setSource( m_pldC==0 ? B : C, false );
            splitAtDiff3LineIdx( i->d3lLineIdx+1 );
         }
      }
   }
   update();
}

// This doesn't detect user modifications and should only be called after automatic merge
// This will only do something for three file merge.
// Irrelevant changes are those where all contributions from B are already contained in C.
// Also irrelevant are conflicts automatically solved (automerge regexp and history automerge)
// Precondition: The VCS-keyword would also be C.
bool MergeResultWindow::doRelevantChangesExist()
{
   if ( m_pldC==0 || m_mergeLineList.size() <= 1 )
      return true;

   MergeLineList::iterator i;
   for ( i=m_mergeLineList.begin(); i!=m_mergeLineList.end(); ++i )
   {
      if ( ( i->bConflict && i->mergeEditLineList.begin()->src()!=C )
         || i->srcSelect == B )
      {
         return true;
      }
   }

   return false;
}

// Returns the iterator to the MergeLine after the split
MergeResultWindow::MergeLineList::iterator MergeResultWindow::splitAtDiff3LineIdx( int d3lLineIdx )
{
   MergeLineList::iterator i;
   for ( i = m_mergeLineList.begin();  i!=m_mergeLineList.end(); ++i )
   {
      if ( i->d3lLineIdx==d3lLineIdx )
      {
         // No split needed, this is the beginning of a MergeLine
         return i;
      }
      else if ( i->d3lLineIdx > d3lLineIdx )
      {
         // The split must be in the previous MergeLine
         --i;
         MergeLine& ml = *i;
         MergeLine newML;
         ml.split(newML,d3lLineIdx);
         ++i;
         return m_mergeLineList.insert( i, newML );
      }
   }
   // The split must be in the previous MergeLine
   --i;
   MergeLine& ml = *i;
   MergeLine newML;
   ml.split(newML,d3lLineIdx);
   ++i;
   return m_mergeLineList.insert( i, newML );
}

void MergeResultWindow::slotSplitDiff( int firstD3lLineIdx, int lastD3lLineIdx )
{
   if (lastD3lLineIdx>=0)
      splitAtDiff3LineIdx( lastD3lLineIdx + 1 );
   setFastSelector( splitAtDiff3LineIdx(firstD3lLineIdx) );
}

void MergeResultWindow::slotJoinDiffs( int firstD3lLineIdx, int lastD3lLineIdx )
{
   MergeLineList::iterator i;
   MergeLineList::iterator iMLLStart = m_mergeLineList.end();
   MergeLineList::iterator iMLLEnd   = m_mergeLineList.end();
   for ( i=m_mergeLineList.begin(); i!=m_mergeLineList.end(); ++i )
   {
      MergeLine& ml = *i;
      if ( firstD3lLineIdx >= ml.d3lLineIdx && firstD3lLineIdx < ml.d3lLineIdx + ml.srcRangeLength )
      {
         iMLLStart = i;
      }
      if ( lastD3lLineIdx >= ml.d3lLineIdx && lastD3lLineIdx < ml.d3lLineIdx + ml.srcRangeLength )
      {
         iMLLEnd = i;
         ++iMLLEnd;
         break;
      }
   }

   bool bJoined = false;
   for( i=iMLLStart;  i!=iMLLEnd && i!=m_mergeLineList.end(); )
   {
      if ( i==iMLLStart )
      {
         ++i;
      }
      else
      {
         iMLLStart->join(*i);
         i = m_mergeLineList.erase( i );
         bJoined = true;
      }
   }
   if (bJoined)
   {
      iMLLStart->mergeEditLineList.clear();
      // Insert a conflict line as placeholder
      iMLLStart->mergeEditLineList.push_back( MergeEditLine( iMLLStart->id3l ) );
   }
   setFastSelector( iMLLStart );
}

void MergeResultWindow::myUpdate(int afterMilliSecs)
{
   if ( m_delayedDrawTimer )
      killTimer(m_delayedDrawTimer);
   m_bMyUpdate = true;
   m_delayedDrawTimer = startTimer( afterMilliSecs );
}

void MergeResultWindow::timerEvent(QTimerEvent*)
{
   killTimer(m_delayedDrawTimer);
   m_delayedDrawTimer = 0;

   if ( m_bMyUpdate )
   {
      update();
      m_bMyUpdate = false;
   }

   if ( m_scrollDeltaX != 0 || m_scrollDeltaY != 0 )
   {
      m_selection.end( m_selection.lastLine + m_scrollDeltaY, m_selection.lastPos +  m_scrollDeltaX );
      emit scroll( m_scrollDeltaX, m_scrollDeltaY );
      killTimer(m_delayedDrawTimer);
      m_delayedDrawTimer = startTimer(50);
   }
}

QString MergeResultWindow::MergeEditLine::getString( const MergeResultWindow* mrw )
{
   if ( isRemoved() )   { return QString(); }

   if ( ! isModified() )
   {
      int src = m_src;
      if ( src == 0 )   { return QString(); }
      const Diff3Line& d3l = *m_id3l;
      const LineData* pld = 0;
      assert( src == A || src == B || src == C );
      if      ( src == A && d3l.lineA!=-1 ) pld = &mrw->m_pldA[ d3l.lineA ];
      else if ( src == B && d3l.lineB!=-1 ) pld = &mrw->m_pldB[ d3l.lineB ];
      else if ( src == C && d3l.lineC!=-1 ) pld = &mrw->m_pldC[ d3l.lineC ];

      if ( pld == 0 )
      {
         // assert(false);      This is no error.
         return QString();
      }

      return QString( pld->pLine, pld->size );
   }
   else
   {
      return m_str;
   }
   return 0;
}

/// Converts the cursor-posOnScreen into a text index, considering tabulators.
int convertToPosInText( const QString& s, int posOnScreen, int tabSize )
{
   int localPosOnScreen = 0;
   int size=s.length();
   for ( int i=0; i<size; ++i )
   {
      if ( localPosOnScreen>=posOnScreen )
         return i;

      // All letters except tabulator have width one.
      int letterWidth = s[i]!='\t' ? 1 : tabber( localPosOnScreen, tabSize );

      localPosOnScreen += letterWidth;

      if ( localPosOnScreen>posOnScreen )
         return i;
   }
   return size;
}


/// Converts the index into the text to a cursor-posOnScreen considering tabulators.
int convertToPosOnScreen( const QString& p, int posInText, int tabSize )
{
   int posOnScreen = 0;
   for ( int i=0; i<posInText; ++i )
   {
      // All letters except tabulator have width one.
      int letterWidth = p[i]!='\t' ? 1 : tabber( posOnScreen, tabSize );

      posOnScreen += letterWidth;
   }
   return posOnScreen;
}

void MergeResultWindow::writeLine(
   MyPainter& p, int line, const QString& str,
   int srcSelect, e_MergeDetails mergeDetails, int rangeMark, bool bUserModified, bool bLineRemoved, bool bWhiteSpaceConflict
   )
{
   const QFontMetrics& fm = fontMetrics();
   int fontHeight = fm.height();
   int fontWidth = fm.width("W");
   int fontAscent = fm.ascent();

   int topLineYOffset = 0;
   int xOffset = fontWidth * leftInfoWidth;

   int yOffset = ( line-m_firstLine ) * fontHeight;
   if ( yOffset < 0 || yOffset > height() )
      return;

   yOffset += topLineYOffset;

   QString srcName = " ";
   if ( bUserModified )                                     srcName = "m";
   else if ( srcSelect == A && mergeDetails != eNoChange )  srcName = "A";
   else if ( srcSelect == B )                               srcName = "B";
   else if ( srcSelect == C )                               srcName = "C";

   if ( rangeMark & 4 )
   {
      p.fillRect( xOffset, yOffset, width(), fontHeight, m_pOptions->m_currentRangeBgColor );
   }

   if( (srcSelect > 0 || bUserModified ) && !bLineRemoved )
   {
      int outPos = 0;
      QString s;
      int size = str.length();
      for ( int i=0; i<size; ++i )
      {
         int spaces = 1;
         if ( str[i]=='\t' )
         {
            spaces = tabber( outPos, m_pOptions->m_tabSize );
            for( int j=0; j<spaces; ++j )
               s+=' ';
         }
         else
         {
            s+=str[i];
         }
         outPos += spaces;
      }

      if ( m_selection.lineWithin( line ) )
      {
         int firstPosInLine = convertToPosOnScreen( str, convertToPosInText( str, m_selection.firstPosInLine(line), m_pOptions->m_tabSize ),m_pOptions->m_tabSize );
         int lastPosInLine  = convertToPosOnScreen( str, convertToPosInText( str, m_selection.lastPosInLine(line), m_pOptions->m_tabSize ), m_pOptions->m_tabSize );
         int lengthInLine = max2(0,lastPosInLine - firstPosInLine);
         if (lengthInLine>0) m_selection.bSelectionContainsData = true;

         if ( lengthInLine < int(s.length()) )
         {                                // Draw a normal line first
            p.setPen( m_pOptions->m_fgColor );
            p.drawText( xOffset, yOffset+fontAscent,  s.mid(m_firstColumn), true );
         }
         int firstPosInLine2 = max2( firstPosInLine, m_firstColumn );
         int lengthInLine2 = max2(0,lastPosInLine - firstPosInLine2);

         if( m_selection.lineWithin( line+1 ) )
            p.fillRect( xOffset + fontWidth*(firstPosInLine2-m_firstColumn), yOffset,
               width(), fontHeight, palette().highlight() );
         else if ( lengthInLine2>0 )
            p.fillRect( xOffset + fontWidth*(firstPosInLine2-m_firstColumn), yOffset,
               fontWidth*lengthInLine2, fontHeight, palette().highlight() );

         p.setPen( palette().highlightedText().color() );
         p.drawText( xOffset + fontWidth*(firstPosInLine2-m_firstColumn), yOffset+fontAscent,
            s.mid(firstPosInLine2,lengthInLine2), true );
      }
      else
      {
         p.setPen( m_pOptions->m_fgColor );
         p.drawText( xOffset, yOffset+fontAscent, s.mid(m_firstColumn), true );
      }

      p.setPen( m_pOptions->m_fgColor );
      if ( m_cursorYPos==line )
      {
         m_cursorXPos = minMaxLimiter( m_cursorXPos, 0, outPos );
         m_cursorXPos = convertToPosOnScreen( str, convertToPosInText( str, m_cursorXPos, m_pOptions->m_tabSize ),m_pOptions->m_tabSize );
      }

      p.drawText( 1, yOffset+fontAscent, srcName, true );
   }
   else if ( bLineRemoved )
   {
      p.setPen( m_pOptions->m_colorForConflict );
      p.drawText( xOffset, yOffset+fontAscent, i18n("<No src line>") );
      p.drawText( 1, yOffset+fontAscent, srcName );
      if ( m_cursorYPos==line ) m_cursorXPos = 0;
   }
   else if ( srcSelect == 0 )
   {
      p.setPen( m_pOptions->m_colorForConflict );
      if ( bWhiteSpaceConflict )
         p.drawText( xOffset, yOffset+fontAscent, i18n("<Merge Conflict (Whitespace only)>") );
      else
         p.drawText( xOffset, yOffset+fontAscent, i18n("<Merge Conflict>") );
      p.drawText( 1, yOffset+fontAscent, "?" );
      if ( m_cursorYPos==line ) m_cursorXPos = 0;
   }
   else assert(false);

   xOffset -= fontWidth;
   p.setPen( m_pOptions->m_fgColor );
   if ( rangeMark & 1 ) // begin mark
   {
      p.drawLine( xOffset, yOffset+1, xOffset, yOffset+fontHeight/2 );
      p.drawLine( xOffset, yOffset+1, xOffset-2, yOffset+1 );
   }
   else
   {
      p.drawLine( xOffset, yOffset, xOffset, yOffset+fontHeight/2 );
   }

   if ( rangeMark & 2 ) // end mark
   {
      p.drawLine( xOffset, yOffset+fontHeight/2, xOffset, yOffset+fontHeight-1 );
      p.drawLine( xOffset, yOffset+fontHeight-1, xOffset-2, yOffset+fontHeight-1 );
   }
   else
   {
      p.drawLine( xOffset, yOffset+fontHeight/2, xOffset, yOffset+fontHeight );
   }

   if ( rangeMark & 4 )
   {
      p.fillRect( xOffset + 3, yOffset, 3, fontHeight, m_pOptions->m_fgColor );
/*      p.setPen( blue );
      p.drawLine( xOffset+2, yOffset, xOffset+2, yOffset+fontHeight-1 );
      p.drawLine( xOffset+3, yOffset, xOffset+3, yOffset+fontHeight-1 );*/
   }
}

void MergeResultWindow::setPaintingAllowed(bool bPaintingAllowed)
{
   m_bPaintingAllowed = bPaintingAllowed;
   if ( !m_bPaintingAllowed )
   {
      m_currentMergeLineIt = m_mergeLineList.end();
      reset();
   }
   update();
}

void MergeResultWindow::paintEvent( QPaintEvent* )
{
   if (m_pDiff3LineList==0 || !m_bPaintingAllowed) 
      return;

   bool bOldSelectionContainsData = m_selection.bSelectionContainsData;
   const QFontMetrics& fm = fontMetrics();
   int fontHeight = fm.height();
   int fontWidth = fm.width("W");
   int fontAscent = fm.ascent();

   if ( !m_bCursorUpdate )  // Don't redraw everything for blinking cursor?
   {
      m_selection.bSelectionContainsData = false;
      if ( size() != m_pixmap.size() )
         m_pixmap = QPixmap(size());

      MyPainter p(&m_pixmap, m_pOptions->m_bRightToLeftLanguage, width(), fontWidth);
      p.setFont( font() );
      p.QPainter::fillRect( rect(), m_pOptions->m_bgColor );

      //int visibleLines = height() / fontHeight;

      int lastVisibleLine = m_firstLine + getNofVisibleLines() + 5;
      int nofColumns = 0;
      int line = 0;
      MergeLineList::iterator mlIt = m_mergeLineList.begin();
      for(mlIt = m_mergeLineList.begin();mlIt!=m_mergeLineList.end(); ++mlIt)
      {
         MergeLine& ml = *mlIt;
         if ( line > lastVisibleLine  || line + ml.mergeEditLineList.size() < m_firstLine)
         {
            line += ml.mergeEditLineList.size();
         }
         else
         {
            MergeEditLineList::iterator melIt;
            for( melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt )
            {
               if (line>=m_firstLine && line<=lastVisibleLine)
               {
                  MergeEditLine& mel = *melIt;
                  MergeEditLineList::iterator melIt1 = melIt;
                  ++melIt1;

                  int rangeMark = 0;
                  if ( melIt==ml.mergeEditLineList.begin() ) rangeMark |= 1; // Begin range mark
                  if ( melIt1==ml.mergeEditLineList.end() )  rangeMark |= 2; // End range mark

                  if ( mlIt == m_currentMergeLineIt )        rangeMark |= 4; // Mark of the current line

                  QString s;
                  s = mel.getString( this );
                  if ( convertToPosOnScreen(s,s.length(),m_pOptions->m_tabSize) >nofColumns)
                     nofColumns = s.length();

                  writeLine( p, line, s, mel.src(), ml.mergeDetails, rangeMark,
                     mel.isModified(), mel.isRemoved(), ml.bWhiteSpaceConflict );
               }
               ++line;
            }
         }
      }

      if ( line != m_nofLines || nofColumns != m_nofColumns )
      {
         m_nofLines = line;
         assert( m_nofLines == m_totalSize );

         m_nofColumns = nofColumns;
         emit resizeSignal();
      }

      p.end();
   }

   QPainter painter(this);

   //int topLineYOffset = 0;
   //int xOffset = fontWidth * leftInfoWidth;
   //int yOffset = ( m_cursorYPos - m_firstLine ) * fontHeight + topLineYOffset;
   //int xCursor = ( m_cursorXPos - m_firstColumn ) * fontWidth + xOffset;

   if ( !m_bCursorUpdate )
      painter.drawPixmap(0,0, m_pixmap);
   else
   {
      painter.drawPixmap(0,0, m_pixmap ); // Draw everything. (Internally cursor rect is clipped anyway.)
      //if (!m_pOptions->m_bRightToLeftLanguage)
      //   painter.drawPixmap(xCursor-2, yOffset, m_pixmap,
      //                      xCursor-2, yOffset, 5, fontAscent+2 );
      //else
      //   painter.drawPixmap(width()-1-4-(xCursor-2), yOffset, m_pixmap,
      //                      width()-1-4-(xCursor-2), yOffset, 5, fontAscent+2 );
      m_bCursorUpdate = false;
   }
   painter.end();

   if ( m_bCursorOn && hasFocus() && m_cursorYPos>=m_firstLine )
   {
      MyPainter painter(this, m_pOptions->m_bRightToLeftLanguage, width(), fontWidth);
      int topLineYOffset = 0;
      int xOffset = fontWidth * leftInfoWidth;

      int yOffset = ( m_cursorYPos-m_firstLine ) * fontHeight + topLineYOffset;

      int xCursor = ( m_cursorXPos - m_firstColumn ) * fontWidth + xOffset;

      painter.setPen( m_pOptions->m_fgColor );

      painter.drawLine( xCursor, yOffset, xCursor, yOffset+fontAscent );
      painter.drawLine( xCursor-2, yOffset, xCursor+2, yOffset );
      painter.drawLine( xCursor-2, yOffset+fontAscent+1, xCursor+2, yOffset+fontAscent+1 );
   }

   if( !bOldSelectionContainsData  &&  m_selection.bSelectionContainsData )
      emit newSelection();
}

void MergeResultWindow::updateSourceMask()
{
   int srcMask=0; 
   int enabledMask = 0;
   if( !hasFocus() || m_pDiff3LineList==0 || !m_bPaintingAllowed || m_currentMergeLineIt == m_mergeLineList.end() )
   {
      srcMask = 0;
      enabledMask = 0;
   }
   else
   {
      enabledMask = m_pldC==0 ? 3 : 7;
      MergeLine& ml = *m_currentMergeLineIt;

      srcMask = 0;
      bool bModified = false;
      MergeEditLineList::iterator melIt;
      for( melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt )
      {
         MergeEditLine& mel = *melIt;
         if ( mel.src()==1 ) srcMask |= 1;
         if ( mel.src()==2 ) srcMask |= 2;
         if ( mel.src()==3 ) srcMask |= 4;
         if ( mel.isModified() || !mel.isEditableText() ) bModified = true;
      }

      if ( ml.mergeDetails == eNoChange ) 
      {
         srcMask = 0; 
         enabledMask = bModified ? 1 : 0; 
      }
   }

   emit sourceMask( srcMask, enabledMask );
}

void MergeResultWindow::focusInEvent( QFocusEvent* e )
{
   updateSourceMask();
   QWidget::focusInEvent(e);
}

void MergeResultWindow::convertToLinePos( int x, int y, int& line, int& pos )
{
   const QFontMetrics& fm = fontMetrics();
   int fontHeight = fm.height();
   int fontWidth = fm.width('W');
   int xOffset = (leftInfoWidth-m_firstColumn)*fontWidth;
   int topLineYOffset = 0;

   int yOffset = topLineYOffset - m_firstLine * fontHeight;

   line = min2( ( y - yOffset ) / fontHeight, m_totalSize-1 );
   if ( ! m_pOptions->m_bRightToLeftLanguage )
      pos  = ( x - xOffset ) / fontWidth;
   else 
      pos  = ( (width() - 1 - x) - xOffset ) / fontWidth;
}

void MergeResultWindow::mousePressEvent ( QMouseEvent* e )
{
   m_bCursorOn = true;

   int line;
   int pos;
   convertToLinePos( e->x(), e->y(), line, pos );

   bool bLMB = e->button() == Qt::LeftButton;
   bool bMMB = e->button() == Qt::MidButton;
   bool bRMB = e->button() == Qt::RightButton;

   if ( (bLMB && (pos < m_firstColumn)) || bRMB )       // Fast range selection
   {
      m_cursorXPos = 0;
      m_cursorOldXPos = 0;
      m_cursorYPos = max2(line,0);
      int l = 0;
      MergeLineList::iterator i = m_mergeLineList.begin();
      for(i = m_mergeLineList.begin();i!=m_mergeLineList.end(); ++i)
      {
         if (l==line)
            break;

         l += i->mergeEditLineList.size();
         if (l>line)
            break;
      }
      m_selection.reset();     // Disable current selection

      m_bCursorOn = true;
      setFastSelector( i );

      if (bRMB)
      {
         showPopupMenu( QCursor::pos() );
      }
   }
   else if ( bLMB )                  // Normal cursor placement
   {
      pos = max2(pos,0);
      line = max2(line,0);
      if ( e->QInputEvent::modifiers() & Qt::ShiftModifier )
      {
         if (m_selection.firstLine==-1)
            m_selection.start( line, pos );
         m_selection.end( line, pos );
      }
      else
      {
         // Selection
         m_selection.reset();
         m_selection.start( line, pos );
         m_selection.end( line, pos );
      }
      m_cursorXPos = pos;
      m_cursorOldXPos = pos;
      m_cursorYPos = line;

      update();
      //showStatusLine( line, m_winIdx, m_pFilename, m_pDiff3LineList, m_pStatusBar );
   }
   else if ( bMMB )        // Paste clipboard
   {
      pos = max2(pos,0);
      line = max2(line,0);

      m_selection.reset();
      m_cursorXPos = pos;
      m_cursorOldXPos = pos;
      m_cursorYPos = line;

      pasteClipboard( true );
   }
}

void MergeResultWindow::mouseDoubleClickEvent( QMouseEvent* e )
{
   if ( e->button() == Qt::LeftButton )
   {
      int line;
      int pos;
      convertToLinePos( e->x(), e->y(), line, pos );
      m_cursorXPos = pos;
      m_cursorOldXPos = pos;
      m_cursorYPos = line;

      // Get the string data of the current line

      MergeLineList::iterator mlIt;
      MergeEditLineList::iterator melIt;
      calcIteratorFromLineNr( line, mlIt, melIt );
      QString s = melIt->getString( this );

      if ( !s.isEmpty() )
      {
         int pos1, pos2;

         calcTokenPos( s, pos, pos1, pos2, m_pOptions->m_tabSize );

         resetSelection();
         m_selection.start( line, convertToPosOnScreen( s, pos1, m_pOptions->m_tabSize ) );
         m_selection.end( line, convertToPosOnScreen( s, pos2, m_pOptions->m_tabSize ) );

         update();
         // emit selectionEnd() happens in the mouseReleaseEvent.
      }
   }
}

void MergeResultWindow::mouseReleaseEvent ( QMouseEvent * e )
{
   if ( e->button() == Qt::LeftButton )
   {
      if (m_delayedDrawTimer)
      {
         killTimer(m_delayedDrawTimer);
         m_delayedDrawTimer = 0;
      }

      if (m_selection.firstLine != -1 )
      {
         emit selectionEnd();
      }
   }
}

void MergeResultWindow::mouseMoveEvent ( QMouseEvent * e )
{
   int line;
   int pos;
   convertToLinePos( e->x(), e->y(), line, pos );
   m_cursorXPos = pos;
   m_cursorOldXPos = pos;
   m_cursorYPos = line;
   if (m_selection.firstLine != -1 )
   {
      m_selection.end( line, pos );
      myUpdate(0);

      //showStatusLine( line, m_winIdx, m_pFilename, m_pDiff3LineList, m_pStatusBar );

      // Scroll because mouse moved out of the window
      const QFontMetrics& fm = fontMetrics();
      int fontWidth = fm.width('W');
      int topLineYOffset = 0;
      int deltaX=0;
      int deltaY=0;
      if ( ! m_pOptions->m_bRightToLeftLanguage )
      {
         if ( e->x() < leftInfoWidth*fontWidth )       deltaX=-1;
         if ( e->x() > width()     )                   deltaX=+1;
      }
      else
      {
         if ( e->x() > width()-1-leftInfoWidth*fontWidth )   deltaX=-1;
         if ( e->x() < fontWidth )                           deltaX=+1;
      }
      if ( e->y() < topLineYOffset )    deltaY=-1;
      if ( e->y() > height() )          deltaY=+1;
      m_scrollDeltaX = deltaX;
      m_scrollDeltaY = deltaY;
      if ( deltaX != 0 || deltaY!= 0)
      {
         emit scroll( deltaX, deltaY );
      }
   }
}


void MergeResultWindow::slotCursorUpdate()
{
   m_cursorTimer.stop();
   m_bCursorOn = !m_bCursorOn;

   if ( isVisible() )
   {
      m_bCursorUpdate = true;

      const QFontMetrics& fm = fontMetrics();
      int fontWidth = fm.width("W");
      int topLineYOffset = 0;
      int xOffset = fontWidth * leftInfoWidth;
      int yOffset = ( m_cursorYPos - m_firstLine ) * fm.height() + topLineYOffset;
      int xCursor = ( m_cursorXPos - m_firstColumn ) * fontWidth + xOffset;

      if (!m_pOptions->m_bRightToLeftLanguage)
         repaint( xCursor-2, yOffset, 5, fm.ascent()+2 );
      else
         repaint( width()-1-4-(xCursor-2), yOffset, 5, fm.ascent()+2 );

      m_bCursorUpdate=false;
   }

   m_cursorTimer.start(500);
}


void MergeResultWindow::wheelEvent( QWheelEvent* e )
{
    int d = -e->delta()*QApplication::wheelScrollLines()/120;
    e->accept();
    scroll( 0, min2(d, getNofVisibleLines()) );
}

bool MergeResultWindow::event( QEvent* e )
{
   if ( e->type()==QEvent::KeyPress )
   {
      QKeyEvent *ke = static_cast<QKeyEvent *>(e);
      if (ke->key() == Qt::Key_Tab) 
      {
          // special tab handling here to avoid moving focus
          keyPressEvent( ke );
          return true;
      }
   }
   return QWidget::event(e);
}
void MergeResultWindow::keyPressEvent( QKeyEvent* e )
{
   int y = m_cursorYPos;
   MergeLineList::iterator mlIt;
   MergeEditLineList::iterator melIt;
   calcIteratorFromLineNr( y, mlIt, melIt );

   QString str = melIt->getString( this );
   int x = convertToPosInText( str, m_cursorXPos, m_pOptions->m_tabSize );

   bool bCtrl  = ( e->QInputEvent::modifiers() & Qt::ControlModifier ) != 0 ;
   bool bShift = ( e->QInputEvent::modifiers() & Qt::ShiftModifier   ) != 0 ;
   #ifdef _WIN32
   bool bAlt   = ( e->QInputEvent::modifiers() & Qt::AltModifier     ) != 0 ;
   if ( bCtrl && bAlt ){ bCtrl=false; bAlt=false; }  // AltGr-Key pressed.
   #endif

   bool bYMoveKey = false;
   // Special keys
   switch ( e->key() )
   {
      case  Qt::Key_Escape:       break;
      //case  Key_Tab:          break;
      case  Qt::Key_Backtab:      break;
      case  Qt::Key_Delete:
      {
         if ( deleteSelection2( str, x, y, mlIt, melIt )) break;
         if( !melIt->isEditableText() )  break;
         if (x>=(int)str.length())
         {
            if ( y<m_totalSize-1 )
            {
               setModified();
               MergeLineList::iterator mlIt1;
               MergeEditLineList::iterator melIt1;
               calcIteratorFromLineNr( y+1, mlIt1, melIt1 );
               if ( melIt1->isEditableText() )
               {
                  QString s2 = melIt1->getString( this );
                  melIt->setString( str + s2 );

                  // Remove the line
                  if ( mlIt1->mergeEditLineList.size()>1 )
                     mlIt1->mergeEditLineList.erase( melIt1 );
                  else
                     melIt1->setRemoved();
               }
            }
         }
         else
         {
            QString s = str.left(x);
            s += str.mid( x+1 );
            melIt->setString( s );
            setModified();
         }
         break;
      }
      case  Qt::Key_Backspace:
      {
         if ( deleteSelection2( str, x, y, mlIt, melIt )) break;
         if( !melIt->isEditableText() )  break;
         if (x==0)
         {
            if ( y>0 )
            {
               setModified();
               MergeLineList::iterator mlIt1;
               MergeEditLineList::iterator melIt1;
               calcIteratorFromLineNr( y-1, mlIt1, melIt1 );
               if ( melIt1->isEditableText() )
               {
                  QString s1 = melIt1->getString( this );
                  melIt1->setString( s1 + str );

                  // Remove the previous line
                  if ( mlIt->mergeEditLineList.size()>1 )
                     mlIt->mergeEditLineList.erase( melIt );
                  else
                     melIt->setRemoved();

                  --y;
                  x=str.length();
               }
            }
         }
         else
         {
            QString s = str.left( x-1 );
            s += str.mid( x );
            --x;
            melIt->setString( s );
            setModified();
         }
         break;
      }
      case  Qt::Key_Return:
      case  Qt::Key_Enter:
      {
         if( !melIt->isEditableText() )  break;
         deleteSelection2( str, x, y, mlIt, melIt );
         setModified();
         QString indentation;
         if ( m_pOptions->m_bAutoIndentation )
         {  // calc last indentation
            MergeLineList::iterator mlIt1 = mlIt;
            MergeEditLineList::iterator melIt1 = melIt;
            for(;;) {
               const QString s = melIt1->getString(this);
               if ( !s.isEmpty() ) {
                  int i;
                  for( i=0; i<s.length(); ++i ){ if(s[i]!=' ' && s[i]!='\t') break; }
                  if (i<s.length()) {
                     indentation = s.left(i);
                     break;
                  }
               }
               // Go back one line
               if ( melIt1 != mlIt1->mergeEditLineList.begin() )
                  --melIt1;
               else
               {
                  if ( mlIt1 == m_mergeLineList.begin() ) break;
                  --mlIt1;
                  melIt1 = mlIt1->mergeEditLineList.end();
                  --melIt1;
               }
            }
         }
         MergeEditLine mel(mlIt->id3l);  // Associate every mel with an id3l, even if not really valid.
         mel.setString( indentation + str.mid(x) );

         if ( x<(int)str.length() ) // Cut off the old line.
         {
            // Since ps possibly points into melIt->str, first copy it into a temporary.
            QString temp = str.left(x);
            melIt->setString( temp );
         }

         ++melIt;
         mlIt->mergeEditLineList.insert( melIt, mel );
         x = indentation.length();
         ++y;
         break;
      }
      case  Qt::Key_Insert:   m_bInsertMode = !m_bInsertMode;    break;
      case  Qt::Key_Pause:        break;
      case  Qt::Key_Print:        break;
      case  Qt::Key_SysReq:       break;
      case  Qt::Key_Home:     x=0;        if(bCtrl){y=0;      }  break;   // cursor movement
      case  Qt::Key_End:      x=INT_MAX;  if(bCtrl){y=INT_MAX;}  break;

      case  Qt::Key_Left:
      case  Qt::Key_Right:
         if ( (e->key()==Qt::Key_Left) ^ m_pOptions->m_bRightToLeftLanguage ) // operator^: XOR
         {
            if ( !bCtrl )
            {
               --x;
               if(x<0 && y>0){--y; x=INT_MAX;}
            }
            else
            {
               while( x>0  &&  (str[x-1]==' ' || str[x-1]=='\t') ) --x;
               while( x>0  &&  (str[x-1]!=' ' && str[x-1]!='\t') ) --x;
            }
         }
         else
         {
            if ( !bCtrl )
            {
               ++x;  if(x>(int)str.length() && y<m_totalSize-1){ ++y; x=0; }
            }

            else
            {
               while( x<(int)str.length()  &&  (str[x]==' ' || str[x]=='\t') ) ++x;
               while( x<(int)str.length()  &&  (str[x]!=' ' && str[x]!='\t') ) ++x;
            }
         }
         break;

      case  Qt::Key_Up:       if (!bCtrl){ --y;                     bYMoveKey=true; }  break;
      case  Qt::Key_Down:     if (!bCtrl){ ++y;                     bYMoveKey=true; }  break;
      case  Qt::Key_PageUp:   if (!bCtrl){ y-=getNofVisibleLines(); bYMoveKey=true; }  break;
      case  Qt::Key_PageDown: if (!bCtrl){ y+=getNofVisibleLines(); bYMoveKey=true; }  break;
      default:
      {
         QString t = e->text();
         if( t.isEmpty() || bCtrl )
         {  e->ignore();       return; }
         else
         {
            if( bCtrl )
            {
               e->ignore();       return;
            }
            else
            {
               if( !melIt->isEditableText() )  break;
               deleteSelection2( str, x, y, mlIt, melIt );

               setModified();
               // Characters to insert
               QString s=str;
               if ( t[0]=='\t' && m_pOptions->m_bReplaceTabs )
               {
                  int spaces = (m_cursorXPos / m_pOptions->m_tabSize + 1)*m_pOptions->m_tabSize - m_cursorXPos;
                  t.fill( ' ', spaces );
               }
               if ( m_bInsertMode )
                  s.insert( x, t );
               else
                  s.replace( x, t.length(), t );

               melIt->setString( s );
               x += t.length();
               bShift = false;
            }
         }
      }
   }

   y = minMaxLimiter( y, 0, m_totalSize-1 );

   calcIteratorFromLineNr( y, mlIt, melIt );
   str = melIt->getString( this );

   x = minMaxLimiter( x, 0, (int)str.length() );

   int newFirstLine = m_firstLine;
   int newFirstColumn = m_firstColumn;

   if ( y<m_firstLine )
      newFirstLine = y;
   else if ( y > m_firstLine + getNofVisibleLines() )
      newFirstLine = y - getNofVisibleLines();

   if (bYMoveKey)
      x=convertToPosInText( str, m_cursorOldXPos, m_pOptions->m_tabSize );

   int xOnScreen = convertToPosOnScreen( str, x, m_pOptions->m_tabSize );
   if ( xOnScreen<m_firstColumn )
      newFirstColumn = xOnScreen;
   else if ( xOnScreen > m_firstColumn + getNofVisibleColumns() )
      newFirstColumn = xOnScreen - getNofVisibleColumns();

   if ( bShift )
   {
      if (m_selection.firstLine==-1)
         m_selection.start( m_cursorYPos, m_cursorXPos );

      m_selection.end( y, xOnScreen );
   }
   else
      m_selection.reset();

   m_cursorYPos = y;
   m_cursorXPos = xOnScreen;
   if ( m_cursorXPos>m_nofColumns )
   {
      m_nofColumns = m_cursorXPos;
      emit resizeSignal();
   }
   if ( ! bYMoveKey )
      m_cursorOldXPos = m_cursorXPos;

   m_bCursorOn = false;

   if ( newFirstLine!=m_firstLine  ||  newFirstColumn!=m_firstColumn )
   {
      m_bCursorOn = true;
      scroll( newFirstColumn-m_firstColumn, newFirstLine-m_firstLine );
      return;
   }

   m_bCursorOn = true;
   update();
}

void MergeResultWindow::calcIteratorFromLineNr(
   int line,
   MergeResultWindow::MergeLineList::iterator& mlIt,
   MergeResultWindow::MergeEditLineList::iterator& melIt
   )
{
   for( mlIt = m_mergeLineList.begin(); mlIt!=m_mergeLineList.end(); ++mlIt)
   {
      MergeLine& ml = *mlIt;
      if ( line > ml.mergeEditLineList.size() )
      {
         line -= ml.mergeEditLineList.size();
      }
      else
      {
         for( melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt )
         {
            --line;
            if (line<0) return;
         }
      }
   }
   assert(false);
}


QString MergeResultWindow::getSelection()
{
   QString selectionString;

   int line = 0;
   MergeLineList::iterator mlIt = m_mergeLineList.begin();
   for(mlIt = m_mergeLineList.begin();mlIt!=m_mergeLineList.end(); ++mlIt)
   {
      MergeLine& ml = *mlIt;
      MergeEditLineList::iterator melIt;
      for( melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt )
      {
         MergeEditLine& mel = *melIt;

         if ( m_selection.lineWithin(line) )
         {
            int outPos = 0;
            if (mel.isEditableText())
            {
               const QString str = mel.getString( this );

               // Consider tabs

               for( int i=0; i<str.length(); ++i )
               {
                  int spaces = 1;
                  if ( str[i]=='\t' )
                  {
                     spaces = tabber( outPos, m_pOptions->m_tabSize );
                  }

                  if( m_selection.within( line, outPos ) )
                  {
                     selectionString += str[i];
                  }

                  outPos += spaces;
               }
            }
            else if ( mel.isConflict() )
            {
               selectionString += i18n("<Merge Conflict>");
            }

            if( m_selection.within( line, outPos ) )
            {
               #ifdef _WIN32
               selectionString += '\r';
               #endif
               selectionString += '\n';
            }
         }

         ++line;
      }
   }

   return selectionString;
}

bool MergeResultWindow::deleteSelection2( QString& s, int& x, int& y,
                MergeLineList::iterator& mlIt, MergeEditLineList::iterator& melIt )
{
   if (m_selection.firstLine!=-1  &&  m_selection.bSelectionContainsData )
   {
      deleteSelection();
      y = m_cursorYPos;
      calcIteratorFromLineNr( y, mlIt, melIt );
      s = melIt->getString( this );
      x = convertToPosInText( s, m_cursorXPos, m_pOptions->m_tabSize );
      return true;
   }
   return false;
}

void MergeResultWindow::deleteSelection()
{
   if ( m_selection.firstLine==-1 ||  !m_selection.bSelectionContainsData )
   {
      return;
   }
   setModified();

   int line = 0;
   MergeLineList::iterator mlItFirst;
   MergeEditLineList::iterator melItFirst;
   QString firstLineString;

   int firstLine = -1;
   int lastLine = -1;

   MergeLineList::iterator mlIt;
   for(mlIt = m_mergeLineList.begin();mlIt!=m_mergeLineList.end(); ++mlIt)
   {
      MergeLine& ml = *mlIt;
      MergeEditLineList::iterator melIt;
      for( melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt )
      {
         MergeEditLine& mel = *melIt;

         if ( mel.isEditableText()  &&  m_selection.lineWithin(line) )
         {
            if ( firstLine==-1 )
               firstLine = line;
            lastLine = line;
         }

         ++line;
      }
   }

   if ( firstLine == -1 )
   {
      return; // Nothing to delete.
   }

   line = 0;
   for(mlIt = m_mergeLineList.begin();mlIt!=m_mergeLineList.end(); ++mlIt)
   {
      MergeLine& ml = *mlIt;
      MergeEditLineList::iterator melIt, melIt1;
      for( melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); )
      {
         MergeEditLine& mel = *melIt;
         melIt1 = melIt;
         ++melIt1;

         if ( mel.isEditableText()  &&  m_selection.lineWithin(line) )
         {
            QString lineString = mel.getString( this );

            int firstPosInLine = m_selection.firstPosInLine(line);
            int lastPosInLine = m_selection.lastPosInLine(line);

            if ( line==firstLine )
            {
               mlItFirst = mlIt;
               melItFirst = melIt;
               int pos = convertToPosInText( lineString, firstPosInLine, m_pOptions->m_tabSize );
               firstLineString = lineString.left( pos );
            }

            if ( line==lastLine )
            {
               // This is the last line in the selection
               int pos = convertToPosInText( lineString, lastPosInLine, m_pOptions->m_tabSize );
               firstLineString += lineString.mid( pos ); // rest of line
               melItFirst->setString( firstLineString );
            }

            if ( line!=firstLine )
            {
               // Remove the line
               if ( mlIt->mergeEditLineList.size()>1 )
                  mlIt->mergeEditLineList.erase( melIt );
               else
                  melIt->setRemoved();
            }
         }

         ++line;
         melIt = melIt1;
      }
   }

   m_cursorYPos = m_selection.beginLine();
   m_cursorXPos = m_selection.beginPos();
   m_cursorOldXPos = m_cursorXPos;

   m_selection.reset();
}

void MergeResultWindow::pasteClipboard( bool bFromSelection )
{
   if (m_selection.firstLine != -1 )
      deleteSelection();

   setModified();

   int y = m_cursorYPos;
   MergeLineList::iterator mlIt;
   MergeEditLineList::iterator melIt, melItAfter;
   calcIteratorFromLineNr( y, mlIt, melIt );
   melItAfter = melIt;
   ++melItAfter;
   QString str = melIt->getString( this );
   int x = convertToPosInText( str, m_cursorXPos, m_pOptions->m_tabSize );

   if ( !QApplication::clipboard()->supportsSelection() )
      bFromSelection = false;

   QString clipBoard = QApplication::clipboard()->text( bFromSelection ? QClipboard::Selection : QClipboard::Clipboard );

   QString currentLine = str.left(x);
   QString endOfLine = str.mid(x);
   int i;
   int len = clipBoard.length();
   for( i=0; i<len; ++i )
   {
      QChar c = clipBoard[i];
      if ( c == '\r' ) continue;
      if ( c == '\n' )
      {
         melIt->setString( currentLine );
         MergeEditLine mel(mlIt->id3l); // Associate every mel with an id3l, even if not really valid.
         melIt = mlIt->mergeEditLineList.insert( melItAfter, mel );
         currentLine = "";
         x=0;
         ++y;
      }
      else
      {
         currentLine += c;
         ++x;
      }
   }

   currentLine += endOfLine;
   melIt->setString( currentLine );

   m_cursorYPos = y;
   m_cursorXPos = convertToPosOnScreen( currentLine, x, m_pOptions->m_tabSize );
   m_cursorOldXPos = m_cursorXPos;

   update();
}

void MergeResultWindow::resetSelection()
{
   m_selection.reset();
   update();
}

void MergeResultWindow::setModified(bool bModified)
{
   if (bModified != m_bModified)
   {
      m_bModified = bModified;
      emit modifiedChanged(m_bModified);
   }
}

/// Saves and returns true when successful.
bool MergeResultWindow::saveDocument( const QString& fileName, QTextCodec* pEncoding, e_LineEndStyle eLineEndStyle )
{
   // Are still conflicts somewhere?
   if ( getNrOfUnsolvedConflicts()>0 )
   {
      KMessageBox::error( this,
         i18n("Not all conflicts are solved yet.\n"
              "File not saved.\n"),
         i18n("Conflicts Left"));
      return false;
   }

   if ( eLineEndStyle==eLineEndStyleConflict || eLineEndStyle==eLineEndStyleUndefined )
   {
      KMessageBox::error( this,
         i18n("There is a line end style conflict. Please choose the line end style manually.\n"
              "File not saved.\n"),
         i18n("Conflicts Left"));
      return false;
   }

   update();

   FileAccess file( fileName, true /*bWantToWrite*/ );
   if ( m_pOptions->m_bDmCreateBakFiles && file.exists() )
   {
      bool bSuccess = file.createBackup(".orig");
      if ( !bSuccess )
      {
         KMessageBox::error( this, file.getStatusText() + i18n("\n\nCreating backup failed. File not saved."), i18n("File Save Error") );
         return false;
      }
   }

   QByteArray dataArray;
   QTextStream textOutStream(&dataArray, QIODevice::WriteOnly);
   if ( pEncoding->name()=="UTF-8" )
      textOutStream.setGenerateByteOrderMark( false ); // Shouldn't be necessary. Bug in Qt or docs
   else
      textOutStream.setGenerateByteOrderMark( true ); // Only for UTF-16
   textOutStream.setCodec( pEncoding );

   int line = 0;
   MergeLineList::iterator mlIt = m_mergeLineList.begin();
   for(mlIt = m_mergeLineList.begin();mlIt!=m_mergeLineList.end(); ++mlIt)
   {
      MergeLine& ml = *mlIt;
      MergeEditLineList::iterator melIt;
      for( melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt )
      {
         MergeEditLine& mel = *melIt;

         if ( mel.isEditableText() )
         {
            QString str = mel.getString( this );

            if (line>0) // Prepend line feed, but not for first line
            {
               if ( eLineEndStyle == eLineEndStyleDos )
               {   str.prepend("\r\n"); }
               else
               {   str.prepend("\n");   }
            }

            textOutStream << str;
            ++line;
         }
      }
   }
   textOutStream.flush();
   bool bSuccess = file.writeFile( dataArray.data(), dataArray.size() );
   if ( ! bSuccess )
   {
      KMessageBox::error( this, i18n("Error while writing."), i18n("File Save Error") );
      return false;
   }

   setModified( false );
   update();

   return true;
}

QString MergeResultWindow::getString( int lineIdx )
{
   MergeResultWindow::MergeLineList::iterator mlIt;
   MergeResultWindow::MergeEditLineList::iterator melIt;
   calcIteratorFromLineNr( lineIdx, mlIt, melIt );
   QString s = melIt->getString( this );
   return s;
}

bool MergeResultWindow::findString( const QString& s, int& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive )
{
   int it = d3vLine;
   int endIt = bDirDown ? getNofLines() : -1;
   int step =  bDirDown ? 1 : -1;
   int startPos = posInLine;

   for( ; it!=endIt; it+=step )
   {
      QString line = getString( it );
      if ( !line.isEmpty() )
      {
         int pos = line.indexOf( s, startPos, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive );
         if ( pos != -1 )
         {
            d3vLine = it;
            posInLine = pos;
            return true;
         }

         startPos = 0;
      }
   }
   return false;
}

void MergeResultWindow::setSelection( int firstLine, int startPos, int lastLine, int endPos )
{
   if ( lastLine >= getNofLines() )
   {
      lastLine = getNofLines()-1;
      QString s = getString( lastLine );
      endPos = s.length();
   }
   m_selection.reset();
   m_selection.start( firstLine, convertToPosOnScreen( getString(firstLine), startPos, m_pOptions->m_tabSize ) );
   m_selection.end( lastLine, convertToPosOnScreen( getString(lastLine), endPos, m_pOptions->m_tabSize ) );
   update();
}

Overview::Overview( Options* pOptions )
//: QWidget( pParent, 0, Qt::WNoAutoErase )
{
   m_pDiff3LineList = 0;
   m_pOptions = pOptions;
   m_bTripleDiff = false;
   m_eOverviewMode = eOMNormal;
   m_nofLines = 1;
   m_bPaintingAllowed = false;
   setFixedWidth(20);
}

void Overview::init( Diff3LineList* pDiff3LineList, bool bTripleDiff )
{
   m_pDiff3LineList = pDiff3LineList;
   m_bTripleDiff = bTripleDiff;
   m_pixmap = QPixmap( QSize(0,0) );   // make sure that a redraw happens
   update();
}

void Overview::reset()
{
   m_pDiff3LineList = 0;
}

void Overview::slotRedraw()
{
   m_pixmap = QPixmap( QSize(0,0) );   // make sure that a redraw happens
   update();
}

void Overview::setRange( int firstLine, int pageHeight )
{
   m_firstLine = firstLine;
   m_pageHeight = pageHeight;
   update();
}
void Overview::setFirstLine( int firstLine )
{
   m_firstLine = firstLine;
   update();
}

void Overview::setOverviewMode( e_OverviewMode eOverviewMode )
{
   m_eOverviewMode = eOverviewMode;
   slotRedraw();
}

Overview::e_OverviewMode Overview::getOverviewMode()
{
   return m_eOverviewMode;
}

void Overview::mousePressEvent( QMouseEvent* e )
{
   int h = height()-1;
   int h1 = h * m_pageHeight / max2(1,m_nofLines)+3;
   if ( h>0 )
      emit setLine( ( e->y() - h1/2 )*m_nofLines/h );
}

void Overview::mouseMoveEvent( QMouseEvent* e )
{
   mousePressEvent(e);
}

void Overview::setPaintingAllowed( bool bAllowPainting )
{
   if (m_bPaintingAllowed != bAllowPainting)
   {
      m_bPaintingAllowed = bAllowPainting;
      if ( m_bPaintingAllowed ) update();
      else reset();
   }
}

void Overview::drawColumn( QPainter& p, e_OverviewMode eOverviewMode, int x, int w, int h, int nofLines )
{
   p.setPen(Qt::black);
   p.drawLine( x, 0, x, h );

   if (nofLines==0) return;
   
   int line = 0;
   int oldY = 0;
   int oldConflictY = -1;
   int wrapLineIdx=0;
   Diff3LineList::const_iterator i;
   for( i = m_pDiff3LineList->begin(); i!= m_pDiff3LineList->end();  )
   {
      const Diff3Line& d3l = *i;
      int y = h * (line+1) / nofLines;
      e_MergeDetails md;
      bool bConflict;
      bool bLineRemoved;
      int src;
      mergeOneLine( d3l, md, bConflict, bLineRemoved, src, !m_bTripleDiff );

      QColor c = m_pOptions->m_bgColor;
      bool bWhiteSpaceChange = false;
      //if( bConflict )  c=m_pOptions->m_colorForConflict;
      //else
      if ( eOverviewMode==eOMNormal )
      {
         switch( md )
         {
         case eDefault:
         case eNoChange:
                        c = m_pOptions->m_bgColor;
                        break;

         case eBAdded:
         case eBDeleted:
         case eBChanged:
                        c = bConflict ? m_pOptions->m_colorForConflict : m_pOptions->m_colorB;
                        bWhiteSpaceChange = d3l.bAEqB || (d3l.bWhiteLineA && d3l.bWhiteLineB);
                        break;

         case eCAdded:
         case eCDeleted:
         case eCChanged:
                        bWhiteSpaceChange = d3l.bAEqC || (d3l.bWhiteLineA && d3l.bWhiteLineC);
                        c = bConflict ? m_pOptions->m_colorForConflict : m_pOptions->m_colorC;
                        break;

         case eBCChanged:         // conflict
         case eBCChangedAndEqual: // possible conflict
         case eBCDeleted:         // possible conflict
         case eBChanged_CDeleted: // conflict
         case eCChanged_BDeleted: // conflict
         case eBCAdded:           // conflict
         case eBCAddedAndEqual:   // possible conflict
                     c=m_pOptions->m_colorForConflict;
                     break;
         default: assert(false); break;
         }
      }
      else if ( eOverviewMode==eOMAvsB )
      {
         switch( md )
         {
         case eDefault:
         case eNoChange:
         case eCAdded:
         case eCDeleted:
         case eCChanged:  break;
         default:         c = m_pOptions->m_colorForConflict;
                          bWhiteSpaceChange = d3l.bAEqB || (d3l.bWhiteLineA && d3l.bWhiteLineB);
                          break;
         }
      }
      else if ( eOverviewMode==eOMAvsC )
      {
         switch( md )
         {
         case eDefault:
         case eNoChange:
         case eBAdded:
         case eBDeleted:
         case eBChanged:  break;
         default:         c = m_pOptions->m_colorForConflict;
                          bWhiteSpaceChange = d3l.bAEqC || (d3l.bWhiteLineA && d3l.bWhiteLineC);
                          break;
         }
      }
      else if ( eOverviewMode==eOMBvsC )
      {
         switch( md )
         {
         case eDefault:
         case eNoChange:
         case eBCChangedAndEqual:
         case eBCDeleted:      
         case eBCAddedAndEqual:   break;
         default:                 c=m_pOptions->m_colorForConflict;
                                  bWhiteSpaceChange = d3l.bBEqC || (d3l.bWhiteLineB && d3l.bWhiteLineC);
                                  break;
         }
      }
      
      int x2 = x;
      int w2 = w;
      
      if ( ! m_bTripleDiff )
      {
         if ( d3l.lineA == -1 && d3l.lineB>=0 )
         {
            c = m_pOptions->m_colorA;
            x2 = w/2;
            w2 = x2;
         }
         if ( d3l.lineA >= 0 && d3l.lineB==-1 )
         {
            c = m_pOptions->m_colorB;
            w2 = w/2;
         }
      }

      if (!bWhiteSpaceChange || m_pOptions->m_bShowWhiteSpace )
      {
         // Make sure that lines with conflict are not overwritten.
         if (  c == m_pOptions->m_colorForConflict )
         {
            p.fillRect(x2+1, oldY, w2, max2(1,y-oldY), bWhiteSpaceChange ? QBrush(c,Qt::Dense4Pattern) : QBrush(c) );
            oldConflictY = oldY;
         }
         else if ( c!=m_pOptions->m_bgColor  &&  oldY>oldConflictY )
         {
            p.fillRect(x2+1, oldY, w2, max2(1,y-oldY), bWhiteSpaceChange ? QBrush(c,Qt::Dense4Pattern) : QBrush(c) );
         }
      }

      oldY = y;

      ++line;
      if ( m_pOptions->m_bWordWrap )
      {
         ++wrapLineIdx;
         if(wrapLineIdx>=d3l.linesNeededForDisplay)
         {
            wrapLineIdx=0;
            ++i;
         }
      }
      else
      {
         ++i;
      }
   }
}

void Overview::paintEvent( QPaintEvent* )
{
   if (m_pDiff3LineList==0 || !m_bPaintingAllowed ) return;
   int h = height()-1;
   int w = width();
   

   if ( m_pixmap.size() != size() )
   {
      if ( m_pOptions->m_bWordWrap )
      {
         m_nofLines = 0;
         Diff3LineList::const_iterator i;
         for( i = m_pDiff3LineList->begin(); i!= m_pDiff3LineList->end(); ++i )
         {
            m_nofLines += i->linesNeededForDisplay;
         }
      }
      else
      {
         m_nofLines = m_pDiff3LineList->size();
      }
   
      m_pixmap = QPixmap( size() );

      QPainter p(&m_pixmap);
      p.fillRect( rect(), m_pOptions->m_bgColor );

      if ( !m_bTripleDiff || m_eOverviewMode == eOMNormal )
      {
         drawColumn( p, eOMNormal, 0, w, h, m_nofLines );
      }
      else
      {
         drawColumn( p, eOMNormal, 0, w/2, h, m_nofLines );
         drawColumn( p, m_eOverviewMode, w/2, w/2, h, m_nofLines );
      }
   }

   QPainter painter( this );
   painter.drawPixmap( 0,0, m_pixmap );

   int y1 = h * m_firstLine / m_nofLines-1;
   int h1 = h * m_pageHeight / m_nofLines+3;
   painter.setPen(Qt::black);
   painter.drawRect( 1, y1, w-1, h1 );
}

WindowTitleWidget::WindowTitleWidget(Options* pOptions)
{
   m_pOptions = pOptions;
   setAutoFillBackground(true);

   QHBoxLayout* pHLayout = new QHBoxLayout(this);
   pHLayout->setMargin(2);
   pHLayout->setSpacing(2);

   m_pLabel = new QLabel(i18n("Output")+":");
   pHLayout->addWidget( m_pLabel );

   m_pFileNameLineEdit = new QLineEdit();
   pHLayout->addWidget( m_pFileNameLineEdit, 6 );
   m_pFileNameLineEdit->installEventFilter( this );
   m_pFileNameLineEdit->setReadOnly( true );

   //m_pBrowseButton = new QPushButton("...");
   //pHLayout->addWidget( m_pBrowseButton, 0 );
   //connect( m_pBrowseButton, SIGNAL(clicked()), this, SLOT(slotBrowseButtonClicked()));

   m_pModifiedLabel = new QLabel(i18n("[Modified]"));
   pHLayout->addWidget( m_pModifiedLabel );
   m_pModifiedLabel->setMinimumSize( m_pModifiedLabel->sizeHint() );
   m_pModifiedLabel->setText("");

   pHLayout->addStretch(1);

   m_pEncodingLabel = new QLabel(i18n("Encoding for saving")+":");
   pHLayout->addWidget( m_pEncodingLabel );

   m_pEncodingSelector = new QComboBox();
   m_pEncodingSelector->setSizeAdjustPolicy( QComboBox::AdjustToContents );
   pHLayout->addWidget( m_pEncodingSelector, 2 );
   setEncodings(0,0,0);

   m_pLineEndStyleLabel = new QLabel( i18n("Line end style:") );
   pHLayout->addWidget( m_pLineEndStyleLabel );
   m_pLineEndStyleSelector = new QComboBox();
   m_pLineEndStyleSelector->setSizeAdjustPolicy( QComboBox::AdjustToContents );
   pHLayout->addWidget( m_pLineEndStyleSelector );
   setLineEndStyles(eLineEndStyleUndefined,eLineEndStyleUndefined,eLineEndStyleUndefined);
}

void WindowTitleWidget::setFileName( const QString& fileName )
{
   m_pFileNameLineEdit->setText( QDir::toNativeSeparators(fileName) );
}

QString WindowTitleWidget::getFileName()
{
   return m_pFileNameLineEdit->text();
}

//static QString getLineEndStyleName( e_LineEndStyle eLineEndStyle )
//{
//   if ( eLineEndStyle == eLineEndStyleDos )
//      return "DOS";
//   else if ( eLineEndStyle == eLineEndStyleUnix )
//      return "Unix";
//   return QString();
//}

void WindowTitleWidget::setLineEndStyles( e_LineEndStyle eLineEndStyleA, e_LineEndStyle eLineEndStyleB, e_LineEndStyle eLineEndStyleC)
{
   m_pLineEndStyleSelector->clear();
   QString dosUsers;
   if ( eLineEndStyleA == eLineEndStyleDos )
      dosUsers += "A";
   if ( eLineEndStyleB == eLineEndStyleDos )
      dosUsers += (dosUsers.isEmpty() ? "" : ", ") + QString("B");
   if ( eLineEndStyleC == eLineEndStyleDos )
      dosUsers += (dosUsers.isEmpty() ? "" : ", ") + QString("C");
   QString unxUsers;
   if ( eLineEndStyleA == eLineEndStyleUnix )
      unxUsers += "A";
   if ( eLineEndStyleB == eLineEndStyleUnix )
      unxUsers += (unxUsers.isEmpty() ? "" : ", ") + QString("B");
   if ( eLineEndStyleC == eLineEndStyleUnix )
      unxUsers += (unxUsers.isEmpty() ? "" : ", ") + QString("C");

   m_pLineEndStyleSelector->addItem( i18n("Unix") + (unxUsers.isEmpty() ? QString("") : " (" + unxUsers + ")" )  );
   m_pLineEndStyleSelector->addItem( i18n("DOS")  + (dosUsers.isEmpty() ? QString("") : " (" + dosUsers + ")" )  );

   e_LineEndStyle autoChoice = (e_LineEndStyle)m_pOptions->m_lineEndStyle;

   if ( m_pOptions->m_lineEndStyle == eLineEndStyleAutoDetect )
   {
      if ( eLineEndStyleA != eLineEndStyleUndefined && eLineEndStyleB != eLineEndStyleUndefined && eLineEndStyleC != eLineEndStyleUndefined )
      {
         if ( eLineEndStyleA == eLineEndStyleB )
            autoChoice = eLineEndStyleC;
         else if ( eLineEndStyleA == eLineEndStyleC )
            autoChoice = eLineEndStyleB;
         else
            autoChoice = eLineEndStyleConflict;          //conflict (not likely while only two values exist)
      }
      else 
      {
         e_LineEndStyle c1, c2;
         if     ( eLineEndStyleA == eLineEndStyleUndefined ) { c1 = eLineEndStyleB; c2 = eLineEndStyleC; }
         else if( eLineEndStyleB == eLineEndStyleUndefined ) { c1 = eLineEndStyleA; c2 = eLineEndStyleC; }
         else /*if( eLineEndStyleC == eLineEndStyleUndefined )*/ { c1 = eLineEndStyleA; c2 = eLineEndStyleB; }
         if ( c1 == c2 && c1!=eLineEndStyleUndefined )
            autoChoice = c1;
         else
            autoChoice = eLineEndStyleConflict;
      }
   }

   if ( autoChoice == eLineEndStyleUnix )
      m_pLineEndStyleSelector->setCurrentIndex(0);
   else if ( autoChoice == eLineEndStyleDos )
      m_pLineEndStyleSelector->setCurrentIndex(1);
   else if ( autoChoice == eLineEndStyleConflict )
   {
      m_pLineEndStyleSelector->addItem( i18n("Conflict") );
      m_pLineEndStyleSelector->setCurrentIndex(2);
   }
}

e_LineEndStyle WindowTitleWidget::getLineEndStyle( )
{
   
   int current = m_pLineEndStyleSelector->currentIndex();
   if (current == 0)
      return eLineEndStyleUnix;
   else if (current == 1)
      return eLineEndStyleDos;
   else
      return eLineEndStyleConflict;
}

void WindowTitleWidget::setEncodings( QTextCodec* pCodecForA, QTextCodec* pCodecForB, QTextCodec* pCodecForC )
{
   m_pEncodingSelector->clear();

   // First sort codec names:
   std::map<QString, QTextCodec*> names;
   QList<int> mibs = QTextCodec::availableMibs();
   foreach(int i, mibs)
   {
      QTextCodec* c = QTextCodec::codecForMib(i);
      if ( c!=0 )
         names[QString(c->name())]=c;
   }

   if ( pCodecForA )
      m_pEncodingSelector->addItem( i18n("Codec from") + " A: " + pCodecForA->name(), QVariant::fromValue((void*)pCodecForA) );
   if ( pCodecForB )
      m_pEncodingSelector->addItem( i18n("Codec from") + " B: " + pCodecForB->name(), QVariant::fromValue((void*)pCodecForB) );
   if ( pCodecForC )
      m_pEncodingSelector->addItem( i18n("Codec from") + " C: " + pCodecForC->name(), QVariant::fromValue((void*)pCodecForC) );

   std::map<QString, QTextCodec*>::iterator it;
   for(it=names.begin();it!=names.end();++it)
   {
      m_pEncodingSelector->addItem( it->first, QVariant::fromValue((void*)it->second) );
   }
   m_pEncodingSelector->setMinimumSize( m_pEncodingSelector->sizeHint() );
   
   if ( pCodecForC && pCodecForB && pCodecForA )
   {
      if ( pCodecForA == pCodecForB )
         m_pEncodingSelector->setCurrentIndex( 2 ); // C
      else if ( pCodecForA == pCodecForC )
         m_pEncodingSelector->setCurrentIndex( 1 ); // B
      else
         m_pEncodingSelector->setCurrentIndex( 2 ); // C
   }
   else if ( pCodecForA && pCodecForB )
      m_pEncodingSelector->setCurrentIndex( 1 ); // B
   else
      m_pEncodingSelector->setCurrentIndex( 0 );
}

QTextCodec* WindowTitleWidget::getEncoding()
{
   return (QTextCodec*)m_pEncodingSelector->itemData( m_pEncodingSelector->currentIndex() ).value<void*>();
}

void WindowTitleWidget::setEncoding(QTextCodec* pEncoding)
{
   int idx = m_pEncodingSelector->findText( pEncoding->name() );
   if (idx>=0)
      m_pEncodingSelector->setCurrentIndex( idx );
}

//void WindowTitleWidget::slotBrowseButtonClicked()
//{
//   QString current = m_pFileNameLineEdit->text();
//
//   KUrl newURL = KFileDialog::getSaveUrl( current, 0, this, i18n("Select file (not saving yet)"));
//   if ( !newURL.isEmpty() )
//   {
//      m_pFileNameLineEdit->setText( newURL.url() ); 
//   }
//}

void WindowTitleWidget::slotSetModified( bool bModified )
{
   m_pModifiedLabel->setText( bModified ? i18n("[Modified]") : "" );
}

bool WindowTitleWidget::eventFilter( QObject* o, QEvent* e )
{
   if ( e->type()==QEvent::FocusIn || e->type()==QEvent::FocusOut )
   {
      QPalette p = m_pLabel->palette();

      QColor c1 = m_pOptions->m_fgColor;
      QColor c2 = Qt::lightGray;
      if ( e->type()==QEvent::FocusOut )
         c2 = m_pOptions->m_bgColor;

      p.setColor(QPalette::Window, c2);
      setPalette( p );

      p.setColor(QPalette::WindowText, c1);
      m_pLabel->setPalette( p );
      m_pEncodingLabel->setPalette( p );
      m_pEncodingSelector->setPalette( p );
   }
   if (o == m_pFileNameLineEdit && e->type()==QEvent::Drop)
   {
      QDropEvent* d = static_cast<QDropEvent*>(e);
      
      if ( d->mimeData()->hasUrls() ) 
      {
         QList<QUrl> lst = d->mimeData()->urls();

         if ( lst.count() > 0 )
         {
            static_cast<QLineEdit*>(o)->setText( lst[0].toString() );
            static_cast<QLineEdit*>(o)->setFocus();            
            return true;
         }
      }
   }
   return false;
}

//#include "mergeresultwindow.moc"
