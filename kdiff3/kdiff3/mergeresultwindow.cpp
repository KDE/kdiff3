/***************************************************************************
                          mergeresultwindow.cpp  -  description
                             -------------------
    begin                : Sun Apr 14 2002
    copyright            : (C) 2002 by Joachim Eibl
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

/***************************************************************************
 * $Log$
 * Revision 1.1  2002/08/18 16:24:13  joachim99
 * Initial revision
 *                                                                   *
 ***************************************************************************/

#include "diff.h"
#include <stdio.h>
#include <qpainter.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <optiondialog.h>
#include <klocale.h>
#include <kmessagebox.h>

#define leftInfoWidth 3

MergeResultWindow::MergeResultWindow(
   QWidget* pParent,
   const LineData* pLineDataA,
   const LineData* pLineDataB,
   const LineData* pLineDataC,
   const Diff3LineList* pDiff3LineList,
   QString fileName,
   OptionDialog* pOptionDialog
   )
: QWidget( pParent, 0, WRepaintNoErase )
{
   setFocusPolicy( QWidget::ClickFocus );

   m_fileName = fileName;
   m_pldA = pLineDataA;
   m_pldB = pLineDataB;
   m_pldC = pLineDataC;

   m_pDiff3LineList = pDiff3LineList;

   m_firstLine = 0;
   m_firstColumn = 0;
   m_nofColumns = 0;
   m_nofLines = 0;
   m_bMyUpdate = false;
   m_bInsertMode = true;
   m_scrollDeltaX = 0;
   m_scrollDeltaY = 0;
   m_bModified = false;

   m_pOptionDialog = pOptionDialog;

   m_cursorXPos=0;
   m_cursorOldXPos=0;
   m_cursorYPos=0;
   m_bCursorOn = true;
   connect( &m_cursorTimer, SIGNAL(timeout()), this, SLOT( slotCursorUpdate() ) );
   m_cursorTimer.start( 500 /*ms*/, true /*single shot*/ );
   m_selection.reset();

   setMinimumSize( QSize(20,20) );

   merge();
}


const int A=1, B=2, C=3;

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

   // Assume A is base.
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
         mergeDetails = eBCChangedAndEqual;   bConflict = true;
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
         mergeDetails = eBCAddedAndEqual;     bConflict = true;
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
      mergeDetails = eBCDeleted;              bConflict = true;
   }
   else
      assert(false);
}

bool MergeResultWindow::sameKindCheck( const MergeLine& ml1, const MergeLine& ml2 )
{
   return (
      ml1.bConflict && ml2.bConflict ||
      !ml1.bConflict && !ml2.bConflict && ml1.bDelta && ml2.bDelta && ml1.srcSelect == ml2.srcSelect ||
      !ml1.bDelta && !ml2.bDelta
      );
}

