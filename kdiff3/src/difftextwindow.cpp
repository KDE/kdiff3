/***************************************************************************
                          difftextwindow.cpp  -  description
                             -------------------
    begin                : Mon Apr 8 2002
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

#include <iostream>
#include "diff.h"
#include "merger.h"
#include <qpainter.h>
#include <assert.h>
#include <qpixmap.h>
#include <qstatusbar.h>
#include <qapplication.h>
#include <qtooltip.h>
#include <qfont.h>
#include <qstringlist.h>
#include <optiondialog.h>
#include <math.h>
#include <qdragobject.h>
#include <klocale.h>

#undef leftInfoWidth
#define leftInfoWidth (4+m_lineNumberWidth)   // Nr of information columns on left side


DiffTextWindow::DiffTextWindow(
   QWidget* pParent,
   QStatusBar* pStatusBar,
   OptionDialog* pOptionDialog
   )
: QWidget(pParent, 0, WRepaintNoErase)
{
   setFocusPolicy( QWidget::ClickFocus );
   setAcceptDrops( true );

   m_pOptionDialog = pOptionDialog;
   init( 0, 0, 0, 0, 0, false );

   setBackgroundMode( PaletteBase );
   setMinimumSize(QSize(20,20));

   m_pStatusBar = pStatusBar;
   m_bPaintingAllowed = true;
   m_bWordWrap = false;

   setFont(m_pOptionDialog->m_font);
}


void DiffTextWindow::init(
   const QString& filename,
   const LineData* pLineData,
   int size,
   const Diff3LineVector* pDiff3LineVector,
   int winIdx,
   bool bTriple
   )
{
   m_filename = filename;
   m_pLineData = pLineData;
   m_size = size;
   m_pDiff3LineVector = pDiff3LineVector;
   m_diff3WrapLineVector.clear();
   m_winIdx = winIdx;

   m_firstLine = 0;
   m_oldFirstLine = -1;
   m_firstColumn = 0;
   m_oldFirstColumn = -1;
   m_bTriple = bTriple;
   m_scrollDeltaX=0;
   m_scrollDeltaY=0;
   m_bMyUpdate = false;
   m_fastSelectorLine1 = 0;
   m_fastSelectorNofLines = 0;
   m_lineNumberWidth = 0;
   selection.reset();
   selection.oldFirstLine = -1; // reset is not enough here.
   selection.oldLastLine = -1;
   selection.lastLine = -1;

   QToolTip::remove(this);
   QToolTip::add( this, QRect( 0,0, 1024, fontMetrics().height() ), m_filename );
   update();
}

void DiffTextWindow::setPaintingAllowed( bool bAllowPainting )
{
   if (m_bPaintingAllowed != bAllowPainting)
   {
      m_bPaintingAllowed = bAllowPainting;
      if ( m_bPaintingAllowed ) update();
   }
}

void DiffTextWindow::dragEnterEvent( QDragEnterEvent* e )
{
   e->accept( QUriDrag::canDecode(e) || QTextDrag::canDecode(e) );
   // Note that the corresponding drop is handled in KDiff3App::eventFilter().
}

void DiffTextWindow::setFirstLine(int firstLine)
{
   int fontHeight = fontMetrics().height();
   int topLineYOffset = fontHeight + 3;   
   
   int newFirstLine = max2(0,firstLine);

   int deltaY = fontHeight * ( m_firstLine - newFirstLine );
   
   m_firstLine = newFirstLine;
   
   if ( m_scrollDeltaY > 0 )
   {
      update( 0, height()-fontHeight*3, width(), fontHeight*3 );
   }
   else if ( m_scrollDeltaY<0 )
   {
      update( 0, 0, width(), topLineYOffset+fontHeight*2 );
   }
   else
   {
      update( 0, 0, width(), topLineYOffset );
   }
   
   QRect r( 0, topLineYOffset, width(), height()-topLineYOffset );   
   QWidget::scroll( 0, deltaY, r );
}

void DiffTextWindow::setFirstColumn(int firstCol)
{
   int fontHeight = fontMetrics().height();
   int fontWidth = fontMetrics().width('W');
   int topLineYOffset = fontHeight + 3;
   int xOffset = leftInfoWidth * fontWidth;
   
   int newFirstColumn = max2(0,firstCol);

   int deltaX = fontWidth * ( m_firstColumn - newFirstColumn );
   
   m_firstColumn = newFirstColumn;

   QRect r( xOffset, topLineYOffset, width()-xOffset, height()-topLineYOffset );
   
   QWidget::scroll( deltaX, 0, r );
}

int DiffTextWindow::getNofColumns()
{
   if (m_bWordWrap)
   {
      return getNofVisibleColumns();
   }
   else
   {
      int nofColumns = 0;
      for( int i = 0; i< m_size; ++i )
      {
         if ( m_pLineData[i].width() > nofColumns )
            nofColumns = m_pLineData[i].width();
      }
      return nofColumns;
   }
}

int DiffTextWindow::getNofLines()
{
   return m_bWordWrap ? m_diff3WrapLineVector.size() : 
                        m_pDiff3LineVector->size();
}


int DiffTextWindow::convertLineToDiff3LineIdx( int line )
{
   if ( m_bWordWrap && m_diff3WrapLineVector.size()>0 )
      return m_diff3WrapLineVector[ min2( line, (int)m_diff3WrapLineVector.size()-1 ) ].diff3LineIndex;
   else
      return line;
}

int DiffTextWindow::convertDiff3LineIdxToLine( int d3lIdx )
{
   if ( m_bWordWrap && m_pDiff3LineVector!=0 && m_pDiff3LineVector->size()>0 )
      return (*m_pDiff3LineVector)[ min2( d3lIdx, (int)m_pDiff3LineVector->size()-1 ) ]->sumLinesNeededForDisplay;
   else
      return d3lIdx;
}

/** Returns a line number where the linerange [line, line+nofLines] can
    be displayed best. If it fits into the currently visible range then
    the returned value is the current firstLine.
*/
int getBestFirstLine( int line, int nofLines, int firstLine, int visibleLines )
{
   int newFirstLine = firstLine;
   if ( line < firstLine  ||  line + nofLines + 2 > firstLine + visibleLines )
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
   if ( isVisible() )
   {
      int newFirstLine = getBestFirstLine( 
         convertDiff3LineIdxToLine(m_fastSelectorLine1), 
         convertDiff3LineIdxToLine(m_fastSelectorLine1+m_fastSelectorNofLines)-convertDiff3LineIdxToLine(m_fastSelectorLine1), 
         m_firstLine, 
         getNofVisibleLines() 
         );
      if ( newFirstLine != m_firstLine )
      {
         scroll( 0, newFirstLine - m_firstLine );
      }

      update();
   }
}


