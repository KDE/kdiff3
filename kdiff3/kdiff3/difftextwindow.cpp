/***************************************************************************
                          difftextwindow.cpp  -  description
                             -------------------
    begin                : Mon Apr 8 2002
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
 * Revision 1.1  2002/08/18 16:24:14  joachim99
 * Initial revision
 *                                                                   *
 ***************************************************************************/

#include <iostream>
#include "diff.h"
#include "merger.h"
#include <qpainter.h>
#include <assert.h>
#include <qpixmap.h>
#include <qstatusbar.h>
#include <qapplication.h>
#include <optiondialog.h>

#define leftInfoWidth 4   // Nr of information columns on left side

using namespace std;

DiffTextWindow::DiffTextWindow(
   QWidget* pParent,
   QStatusBar* pStatusBar,
   OptionDialog* pOptionDialog
   )
: QWidget(pParent, 0, WRepaintNoErase)
{
   setFocusPolicy( QWidget::ClickFocus );

   m_pOptionDialog = pOptionDialog;
   init( 0, 0, 0, 0, 0, false );

   setBackgroundMode( PaletteBase );
   setMinimumSize(QSize(20,20));

   m_pStatusBar = pStatusBar;

   m_fastSelectorLine1 = 0;
   m_fastSelectorNofLines = 0;
}


void DiffTextWindow::init(
   const char* pFilename,
   LineData* pLineData,
   int size,
   const Diff3LineList* pDiff3LineList,
   int winIdx,
   bool bTriple
   )
{
   m_pFilename = pFilename;
   m_pLineData = pLineData;
   m_size = size;
   m_pDiff3LineList = pDiff3LineList;
   m_winIdx = winIdx;

   m_firstLine = 0;
   m_oldFirstLine = -1;
   m_firstColumn = 0;
   m_oldFirstColumn = -1;
   m_bTriple = bTriple;
   m_scrollDeltaX=0;
   m_scrollDeltaY=0;
   m_bMyUpdate = false;
}

void DiffTextWindow::setFirstLine(int firstLine)
{
   m_firstLine = max(0,firstLine);
   myUpdate(0); // Immediately
}

void DiffTextWindow::setFirstColumn(int firstCol)
{
   m_firstColumn = max(0,firstCol);
   myUpdate(0); // Immediately
}

int DiffTextWindow::getNofColumns()
{
   int nofColumns = 0;
   for( int i = 0; i< m_size; ++i )
   {
      if ( m_pLineData[i].width() > nofColumns )
         nofColumns = m_pLineData[i].width();
   }
   return nofColumns;
}

int DiffTextWindow::getNofLines()
{
   return m_pDiff3LineList->size();
}

/** Returns a line number where the linerange [line, line+nofLines] can
    be displayed best. If it fits into the currently visible range then
    the returned value is the current firstLine.
*/
int getBestFirstLine( int line, int nofLines, int firstLine, int visibleLines )
{
   int newFirstLine = firstLine;
   if ( line < firstLine  ||  line + nofLines > firstLine + visibleLines )
   {
      if ( nofLines > visibleLines || nofLines <= ( 2*visibleLines / 3 - 1)  )
         newFirstLine = line - visibleLines/3;
      else
         newFirstLine = line - (visibleLines - nofLines);
   }

   return newFirstLine;
}


void DiffTextWindow::setFastSelectorRange( int line1, int nofLines )
{
   m_fastSelectorLine1 = line1;
   m_fastSelectorNofLines = nofLines;

   int newFirstLine = getBestFirstLine( line1, nofLines, m_firstLine, m_visibleLines );
   if ( newFirstLine != m_firstLine )
   {
      scroll( 0, newFirstLine - m_firstLine );
   }

   update();
}

static void showStatusLine(int line, int winIdx, const char* pFilename, const Diff3LineList* pd3ll, QStatusBar* sb)
{
   int l=0;
   Diff3LineList::const_iterator i;
   for ( i=pd3ll->begin();i!=pd3ll->end() && l!=line; ++i,++l )
   {
   }
   if(i!=pd3ll->end())
   {
      if      ( winIdx==1 ) l=(*i).lineA;
      else if ( winIdx==2 ) l=(*i).lineB;
      else if ( winIdx==3 ) l=(*i).lineC;
      else assert(false);

      QString s;
      if ( l!=-1 )
         s.sprintf("File %s: Line %d", pFilename, l+1 );
      else
         s.sprintf("File %s: Line not available", pFilename );
      sb->message(s);
   }
}


