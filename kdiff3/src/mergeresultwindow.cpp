/***************************************************************************
                          mergeresultwindow.cpp  -  description
                             -------------------
    begin                : Sun Apr 14 2002
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
#include <stdio.h>
#include <qpainter.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qdir.h>
#include <qfile.h>
#include <qcursor.h>
#include <qpopupmenu.h>
#include <optiondialog.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <iostream>
#include <qstatusbar.h>

int g_bAutoSolve = true;

#undef leftInfoWidth
#define leftInfoWidth 3

MergeResultWindow::MergeResultWindow(
   QWidget* pParent,
   OptionDialog* pOptionDialog,
   QStatusBar* pStatusBar
   )
: QWidget( pParent, 0, WRepaintNoErase )
{
   setFocusPolicy( QWidget::ClickFocus );

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

   m_fileName = "";
   m_pldA = 0;
   m_pldB = 0;
   m_pldC = 0;

   m_pDiff3LineList = 0;
   m_pTotalDiffStatus = 0;
   m_pStatusBar = pStatusBar;

   m_pOptionDialog = pOptionDialog;
   m_bPaintingAllowed = false;

   m_cursorXPos=0;
   m_cursorOldXPos=0;
   m_cursorYPos=0;
   m_bCursorOn = true;
   connect( &m_cursorTimer, SIGNAL(timeout()), this, SLOT( slotCursorUpdate() ) );
   m_cursorTimer.start( 500 /*ms*/, true /*single shot*/ );
   m_selection.reset();

   setMinimumSize( QSize(20,20) );
   setFont( m_pOptionDialog->m_font );
}

void MergeResultWindow::init(
   const LineData* pLineDataA,
   const LineData* pLineDataB,
   const LineData* pLineDataC,
   const Diff3LineList* pDiff3LineList,
   TotalDiffStatus* pTotalDiffStatus,
   QString fileName
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
   m_bModified = false;
   
   m_fileName = fileName;
   m_pldA = pLineDataA;
   m_pldB = pLineDataB;
   m_pldC = pLineDataC;

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
         !ml1.bConflict && !ml2.bConflict && ml1.bDelta && ml2.bDelta && ml1.srcSelect == ml2.srcSelect ||
         !ml1.bDelta && !ml2.bDelta
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
            i18n("Warning"), i18n("C&ontinue"), i18n("&Cancel"));
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
              ( m_pldC==0 && (d.bAEqB || d.bWhiteLineA && d.bWhiteLineB)  ||
                m_pldC!=0 && (d.bAEqB && d.bAEqC || d.bWhiteLineA && d.bWhiteLineB && d.bWhiteLineC ) ) )
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
            if (back!=0  &&  back->bWhiteSpaceConflict )
            {
               if ( m_pldC==0 && m_pOptionDialog->m_whiteSpace2FileMergeDefault != 0 )  // Only two inputs
               {
                  back->srcSelect = m_pOptionDialog->m_whiteSpace2FileMergeDefault;
                  back->bConflict = false;
               }
               else if ( m_pldC!=0 && m_pOptionDialog->m_whiteSpace3FileMergeDefault != 0 )
               {
                  back->srcSelect = m_pOptionDialog->m_whiteSpace3FileMergeDefault;
                  back->bConflict = false;
               }
            }
            ml.mergeEditLineList.setTotalSizePtr(&m_totalSize);
            m_mergeLineList.push_back( ml );
         }

         if ( ! ml.bConflict )
         {
            MergeLine& tmpBack = m_mergeLineList.back();
            MergeEditLine mel;
            mel.setSource( ml.srcSelect, ml.id3l, bLineRemoved );
            tmpBack.mergeEditLineList.push_back(mel);
         }
         else if ( back==0  || ! back->bConflict || !bSame )
         {
            MergeLine& tmpBack = m_mergeLineList.back();
            MergeEditLine mel;
            mel.setConflict();
            tmpBack.mergeEditLineList.push_back(mel);
         }
      }
   }

   if ( !bAutoSolve )
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
               MergeEditLine mel;
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
                  MergeEditLine mel;
                  mel.setSource( defaultSelector, d3llit, false );

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
                  MergeEditLine mel;
                  mel.setSource( defaultSelector, ml.id3l, false );
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

         int srcLine = melsrc==1 ? mel.id3l()->lineA :
                       melsrc==2 ? mel.id3l()->lineB :
                       melsrc==3 ? mel.id3l()->lineC : -1;

         if ( srcLine == -1 && oldSrcLine==-1 && oldSrc == melsrc )
            melIt = ml.mergeEditLineList.erase( melIt );
         else
            ++melIt;

         oldSrcLine = srcLine;
         oldSrc = melsrc;
      }
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

   m_bModified = false;

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