void DiffTextWindow::showStatusLine(int line )
{
   int d3lIdx = convertLineToDiff3LineIdx( line );
   int l=0;
   const Diff3Line* pD3l = (*m_pDiff3LineVector)[d3lIdx];
   if(d3lIdx >= 0 && d3lIdx<(int)m_pDiff3LineVector->size() && pD3l != 0 )
   {
      if      ( m_winIdx==1 ) l = pD3l->lineA;
      else if ( m_winIdx==2 ) l = pD3l->lineB;
      else if ( m_winIdx==3 ) l = pD3l->lineC;
      else assert(false);

      QString s;
      if ( l!=-1 )
         s.sprintf("File %s: Line %d", m_filename.ascii(), l+1 );
      else
         s.sprintf("File %s: Line not available", m_filename.ascii() );
      if (m_pStatusBar!=0) m_pStatusBar->message(s);
   }
}

void DiffTextWindow::focusInEvent(QFocusEvent* e)
{
   emit gotFocus();
   QWidget::focusInEvent(e);
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
         emit setFastSelectorLine( convertLineToDiff3LineIdx(line) );
         selection.firstLine = -1;     // Disable current selection
      }
      else
      {  // Selection
         resetSelection();
         selection.start( line, pos );
         selection.end( line, pos );

         showStatusLine( line );
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
   int pos = convertToPosInText( p, size, max2( 0, posOnScreen ) );
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
      QCString s;
      if ( m_bWordWrap )
      {
         const Diff3WrapLine& d = m_diff3WrapLineVector[line];
         s = getString( d.diff3LineIndex ).mid( d.wrapLineOffset, d.wrapLineLength );
      }
      else
      {
         s = getString( line );
      }
      
      if ( ! s.isEmpty() )
      {
         int pos1, pos2;
         calcTokenPos( s, s.length(), pos, pos1, pos2 );

         resetSelection();
         selection.start( line, convertToPosOnScreen( s, pos1 ) );
         selection.end( line, convertToPosOnScreen( s, pos2 ) );
         update();
         // emit selectionEnd() happens in the mouseReleaseEvent.
         showStatusLine( line );
      }
   }
}