void DiffTextWindow::mousePressEvent ( QMouseEvent* e )
{
   if ( e->button() == LeftButton )
   {
      int line;
      int pos;
      convertToLinePos( e->x(), e->y(), line, pos );
      if ( pos < m_firstColumn )
      {
         emit setFastSelectorLine( line );
         selection.firstLine = -1;     // Disable current selection
      }
      else
      {  // Selection
         resetSelection();
         selection.start( line, pos );
         selection.end( line, pos );

         showStatusLine( line, m_winIdx, m_pFilename, m_pDiff3LineList, m_pStatusBar );
      }
   }
}

bool isCTokenChar( char c )
{
   return (c=='_')  ||
          ( c>='A' && c<='Z' ) || ( c>='a' && c<='z' ) ||
          (c>='0' && c<='9');
}

/// Calculate where a token starts and ends, given the x-position on screen.
void calcTokenPos( const char* p, int size, int posOnScreen, int& pos1, int& pos2 )
{
   // Cursor conversions that consider g_tabSize
   int pos = convertToPosInText( p, size, max( 0, posOnScreen ) );
   if ( pos>=size )
   {
      pos1=size;
      pos2=size;
      return;
   }

   pos1 = pos;
   pos2 = pos+1;

   if( isCTokenChar( p[pos1] ) )
   {
      while( pos1>=0 && isCTokenChar( p[pos1] ) )
         --pos1;
      ++pos1;

      while( pos2<size && isCTokenChar( p[pos2] ) )
         ++pos2;
   }
}

void DiffTextWindow::mouseDoubleClickEvent( QMouseEvent* e )
{
   if ( e->button() == LeftButton )
   {
      int line;
      int pos;
      convertToLinePos( e->x(), e->y(), line, pos );

      // Get the string data of the current line
      QCString s = getString( line );

      if ( ! s.isEmpty() )
      {
         int pos1, pos2;
         calcTokenPos( s, s.length(), pos, pos1, pos2 );

         resetSelection();
         selection.start( line, convertToPosOnScreen( s, pos1 ) );
         selection.end( line, convertToPosOnScreen( s, pos2 ) );
         update();
         // emit selectionEnd() happens in the mouseReleaseEvent.
         showStatusLine( line, m_winIdx, m_pFilename, m_pDiff3LineList, m_pStatusBar );
      }
   }
}

void DiffTextWindow::mouseReleaseEvent ( QMouseEvent * e )
{
   if ( e->button() == LeftButton )
   {
      killTimers();
      if (selection.firstLine != -1 )
      {
         emit selectionEnd();
      }
   }
}