// Go to prev/next delta/conflict or first/last delta.
void MergeResultWindow::go( e_Direction eDir, e_EndPoint eEndPoint )
{
   assert( eDir==eUp || eDir==eDown );
   MergeLineList::iterator i = m_currentMergeLineIt;
   bool bSkipWhiteConflicts = ! m_pOptionDialog->m_bShowWhiteSpace;
   if( eEndPoint==eEnd )
   {
      if (eDir==eUp) i = m_mergeLineList.begin();     // first mergeline
      else           i = --m_mergeLineList.end();     // last mergeline

      while ( i!=m_mergeLineList.end() && ! i->bDelta )
      {
         if ( eDir==eUp )  ++i;                       // search downwards
         else              --i;                       // search upwards
      }
   }
   else if ( eEndPoint == eDelta  &&  i!=m_mergeLineList.end())
   {
      do
      {
         if ( eDir==eUp )  --i;
         else              ++i;
      }
      while ( i!=m_mergeLineList.end() && ( i->bDelta == false || bSkipWhiteConflicts && i->bWhiteSpaceConflict ) );
   }
   else if ( eEndPoint == eConflict  &&  i!=m_mergeLineList.end() )
   {
      do
      {
         if ( eDir==eUp )  --i;
         else              ++i;
      }
      while ( i!=m_mergeLineList.end() && (i->bConflict == false || bSkipWhiteConflicts && i->bWhiteSpaceConflict ) );
   }
   else if ( i!=m_mergeLineList.end()  &&  eEndPoint == eUnsolvedConflict )
   {
      do
      {
         if ( eDir==eUp )  --i;
         else              ++i;
      }
      while ( i!=m_mergeLineList.end() && ! i->mergeEditLineList.begin()->isConflict() );
   }

   if ( isVisible() )
      setFocus();
   
   setFastSelector( i );
}

bool MergeResultWindow::isDeltaAboveCurrent()
{
   bool bSkipWhiteConflicts = ! m_pOptionDialog->m_bShowWhiteSpace;
   if (m_mergeLineList.empty()) return false;
   MergeLineList::iterator i = m_currentMergeLineIt;
   if (i == m_mergeLineList.begin()) return false;
   do
   {
      --i;
      if ( i->bDelta && !( bSkipWhiteConflicts && i->bWhiteSpaceConflict ) ) return true;
   } 
   while (i!=m_mergeLineList.begin());
   
   return false;
}

bool MergeResultWindow::isDeltaBelowCurrent()
{
   bool bSkipWhiteConflicts = ! m_pOptionDialog->m_bShowWhiteSpace;
   if (m_mergeLineList.empty()) return false;
   
   MergeLineList::iterator i = m_currentMergeLineIt;
   if (i!=m_mergeLineList.end())
   {
      ++i;
      for( ; i!=m_mergeLineList.end(); ++i )
      {
         if ( i->bDelta && !( bSkipWhiteConflicts && i->bWhiteSpaceConflict ) ) return true;
      }
   }
   return false;
}

bool MergeResultWindow::isConflictAboveCurrent()
{
   if (m_mergeLineList.empty()) return false;
   MergeLineList::iterator i = m_currentMergeLineIt;
   if (i == m_mergeLineList.begin()) return false;
   
   do
   {
      --i;
      if ( i->bConflict ) return true;
   } 
   while (i!=m_mergeLineList.begin());
   
   return false;
}

bool MergeResultWindow::isConflictBelowCurrent()
{
   MergeLineList::iterator i = m_currentMergeLineIt;
   if (m_mergeLineList.empty()) return false;
   
   if (i!=m_mergeLineList.end())
   {
      ++i;
      for( ; i!=m_mergeLineList.end(); ++i )
      {
         if ( i->bConflict ) return true;
      }
   }
   return false;   
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
         if ( i->bDelta )
         {
            setFastSelector( i );
         }
         break;
      }
   }
}

int MergeResultWindow::getNrOfUnsolvedConflicts()
{
   int nrOfUnsolvedConflicts = 0;

   MergeLineList::iterator mlIt = m_mergeLineList.begin();
   for(mlIt = m_mergeLineList.begin();mlIt!=m_mergeLineList.end(); ++mlIt)
   {
      MergeLine& ml = *mlIt;
      MergeEditLineList::iterator melIt = ml.mergeEditLineList.begin();
      if ( melIt->isConflict() )
         ++nrOfUnsolvedConflicts;
   }

   return nrOfUnsolvedConflicts;
}