void MergeResultWindow::merge()
{
   m_mergeLineList.clear();
   int lineIdx = 0;
   Diff3LineList::const_iterator it;
   for( it=m_pDiff3LineList->begin(); it!=m_pDiff3LineList->end(); ++it, ++lineIdx )
   {
      const Diff3Line& d = *it;

      MergeLine ml;
      bool bLineRemoved;
      mergeOneLine( d, ml.mergeDetails, ml.bConflict, bLineRemoved, ml.srcSelect, m_pldC==0 );

      ml.d3lLineIdx   = lineIdx;
      ml.bDelta       = ml.srcSelect != A;
      ml.id3l         = it;
      ml.srcRangeLength = 1;

      MergeLine* back = m_mergeLineList.empty() ? 0 : &m_mergeLineList.back();

      if( back!=0 && sameKindCheck( ml, *back ) )
      {
         ++back->srcRangeLength;
      }
      else
      {
         m_mergeLineList.push_back( ml );
      }

      if ( ! ml.bConflict )
      {
         MergeLine& newBack = m_mergeLineList.back();
         MergeEditLine mel;
         mel.setSource( ml.srcSelect, ml.id3l, bLineRemoved );
         newBack.mergeEditLineList.push_back(mel);
      }
      else if ( back==0  || ! back->bConflict )
      {
         MergeLine& newBack = m_mergeLineList.back();
         MergeEditLine mel;
         mel.setConflict();
         newBack.mergeEditLineList.push_back(mel);
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

   m_currentMergeLineIt = m_mergeLineList.begin();
}

void MergeResultWindow::setFirstLine(int firstLine)
{
   m_firstLine = max(0,firstLine);
   update();
}

void MergeResultWindow::setFirstColumn(int firstCol)
{
   m_firstColumn = max(0,firstCol);
   update();
}

int MergeResultWindow::getNofColumns()
{
   return m_nofColumns;
}

int MergeResultWindow::getNofLines()
{
   return m_nofLines;
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
   if( eEndPoint==eEnd )
   {
      if (eDir==eUp) i = m_mergeLineList.begin();     // first mergeline
      else           i = --m_mergeLineList.end();     // last mergeline

//      while ( ! i->bDelta   &&   i!=m_mergeLineList.end() )
//      {
//         if ( eDir==eUp )  ++i;                       // search downwards
//         else              --i;                       // search upwards
//      }
   }
   else if ( eEndPoint == eDelta  &&  i!=m_mergeLineList.end())
   {
      do
      {
         if ( eDir==eUp )  --i;
         else              ++i;
      }
      while ( i->bDelta == false  &&  i!=m_mergeLineList.end() );
   }
   else if ( eEndPoint == eConflict  &&  i!=m_mergeLineList.end())
   {
      do
      {
         if ( eDir==eUp )  --i;
         else              ++i;
      }
      while ( i->bConflict == false  &&  i!=m_mergeLineList.end() );
   }

   setFastSelector( i );
}

void MergeResultWindow::slotGoTop()
{
   go( eUp, eEnd );
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

   update();
}

void MergeResultWindow::choose( int selector )
{
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

   update();
}

void MergeResultWindow::slotChooseA()
{
   choose( A );
}

void MergeResultWindow::slotChooseB()
{
   choose( B );
}

void MergeResultWindow::slotChooseC()
{
   choose( C );
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
         assert(false);
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
int convertToPosOnScreen( const char* p, int posInText )
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
         int lengthInLine = max(0,lastPosInLine - firstPosInLine);
         if (lengthInLine>0) m_selection.bSelectionContainsData = true;

         if ( lengthInLine < int(s.length()) )
         {                                // Draw a normal line first
            p.setPen( m_pOptionDialog->m_fgColor );
            p.drawText( xOffset, yOffset+fontAscent, s.mid(m_firstColumn) );
         }
         int firstPosInLine2 = max( firstPosInLine, m_firstColumn );
         int lengthInLine2 = max(0,lastPosInLine - firstPosInLine2);

         if( m_selection.lineWithin( line+1 ) )
            p.fillRect( xOffset + fontWidth*(firstPosInLine2-m_firstColumn), yOffset,
               width(), fontHeight, colorGroup().highlight() );
         else
            p.fillRect( xOffset + fontWidth*(firstPosInLine2-m_firstColumn), yOffset,
               fontWidth*lengthInLine2, fontHeight, colorGroup().highlight() );

         p.setPen( colorGroup().highlightedText() );
         p.drawText( xOffset + fontWidth*(firstPosInLine2-m_firstColumn), yOffset+fontAscent,
            s.mid(firstPosInLine2,lengthInLine2) );
      }
      else
      {
         p.setPen( m_pOptionDialog->m_fgColor );
         p.drawText( xOffset, yOffset+fontAscent, s.mid(m_firstColumn) );
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
      p.drawText( xOffset, yOffset+fontAscent, "<No src line>" );
      p.drawText( 1, yOffset+fontAscent, srcName );
      if ( m_cursorYPos==line ) m_cursorXPos = 0;
   }
   else if ( srcSelect == 0 )
   {
      p.setPen( m_pOptionDialog->m_colorForConflict );
      p.drawText( xOffset, yOffset+fontAscent, "<Merge Conflict>" );
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
      p.setPen( blue );
      p.drawLine( xOffset+2, yOffset, xOffset+2, yOffset+fontHeight-1 );
      p.drawLine( xOffset+3, yOffset, xOffset+3, yOffset+fontHeight-1 );
   }
}

void MergeResultWindow::paintEvent( QPaintEvent* e )
{
   bool bOldSelectionContainsData = m_selection.bSelectionContainsData;
   if (font() != m_pOptionDialog->m_font )
   {
      setFont( m_pOptionDialog->m_font );
   }
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
         QString s;
         s.sprintf(" Output : %s ", (const char*)m_fileName );
         // s.sprintf(" Output : %s : Line %d",(const char*) m_fileName, m_firstLine+1 );
         if (m_bModified)
            s += i18n("[Modified]");

         int topLineYOffset = fontHeight + 3;

         if (hasFocus())
         {
            p.fillRect( 0, 0, width(), topLineYOffset, m_pOptionDialog->m_diffBgColor );
         }
         else
         {
            p.fillRect( 0, 0, width(), topLineYOffset, m_pOptionDialog->m_bgColor );
         }
         p.setPen( m_pOptionDialog->m_fgColor );
         p.drawText( 0, fontAscent+1, s );
         p.drawLine( 0, fontHeight + 2, width(), fontHeight + 2 );
      }

      bool bAnyConflict = false;
      int nofColumns = 0;
      int line = 0;
      MergeLineList::iterator mlIt = m_mergeLineList.begin();
      for(mlIt = m_mergeLineList.begin();mlIt!=m_mergeLineList.end(); ++mlIt)
      {
         MergeLine& ml = *mlIt;
         MergeEditLineList::iterator melIt, melIt1;
         for( melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt )
         {
            MergeEditLine& mel = *melIt;
            melIt1 = melIt;
            ++melIt1;

            if( mel.isConflict() )
               bAnyConflict = true;

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
            ++line;
         }
      }

      emit savable( ! bAnyConflict );

      if ( line != m_nofLines || nofColumns != m_nofColumns )
      {
         m_nofLines = line;
         m_nofColumns = nofColumns;
         emit resizeSignal();
      }

      if( m_currentMergeLineIt == m_mergeLineList.end() )
         emit sourceMask( 0, 0 );
      else
      {
         int enabledMask = m_pldC==0 ? 3 : 7;
         MergeLine& ml = *m_currentMergeLineIt;

         int srcMask = 0;
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

         if ( ml.mergeDetails == eNoChange ) emit sourceMask( 0, bModified ? 1 : 0 );
         else                                emit sourceMask( srcMask, enabledMask );
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

   if ( m_bCursorOn && hasFocus() )
   {
      int topLineYOffset = fontHeight + 3;
      int xOffset = fontWidth * leftInfoWidth;

      int yOffset = ( m_cursorYPos-m_firstLine ) * fontHeight + topLineYOffset;

      int xCursor = ( m_cursorXPos - m_firstColumn ) * fontWidth + xOffset;
      painter.drawLine( xCursor, yOffset, xCursor, yOffset+fontAscent );
      painter.drawLine( xCursor-2, yOffset, xCursor+2, yOffset );
      painter.drawLine( xCursor-2, yOffset+fontAscent+1, xCursor+2, yOffset+fontAscent+1 );
   }

   if( !bOldSelectionContainsData  &&  m_selection.bSelectionContainsData )
      emit newSelection();
}

void MergeResultWindow::convertToLinePos( int x, int y, int& line, int& pos )
{
   const QFontMetrics& fm = fontMetrics();
   int fontHeight = fm.height();
   int fontWidth = fm.width('W');
   int xOffset = (leftInfoWidth-m_firstColumn)*fontWidth;
   int topLineYOffset = fontHeight + 3;

   int yOffset = topLineYOffset - m_firstLine * fontHeight;

   line = min( ( y - yOffset ) / fontHeight, m_nofLines-1 );
   pos  = ( x - xOffset ) / fontWidth;
}

void MergeResultWindow::mousePressEvent ( QMouseEvent* e )
{
   m_bCursorOn = true;

   int line;
   int pos;
   convertToLinePos( e->x(), e->y(), line, pos );

   if ( e->button() == LeftButton )
   {
      if ( pos < m_firstColumn )       // Fast range selection
      {
         m_cursorXPos = 0;
         m_cursorOldXPos = 0;
         m_cursorYPos = max(line,0);
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
      }
      else                             // Normal cursor placement
      {
         pos = max(pos,0);
         line = max(line,0);

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
   }
   else if ( e->button() == MidButton )        // Paste clipboard
   {
      pos = max(pos,0);
      line = max(line,0);

      m_selection.reset();
      m_cursorXPos = pos;
      m_cursorOldXPos = pos;
      m_cursorYPos = line;

      pasteClipboard();
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
//   bool bAlt   = ( e->state() & AltButton     ) != 0 ;
   bool bShift = ( e->state() & ShiftButton   ) != 0 ;


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
            if ( y<m_nofLines-1 )
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
            ++x;  if(x>stringLength && y<m_nofLines-1){ ++y; x=0; }
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

   y = minMaxLimiter( y, 0, m_nofLines-1 );

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
      for( melIt = ml.mergeEditLineList.begin(); melIt != ml.mergeEditLineList.end(); ++melIt )
      {
         --line;
         if (line<0) return;
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

         if ( mel.isEditableText() && m_selection.lineWithin(line) )
         {
            int size;
            const char* pLine = mel.getString( this, size );

            // Consider tabs
            int outPos = 0;
            for( int i=0; i<size; ++i )
            {
               int spaces = 1;
               if ( pLine[i]=='\t' )
               {
                  spaces = tabber( outPos, g_tabSize );
               }

               if( m_selection.within( line, outPos ) )
               {
                 selectionString += pLine[i];
               }

               outPos += spaces;
            }

            if( m_selection.within( line, outPos ) )
            {
               #ifdef WIN32
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
               firstLineString += QCString( pLine+pos, 1+max( 0,size-pos));
               melItFirst->setString( firstLineString );
            }

            if ( line!=firstLine )
            {
               // Remove the line
               if ( mlIt->mergeEditLineList.size()>1 )
               {   mlIt->mergeEditLineList.erase( melIt ); --m_nofLines; }
               else
               {   melIt->setRemoved();  }
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

void MergeResultWindow::pasteClipboard()
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

   QString clipBoard = QApplication::clipboard()->text();

   QCString currentLine = QCString( ps, x+1 );
   QCString endOfLine = QCString( ps+x, stringLength-x+1 );
   int i;
   for( i=0; i<(int)clipBoard.length(); ++i )
   {
      QChar uc = clipBoard[i];
      char c = uc;
      if ( c == '\r' ) continue;
      if ( c == '\n' )
      {
         melIt->setString( currentLine );

         melIt = mlIt->mergeEditLineList.insert( melItAfter, MergeEditLine() );
         currentLine = QCString();
      }
      else
      {
         currentLine += c;
      }
   }

   currentLine += endOfLine;
   melIt->setString( currentLine );

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
   update();

   FILE* file = fopen( m_fileName, "wb" );

   if ( file == 0 )
   {
      KMessageBox::error( this, i18n("Could not open file for saving."), i18n("File save error.") );
      return false;
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
            #ifdef WIN32
            s += '\r'; ++size;
            #endif
            s += '\n'; ++size;

            int written = fwrite( (const char*)s, 1, size, file );
            if ( written!=size )
            {
               fclose(file);
               KMessageBox::error( this, i18n("Error while writing."), i18n("File save error.") );
               return false;
            }
         }

         ++line;
      }
   }

   fclose( file );

   m_bModified = false;

   return true;
}



Overview::Overview( QWidget* pParent, Diff3LineList* pDiff3LineList,
  OptionDialog* pOptions, bool bTripleDiff )
: QWidget( pParent, 0, WRepaintNoErase )
{
   m_pDiff3LineList = pDiff3LineList;
   m_pOptions = pOptions;
   m_bTripleDiff = bTripleDiff;
   setFixedWidth(20);
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

void Overview::mousePressEvent( QMouseEvent* e )
{
   int h = height()-1;
   int nofLines = m_pDiff3LineList->size();
   int h1 = h * m_pageHeight / nofLines+3;
   if ( h>0 )
      emit setLine( ( e->y() - h1/2 )*nofLines/h );
}

void Overview::mouseMoveEvent( QMouseEvent* e )
{
   mousePressEvent(e);
}

void Overview::paintEvent( QPaintEvent* e )
{
   int h = height()-1;
   int w = width();
   int nofLines = m_pDiff3LineList->size();

   if ( m_pixmap.size() != size() )
   {
      m_pixmap.resize( size() );
      
      QPainter p(&m_pixmap);

      p.fillRect( rect(), m_pOptions->m_bgColor );
      p.setPen(black);
      p.drawLine( 0, 0, 0, h );

      if (nofLines==0) return;

      int line = 0;
      int oldY = 0;
      int oldConflictY = -1;
      Diff3LineList::const_iterator i;
      for( i = m_pDiff3LineList->begin(); i!= m_pDiff3LineList->end(); ++i )
      {
         const Diff3Line& d3l = *i;
         int y = h * (line+1) / nofLines;
         e_MergeDetails md;
         bool bConflict;
         bool bLineRemoved;
         int src;
         mergeOneLine( d3l, md, bConflict, bLineRemoved, src, !m_bTripleDiff );

         QColor c;
         if( bConflict )  c=m_pOptions->m_colorForConflict;
         else
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
                         c = m_pOptions->m_colorB;
                         break;

            case eCAdded:
            case eCDeleted:
            case eCChanged:
                         c = m_pOptions->m_colorC;
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

         // Make sure that lines with conflict are not overwritten.
         if (  c == m_pOptions->m_colorForConflict )
         {
            p.fillRect(1, oldY, w, max(1,y-oldY), c );
            oldConflictY = oldY;
         }
         else if ( c!=m_pOptions->m_bgColor  &&  oldY>oldConflictY )
         {
            p.fillRect(1, oldY, w, max(1,y-oldY), c );
         }

         oldY = y;

         ++line;
      }
   }

   QPainter painter( this );
   painter.drawPixmap( 0,0, m_pixmap );

   int y1 = h * m_firstLine / nofLines-1;
   int h1 = h * m_pageHeight / nofLines+3;
   painter.setPen(black);
   painter.drawRect( 1, y1, w-1, h1 );
}