void DiffTextWindow::mouseReleaseEvent ( QMouseEvent * /*e*/ )
{
   //if ( e->button() == LeftButton )
   {
      killTimers();
      if (selection.firstLine != -1 )
      {
         emit selectionEnd();
      }
   }
   m_scrollDeltaX=0;
   m_scrollDeltaY=0;
}

void DiffTextWindow::mouseMoveEvent ( QMouseEvent * e )
{
   int line;
   int pos;
   convertToLinePos( e->x(), e->y(), line, pos );
   if (selection.firstLine != -1 )
   {
      selection.end( line, pos );

      showStatusLine( line );

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
      if ( deltaX != 0 && m_scrollDeltaX!=deltaX || deltaY!= 0 && m_scrollDeltaY!=deltaY )
      {
         m_scrollDeltaX = deltaX;
         m_scrollDeltaY = deltaY;
         emit scroll( deltaX, deltaY );
         startTimer(50);
      }
      else
      {
         m_scrollDeltaX = deltaX;
         m_scrollDeltaY = deltaY;
         myUpdate(0);
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
   if ( l1>l2 ){ std::swap(l1,l2); std::swap(p1,p2); }
   if ( l1==l2 && p1>p2 ){ std::swap(p1,p2); }

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

   if ( l1>l2 ){ std::swap(l1,l2); std::swap(p1,p2); }
   if ( l1==l2 && p1>p2 ){ std::swap(p1,p2); }

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
   if ( l1>l2 ){ std::swap(l1,l2); std::swap(p1,p2); }
   if ( l1==l2 && p1>p2 ){ std::swap(p1,p2); }
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

   if ( l1>l2 ){ std::swap(l1,l2); }

   return ( l1 <= l && l <= l2 );
}

void DiffTextWindow::writeLine(
   QPainter& p,
   const LineData* pld,
   const DiffList* pLineDiff1,
   const DiffList* pLineDiff2,
   int line,
   int whatChanged,
   int whatChanged2,
   int srcLineNr,
   int wrapLineOffset,
   int wrapLineLength,
   bool bWrapLine
   )
{
   QFont normalFont = font();
   QFont diffFont = normalFont;
   diffFont.setItalic( m_pOptionDialog->m_bItalicForDeltas );
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

   int fastSelectorLine1 = convertDiff3LineIdxToLine(m_fastSelectorLine1);
   int fastSelectorLine2 = convertDiff3LineIdxToLine(m_fastSelectorLine1+m_fastSelectorNofLines)-1;
      
   bool bFastSelectionRange = (line>=fastSelectorLine1 && line<= fastSelectorLine2 );
   QColor bgColor = m_pOptionDialog->m_bgColor;
   QColor diffBgColor = m_pOptionDialog->m_diffBgColor;

   if ( bFastSelectionRange )
   {
      bgColor = m_pOptionDialog->m_currentRangeBgColor;
      diffBgColor = m_pOptionDialog->m_currentRangeDiffBgColor;
   }

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

   p.fillRect( leftInfoWidth*fontWidth, yOffset, width(), fontHeight, bgColor );

   if (pld!=0)
   {
      // First calculate the "changed" information for each character.
      int i=0;
      std::vector<UINT8> charChanged( pld->size );
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
      
      int lineLength = m_bWordWrap ? wrapLineOffset+wrapLineLength : pld->size;
      for( i=wrapLineOffset; i<lineLength; ++i )
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

         if ( c!=m_pOptionDialog->m_fgColor && whatChanged2==0 && !m_pOptionDialog->m_bShowWhiteSpace )
         {
            // The user doesn't want to see highlighted white space.
            c = m_pOptionDialog->m_fgColor;
         }

         QRect outRect( xOffset + fontWidth*outPos, yOffset, fontWidth*spaces, fontHeight );
         if ( m_invalidRect.intersects( outRect ) )
         {
            if( !selection.within( line, outPos ) )
            {

               if( c!=m_pOptionDialog->m_fgColor )
               {
                  QColor lightc = diffBgColor;
                  p.fillRect( xOffset + fontWidth*outPos, yOffset,
                           fontWidth*spaces, fontHeight, lightc );
                  p.setFont(diffFont);
               }

               p.setPen( c );
               if ( s[0]==' '  &&  c!=m_pOptionDialog->m_fgColor  &&  charChanged[i]!=0 )
               {
                  if ( m_pOptionDialog->m_bShowWhiteSpaceCharacters && m_pOptionDialog->m_bShowWhiteSpace)
                  {
                     p.fillRect( xOffset + fontWidth*outPos, yOffset+fontAscent,
                                 fontWidth*spaces-1, fontDescent+1, c );
                  }
               }
               else
               {
                  p.drawText( xOffset + fontWidth*outPos, yOffset + fontAscent, decodeString(s,m_pOptionDialog) );
               }
               p.setFont(normalFont);
            }
            else
            {
               p.fillRect( xOffset + fontWidth*outPos, yOffset,
                           fontWidth*(spaces), fontHeight, colorGroup().highlight() );

               p.setPen( colorGroup().highlightedText() );
               p.drawText( xOffset + fontWidth*outPos, yOffset + fontAscent, decodeString(s,m_pOptionDialog) );

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

   p.fillRect( 0, yOffset, leftInfoWidth*fontWidth, fontHeight, m_pOptionDialog->m_bgColor );

   xOffset = (m_lineNumberWidth+2)*fontWidth;
   int xLeft =   m_lineNumberWidth*fontWidth;
   p.setPen( m_pOptionDialog->m_fgColor );
   if ( pld!=0 )
   {
      if ( m_pOptionDialog->m_bShowLineNumbers && !bWrapLine )
      {
         QString num;
         num.sprintf( "%0*d", m_lineNumberWidth, srcLineNr);
         p.drawText( 0, yOffset + fontAscent, num );
         //p.drawLine( xLeft -1, yOffset, xLeft -1, yOffset+fontHeight-1 );
      }
      if ( !bWrapLine || wrapLineLength>0 )
      {
         p.setPen( QPen( m_pOptionDialog->m_fgColor, 0, bWrapLine ? DotLine : SolidLine) );
         p.drawLine( xOffset +1, yOffset, xOffset +1, yOffset+fontHeight-1 );
         p.setPen( QPen( m_pOptionDialog->m_fgColor, 0, SolidLine) );
      }
   }
   if ( c!=m_pOptionDialog->m_fgColor && whatChanged2==0 )//&& whatChanged==0 )
   {
      if ( m_pOptionDialog->m_bShowWhiteSpace )
      {
         p.setBrushOrigin(0,0);
         p.fillRect( xLeft, yOffset, fontWidth*2-1, fontHeight, QBrush(c,Dense5Pattern) );
      }
   }
   else
   {
      p.fillRect( xLeft, yOffset, fontWidth*2-1, fontHeight, c==m_pOptionDialog->m_fgColor ? bgColor : c );
   }

   if ( bFastSelectionRange )
   {
      p.fillRect( xOffset + fontWidth-1, yOffset, 3, fontHeight, m_pOptionDialog->m_fgColor );
   }
}

void DiffTextWindow::paintEvent( QPaintEvent* e )
{
   if ( m_pDiff3LineVector==0 || ! m_bPaintingAllowed ||
        ( m_diff3WrapLineVector.empty() && m_bWordWrap ) ) 
      return;

   m_lineNumberWidth =  m_pOptionDialog->m_bShowLineNumbers ? (int)log10((double)m_size)+1 : 0;

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

   bool bOldSelectionContainsData = selection.bSelectionContainsData;
   selection.bSelectionContainsData = false;

   int fontHeight = fontMetrics().height();
   int fontAscent = fontMetrics().ascent();
//   int fontDescent = fontMetrics().descent();

   int topLineYOffset = fontHeight + 3;

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
         firstLine = min2( selection.lastLine, selection.oldLastLine );
         lastLine = max2( selection.lastLine, selection.oldLastLine );
      }
      int y1 = topLineYOffset + ( firstLine - m_firstLine ) * fontHeight;
      int y2 = min2( height(),
                     topLineYOffset + ( lastLine  - m_firstLine + 1 ) * fontHeight );

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

   p.setFont( font() );

   p.setPen( m_cThis );
   if( m_invalidRect.intersects( QRect(0,0,width(), topLineYOffset ) )
      || m_firstLine != m_oldFirstLine )
   {
      int l=-1;
      for ( int i = convertLineToDiff3LineIdx(m_firstLine); i<(int)m_pDiff3LineVector->size(); ++i )
      {
         const Diff3Line* d3l = (*m_pDiff3LineVector)[i];
         if      ( m_winIdx==1 ) l=d3l->lineA;
         else if ( m_winIdx==2 ) l=d3l->lineB;
         else if ( m_winIdx==3 ) l=d3l->lineC;
         else assert(false);
         if (l!=-1) break;
      }

      const char* winId = (   m_winIdx==1 ? (m_bTriple?"A (Base)":"A") :
                            ( m_winIdx==2 ? "B" : "C" ) );

      QString s = QString(" ")+ winId + " : " + m_filename + " : ";
      if ( l!=-1 )
         s += i18n("Top line %1").arg( l+1 );
      else
         s += i18n("End");

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

   int lastVisibleLine = min2( m_firstLine + getNofVisibleLines()+2, 
      (int)(m_bWordWrap ? m_diff3WrapLineVector.size() : m_pDiff3LineVector->size()) );

   for ( int line = m_firstLine; line<lastVisibleLine; ++line )
   {
      int wrapLineOffset=0;
      int wrapLineLength=0;
      const Diff3Line* d3l =0;
      bool bWrapLine = false;
      if (m_bWordWrap)
      {
         Diff3WrapLine& d3wl = m_diff3WrapLineVector[line];
         wrapLineOffset = d3wl.wrapLineOffset;
         wrapLineLength = d3wl.wrapLineLength;
         d3l = d3wl.pD3L;
         bWrapLine = line > 0 && m_diff3WrapLineVector[line-1].pD3L == d3l;
      }
      else
      {
         d3l = (*m_pDiff3LineVector)[line];
      }
      DiffList* pFineDiff1;
      DiffList* pFineDiff2;
      int changed=0;
      int changed2=0;
      int lineIdx;

      getLineInfo( *d3l, lineIdx, pFineDiff1, pFineDiff2, changed, changed2 );

      writeLine(
         p,                         // QPainter
         lineIdx == -1 ? 0 : &m_pLineData[lineIdx],     // Text in this line
         pFineDiff1,
         pFineDiff2,
         line,                      // Line on the screen
         changed,
         changed2,
         lineIdx+1,
         wrapLineOffset,
         wrapLineLength,
         bWrapLine
         );
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

QCString DiffTextWindow::getString( int d3lIdx )
{
   const Diff3Line* d3l = (*m_pDiff3LineVector)[d3lIdx];
   DiffList* pFineDiff1;
   DiffList* pFineDiff2;
   int changed=0;
   int changed2=0;
   int lineIdx;
   getLineInfo( *d3l, lineIdx, pFineDiff1, pFineDiff2, changed, changed2 );

   if (lineIdx==-1) return QCString();
   else
   {
      const LineData* ld = &m_pLineData[lineIdx];
      return QCString( ld->pLine, ld->size + 1 );
   }
   return QCString();
}

QCString DiffTextWindow::getLineString( int line )
{
   if ( m_bWordWrap )
   {
      int d3LIdx = convertLineToDiff3LineIdx(line);
      return getString( d3LIdx ).mid( m_diff3WrapLineVector[line].wrapLineOffset, m_diff3WrapLineVector[line].wrapLineLength );
   }
   else
   {
      return getString( line );
   }
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
   bool bAEqB = d.bAEqB || ( d.bWhiteLineA && d.bWhiteLineB );
   bool bAEqC = d.bAEqC || ( d.bWhiteLineA && d.bWhiteLineC );
   bool bBEqC = d.bBEqC || ( d.bWhiteLineB && d.bWhiteLineC );
   if      ( m_winIdx == 1 ) {
      lineIdx=d.lineA;
      pFineDiff1=d.pFineAB;
      pFineDiff2=d.pFineCA;
      changed |= ((d.lineB==-1)!=(lineIdx==-1) ? 1 : 0) +
                 ((d.lineC==-1)!=(lineIdx==-1) && m_bTriple ? 2 : 0);
      changed2 |= ( bAEqB ? 0 : 1 ) + (bAEqC || !m_bTriple ? 0 : 2);
   }
   else if ( m_winIdx == 2 ) {
      lineIdx=d.lineB;
      pFineDiff1=d.pFineBC;
      pFineDiff2=d.pFineAB;
      changed |= ((d.lineC==-1)!=(lineIdx==-1) && m_bTriple ? 1 : 0) +
                 ((d.lineA==-1)!=(lineIdx==-1) ? 2 : 0);
      changed2 |= ( bBEqC || !m_bTriple ? 0 : 1 ) + (bAEqB ? 0 : 2);
   }
   else if ( m_winIdx == 3 ) {
      lineIdx=d.lineC;
      pFineDiff1=d.pFineCA;
      pFineDiff2=d.pFineBC;
      changed |= ((d.lineA==-1)!=(lineIdx==-1) ? 1 : 0) +
                 ((d.lineB==-1)!=(lineIdx==-1) ? 2 : 0);
      changed2 |= ( bAEqC ? 0 : 1 ) + (bBEqC ? 0 : 2);
   }
   else assert(false);
}



void DiffTextWindow::resizeEvent( QResizeEvent* e )
{
   QSize s = e->size();
   QFontMetrics fm = fontMetrics();
   int visibleLines = s.height()/fm.height()-2;
   int visibleColumns = s.width()/fm.width('W')-leftInfoWidth;
   emit resizeSignal( visibleColumns, visibleLines );
   QWidget::resizeEvent(e);
}

int DiffTextWindow::getNofVisibleLines()
{
   QFontMetrics fm = fontMetrics();
   int fmh = fm.height();
   int h = height();
   return h/fmh -2;//height()/fm.height()-2;
}

int DiffTextWindow::getNofVisibleColumns()
{
   QFontMetrics fm = fontMetrics();
   return width()/fm.width('W')-leftInfoWidth;
}

QString DiffTextWindow::getSelection()
{
   QString selectionString;

   int line=0;
   int lineIdx=0;

   int it;
   int vectorSize = m_bWordWrap ? m_diff3WrapLineVector.size() : m_pDiff3LineVector->size();
   for( it=0; it<vectorSize; ++it )
   {
      const Diff3Line* d = m_bWordWrap ? m_diff3WrapLineVector[it].pD3L : (*m_pDiff3LineVector)[it];
      if      ( m_winIdx == 1 ) {    lineIdx=d->lineA;     }
      else if ( m_winIdx == 2 ) {    lineIdx=d->lineB;     }
      else if ( m_winIdx == 3 ) {    lineIdx=d->lineC;     }
      else assert(false);

      if( lineIdx != -1 )
      {
         const char* pLine = m_pLineData[lineIdx].pLine;
         int size = m_pLineData[lineIdx].size;
         
         if ( m_bWordWrap )
         {
            pLine += m_diff3WrapLineVector[it].wrapLineOffset;
            size = m_diff3WrapLineVector[it].wrapLineLength;
         }

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
               char buf[2];
               buf[0] = pLine[i];
               buf[1] = '\0';
               selectionString += decodeString( buf, m_pOptionDialog );
            }

            outPos += spaces;
         }

         

         if( selection.within( line, outPos ) &&
            !( m_bWordWrap && it+1<vectorSize && d == m_diff3WrapLineVector[it+1].pD3L ) 
           )
         {
            #ifdef _WIN32
            selectionString += '\r';
            #endif
            selectionString += '\n';
         }
      }

      ++line;
   }

   return selectionString;
}

bool DiffTextWindow::findString( const QCString& s, int& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive )
{
   int it = d3vLine;
   int endIt = bDirDown ? (int)m_pDiff3LineVector->size() : -1;
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

void DiffTextWindow::convertD3LCoordsToLineCoords( int d3LIdx, int d3LPos, int& line, int& pos )
{
   if( m_bWordWrap )
   {
      int wrapPos = d3LPos;
      int wrapLine = convertDiff3LineIdxToLine(d3LIdx);
      while ( wrapPos > m_diff3WrapLineVector[wrapLine].wrapLineLength )
      {
         wrapPos -= m_diff3WrapLineVector[wrapLine].wrapLineLength;
         ++wrapLine;
      }
      pos = wrapPos;
      line = wrapLine;
   }
   else
   {
      pos = d3LPos;
      line = d3LIdx;
   }
}

void DiffTextWindow::convertLineCoordsToD3LCoords( int line, int pos, int& d3LIdx, int& d3LPos )
{
   if( m_bWordWrap )
   {
      d3LPos = pos;
      d3LIdx = convertLineToDiff3LineIdx( line );
      int wrapLine = convertDiff3LineIdxToLine(d3LIdx); // First wrap line belonging to this d3LIdx
      while ( wrapLine < line )
      {
         d3LPos += m_diff3WrapLineVector[wrapLine].wrapLineLength;
         ++wrapLine;
      }
   }
   else
   {
      d3LPos = pos;
      d3LIdx = line;
   }
}


void DiffTextWindow::setSelection( int firstLine, int startPos, int lastLine, int endPos, int& l, int& p )
{
   selection.reset();
   if ( m_bWordWrap && m_pDiff3LineVector!=0 )
   {      
      QString s1 = getString(firstLine);
      int firstWrapLine = convertDiff3LineIdxToLine(firstLine);
      int wrapStartPos = startPos;
      while ( wrapStartPos > m_diff3WrapLineVector[firstWrapLine].wrapLineLength )
      {
         wrapStartPos -= m_diff3WrapLineVector[firstWrapLine].wrapLineLength;
         s1 = s1.mid(m_diff3WrapLineVector[firstWrapLine].wrapLineLength);
         ++firstWrapLine;
      }

      QString s2 = getString(lastLine);
      int lastWrapLine = convertDiff3LineIdxToLine(lastLine);
      int wrapEndPos = endPos;
      while ( wrapEndPos > m_diff3WrapLineVector[lastWrapLine].wrapLineLength )
      {
         wrapEndPos -= m_diff3WrapLineVector[lastWrapLine].wrapLineLength;
         s2 = s2.mid(m_diff3WrapLineVector[lastWrapLine].wrapLineLength);
         ++lastWrapLine;
      }

      selection.start( firstWrapLine, convertToPosOnScreen( s1, wrapStartPos ) );
      selection.end( lastWrapLine, convertToPosOnScreen( s2, wrapEndPos ) );
      l=firstWrapLine;
      p=wrapStartPos;
   }
   else
   {
      selection.start( firstLine, convertToPosOnScreen( getString(firstLine), startPos ) );
      selection.end( lastLine, convertToPosOnScreen( getString(lastLine), endPos ) );
      l=firstLine;
      p=startPos;
   }
   update();
}


// Returns the number of wrapped lines
// if pWrappedLines != 0 then the stringlist will contain the wrapped lines.
int wordWrap( const QString& origLine, int nofColumns, Diff3WrapLine* pDiff3WrapLine )
{      
   if (nofColumns<=0)
      nofColumns = 1;
      
   int nofNeededLines = 0;
   int length = origLine.length();
   
   if (length==0)
   {
      nofNeededLines = 1;
      if( pDiff3WrapLine )
      {
         pDiff3WrapLine->wrapLineOffset=0;
         pDiff3WrapLine->wrapLineLength=0;
      }
   }
   else
   {   
      int pos = 0;
      
      while ( pos < length )
      {
         int wrapPos = pos + nofColumns;
         
         if ( length-pos <= nofColumns  )
         {
            wrapPos = length;
         }
         else
         {      
            int wsPos = max2( origLine.findRev( ' ', wrapPos ), origLine.findRev( '\t', wrapPos ) );
            
            if ( wsPos > pos )
            {
               // Wrap line at wsPos
               wrapPos = wsPos;
            }
         }
         
         if ( pDiff3WrapLine )
         {
            pDiff3WrapLine->wrapLineOffset = pos;
            pDiff3WrapLine->wrapLineLength = wrapPos-pos;
            ++pDiff3WrapLine;
         }
         
         pos = wrapPos;
         
         ++nofNeededLines;
      }
   }
   return nofNeededLines;   
}

void DiffTextWindow::convertSelectionToD3LCoords()
{
   if ( m_pDiff3LineVector==0 || ! m_bPaintingAllowed || !isVisible() || selection.isEmpty() )
   {
      return;
   }

   // convert the selection to unwrapped coordinates: Later restore to new coords
   int firstD3LIdx, firstD3LPos;
   QCString s = getLineString( selection.beginLine() );
   int firstPosInText = convertToPosInText( s, s.length(), selection.beginPos() );
   convertLineCoordsToD3LCoords( selection.beginLine(), firstPosInText, firstD3LIdx, firstD3LPos );
   
   int lastD3LIdx, lastD3LPos;
   s = getLineString( selection.endLine() );
   int lastPosInText = convertToPosInText( s, s.length(), selection.endPos() );
   convertLineCoordsToD3LCoords( selection.endLine(), lastPosInText, lastD3LIdx, lastD3LPos );
   
   //selection.reset();
   selection.start( firstD3LIdx, firstD3LPos );
   selection.end( lastD3LIdx, lastD3LPos );
}

void DiffTextWindow::recalcWordWrap( bool bWordWrap, int wrapLineVectorSize )
{   
   if ( m_pDiff3LineVector==0 || ! m_bPaintingAllowed || !isVisible() )
   {
      m_bWordWrap = bWordWrap;
      if (!bWordWrap)  m_diff3WrapLineVector.resize( 0 );
      return;
   }
   
   m_bWordWrap = bWordWrap;
   
   if ( bWordWrap )
   {                  
      m_diff3WrapLineVector.resize( wrapLineVectorSize );      
      
      int nofVisibleColumns = getNofVisibleColumns();
      int i;
      int wrapLineIdx = 0;
      int size = m_pDiff3LineVector->size();
      for( i=0; i<size; ++i )
      {
         QString s = getString( i );
         int linesNeeded = wordWrap( s, nofVisibleColumns, wrapLineVectorSize==0 ? 0 : &m_diff3WrapLineVector[wrapLineIdx] );
         Diff3Line& d3l = *(*m_pDiff3LineVector)[i];
         if ( d3l.linesNeededForDisplay<linesNeeded )
         {
            d3l.linesNeededForDisplay = linesNeeded;
         }
         
         if ( wrapLineVectorSize>0 )
         {
            int j;
            for( j=0; j<d3l.linesNeededForDisplay; ++j, ++wrapLineIdx )
            {
               Diff3WrapLine& d3wl = m_diff3WrapLineVector[wrapLineIdx];
               d3wl.diff3LineIndex = i;
               d3wl.pD3L = (*m_pDiff3LineVector)[i];
               if ( j>=linesNeeded )
               {
                  d3wl.wrapLineOffset=0;
                  d3wl.wrapLineLength=0;
               }
            }
         }
      }   
   
      if ( wrapLineVectorSize>0 )
      {   
         m_firstLine = min2( m_firstLine, wrapLineVectorSize-1 );
         m_firstColumn = 0;      
      }
   }
   else
   {
      m_diff3WrapLineVector.resize( 0 );
   }
   
   if ( !selection.isEmpty() && ( !m_bWordWrap || wrapLineVectorSize>0 ) )
   {  
      // Assume unwrapped coordinates 
      //( Why? ->Conversion to unwrapped coords happened a few lines above in this method. 
      //  Also see KDiff3App::recalcWordWrap() on the role of wrapLineVectorSize)
      
      // Wrap them now.
      
      // convert the selection to unwrapped coordinates.      
      int firstLine, firstPos;
      convertD3LCoordsToLineCoords( selection.beginLine(), selection.beginPos(), firstLine, firstPos );
      
      int lastLine, lastPos;
      convertD3LCoordsToLineCoords( selection.endLine(), selection.endPos(), lastLine, lastPos );
               
      //selection.reset();
      selection.start( firstLine, convertToPosOnScreen( getLineString( firstLine ), firstPos ) );
      selection.end( lastLine, convertToPosOnScreen( getLineString( lastLine ),lastPos ) );
   }      
}




#include <qtextcodec.h>

QString decodeString( const char*s , OptionDialog* pOptions )
{
   if ( pOptions->m_bStringEncoding )
   {
      QTextCodec* c = QTextCodec::codecForLocale();
      if (c!=0)
         return c->toUnicode( s );
      else
         return QString(s);
   }
   else
      return QString(s);
}

QCString encodeString( const QString& s , OptionDialog* pOptions )
{
   if ( pOptions->m_bStringEncoding )
   {
      QTextCodec* c = QTextCodec::codecForLocale();
      if (c!=0)
         return c->fromUnicode( s );
      else
         return QCString( s.ascii() );
   }
   else
      return QCString( s.ascii() );
}