void MergeResultWindow::showNrOfConflicts()
{
   int nrOfSolvedConflicts = 0;
   int nrOfUnsolvedConflicts = 0;

   MergeLineList::iterator i;
   for ( i = m_mergeLineList.begin();  i!=m_mergeLineList.end(); ++i )
   {
      if ( i->bConflict )
         ++nrOfUnsolvedConflicts;
      else if ( i->bDelta )
         ++nrOfSolvedConflicts;
   }
   QString totalInfo;
   if ( m_pTotalDiffStatus->bBinaryAEqB && m_pTotalDiffStatus->bBinaryAEqC )
      totalInfo += i18n("All input files are binary equal.");
   else  if ( m_pTotalDiffStatus->bTextAEqB && m_pTotalDiffStatus->bTextAEqC )
      totalInfo += i18n("All input files contain the same text.");
   else {
      if    ( m_pTotalDiffStatus->bBinaryAEqB ) totalInfo += i18n("Files A and B are binary equal.\n");
      else if ( m_pTotalDiffStatus->bTextAEqB ) totalInfo += i18n("Files A and B have equal text. \n");
      if    ( m_pTotalDiffStatus->bBinaryAEqC ) totalInfo += i18n("Files A and C are binary equal.\n");
      else if ( m_pTotalDiffStatus->bTextAEqC ) totalInfo += i18n("Files A and C have equal text. \n");
      if    ( m_pTotalDiffStatus->bBinaryBEqC ) totalInfo += i18n("Files B and C are binary equal.\n");
      else if ( m_pTotalDiffStatus->bTextBEqC ) totalInfo += i18n("Files B and C have equal text. \n");
   }
   KMessageBox::information( this,
      i18n("Total number of conflicts: ") + QString::number(nrOfSolvedConflicts + nrOfUnsolvedConflicts) +
      i18n("\nNr of automatically solved conflicts: ") + QString::number(nrOfSolvedConflicts) +
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
         MergeEditLine mel;
         mel.setSource( selector, d3llit, false );
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
      MergeEditLine mel;

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
   m_pStatusBar->message( i18n("Number of remaining unsolved conflicts: %1").arg(getNrOfUnsolvedConflicts()) );
}

// bConflictsOnly: automatically choose for conflicts only (true) or for everywhere (false)
void MergeResultWindow::chooseGlobal(int selector, bool bConflictsOnly, bool bWhiteSpaceOnly )
{
   resetSelection();

   merge( false, selector, bConflictsOnly, bWhiteSpaceOnly );
   emit modified();
   update();
   m_pStatusBar->message( i18n("Number of remaining unsolved conflicts: %1").arg(getNrOfUnsolvedConflicts()) );
}

void MergeResultWindow::slotAutoSolve()
{
   resetSelection();
   merge( true, -1 );
   emit modified();
   update();
   m_pStatusBar->message( i18n("Number of remaining unsolved conflicts: %1").arg(getNrOfUnsolvedConflicts()) );
}

void MergeResultWindow::slotUnsolve()
{
   resetSelection();
   merge( false, -1 );
   emit modified();
   update();
   m_pStatusBar->message( i18n("Number of remaining unsolved conflicts: %1").arg(getNrOfUnsolvedConflicts()) );
}

void MergeResultWindow::myUpdate(int afterMilliSecs)
{
   killTimers();
   m_bMyUpdate = true;
   startTimer( afterMilliSecs );
}

void MergeResultWindow::timerEvent(QTimerEvent*)
{
   killTimers();

   if ( m_bMyUpdate )
   {
      update();//paintEvent( 0 );
      m_bMyUpdate = false;
   }

   if ( m_scrollDeltaX != 0 || m_scrollDeltaY != 0 )
   {
      m_selection.end( m_selection.lastLine + m_scrollDeltaY, m_selection.lastPos +  m_scrollDeltaX );
      emit scroll( m_scrollDeltaX, m_scrollDeltaY );
      killTimers();
      startTimer(50);
   }
}

const char* MergeResultWindow::MergeEditLine::getString( const MergeResultWindow* mrw, int& size )
{
   size=-1;
   if ( isRemoved() )   { size=0; return ""; }

   if ( ! isModified() )
   {
      int src = m_src;
      const Diff3Line& d3l = *m_id3l;
      if ( src == 0 )   { size=0; return ""; }

      const LineData* pld = 0;
      assert( src == A || src == B || src == C );
      if      ( src == A && d3l.lineA!=-1 ) pld = &mrw->m_pldA[ d3l.lineA ];
      else if ( src == B && d3l.lineB!=-1 ) pld = &mrw->m_pldB[ d3l.lineB ];
      else if ( src == C && d3l.lineC!=-1 ) pld = &mrw->m_pldC[ d3l.lineC ];

      if ( pld == 0 )
      {
         // assert(false);      This is no error.
         size = 0;
         return "";
      }

      size = pld->size;
      return pld->pLine;
   }
   else
   {
      size = m_str.length();
      return m_str;
   }
   return 0;
}

/// Converts the cursor-posOnScreen into a text index, considering tabulators.
int convertToPosInText( const char* p, int size, int posOnScreen )
{
   int localPosOnScreen = 0;
   for ( int i=0; i<size; ++i )
   {
      if ( localPosOnScreen>=posOnScreen )
         return i;

      // All letters except tabulator have width one.
      int letterWidth = p[i]!='\t' ? 1 : tabber( localPosOnScreen, g_tabSize );

      localPosOnScreen += letterWidth;

      if ( localPosOnScreen>posOnScreen )
         return i;
   }
   return size;
}


/// Converts the index into the text to a cursor-posOnScreen considering tabulators.
int convertToPosOnScreen( const QString& p, int posInText )
{
   int posOnScreen = 0;
   for ( int i=0; i<posInText; ++i )
   {
      // All letters except tabulator have width one.
      int letterWidth = p[i]!='\t' ? 1 : tabber( posOnScreen, g_tabSize );

      posOnScreen += letterWidth;
   }
   return posOnScreen;
}

void MergeResultWindow::writeLine(
   QPainter& p, int line, const char* pStr, int size,
   int srcSelect, e_MergeDetails mergeDetails, int rangeMark, bool bUserModified, bool bLineRemoved
   )
{
   const QFontMetrics& fm = fontMetrics();
   int fontHeight = fm.height();
   int fontWidth = fm.width("W");
   int fontAscent = fm.ascent();

   int topLineYOffset = fontHeight + 3;
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
      p.fillRect( xOffset, yOffset, width(), fontHeight, m_pOptionDialog->m_currentRangeBgColor );
   }
   
   if( (srcSelect > 0 || bUserModified ) && !bLineRemoved )
   {
      int outPos = 0;
      QCString s;
      for ( int i=0; i<size; ++i )
      {
         int spaces = 1;
         if ( pStr[i]=='\t' )
         {
            spaces = tabber( outPos, g_tabSize );
            for( int j=0; j<spaces; ++j )
               s+=' ';
         }
         else
         {
            s+=pStr[i];
         }
         outPos += spaces;
      }

      if ( m_selection.lineWithin( line ) )
      {
         int firstPosInLine = convertToPosOnScreen( pStr, convertToPosInText( pStr, size, m_selection.firstPosInLine(line) ) );
         int lastPosInLine  = convertToPosOnScreen( pStr, convertToPosInText( pStr, size, m_selection.lastPosInLine(line) ) );
         int lengthInLine = max2(0,lastPosInLine - firstPosInLine);
         if (lengthInLine>0) m_selection.bSelectionContainsData = true;

         if ( lengthInLine < int(s.length()) )
         {                                // Draw a normal line first
            p.setPen( m_pOptionDialog->m_fgColor );
            p.drawText( xOffset, yOffset+fontAscent,  decodeString( s.mid(m_firstColumn),m_pOptionDialog) );
         }
         int firstPosInLine2 = max2( firstPosInLine, m_firstColumn );
         int lengthInLine2 = max2(0,lastPosInLine - firstPosInLine2);

         if( m_selection.lineWithin( line+1 ) )
            p.fillRect( xOffset + fontWidth*(firstPosInLine2-m_firstColumn), yOffset,
               width(), fontHeight, colorGroup().highlight() );
         else
            p.fillRect( xOffset + fontWidth*(firstPosInLine2-m_firstColumn), yOffset,
               fontWidth*lengthInLine2, fontHeight, colorGroup().highlight() );

         p.setPen( colorGroup().highlightedText() );
         p.drawText( xOffset + fontWidth*(firstPosInLine2-m_firstColumn), yOffset+fontAscent,
            decodeString( s.mid(firstPosInLine2,lengthInLine2), m_pOptionDialog ) );
      }
      else
      {
         p.setPen( m_pOptionDialog->m_fgColor );
         p.drawText( xOffset, yOffset+fontAscent, decodeString( s.mid(m_firstColumn), m_pOptionDialog ) );
      }

      p.setPen( m_pOptionDialog->m_fgColor );
      if ( m_cursorYPos==line )
      {
         m_cursorXPos = minMaxLimiter( m_cursorXPos, 0, outPos );
         m_cursorXPos = convertToPosOnScreen( pStr, convertToPosInText( pStr, size, m_cursorXPos ) );
      }

      p.drawText( 1, yOffset+fontAscent, srcName );
   }
   else if ( bLineRemoved )
   {
      p.setPen( m_pOptionDialog->m_colorForConflict );
      p.drawText( xOffset, yOffset+fontAscent, i18n("<No src line>") );
      p.drawText( 1, yOffset+fontAscent, srcName );
      if ( m_cursorYPos==line ) m_cursorXPos = 0;
   }
   else if ( srcSelect == 0 )
   {
      p.setPen( m_pOptionDialog->m_colorForConflict );
      p.drawText( xOffset, yOffset+fontAscent, i18n("<Merge Conflict>") );
      p.drawText( 1, yOffset+fontAscent, "?" );
      if ( m_cursorYPos==line ) m_cursorXPos = 0;
   }
   else assert(false);

   xOffset -= fontWidth;
   p.setPen( m_pOptionDialog->m_fgColor );
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
      p.fillRect( xOffset + 3, yOffset, 3, fontHeight, m_pOptionDialog->m_fgColor );
/*      p.setPen( blue );
      p.drawLine( xOffset+2, yOffset, xOffset+2, yOffset+fontHeight-1 );
      p.drawLine( xOffset+3, yOffset, xOffset+3, yOffset+fontHeight-1 );*/
   }
}