void DiffTextWindow::mouseMoveEvent ( QMouseEvent * e )
{
   int line;
   int pos;
   convertToLinePos( e->x(), e->y(), line, pos );
   if (selection.firstLine != -1 )
   {
      selection.end( line, pos );
      myUpdate(0);

      showStatusLine( line, m_winIdx, m_pFilename, m_pDiff3LineList, m_pStatusBar );

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


void DiffTextWindow::myUpdate(int afterMilliSecs)
{
   killTimers();
   m_bMyUpdate = true;
   startTimer( afterMilliSecs );
}

void DiffTextWindow::timerEvent(QTimerEvent*)
{
   killTimers();

   if ( m_bMyUpdate )
   {
      paintEvent( 0 );
      m_bMyUpdate = false;
   }

   if ( m_scrollDeltaX != 0 || m_scrollDeltaY != 0 )
   {
      selection.end( selection.lastLine + m_scrollDeltaY, selection.lastPos +  m_scrollDeltaX );
      emit scroll( m_scrollDeltaX, m_scrollDeltaY );
      killTimers();
      startTimer(50);
   }
}

void DiffTextWindow::resetSelection()
{
   selection.reset();
   update();
}

void DiffTextWindow::convertToLinePos( int x, int y, int& line, int& pos )
{
   const QFontMetrics& fm = fontMetrics();
   int fontHeight = fm.height();
   int fontWidth = fm.width('W');
   int xOffset = (leftInfoWidth-m_firstColumn)*fontWidth;
   int topLineYOffset = fontHeight + 3;

   int yOffset = topLineYOffset - m_firstLine * fontHeight;

   line = ( y - yOffset ) / fontHeight;
   pos  = ( x - xOffset ) / fontWidth;
}

int Selection::firstPosInLine(int l)
{
   assert( firstLine != -1 );

   int l1 = firstLine;
   int l2 = lastLine;
   int p1 = firstPos;
   int p2 = lastPos;
   if ( l1>l2 ){ swap(l1,l2); swap(p1,p2); }
   if ( l1==l2 && p1>p2 ){ swap(p1,p2); }

   if ( l==l1 )
      return p1;
   return 0;
}

int Selection::lastPosInLine(int l)
{
   assert( firstLine != -1 );

   int l1 = firstLine;
   int l2 = lastLine;
   int p1 = firstPos;
   int p2 = lastPos;
   if ( l1>l2 ){ swap(l1,l2); swap(p1,p2); }
   if ( l1==l2 && p1>p2 ){ swap(p1,p2); }

   if ( l==l2 )
      return p2;
   return INT_MAX;
}

bool Selection::within( int l, int p )
{
   if ( firstLine == -1 ) return false;
   int l1 = firstLine;
   int l2 = lastLine;
   int p1 = firstPos;
   int p2 = lastPos;
   if ( l1>l2 ){ swap(l1,l2); swap(p1,p2); }
   if ( l1==l2 && p1>p2 ){ swap(p1,p2); }
   if( l1 <= l && l <= l2 )
   {
      if ( l1==l2 )
         return p>=p1 && p<p2;
      if ( l==l1 )
         return p>=p1;
      if ( l==l2 )
         return p<p2;
      return true;
   }
   return false;
}

bool Selection::lineWithin( int l )
{
   if ( firstLine == -1 ) return false;
   int l1 = firstLine;
   int l2 = lastLine;

   if ( l1>l2 ){ swap(l1,l2); }

   return ( l1 <= l && l <= l2 );
}

void DiffTextWindow::writeLine(
   QPainter& p,
   const LineData* pld,
   const DiffList* pLineDiff1,
   const DiffList* pLineDiff2,
   int line,
   int whatChanged,
   int whatChanged2
   )
{
   const QFontMetrics& fm = fontMetrics();
   int fontHeight = fm.height();
   int fontAscent = fm.ascent();
   int fontDescent = fm.descent();
   int fontWidth = fm.width('W');
   int topLineYOffset = fontHeight + 3;

   int xOffset = (leftInfoWidth - m_firstColumn)*fontWidth;
   int yOffset = (line-m_firstLine) * fontHeight + topLineYOffset;

   QRect lineRect( 0, yOffset, width(), fontHeight );
   if ( ! m_invalidRect.intersects( lineRect ) )
      return;

//   QRect winRect = rect(); //p.window();
   if ( yOffset+fontHeight<0  ||  height() + fontHeight < yOffset )
      return;

   int changed = whatChanged;
   if ( pLineDiff1 != 0 ) changed |= 1;
   if ( pLineDiff2 != 0 ) changed |= 2;

   QColor c = m_pOptionDialog->m_fgColor;
   if ( changed == 2 ) {
      c = m_cDiff2;
   } else if ( changed == 1 ) {
      c = m_cDiff1;
   } else if ( changed == 3 ) {
      c = m_cDiffBoth;
   }


   if (pld!=0)
   {
      // First calculate the "changed" information for each character.
      int i=0;
      vector<UINT8> charChanged( pld->size );
      if ( pLineDiff1!=0 || pLineDiff2 != 0 )
      {
         Merger merger( pLineDiff1, pLineDiff2 );
         while( ! merger.isEndReached() &&  i<pld->size )
         {
            if ( i < pld->size )
            {
               charChanged[i] = merger.whatChanged();
               ++i;
            }
            merger.next();
         }
      }

      QCString s=" ";
      // Convert tabs
      int outPos = 0;
      for( i=0; i<pld->size; ++i )
      {
         int spaces = 1;

         if ( pld->pLine[i]=='\t' )
         {
            spaces = tabber( outPos, g_tabSize );
            s[0] = ' ';
         }
         else
         {
            s[0] = pld->pLine[i];
         }

         QColor c = m_pOptionDialog->m_fgColor;
         int cchanged = charChanged[i] | whatChanged;

         if ( cchanged == 2 ) {
            c = m_cDiff2;
         } else if ( cchanged == 1 ) {
            c = m_cDiff1;
         } else if ( cchanged == 3 ) {
            c = m_cDiffBoth;
         }

         QRect outRect( xOffset + fontWidth*outPos, yOffset, fontWidth*spaces, fontHeight );
         if ( m_invalidRect.intersects( outRect ) )
         {

            if( !selection.within( line, outPos ) )
            {
               if( c!=m_pOptionDialog->m_fgColor )
               {
                  QColor lightc = m_pOptionDialog->m_diffBgColor;
                  p.fillRect( xOffset + fontWidth*outPos, yOffset,
                           fontWidth*spaces, fontHeight, lightc );
               }

               p.setPen( c );
               if ( s[0]==' '  &&  c!=m_pOptionDialog->m_fgColor  &&  charChanged[i]!=0 )
               {
                  p.fillRect( xOffset + fontWidth*outPos, yOffset+fontAscent,
                              fontWidth*(spaces)-1, fontDescent+1, c );
               }
               else
               {
                  p.drawText( xOffset + fontWidth*outPos, yOffset + fontAscent, s );
               }
            }
            else
            {
               p.fillRect( xOffset + fontWidth*outPos, yOffset,
                           fontWidth*(spaces), fontHeight, colorGroup().highlight() );
               p.setPen( colorGroup().highlightedText() );
               p.drawText( xOffset + fontWidth*outPos, yOffset + fontAscent, s );

               selection.bSelectionContainsData = true;
            }
         }

         outPos += spaces;
      }

      if( selection.lineWithin( line ) && selection.lineWithin( line+1 ) )
      {
         p.fillRect( xOffset + fontWidth*outPos, yOffset,
                     width(), fontHeight, colorGroup().highlight() );
      }

   }

   p.fillRect( 0, yOffset, leftInfoWidth*fontWidth-1, fontHeight, m_pOptionDialog->m_bgColor );

   xOffset = 2*fontWidth;
   p.setPen( m_pOptionDialog->m_fgColor );
   if ( pld!=0 )
      p.drawLine( xOffset +1, yOffset, xOffset +1, yOffset+fontHeight-1 );
   if ( c!=m_pOptionDialog->m_fgColor && whatChanged2==0 && whatChanged==0 )
   {
      p.fillRect( 0, yOffset, xOffset-1, fontHeight, QBrush(c,Dense5Pattern) );
   }
   else
   {
      p.fillRect( 0, yOffset, xOffset-1, fontHeight, c==m_pOptionDialog->m_fgColor ? m_pOptionDialog->m_bgColor : c );
   }

   int fastSelectorLine2 = m_fastSelectorLine1+m_fastSelectorNofLines - 1;
   if (line>=m_fastSelectorLine1 && line<= fastSelectorLine2 )
   {
      p.drawLine( xOffset + fontWidth-1, yOffset, xOffset + fontWidth-1, yOffset+fontHeight-1 );
      if ( line == m_fastSelectorLine1 )
      {
         p.drawLine( xOffset + fontWidth-1, yOffset, xOffset + fontWidth+2, yOffset );
      }
      if ( line == fastSelectorLine2 )
      {
         p.drawLine( xOffset + fontWidth-1, yOffset+fontHeight-1, xOffset + fontWidth+2, yOffset+fontHeight-1 );
      }
   }
}

void DiffTextWindow::paintEvent( QPaintEvent* e )
{
   if (e!=0)
   {
      m_invalidRect |= e->rect();
   }

   if ( m_winIdx==1 )
   {
      m_cThis = m_pOptionDialog->m_colorA;
      m_cDiff1 = m_pOptionDialog->m_colorB;
      m_cDiff2 = m_pOptionDialog->m_colorC;
   }
   if ( m_winIdx==2 )
   {
      m_cThis = m_pOptionDialog->m_colorB;
      m_cDiff1 = m_pOptionDialog->m_colorC;
      m_cDiff2 = m_pOptionDialog->m_colorA;
   }
   if ( m_winIdx==3 )
   {
      m_cThis = m_pOptionDialog->m_colorC;
      m_cDiff1 = m_pOptionDialog->m_colorA;
      m_cDiff2 = m_pOptionDialog->m_colorB;
   }
   m_cDiffBoth = m_pOptionDialog->m_colorForConflict;  // Conflict color

   if (font() != m_pOptionDialog->m_font )
   {
      setFont(m_pOptionDialog->m_font);
   }

   bool bOldSelectionContainsData = selection.bSelectionContainsData;
   selection.bSelectionContainsData = false;

   int fontHeight = fontMetrics().height();
   int fontAscent = fontMetrics().ascent();
//   int fontDescent = fontMetrics().descent();
   int fontWidth = fontMetrics().width('W');

   int topLineYOffset = fontHeight + 3;
   int xOffset = leftInfoWidth * fontWidth;

   int firstLineToDraw = 0;
   int lastLineToDraw = (height() - topLineYOffset ) / fontHeight;
   if ( abs(m_oldFirstLine - m_firstLine)>=lastLineToDraw )
   {
      m_oldFirstLine = -1;
      m_invalidRect |= QRect( 0, topLineYOffset, width(), height() );
   }

   if ( m_oldFirstLine != -1 && m_oldFirstLine != m_firstLine )
   {
      int deltaY = fontHeight * ( m_oldFirstLine - m_firstLine );
      if ( deltaY > 0 )
      {  // Move down
         bitBlt( this, 0, deltaY + topLineYOffset /*dy*/, this, 0, topLineYOffset /*sy*/, width(), height()- (deltaY + topLineYOffset), CopyROP, true);
         lastLineToDraw = firstLineToDraw + ( m_oldFirstLine - m_firstLine);
         m_invalidRect |= QRect( 0, topLineYOffset, width(), deltaY );
      }
      else
      {  // Move up
         bitBlt( this, 0, topLineYOffset /*dy*/, this, 0, -deltaY+topLineYOffset /*sy*/, width(), height()-(-deltaY+topLineYOffset), CopyROP, true);
         firstLineToDraw = lastLineToDraw + ( m_oldFirstLine - m_firstLine);
         m_invalidRect |= QRect( 0, height()+deltaY, width(), -deltaY );
      }
   }

   if ( m_oldFirstColumn != -1  && m_oldFirstColumn != m_firstColumn )
   {
      int deltaX = fontWidth * ( m_oldFirstColumn - m_firstColumn );
      if ( deltaX > 0 )
      {  // Move right, scroll left
         bitBlt( this, deltaX+xOffset, topLineYOffset, this, xOffset, topLineYOffset, width(), height(), CopyROP, true);
         m_invalidRect |=
            QRect( xOffset, topLineYOffset, deltaX, height() - topLineYOffset );
      }
      else
      {  // Move left, scroll right
         bitBlt( this, xOffset, topLineYOffset, this, xOffset-deltaX, topLineYOffset, width()-(xOffset-deltaX), height()-topLineYOffset, CopyROP, true);
         m_invalidRect |=
            QRect( width() + deltaX, topLineYOffset, -deltaX, height() - topLineYOffset );
      }
   }

   if ( selection.oldLastLine != -1 )
   {
      int lastLine;
      int firstLine;
      if ( selection.oldFirstLine != -1 )
      {
         firstLine = min3( selection.oldFirstLine, selection.lastLine, selection.oldLastLine );
         lastLine = max3( selection.oldFirstLine, selection.lastLine, selection.oldLastLine );
      }
      else
      {
         firstLine = min( selection.lastLine, selection.oldLastLine );
         lastLine = max( selection.lastLine, selection.oldLastLine );
      }
      int y1 = topLineYOffset + ( firstLine - m_firstLine  ) * fontHeight;
      int y2 = topLineYOffset + ( lastLine  - m_firstLine +1 ) * fontHeight;

      if ( y1<height() && y2>topLineYOffset )
      {
         m_invalidRect |= QRect( 0, y1, width(), y2-y1 );
      }
   }

   if ( m_invalidRect.isEmpty() )
      return;

   QPainter painter(this);
   //QPainter& p=painter;
   QPixmap pixmap( m_invalidRect.size() );
   QPainter p( &pixmap );
   p.translate( -m_invalidRect.x(), -m_invalidRect.y() );

   p.fillRect( m_invalidRect, m_pOptionDialog->m_bgColor );

   firstLineToDraw += m_firstLine;
   lastLineToDraw += m_firstLine;

   p.setFont( font() );

   p.setPen( m_cThis );
   if( m_invalidRect.intersects( QRect(0,0,width(), topLineYOffset ) )
      || m_firstLine != m_oldFirstLine )
   {
      Diff3LineList::const_iterator i;
      int l=0;
      for ( i=m_pDiff3LineList->begin();i!=m_pDiff3LineList->end() && l!=m_firstLine; ++i,++l )
      {
      }
      l=-1;
      if(i!=m_pDiff3LineList->end())
      {
         l=-1;
         for ( ;i!=m_pDiff3LineList->end(); ++i )
         {
            if      ( m_winIdx==1 ) l=(*i).lineA;
            else if ( m_winIdx==2 ) l=(*i).lineB;
            else if ( m_winIdx==3 ) l=(*i).lineC;
            else assert(false);
            if (l!=-1) break;
         }
      }

      const char* winId = (   m_winIdx==1 ? (m_bTriple?"A (Base)":"A") :
                            ( m_winIdx==2 ? "B" : "C" ) );

      QString s;
      if ( l!=-1 )
         s.sprintf(" %s : %s : Topline %d", winId, m_pFilename, l+1 );
      else
         s.sprintf(" %s : %s: End", winId, m_pFilename );


      if (hasFocus())
      {
         painter.fillRect( 0, 0, width(), topLineYOffset, m_cThis );
         painter.setPen( m_pOptionDialog->m_bgColor );
         painter.drawText( 0, fontAscent+1, s );
      }
      else
      {
         painter.fillRect( 0, 0, width(), topLineYOffset, m_pOptionDialog->m_bgColor );
         painter.setPen( m_cThis );
         painter.drawLine( 0, fontHeight + 2, width(), fontHeight + 2 );
         painter.drawText( 0, fontAscent+1, s );
      }
   }

   int line=0;

   Diff3LineList::const_iterator it;
   for( it=m_pDiff3LineList->begin(); it!=m_pDiff3LineList->end(); ++it )
   {
      DiffList* pFineDiff1;
      DiffList* pFineDiff2;
      int changed=0;
      int changed2=0;
      int lineIdx;

      getLineInfo( *it, lineIdx, pFineDiff1, pFineDiff2, changed, changed2 );

      writeLine(
         p,                         // QPainter
         lineIdx == -1 ? 0 : &m_pLineData[lineIdx],     // Text in this line
         pFineDiff1,
         pFineDiff2,
         line,                      // Line on the screen
         changed,
         changed2
         );

      ++line;
   }
   // p.drawLine( m_invalidRect.x(), m_invalidRect.y(), m_invalidRect.right(), m_invalidRect.bottom() );
   p.end();

   painter.setClipRect ( 0, topLineYOffset, width(), height()-topLineYOffset );

   painter.drawPixmap( m_invalidRect.x(), m_invalidRect.y(), pixmap );

   m_oldFirstLine = m_firstLine;
   m_oldFirstColumn = m_firstColumn;
   m_invalidRect = QRect(0,0,0,0);
   selection.oldLastLine = -1;
   if ( selection.oldFirstLine !=-1 )
      selection.oldFirstLine = -1;
   if( !bOldSelectionContainsData  &&  selection.bSelectionContainsData )
      emit newSelection();

}

QCString DiffTextWindow::getString( int line )
{
   int line1=0;
   Diff3LineList::const_iterator it;
   for( it=m_pDiff3LineList->begin(); it!=m_pDiff3LineList->end(); ++it )
   {

      if (line1==line)
      {
         DiffList* pFineDiff1;
         DiffList* pFineDiff2;
         int changed=0;
         int changed2=0;
         int lineIdx;
         getLineInfo( *it, lineIdx, pFineDiff1, pFineDiff2, changed, changed2 );

         if (lineIdx==-1) return QCString();
         else
         {
            LineData* ld = &m_pLineData[lineIdx];
            return QCString( ld->pLine, ld->size + 1 );
         }
      }
      ++line1;
   }
   return QCString();
}


void DiffTextWindow::getLineInfo(
   const Diff3Line& d,
   int& lineIdx,
   DiffList*& pFineDiff1, DiffList*& pFineDiff2,   // return values
   int& changed, int& changed2
   )
{
   changed=0;
   changed2=0;
   if      ( m_winIdx == 1 ) {
      lineIdx=d.lineA;
      pFineDiff1=d.pFineAB;
      pFineDiff2=d.pFineCA;
      changed |= ((d.lineB==-1)!=(lineIdx==-1) ? 1 : 0) +
                 ((d.lineC==-1)!=(lineIdx==-1) && m_bTriple ? 2 : 0);
      changed2 |= ( d.bAEqB ? 0 : 1 ) + (d.bAEqC || !m_bTriple ? 0 : 2);
   }
   else if ( m_winIdx == 2 ) {
      lineIdx=d.lineB;
      pFineDiff1=d.pFineBC;
      pFineDiff2=d.pFineAB;
      changed |= ((d.lineC==-1)!=(lineIdx==-1) && m_bTriple ? 1 : 0) +
                 ((d.lineA==-1)!=(lineIdx==-1) ? 2 : 0);
      changed2 |= ( d.bBEqC || !m_bTriple ? 0 : 1 ) + (d.bAEqB ? 0 : 2);
   }
   else if ( m_winIdx == 3 ) {
      lineIdx=d.lineC;
      pFineDiff1=d.pFineCA;
      pFineDiff2=d.pFineBC;
      changed |= ((d.lineA==-1)!=(lineIdx==-1) ? 1 : 0) +
                 ((d.lineB==-1)!=(lineIdx==-1) ? 2 : 0);
      changed2 |= ( d.bAEqC ? 0 : 1 ) + (d.bBEqC ? 0 : 2);
   }
   else assert(false);
}



void DiffTextWindow::resizeEvent( QResizeEvent* e )
{
   QSize s = e->size();
   QFontMetrics fm = fontMetrics();
   m_visibleLines = s.height()/fm.height()-2;
   m_visibleColumns = s.width()/fm.width('W')-leftInfoWidth;
   emit resizeSignal( m_visibleColumns, m_visibleLines );
   QWidget::resizeEvent(e);
}

QString DiffTextWindow::getSelection()
{
   QString selectionString;

   int line=0;
   int lineIdx=0;

   Diff3LineList::const_iterator it;
   for( it=m_pDiff3LineList->begin(); it!=m_pDiff3LineList->end(); ++it )
   {
      const Diff3Line& d = *it;
      if      ( m_winIdx == 1 ) {    lineIdx=d.lineA;     }
      else if ( m_winIdx == 2 ) {    lineIdx=d.lineB;     }
      else if ( m_winIdx == 3 ) {    lineIdx=d.lineC;     }
      else assert(false);

      if( lineIdx != -1 )
      {
         const char* pLine = m_pLineData[lineIdx].pLine;
         int size = m_pLineData[lineIdx].size;

         // Consider tabs
         int outPos = 0;
         for( int i=0; i<size; ++i )
         {
            int spaces = 1;
            if ( pLine[i]=='\t' )
            {
               spaces = tabber( outPos, g_tabSize );
            }

            if( selection.within( line, outPos ) )
            {
              selectionString += pLine[i];
            }

            outPos += spaces;
         }

         if( selection.within( line, outPos ) )
         {
            #ifdef WIN32
            selectionString += '\r';
            #endif
            selectionString += '\n';
         }
      }

      ++line;
   }

   return selectionString;
}