void MergeResultWindow::setPaintingAllowed(bool bPaintingAllowed)
{
   m_bPaintingAllowed = bPaintingAllowed;
   if ( !m_bPaintingAllowed )
      m_currentMergeLineIt = m_mergeLineList.end();
}

void MergeResultWindow::paintEvent( QPaintEvent* e )
{
   if (m_pDiff3LineList==0 || !m_bPaintingAllowed) return;

   bool bOldSelectionContainsData = m_selection.bSelectionContainsData;
   const QFontMetrics& fm = fontMetrics();
   int fontHeight = fm.height();
   int fontWidth = fm.width("W");
   int fontAscent = fm.ascent();

   if ( e!= 0 )  // e==0 for blinking cursor
   {
      m_selection.bSelectionContainsData = false;
      if ( size() != m_pixmap.size() )
         m_pixmap.resize(size());

      QPainter p(&m_pixmap);
      p.setFont( font() );
      p.fillRect( rect(), m_pOptionDialog->m_bgColor );

      //int visibleLines = height() / fontHeight;

      {  // Draw the topline
         QString s = " " +i18n("Output") + " : " + m_fileName + " ";
         if (m_bModified)
            s += i18n("[Modified]");

         int topLineYOffset = fontHeight + 3;

         if (hasFocus())
         {
            p.fillRect( 0, 0, width(), topLineYOffset, lightGray /*m_pOptionDialog->m_diffBgColor*/ );
         }
         else
         {
            p.fillRect( 0, 0, width(), topLineYOffset, m_pOptionDialog->m_bgColor );
         }
         p.setPen( m_pOptionDialog->m_fgColor );
         p.drawText( 0, fontAscent+1, s );
         p.drawLine( 0, fontHeight + 2, width(), fontHeight + 2 );
      }

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

                  const char* s;
                  int size;
                  s = mel.getString( this, size );
                  if (size>nofColumns)
                     nofColumns = size;

                  writeLine( p, line, s, size, mel.src(), ml.mergeDetails, rangeMark,
                     mel.isModified(), mel.isRemoved() );
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
   if ( e!= 0 )
   {
      painter.drawPixmap(0,0, m_pixmap);
   }

   int topLineYOffset = fontHeight + 3;
   int xOffset = fontWidth * leftInfoWidth;
   int yOffset = ( m_cursorYPos - m_firstLine ) * fontHeight + topLineYOffset;
   int xCursor = ( m_cursorXPos - m_firstColumn ) * fontWidth + xOffset;

   if ( e!= 0 )
      painter.drawPixmap(0,0, m_pixmap);
   else
      painter.drawPixmap(xCursor-2, yOffset, m_pixmap,
                         xCursor-2, yOffset, 5, fontAscent+2 );


   if ( m_bCursorOn && hasFocus() && m_cursorYPos>=m_firstLine )
   {
      int topLineYOffset = fontHeight + 3;
      int xOffset = fontWidth * leftInfoWidth;

      int yOffset = ( m_cursorYPos-m_firstLine ) * fontHeight + topLineYOffset;

      int xCursor = ( m_cursorXPos - m_firstColumn ) * fontWidth + xOffset;

      painter.setPen( m_pOptionDialog->m_fgColor );

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
   int topLineYOffset = fontHeight + 3;

   int yOffset = topLineYOffset - m_firstLine * fontHeight;

   line = min2( ( y - yOffset ) / fontHeight, m_totalSize-1 );
   pos  = ( x - xOffset ) / fontWidth;
}

void MergeResultWindow::mousePressEvent ( QMouseEvent* e )
{
   m_bCursorOn = true;

   int line;
   int pos;
   convertToLinePos( e->x(), e->y(), line, pos );

   bool bLMB = e->button() == LeftButton;
   bool bMMB = e->button() == MidButton;
   bool bRMB = e->button() == RightButton;

   if ( bLMB && pos < m_firstColumn || bRMB )       // Fast range selection
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
      if ( e->state() & ShiftButton )
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
   if ( e->button() == LeftButton )
   {
      int line;
      int pos;
      convertToLinePos( e->x(), e->y(), line, pos );
      m_cursorXPos = pos;
      m_cursorOldXPos = pos;
      m_cursorYPos = line;

      // Get the string data of the current line

      int size;
      MergeLineList::iterator mlIt;
      MergeEditLineList::iterator melIt;
      calcIteratorFromLineNr( line, mlIt, melIt );
      const char* s = melIt->getString( this, size );

      if ( s!=0 && size>0 )
      {
         int pos1, pos2;

         calcTokenPos( s, size, pos, pos1, pos2 );

         resetSelection();
         m_selection.start( line, convertToPosOnScreen( s, pos1 ) );
         m_selection.end( line, convertToPosOnScreen( s, pos2 ) );

         update();
         // emit selectionEnd() happens in the mouseReleaseEvent.
      }
   }
}

void MergeResultWindow::mouseReleaseEvent ( QMouseEvent * e )
{
   if ( e->button() == LeftButton )
   {
      killTimers();

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
      int fontHeight = fm.height();
      int fontWidth = fm.width('W');
      int topLineYOffset = fontHeight + 3;
      int deltaX=0;
      int deltaY=0;
      if ( e->x() < leftInfoWidth*fontWidth )       deltaX=-1;
      if ( e->x() > width()     )       deltaX=+1;
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
      paintEvent(0);

   m_cursorTimer.start(500,true);
}


void MergeResultWindow::wheelEvent( QWheelEvent* e )
{
    int d = -e->delta()*QApplication::wheelScrollLines()/120;
    e->accept();
    scroll( 0, min2(d, getNofVisibleLines()) );
}


void MergeResultWindow::keyPressEvent( QKeyEvent* e )
{
   int y = m_cursorYPos;
   MergeLineList::iterator mlIt;
   MergeEditLineList::iterator melIt;
   calcIteratorFromLineNr( y, mlIt, melIt );

   int stringLength;
   const char* ps = melIt->getString( this, stringLength );
   int x = convertToPosInText( ps, stringLength, m_cursorXPos );

   bool bCtrl  = ( e->state() & ControlButton ) != 0 ;
   bool bShift = ( e->state() & ShiftButton   ) != 0 ;
   #ifdef _WIN32
   bool bAlt   = ( e->state() & AltButton     ) != 0 ;
   if ( bCtrl && bAlt ){ bCtrl=false; bAlt=false; }  // AltGr-Key pressed.
   #endif

   bool bYMoveKey = false;
   // Special keys
   switch ( e->key() )
   {
      case  Key_Escape:       break;
      //case  Key_Tab:          break;
      case  Key_Backtab:      break;
      case  Key_Delete:
      {
         if ( deleteSelection2( ps, stringLength, x, y, mlIt, melIt )) break;
         if( !melIt->isEditableText() )  break;
         if (x>=stringLength)
         {
            if ( y<m_totalSize-1 )
            {
               setModified();
               QCString s1( ps, stringLength+1 );
               MergeLineList::iterator mlIt1;
               MergeEditLineList::iterator melIt1;
               calcIteratorFromLineNr( y+1, mlIt1, melIt1 );
               if ( melIt1->isEditableText() )
               {
                  int stringLength1;
                  ps = melIt1->getString( this, stringLength1 );
                  assert(ps!=0);
                  QCString s2( ps, stringLength1+1 );
                  melIt->setString( s1 + s2 );

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
            QCString s( ps, x+1 );
            s += QCString( ps+x+1, stringLength - x );
            melIt->setString( s );
            setModified();
         }
         break;
      }
      case  Key_Backspace:
      {
         if ( deleteSelection2( ps, stringLength, x, y, mlIt, melIt )) break;
         if( !melIt->isEditableText() )  break;
         if (x==0)
         {
            if ( y>0 )
            {
               setModified();
               QCString s2( ps, stringLength+1 );
               MergeLineList::iterator mlIt1;
               MergeEditLineList::iterator melIt1;
               calcIteratorFromLineNr( y-1, mlIt1, melIt1 );
               if ( melIt1->isEditableText() )
               {
                  int stringLength1;
                  ps = melIt1->getString( this, stringLength1 );
                  QCString s1( ps, stringLength1+1 );
                  melIt1->setString( s1 + s2 );

                  // Remove the previous line
                  if ( mlIt->mergeEditLineList.size()>1 )
                     mlIt->mergeEditLineList.erase( melIt );
                  else
                     melIt->setRemoved();

                  --y;
                  x=stringLength1;
               }
            }
         }
         else
         {
            QCString s( ps, x );
            s += QCString( ps+x, stringLength - x + 1 );
            --x;
            melIt->setString( s );
            setModified();
         }
         break;
      }
      case  Key_Return:
      case  Key_Enter:
      {
         if( !melIt->isEditableText() )  break;
         deleteSelection2( ps, stringLength, x, y, mlIt, melIt );
         setModified();
         QCString indentation;
         if ( m_pOptionDialog->m_bAutoIndentation )
         {  // calc last indentation
            MergeLineList::iterator mlIt1 = mlIt;
            MergeEditLineList::iterator melIt1 = melIt;
            for(;;) {
               int size;
               const char* s = melIt1->getString(this, size);
               if ( s!=0 ) {
                  int i;
                  for( i=0; i<size; ++i ){ if(s[i]!=' ' && s[i]!='\t') break; }
                  if (i<size) {
                     indentation = QCString( s, i+1 );
                     break;
                  }
               }
               --melIt1;
               if ( melIt1 == mlIt1->mergeEditLineList.end() ) {
                  --mlIt1;
                  if ( mlIt1 == m_mergeLineList.end() ) break;
                  melIt1 = mlIt1->mergeEditLineList.end();
                  --melIt1;
               }
            }
         }
         MergeEditLine mel;
         mel.setString( indentation + QCString( ps+x, stringLength - x + 1 ) );

         if ( x<stringLength ) // Cut off the old line.
         {
            // Since ps possibly points into melIt->str, first copy it into a temporary.
            QCString temp = QCString( ps, x + 1 );
            melIt->setString( temp );
         }

         ++melIt;
         mlIt->mergeEditLineList.insert( melIt, mel );
         x=indentation.length();
         ++y;
         break;
      }
      case  Key_Insert:   m_bInsertMode = !m_bInsertMode;    break;
      case  Key_Pause:        break;
      case  Key_Print:        break;
      case  Key_SysReq:       break;
      case  Key_Home:     x=0;        if(bCtrl){y=0;      }  break;   // cursor movement
      case  Key_End:      x=INT_MAX;  if(bCtrl){y=INT_MAX;}  break;

      case  Key_Left:
         if ( !bCtrl )
         {
            --x;
            if(x<0 && y>0){--y; x=INT_MAX;}
         }
         else
         {
            while( x>0  &&  (ps[x-1]==' ' || ps[x-1]=='\t') ) --x;
            while( x>0  &&  (ps[x-1]!=' ' && ps[x-1]!='\t') ) --x;
         }
         break;

      case  Key_Right:
         if ( !bCtrl )
         {
            ++x;  if(x>stringLength && y<m_totalSize-1){ ++y; x=0; }
         }

         else
         {
            while( x<stringLength  &&  (ps[x]==' ' || ps[x]=='\t') ) ++x;
            while( x<stringLength  &&  (ps[x]!=' ' && ps[x]!='\t') ) ++x;
         }
         break;

      case  Key_Up:       --y;                     bYMoveKey=true;   break;
      case  Key_Down:     ++y;                     bYMoveKey=true;    break;
      case  Key_PageUp:   y-=getNofVisibleLines(); bYMoveKey=true;    break;
      case  Key_PageDown: y+=getNofVisibleLines(); bYMoveKey=true;    break;
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
               deleteSelection2( ps, stringLength, x, y, mlIt, melIt );

               setModified();
               // Characters to insert
               QCString s( ps, stringLength+1 );
               if ( t[0]=='\t' && m_pOptionDialog->m_bReplaceTabs )
               {
                  int spaces = (m_cursorXPos / g_tabSize + 1)*g_tabSize - m_cursorXPos;
                  t.fill( ' ', spaces );
               }
               if ( m_bInsertMode )
                  s.insert( x, encodeString(t, m_pOptionDialog) );
               else
                  s.replace( x, t.length(), encodeString(t, m_pOptionDialog) );

               melIt->setString( s );
               x += t.length();
               bShift = false;
            }
         }
      }
   }

   y = minMaxLimiter( y, 0, m_totalSize-1 );

   calcIteratorFromLineNr( y, mlIt, melIt );
   ps = melIt->getString( this, stringLength );

   x = minMaxLimiter( x, 0, stringLength );

   int newFirstLine = m_firstLine;
   int newFirstColumn = m_firstColumn;

   if ( y<m_firstLine )
      newFirstLine = y;
   else if ( y > m_firstLine + getNofVisibleLines() )
      newFirstLine = y - getNofVisibleLines();

   if (bYMoveKey)
      x=convertToPosInText( ps, stringLength, m_cursorOldXPos );

   int xOnScreen = convertToPosOnScreen( ps, x );
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
               int size;
               const char* pLine = mel.getString( this, size );

               // Consider tabs

               for( int i=0; i<size; ++i )
               {
                  int spaces = 1;
                  if ( pLine[i]=='\t' )
                  {
                     spaces = tabber( outPos, g_tabSize );
                  }

                  if( m_selection.within( line, outPos ) )
                  {
                     char buf[2];
                     buf[0] = pLine[i];
                     buf[1] = '\0';
                     selectionString += decodeString( buf, m_pOptionDialog );
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

bool MergeResultWindow::deleteSelection2( const char*& ps, int& stringLength, int& x, int& y,
                MergeLineList::iterator& mlIt, MergeEditLineList::iterator& melIt )
{
   if (m_selection.firstLine!=-1  &&  m_selection.bSelectionContainsData )
   {
      deleteSelection();
      y = m_cursorYPos;
      calcIteratorFromLineNr( y, mlIt, melIt );
      ps = melIt->getString( this, stringLength );
      x = convertToPosInText( ps, stringLength, m_cursorXPos );
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
   QCString firstLineString;

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
            int size;
            const char* pLine = mel.getString( this, size );

            int firstPosInLine = m_selection.firstPosInLine(line);
            int lastPosInLine = m_selection.lastPosInLine(line);

            if ( line==firstLine )
            {
               mlItFirst = mlIt;
               melItFirst = melIt;
               int pos = convertToPosInText( pLine, size, firstPosInLine );
               firstLineString = QCString( pLine, pos+1 );
            }

            if ( line==lastLine )
            {
               // This is the last line in the selection
               int pos = convertToPosInText( pLine, size, lastPosInLine );
               firstLineString += QCString( pLine+pos, 1+max2( 0,size-pos));
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
   int stringLength;
   const char* ps = melIt->getString( this, stringLength );
   int x = convertToPosInText( ps, stringLength, m_cursorXPos );

   if ( !QApplication::clipboard()->supportsSelection() )
      bFromSelection = false;
      
   QCString clipBoard = encodeString( 
      QApplication::clipboard()->text( bFromSelection ? QClipboard::Selection : QClipboard::Clipboard ), 
      m_pOptionDialog 
      );

   QCString currentLine = QCString( ps, x+1 );
   QCString endOfLine = QCString( ps+x, stringLength-x+1 );
   int i;
   for( i=0; i<(int)clipBoard.length(); ++i )
   {
      char c = clipBoard[i];
      if ( c == '\r' ) continue;
      if ( c == '\n' )
      {
         melIt->setString( currentLine );

         melIt = mlIt->mergeEditLineList.insert( melItAfter, MergeEditLine() );
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
   m_cursorXPos = convertToPosOnScreen( currentLine, x );
   m_cursorOldXPos = m_cursorXPos;

   update();
}

void MergeResultWindow::resetSelection()
{
   m_selection.reset();
   update();
}

void MergeResultWindow::setModified()
{
   if (!m_bModified)
   {
      m_bModified = true;
      emit modified();
   }
}

/// Saves and returns true when successful.
bool MergeResultWindow::saveDocument( const QString& fileName )
{
   m_fileName = fileName;

   // Are still conflicts somewhere?
   if ( getNrOfUnsolvedConflicts()>0 )
   {
      KMessageBox::error( this,
         i18n("Not all conflicts are solved yet.\n"
              "File not saved.\n"),
         i18n("Conflicts Left"));
      return false;
   }

   update();

   FileAccess file( fileName, true /*bWantToWrite*/ );
   if ( m_pOptionDialog->m_bDmCreateBakFiles && file.exists() )
   {
      bool bSuccess = file.createBackup(".orig");
      if ( !bSuccess )
      {
         KMessageBox::error( this, file.getStatusText() + i18n("\n\nFile not saved."), i18n("File Save Error") );
         return false;
      }
   }

   // Loop twice over all data: First to calculate the needed size,
   // then alloc memory and in the second loop copy the data into the memory.
   long neededBufferSize = 0;
   long dataIndex = 0;
   QByteArray dataArray;
   for ( int i=0; i<2; ++i )
   {
      if(i==1)
      {
         if ( ! dataArray.resize(neededBufferSize) )
         {
            KMessageBox::error(this, i18n("Out of memory while preparing to save.") );
            return false;
         }
      }

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
               int size;
               const char* pLine = mel.getString( this, size );

               QCString s(pLine, size+1);

               if (line>0) // Prepend line feed, but not for first line
               {
                  if ( m_pOptionDialog->m_lineEndStyle == eLineEndDos )
                  {   s.prepend("\r\n"); size+=2; }
                  else
                  {   s.prepend("\n");   size+=1; }
               }

               if (i==0) neededBufferSize += size;
               else
               {
                  memcpy( dataArray.data() + dataIndex, s, size );
                  dataIndex+=size;
               }
            }

            ++line;
         }
      }
   }
   bool bSuccess = file.writeFile( dataArray.data(), neededBufferSize );
   if ( ! bSuccess )
   {
      KMessageBox::error( this, i18n("Error while writing."), i18n("File Save Error") );
      return false;
   }

   m_bModified = false;
   update();

   return true;
}

QCString MergeResultWindow::getString( int lineIdx )
{
   MergeResultWindow::MergeLineList::iterator mlIt;
   MergeResultWindow::MergeEditLineList::iterator melIt;
   calcIteratorFromLineNr( lineIdx, mlIt, melIt );
   int length=0;   
   const char* p = melIt->getString( this, length );
   QCString line( p, length+1 );
   return line;
}

bool MergeResultWindow::findString( const QCString& s, int& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive )
{
   int it = d3vLine;
   int endIt = bDirDown ? getNofLines() : -1;
   int step =  bDirDown ? 1 : -1;
   int startPos = posInLine;

   for( ; it!=endIt; it+=step )
   {
      QCString line = getString( it );
      if ( !line.isEmpty() )
      {
         int pos = line.find( s, startPos, bCaseSensitive );
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
   m_selection.reset();
   m_selection.start( firstLine, convertToPosOnScreen( getString(firstLine), startPos ) );
   m_selection.end( lastLine, convertToPosOnScreen( getString(lastLine), endPos ) );
   update();
}

Overview::Overview( QWidget* pParent, OptionDialog* pOptions )
: QWidget( pParent, 0, WRepaintNoErase )
{
   m_pDiff3LineList = 0;
   m_pOptions = pOptions;
   m_bTripleDiff = false;
   m_eOverviewMode = eOMNormal;
   m_nofLines = 1;
   setFixedWidth(20);
}

void Overview::init( Diff3LineList* pDiff3LineList, bool bTripleDiff )
{
   m_pDiff3LineList = pDiff3LineList;
   m_bTripleDiff = bTripleDiff;
   m_pixmap.resize( QSize(0,0) );   // make sure that a redraw happens
   update();
}

void Overview::slotRedraw()
{
   m_pixmap.resize( QSize(0,0) );   // make sure that a redraw happens
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
   }
}

void Overview::drawColumn( QPainter& p, e_OverviewMode eOverviewMode, int x, int w, int h, int nofLines )
{
   p.setPen(black);
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
                        bWhiteSpaceChange = d3l.bAEqB || d3l.bWhiteLineA && d3l.bWhiteLineB;
                        break;

         case eCAdded:
         case eCDeleted:
         case eCChanged:
                        bWhiteSpaceChange = d3l.bAEqC || d3l.bWhiteLineA && d3l.bWhiteLineC;
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
                          bWhiteSpaceChange = d3l.bAEqB || d3l.bWhiteLineA && d3l.bWhiteLineB;
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
                          bWhiteSpaceChange = d3l.bAEqC || d3l.bWhiteLineA && d3l.bWhiteLineC;
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
                                  bWhiteSpaceChange = d3l.bBEqC || d3l.bWhiteLineB && d3l.bWhiteLineC;
                                  break;
         }
      }

      if (!bWhiteSpaceChange || m_pOptions->m_bShowWhiteSpace )
      {
         // Make sure that lines with conflict are not overwritten.
         if (  c == m_pOptions->m_colorForConflict )
         {
            p.fillRect(x+1, oldY, w, max2(1,y-oldY), bWhiteSpaceChange ? QBrush(c,Dense4Pattern) : c );
            oldConflictY = oldY;
         }
         else if ( c!=m_pOptions->m_bgColor  &&  oldY>oldConflictY )
         {
            p.fillRect(x+1, oldY, w, max2(1,y-oldY), bWhiteSpaceChange ? QBrush(c,Dense4Pattern) : c );
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
   
      m_pixmap.resize( size() );

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
   painter.setPen(black);
   painter.drawRect( 1, y1, w-1, h1 );
}


