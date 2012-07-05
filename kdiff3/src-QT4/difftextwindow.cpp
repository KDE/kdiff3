/***************************************************************************
                          difftextwindow.cpp  -  description
                             -------------------
    begin                : Mon Apr 8 2002
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

#include "difftextwindow.h"
#include "merger.h"
#include "options.h"

#include <qnamespace.h>
#include <QDragEnterEvent>
#include <QDir>
#include <QStatusBar>
#include <QApplication>
#include <QToolTip>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QLayout>
#include <QTextCodec>
#include <QUrl>
#include <QMenu>

#include <klocale.h>
#include <kfiledialog.h>

#include <cmath>
#include <cstdlib>
#include <assert.h>


class DiffTextWindowData
{
public:
   DiffTextWindowData( DiffTextWindow* p )
   {
      m_pDiffTextWindow = p;
      m_bPaintingAllowed = false;
      m_pLineData = 0;
      m_size = 0;
      m_bWordWrap = false;
      m_delayedDrawTimer = 0;
      m_pDiff3LineVector = 0;
      m_pManualDiffHelpList = 0;
      m_pOptions = 0;
      m_fastSelectorLine1 = 0;
      m_fastSelectorNofLines = 0;
      m_bTriple = 0;
      m_winIdx = 0;
      m_firstLine = 0;
      m_oldFirstLine = 0;
      m_oldFirstColumn = 0;
      m_firstColumn = 0;
      m_lineNumberWidth = 0;
      m_pStatusBar = 0;
      m_scrollDeltaX = 0;
      m_scrollDeltaY = 0;
      m_bMyUpdate = false;
      m_bSelectionInProgress = false;
      m_pTextCodec = 0;
      #if defined(_WIN32) || defined(Q_OS_OS2)
      m_eLineEndStyle = eLineEndStyleDos;
      #else
      m_eLineEndStyle = eLineEndStyleUnix;
      #endif
   }
   DiffTextWindow* m_pDiffTextWindow;
   DiffTextWindowFrame* m_pDiffTextWindowFrame;
   QTextCodec* m_pTextCodec;
   e_LineEndStyle m_eLineEndStyle;

   bool m_bPaintingAllowed;
   const LineData* m_pLineData;
   int m_size;
   QString m_filename;
   bool m_bWordWrap;
   int m_delayedDrawTimer;

   const Diff3LineVector* m_pDiff3LineVector;
   Diff3WrapLineVector m_diff3WrapLineVector;
   const ManualDiffHelpList* m_pManualDiffHelpList;

   Options* m_pOptions;
   QColor m_cThis;
   QColor m_cDiff1;
   QColor m_cDiff2;
   QColor m_cDiffBoth;

   int m_fastSelectorLine1;
   int m_fastSelectorNofLines;

   bool m_bTriple;
   int m_winIdx;
   int m_firstLine;
   int m_oldFirstLine;
   int m_oldFirstColumn;
   int m_firstColumn;
   int m_lineNumberWidth;

   void getLineInfo(
           const Diff3Line& d,
           int& lineIdx,
           DiffList*& pFineDiff1, DiffList*& pFineDiff2,   // return values
           int& changed, int& changed2  );

   QString getString( int d3lIdx );
   QString getLineString( int line );

   void writeLine(
         MyPainter& p, const LineData* pld,
         const DiffList* pLineDiff1, const DiffList* pLineDiff2, int line,
         int whatChanged, int whatChanged2, int srcLineIdx,
         int wrapLineOffset, int wrapLineLength, bool bWrapLine, const QRect& invalidRect, int deviceWidth
         );

   void draw( MyPainter& p, const QRect& invalidRect, int deviceWidth, int beginLine, int endLine );

   QStatusBar* m_pStatusBar;

   Selection m_selection;

   int m_scrollDeltaX;
   int m_scrollDeltaY;

   bool m_bMyUpdate;
   void myUpdate(int afterMilliSecs );

   int leftInfoWidth() { return 4+m_lineNumberWidth; }   // Nr of information columns on left side
   int convertLineOnScreenToLineInSource( int lineOnScreen, e_CoordType coordType, bool bFirstLine );

   bool m_bSelectionInProgress;
   QPoint m_lastKnownMousePos;
};

DiffTextWindow::DiffTextWindow(
   DiffTextWindowFrame* pParent,
   QStatusBar* pStatusBar,
   Options* pOptions,
   int winIdx
   )
   : QWidget(pParent)
{
   setObjectName(QString("DiffTextWindow%1").arg(winIdx));
   setAttribute( Qt::WA_OpaquePaintEvent );
   //setAttribute( Qt::WA_PaintOnScreen );

   d = new DiffTextWindowData(this);
   d->m_pDiffTextWindowFrame = pParent;
   setFocusPolicy( Qt::ClickFocus );
   setAcceptDrops( true );

   d->m_pOptions = pOptions;
   init( 0, 0, d->m_eLineEndStyle, 0, 0, 0, 0, false );

   setMinimumSize(QSize(20,20));

   d->m_pStatusBar = pStatusBar;
   d->m_bPaintingAllowed = true;
   d->m_bWordWrap = false;
   d->m_winIdx = winIdx;

   setFont(d->m_pOptions->m_font);
}

DiffTextWindow::~DiffTextWindow()
{
   delete d;
}

void DiffTextWindow::init(
   const QString& filename,
   QTextCodec* pTextCodec,
   e_LineEndStyle eLineEndStyle,
   const LineData* pLineData,
   int size,
   const Diff3LineVector* pDiff3LineVector,
   const ManualDiffHelpList* pManualDiffHelpList,
   bool bTriple
   )
{
   d->m_filename = filename;
   d->m_pLineData = pLineData;
   d->m_size = size;
   d->m_pDiff3LineVector = pDiff3LineVector;
   d->m_diff3WrapLineVector.clear();
   d->m_pManualDiffHelpList = pManualDiffHelpList;

   d->m_firstLine = 0;
   d->m_oldFirstLine = -1;
   d->m_firstColumn = 0;
   d->m_oldFirstColumn = -1;
   d->m_bTriple = bTriple;
   d->m_scrollDeltaX=0;
   d->m_scrollDeltaY=0;
   d->m_bMyUpdate = false;
   d->m_fastSelectorLine1 = 0;
   d->m_fastSelectorNofLines = 0;
   d->m_lineNumberWidth = 0;
   d->m_selection.reset();
   d->m_selection.oldFirstLine = -1; // reset is not enough here.
   d->m_selection.oldLastLine = -1;
   d->m_selection.lastLine = -1;

   d->m_pTextCodec = pTextCodec;
   d->m_eLineEndStyle = eLineEndStyle;

   update();
   d->m_pDiffTextWindowFrame->init();
}

void DiffTextWindow::reset()
{
   d->m_pLineData=0;
   d->m_size=0;
   d->m_pDiff3LineVector=0;
   d->m_filename="";
   d->m_diff3WrapLineVector.clear();
}

void DiffTextWindow::setPaintingAllowed( bool bAllowPainting )
{
   if (d->m_bPaintingAllowed != bAllowPainting)
   {
      d->m_bPaintingAllowed = bAllowPainting;
      if ( d->m_bPaintingAllowed ) update();
      else reset();
   }
}

void DiffTextWindow::dragEnterEvent( QDragEnterEvent* e )
{
   e->setAccepted( e->mimeData()->hasUrls() || e->mimeData()->hasText() );
   // Note that the corresponding drop is handled in KDiff3App::eventFilter().
}


void DiffTextWindow::setFirstLine(int firstLine)
{
   int fontHeight = fontMetrics().height();

   int newFirstLine = max2(0,firstLine);

   int deltaY = fontHeight * ( d->m_firstLine - newFirstLine );

   d->m_firstLine = newFirstLine;

   if ( d->m_bSelectionInProgress && d->m_selection.firstLine != -1 )
   {
      int line, pos;
      convertToLinePos( d->m_lastKnownMousePos.x(), d->m_lastKnownMousePos.y(), line, pos );
      d->m_selection.end( line, pos );
      update();
   }
   else
   {
      QWidget::scroll( 0, deltaY );
   }
   d->m_pDiffTextWindowFrame->setFirstLine( d->m_firstLine );
}

int DiffTextWindow::getFirstLine()
{
   return d->m_firstLine;
}

void DiffTextWindow::setFirstColumn(int firstCol)
{
   int fontWidth = fontMetrics().width('W');
   int xOffset = d->leftInfoWidth() * fontWidth;

   int newFirstColumn = max2(0,firstCol);

   int deltaX = fontWidth * ( d->m_firstColumn - newFirstColumn );

   d->m_firstColumn = newFirstColumn;

   QRect r( xOffset, 0, width()-xOffset, height() );

   if ( d->m_pOptions->m_bRightToLeftLanguage )
   {
      deltaX = -deltaX;
      r = QRect( width()-1-xOffset, 0, -(width()-xOffset), height() ).normalized();
   }

   if ( d->m_bSelectionInProgress && d->m_selection.firstLine != -1 )
   {
      int line, pos;
      convertToLinePos( d->m_lastKnownMousePos.x(), d->m_lastKnownMousePos.y(), line, pos );
      d->m_selection.end( line, pos );
      update();
   }
   else
   {
      QWidget::scroll( deltaX, 0, r );
   }
}

int DiffTextWindow::getNofColumns()
{
   if (d->m_bWordWrap)
   {
      return getNofVisibleColumns();
   }
   else
   {
      int nofColumns = 0;
      for( int i = 0; i< d->m_size; ++i )
      {
         if ( d->m_pLineData[i].width( d->m_pOptions->m_tabSize ) > nofColumns )
            nofColumns = d->m_pLineData[i].width( d->m_pOptions->m_tabSize );
      }
      return nofColumns;
   }
}

int DiffTextWindow::getNofLines()
{
   return d->m_bWordWrap ? d->m_diff3WrapLineVector.size() : 
                        d->m_pDiff3LineVector->size();
}


int DiffTextWindow::convertLineToDiff3LineIdx( int line )
{
   if ( d->m_bWordWrap && d->m_diff3WrapLineVector.size()>0 )
      return d->m_diff3WrapLineVector[ min2( line, (int)d->m_diff3WrapLineVector.size()-1 ) ].diff3LineIndex;
   else
      return line;
}

int DiffTextWindow::convertDiff3LineIdxToLine( int d3lIdx )
{
   if ( d->m_bWordWrap && d->m_pDiff3LineVector!=0 && d->m_pDiff3LineVector->size()>0 )
      return (*d->m_pDiff3LineVector)[ min2( d3lIdx, (int)d->m_pDiff3LineVector->size()-1 ) ]->sumLinesNeededForDisplay;
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
   d->m_fastSelectorLine1 = line1;
   d->m_fastSelectorNofLines = nofLines;
   if ( isVisible() )
   {
      int newFirstLine = getBestFirstLine( 
         convertDiff3LineIdxToLine(d->m_fastSelectorLine1), 
         convertDiff3LineIdxToLine(d->m_fastSelectorLine1+d->m_fastSelectorNofLines)-convertDiff3LineIdxToLine(d->m_fastSelectorLine1), 
         d->m_firstLine, 
         getNofVisibleLines() 
         );
      if ( newFirstLine != d->m_firstLine )
      {
         scroll( 0, newFirstLine - d->m_firstLine );
      }

      update();
   }
}


void DiffTextWindow::showStatusLine(int line )
{
   int d3lIdx = convertLineToDiff3LineIdx( line );
   if( d->m_pDiff3LineVector!=0 && d3lIdx >= 0 && d3lIdx<(int)d->m_pDiff3LineVector->size() )
   {
      const Diff3Line* pD3l = (*d->m_pDiff3LineVector)[d3lIdx];
      if ( pD3l != 0 )
      {
         int l = pD3l->getLineInFile( d->m_winIdx );

         QString s = i18n("File") + " " + d->m_filename;
         if ( l!=-1 )
            s += ": " + i18n("Line") + " " + QString::number( l+1 );
         else
            s += ": " + i18n("Line not available");
         if (d->m_pStatusBar!=0) d->m_pStatusBar->showMessage(s);

         emit lineClicked( d->m_winIdx, l );
      }
   }
}

void DiffTextWindow::focusInEvent(QFocusEvent* e)
{
   emit gotFocus();
   QWidget::focusInEvent(e);
}

void DiffTextWindow::mousePressEvent ( QMouseEvent* e )
{
   if ( e->button() == Qt::LeftButton )
   {
      int line;
      int pos;
      convertToLinePos( e->x(), e->y(), line, pos );
      if ( pos < d->m_firstColumn )
      {
         emit setFastSelectorLine( convertLineToDiff3LineIdx(line) );
         d->m_selection.firstLine = -1;     // Disable current d->m_selection
      }
      else
      {  // Selection
         resetSelection();
         d->m_selection.start( line, pos );
         d->m_selection.end( line, pos );
         d->m_bSelectionInProgress = true;
         d->m_lastKnownMousePos = e->pos();

         showStatusLine( line );
      }
   }
}

bool isCTokenChar( QChar c )
{
   return (c=='_')  ||
          ( c>='A' && c<='Z' ) || ( c>='a' && c<='z' ) ||
          (c>='0' && c<='9');
}

/// Calculate where a token starts and ends, given the x-position on screen.
void calcTokenPos( const QString& s, int posOnScreen, int& pos1, int& pos2, int tabSize )
{
   // Cursor conversions that consider g_tabSize
   int pos = convertToPosInText( s, max2( 0, posOnScreen ), tabSize );
   if ( pos>=(int)s.length() )
   {
      pos1=s.length();
      pos2=s.length();
      return;
   }

   pos1 = pos;
   pos2 = pos+1;

   if( isCTokenChar( s[pos1] ) )
   {
      while( pos1>=0 && isCTokenChar( s[pos1] ) )
         --pos1;
      ++pos1;

      while( pos2<(int)s.length() && isCTokenChar( s[pos2] ) )
         ++pos2;
   }
}

void DiffTextWindow::mouseDoubleClickEvent( QMouseEvent* e )
{
   d->m_bSelectionInProgress = false;
   d->m_lastKnownMousePos = e->pos();
   if ( e->button() == Qt::LeftButton )
   {
      int line;
      int pos;
      convertToLinePos( e->x(), e->y(), line, pos );

      // Get the string data of the current line
      QString s;
      if ( d->m_bWordWrap )
      {
         if ( line<0 || line >= (int)d->m_diff3WrapLineVector.size() )
            return;
         const Diff3WrapLine& d3wl = d->m_diff3WrapLineVector[line];
         s = d->getString( d3wl.diff3LineIndex ).mid( d3wl.wrapLineOffset, d3wl.wrapLineLength );
      }
      else
      {
         if ( line<0 || line >= (int)d->m_pDiff3LineVector->size() )
            return;
         s = d->getString( line );
      }

      if ( ! s.isEmpty() )
      {
         int pos1, pos2;
         calcTokenPos( s, pos, pos1, pos2, d->m_pOptions->m_tabSize );

         resetSelection();
         d->m_selection.start( line, convertToPosOnScreen( s, pos1, d->m_pOptions->m_tabSize ) );
         d->m_selection.end( line, convertToPosOnScreen( s, pos2, d->m_pOptions->m_tabSize ) );
         update();
         // emit d->m_selectionEnd() happens in the mouseReleaseEvent.
         showStatusLine( line );
      }
   }
}

void DiffTextWindow::mouseReleaseEvent ( QMouseEvent* e )
{
   d->m_bSelectionInProgress = false;
   d->m_lastKnownMousePos = e->pos();
   //if ( e->button() == LeftButton )
   {
      if (d->m_delayedDrawTimer)
         killTimer(d->m_delayedDrawTimer);
      d->m_delayedDrawTimer = 0;
      if (d->m_selection.firstLine != -1 )
      {
         emit selectionEnd();
      }
   }
   d->m_scrollDeltaX=0;
   d->m_scrollDeltaY=0;
}

inline int sqr(int x){return x*x;}

void DiffTextWindow::mouseMoveEvent ( QMouseEvent * e )
{
   int line;
   int pos;
   convertToLinePos( e->x(), e->y(), line, pos );
   d->m_lastKnownMousePos = e->pos();

   if (d->m_selection.firstLine != -1 )
   {
      d->m_selection.end( line, pos );

      showStatusLine( line );

      // Scroll because mouse moved out of the window
      const QFontMetrics& fm = fontMetrics();
      int fontWidth = fm.width('W');
      int deltaX=0;
      int deltaY=0;
      if ( ! d->m_pOptions->m_bRightToLeftLanguage )
      {
         if ( e->x() < d->leftInfoWidth()*fontWidth )  deltaX = -1 - abs(e->x()-d->leftInfoWidth()*fontWidth)/fontWidth;
         if ( e->x() > width()     )                   deltaX = +1 + abs(e->x()-width())/fontWidth;
      }
      else
      {
         if ( e->x() > width()-1-d->leftInfoWidth()*fontWidth ) deltaX=+1+ abs(e->x() - (width()-1-d->leftInfoWidth()*fontWidth)) / fontWidth;
         if ( e->x() < fontWidth )                              deltaX=-1- abs(e->x()-fontWidth)/fontWidth;
      }
      if ( e->y() < 0 )          deltaY = -1 - sqr( e->y() ) / sqr(fm.height());
      if ( e->y() > height() )   deltaY = +1 + sqr( e->y() - height() ) / sqr(fm.height());
      if ( (deltaX != 0 && d->m_scrollDeltaX!=deltaX) || (deltaY!= 0 && d->m_scrollDeltaY!=deltaY) )
      {
         d->m_scrollDeltaX = deltaX;
         d->m_scrollDeltaY = deltaY;
         emit scroll( deltaX, deltaY );
         if (d->m_delayedDrawTimer)
            killTimer( d->m_delayedDrawTimer );
         d->m_delayedDrawTimer = startTimer(50);
      }
      else
      {
         d->m_scrollDeltaX = deltaX;
         d->m_scrollDeltaY = deltaY;
         d->myUpdate(0);
      }
   }
}


void DiffTextWindowData::myUpdate(int afterMilliSecs)
{
   if (m_delayedDrawTimer)
      m_pDiffTextWindow->killTimer( m_delayedDrawTimer );
   m_bMyUpdate = true;
   m_delayedDrawTimer = m_pDiffTextWindow->startTimer( afterMilliSecs );
}

void DiffTextWindow::timerEvent(QTimerEvent*)
{
   killTimer(d->m_delayedDrawTimer);
   d->m_delayedDrawTimer = 0;

   if ( d->m_bMyUpdate )
   {
      int fontHeight = fontMetrics().height();

      if ( d->m_selection.oldLastLine != -1 )
      {
         int lastLine;
         int firstLine;
         if ( d->m_selection.oldFirstLine != -1 )
         {
            firstLine = min3( d->m_selection.oldFirstLine, d->m_selection.lastLine, d->m_selection.oldLastLine );
            lastLine = max3( d->m_selection.oldFirstLine, d->m_selection.lastLine, d->m_selection.oldLastLine );
         }
         else
         {
            firstLine = min2( d->m_selection.lastLine, d->m_selection.oldLastLine );
            lastLine = max2( d->m_selection.lastLine, d->m_selection.oldLastLine );
         }
         int y1 = ( firstLine - d->m_firstLine ) * fontHeight;
         int y2 = min2( height(), ( lastLine  - d->m_firstLine + 1 ) * fontHeight );

         if ( y1<height() && y2>0 )
         {
            QRect invalidRect = QRect( 0, y1, width(), y2-y1 );
            update( invalidRect );
         }
      }

      d->m_bMyUpdate = false;
   }

   if ( d->m_scrollDeltaX != 0 || d->m_scrollDeltaY != 0 )
   {
      d->m_selection.end( d->m_selection.lastLine + d->m_scrollDeltaY, d->m_selection.lastPos +  d->m_scrollDeltaX );
      emit scroll( d->m_scrollDeltaX, d->m_scrollDeltaY );
      killTimer(d->m_delayedDrawTimer);
      d->m_delayedDrawTimer = startTimer(50);
   }
}

void DiffTextWindow::resetSelection()
{
   d->m_selection.reset();
   update();
}

void DiffTextWindow::convertToLinePos( int x, int y, int& line, int& pos )
{
   const QFontMetrics& fm = fontMetrics();
   int fontHeight = fm.height();
   int fontWidth = fm.width('W');
   int xOffset = ( d->leftInfoWidth() - d->m_firstColumn ) * fontWidth;

   int yOffset = - d->m_firstLine * fontHeight;

   line = ( y - yOffset ) / fontHeight;
   if ( ! d->m_pOptions->m_bRightToLeftLanguage )
      pos  = ( x - xOffset ) / fontWidth;
   else 
      pos  = ( (width() - 1 - x) - xOffset ) / fontWidth;
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

class DrawTextOptimizer
// Drawing continuous text is much faster than drawing one char after another.
{
private:
   QFont m_font;
   QPen m_pen;
   QString m_text;
   QPoint m_pos;
   MyPainter* m_pPainter;
   QFontMetrics m_fm;
   void draw()
   {
      if ( m_pPainter && !m_text.isEmpty() )
      {
         m_pPainter->setFont( m_font );
         m_pPainter->setPen( m_pen );
         m_pPainter->drawText( m_pos.x(), m_pos.y(), m_text, true );
         m_text = QString();
      }
   }
public:
   DrawTextOptimizer( MyPainter* pPainter ) 
      : m_fm(pPainter->fontMetrics())
   { 
      m_pPainter = pPainter;
      m_font = m_pPainter->font();
      m_pen = m_pPainter->pen();
   }
   ~DrawTextOptimizer() { end(); }
   void setFont( const QFont& f )
   {
      if ( f!=m_font )
      {
         draw();
         m_font = f;
         m_fm = m_pPainter->fontMetrics();
      }
   }
   void setPen( const QPen& pen )
   {
      if ( pen!=m_pen )
      {
         draw();
         m_pen = pen;
      }
   }
   void drawText( int x, int y, const QString& s )
   {
      if ( y!=m_pos.y() || x != m_pos.x()+m_text.length()*m_fm.width("W") )
      {
         draw();
         m_pos = QPoint(x,y);
      }
      m_text += s;
   }
   void end()
   {
      draw();
   }
};

void DiffTextWindowData::writeLine(
   MyPainter& p,
   const LineData* pld,
   const DiffList* pLineDiff1,
   const DiffList* pLineDiff2,
   int line,
   int whatChanged,
   int whatChanged2,
   int srcLineIdx,
   int wrapLineOffset,
   int wrapLineLength,
   bool bWrapLine,
   const QRect& invalidRect,
   int deviceWidth
   )
{
   QFont normalFont = p.font();
   QFont diffFont = normalFont;
   diffFont.setItalic( m_pOptions->m_bItalicForDeltas );
   const QFontMetrics& fm = p.fontMetrics();
   int fontHeight = fm.height();
   int fontAscent = fm.ascent();
   int fontDescent = fm.descent();
   int fontWidth = fm.width('W');

   int xOffset = (leftInfoWidth() - m_firstColumn)*fontWidth;
   int yOffset = (line-m_firstLine) * fontHeight;

   QRect lineRect( 0, yOffset, deviceWidth, fontHeight );
   if ( ! invalidRect.intersects( lineRect ) )
   {
      return;
   }

   int fastSelectorLine1 = m_pDiffTextWindow->convertDiff3LineIdxToLine(m_fastSelectorLine1);
   int fastSelectorLine2 = m_pDiffTextWindow->convertDiff3LineIdxToLine(m_fastSelectorLine1+m_fastSelectorNofLines)-1;

   bool bFastSelectionRange = (line>=fastSelectorLine1 && line<= fastSelectorLine2 );
   QColor bgColor = m_pOptions->m_bgColor;
   QColor diffBgColor = m_pOptions->m_diffBgColor;

   if ( bFastSelectionRange )
   {
      bgColor = m_pOptions->m_currentRangeBgColor;
      diffBgColor = m_pOptions->m_currentRangeDiffBgColor;
   }

   if ( yOffset+fontHeight<invalidRect.top()  ||  invalidRect.bottom() < yOffset-fontHeight )
      return;

   int changed = whatChanged;
   if ( pLineDiff1 != 0 ) changed |= 1;
   if ( pLineDiff2 != 0 ) changed |= 2;

   QColor c = m_pOptions->m_fgColor;
   p.setPen(c);
   if ( changed == 2 ) {
      c = m_cDiff2;
   } else if ( changed == 1 ) {
      c = m_cDiff1;
   } else if ( changed == 3 ) {
      c = m_cDiffBoth;
   }

   p.fillRect( leftInfoWidth()*fontWidth, yOffset, deviceWidth, fontHeight, bgColor );

   if (pld!=0)
   {
      // First calculate the "changed" information for each character.
      int i=0;
      QString lineString( pld->pLine, pld->size );
      std::vector<UINT8> charChanged( pld->size );
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

      QString s=" ";
      // Convert tabs
      int outPos = 0;

      int lineLength = m_bWordWrap ? wrapLineOffset+wrapLineLength : lineString.length();

      DrawTextOptimizer dto( &p );

      for( i=wrapLineOffset; i<lineLength; ++i )
      {
         int spaces = 1;

         if ( lineString[i]=='\t' )
         {
            spaces = tabber( outPos, m_pOptions->m_tabSize );
            s[0] = ' ';
         }
         else
         {
            s[0] = lineString[i];
         }

         QColor c = m_pOptions->m_fgColor;
         int cchanged = charChanged[i] | whatChanged;

         if ( cchanged == 2 ) {
            c = m_cDiff2;
         } else if ( cchanged == 1 ) {
            c = m_cDiff1;
         } else if ( cchanged == 3 ) {
            c = m_cDiffBoth;
         }

         if ( c!=m_pOptions->m_fgColor && whatChanged2==0 && !m_pOptions->m_bShowWhiteSpace )
         {
            // The user doesn't want to see highlighted white space.
            c = m_pOptions->m_fgColor;
         }

         QRect outRect( xOffset + fontWidth*outPos, yOffset, fontWidth*spaces, fontHeight );
         if ( m_pOptions->m_bRightToLeftLanguage )
            outRect = QRect( deviceWidth-1-(xOffset + fontWidth*outPos), yOffset, -fontWidth*spaces, fontHeight ).normalized();
         if ( invalidRect.intersects( outRect ) )
         {
            if( !m_selection.within( line, outPos ) )
            {

               if( c!=m_pOptions->m_fgColor )
               {
                  QColor lightc = diffBgColor;
                  p.fillRect( xOffset + fontWidth*outPos, yOffset,
                           fontWidth*spaces, fontHeight, lightc );
                  dto.setFont(diffFont);
               }

               dto.setPen( c );
               if ( s[0]==' '  &&  c!=m_pOptions->m_fgColor  &&  charChanged[i]!=0 )
               {
                  if ( m_pOptions->m_bShowWhiteSpaceCharacters && m_pOptions->m_bShowWhiteSpace)
                  {
                     p.fillRect( xOffset + fontWidth*outPos, yOffset+fontAscent,
                                 fontWidth*spaces-1, fontDescent, c );
                  }
               }
               else
               {
                  dto.drawText( xOffset + fontWidth*outPos, yOffset + fontAscent, s );
               }
               dto.setFont(normalFont);
            }
            else
            {
               p.fillRect( xOffset + fontWidth*outPos, yOffset,
                           fontWidth*(spaces), fontHeight, m_pDiffTextWindow->palette().highlight() );

               dto.setPen( m_pDiffTextWindow->palette().highlightedText().color() );
               dto.drawText( xOffset + fontWidth*outPos, yOffset + fontAscent, s );

               m_selection.bSelectionContainsData = true;
            }
         }

         outPos += spaces;
      } // end for
      dto.end();
      if( m_selection.lineWithin( line ) && m_selection.lineWithin( line+1 ) )
      {
         p.fillRect( xOffset + fontWidth*outPos, yOffset,
                     deviceWidth, fontHeight, m_pDiffTextWindow->palette().highlight() );
      }
   }

   p.fillRect( 0, yOffset, leftInfoWidth()*fontWidth, fontHeight, m_pOptions->m_bgColor );

   xOffset = (m_lineNumberWidth+2)*fontWidth;
   int xLeft =   m_lineNumberWidth*fontWidth;
   p.setPen( m_pOptions->m_fgColor );
   if ( pld!=0 )
   {
      if ( m_pOptions->m_bShowLineNumbers && !bWrapLine )
      {
         QString num;
         num.sprintf( "%0*d", m_lineNumberWidth, srcLineIdx+1);
         p.drawText( 0, yOffset + fontAscent, num );
         //p.drawLine( xLeft -1, yOffset, xLeft -1, yOffset+fontHeight-1 );
      }
      if ( !bWrapLine || wrapLineLength>0 )
      {
         p.setPen( QPen( m_pOptions->m_fgColor, 0, bWrapLine ? Qt::DotLine : Qt::SolidLine) );
         p.drawLine( xOffset +1, yOffset, xOffset +1, yOffset+fontHeight-1 );
         p.setPen( QPen( m_pOptions->m_fgColor, 0, Qt::SolidLine) );
      }
   }
   if ( c!=m_pOptions->m_fgColor && whatChanged2==0 )//&& whatChanged==0 )
   {
      if ( m_pOptions->m_bShowWhiteSpace )
      {
         p.setBrushOrigin(0,0);
         p.fillRect( xLeft, yOffset, fontWidth*2-1, fontHeight, QBrush(c,Qt::Dense5Pattern) );
      }
   }
   else
   {
      p.fillRect( xLeft, yOffset, fontWidth*2-1, fontHeight, c==m_pOptions->m_fgColor ? bgColor : c );
   }

   if ( bFastSelectionRange )
   {
      p.fillRect( xOffset + fontWidth-1, yOffset, 3, fontHeight, m_pOptions->m_fgColor );
   }

   // Check if line needs a manual diff help mark
   ManualDiffHelpList::const_iterator ci;
   for( ci = m_pManualDiffHelpList->begin(); ci!=m_pManualDiffHelpList->end(); ++ci)
   {
      const ManualDiffHelpEntry& mdhe=*ci;
      int rangeLine1 = -1;
      int rangeLine2 = -1;
      if (m_winIdx==1 ) { rangeLine1 = mdhe.lineA1; rangeLine2= mdhe.lineA2; }
      if (m_winIdx==2 ) { rangeLine1 = mdhe.lineB1; rangeLine2= mdhe.lineB2; }
      if (m_winIdx==3 ) { rangeLine1 = mdhe.lineC1; rangeLine2= mdhe.lineC2; }
      if ( rangeLine1>=0 && rangeLine2>=0 && srcLineIdx >= rangeLine1 && srcLineIdx <= rangeLine2 )
      {
         p.fillRect( xOffset - fontWidth, yOffset, fontWidth-1, fontHeight, m_pOptions->m_manualHelpRangeColor );
         break;
      }
   }
}

void DiffTextWindow::paintEvent( QPaintEvent* e )
{
   QRect invalidRect = e->rect();
   if ( invalidRect.isEmpty() || ! d->m_bPaintingAllowed )
      return;

   if ( d->m_pDiff3LineVector==0  ||  ( d->m_diff3WrapLineVector.empty() && d->m_bWordWrap ) ) 
   {
      QPainter p(this);
      p.fillRect( invalidRect, d->m_pOptions->m_bgColor );
      return;
   }
   
   bool bOldSelectionContainsData = d->m_selection.bSelectionContainsData;
   d->m_selection.bSelectionContainsData = false;

   int endLine = min2( d->m_firstLine + getNofVisibleLines()+2, getNofLines() );

   //if ( invalidRect.size()==size() )
   {  // double buffering, obsolete with Qt4
      //QPainter painter(this); // Remove for Qt4
      //QPixmap pixmap( invalidRect.size() );// Remove for Qt4

      MyPainter p( this, d->m_pOptions->m_bRightToLeftLanguage, width(), fontMetrics().width('W') ); // For Qt4 change pixmap to this

      //p.translate( -invalidRect.x(), -invalidRect.y() );// Remove for Qt4

      p.setFont( font() );
      p.QPainter::fillRect( invalidRect, d->m_pOptions->m_bgColor );

      d->draw( p, invalidRect, width(), d->m_firstLine, endLine );
      // p.drawLine( m_invalidRect.x(), m_invalidRect.y(), m_invalidRect.right(), m_invalidRect.bottom() ); // For test only
      p.end();

      //painter.drawPixmap( invalidRect.x(), invalidRect.y(), pixmap );// Remove for Qt4
   }
//    else
//    {  // no double buffering
//       MyPainter p( this, d->m_pOptions->m_bRightToLeftLanguage, width(), fontMetrics().width('W') );
//       p.setFont( font() );
//       p.QPainter::fillRect( invalidRect, d->m_pOptions->m_bgColor );
//       d->draw( p, invalidRect, width(), d->m_firstLine, endLine );
//    }


   d->m_oldFirstLine = d->m_firstLine;
   d->m_oldFirstColumn = d->m_firstColumn;
   d->m_selection.oldLastLine = -1;
   if ( d->m_selection.oldFirstLine !=-1 )
      d->m_selection.oldFirstLine = -1;

   if( !bOldSelectionContainsData  &&  d->m_selection.bSelectionContainsData )
      emit newSelection();
}

void DiffTextWindow::print( MyPainter& p, const QRect&, int firstLine, int nofLinesPerPage )
{
   if ( d->m_pDiff3LineVector==0 || ! d->m_bPaintingAllowed ||
        ( d->m_diff3WrapLineVector.empty() && d->m_bWordWrap ) ) 
      return;
   resetSelection();
//   MyPainter p( this, d->m_pOptions->m_bRightToLeftLanguage, width(), fontMetrics().width('W') );
   int oldFirstLine = d->m_firstLine;
   d->m_firstLine = firstLine;
   QRect invalidRect = QRect(0,0,1000000000,1000000000);
   QColor bgColor = d->m_pOptions->m_bgColor;
   d->m_pOptions->m_bgColor = Qt::white;
   d->draw( p, invalidRect, p.window().width(), firstLine, min2(firstLine+nofLinesPerPage,getNofLines()) );
   d->m_pOptions->m_bgColor = bgColor;
   d->m_firstLine = oldFirstLine;
}

void DiffTextWindowData::draw( MyPainter& p, const QRect& invalidRect, int deviceWidth, int beginLine, int endLine )
{
   m_lineNumberWidth = m_pOptions->m_bShowLineNumbers ? (int)log10((double)qMax(m_size,1))+1 : 0;

   if ( m_winIdx==1 )
   {
      m_cThis = m_pOptions->m_colorA;
      m_cDiff1 = m_pOptions->m_colorB;
      m_cDiff2 = m_pOptions->m_colorC;
   }
   if ( m_winIdx==2 )
   {
      m_cThis = m_pOptions->m_colorB;
      m_cDiff1 = m_pOptions->m_colorC;
      m_cDiff2 = m_pOptions->m_colorA;
   }
   if ( m_winIdx==3 )
   {
      m_cThis = m_pOptions->m_colorC;
      m_cDiff1 = m_pOptions->m_colorA;
      m_cDiff2 = m_pOptions->m_colorB;
   }
   m_cDiffBoth = m_pOptions->m_colorForConflict;  // Conflict color

   p.setPen( m_cThis );

   for ( int line = beginLine; line<endLine; ++line )
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

      int srcLineIdx=-1;
      getLineInfo( *d3l, srcLineIdx, pFineDiff1, pFineDiff2, changed, changed2 );

      writeLine(
         p,                         // QPainter
         srcLineIdx == -1 ? 0 : &m_pLineData[srcLineIdx],     // Text in this line
         pFineDiff1,
         pFineDiff2,
         line,                      // Line on the screen
         changed,
         changed2,
         srcLineIdx,
         wrapLineOffset,
         wrapLineLength,
         bWrapLine,
         invalidRect,
         deviceWidth
         );
   }
}

QString DiffTextWindowData::getString( int d3lIdx )
{
   if ( d3lIdx<0 || d3lIdx>=(int)m_pDiff3LineVector->size() )
      return QString();
   const Diff3Line* d3l = (*m_pDiff3LineVector)[d3lIdx];
   DiffList* pFineDiff1;
   DiffList* pFineDiff2;
   int changed=0;
   int changed2=0;
   int lineIdx;
   getLineInfo( *d3l, lineIdx, pFineDiff1, pFineDiff2, changed, changed2 );

   if (lineIdx==-1) return QString();
   else
   {
      const LineData* ld = &m_pLineData[lineIdx];
      return QString( ld->pLine, ld->size );
   }
   return QString();
}

QString DiffTextWindowData::getLineString( int line )
{
   if ( m_bWordWrap )
   {
      int d3LIdx = m_pDiffTextWindow->convertLineToDiff3LineIdx(line);
      return getString( d3LIdx ).mid( m_diff3WrapLineVector[line].wrapLineOffset, m_diff3WrapLineVector[line].wrapLineLength );
   }
   else
   {
      return getString( line );
   }
}

void DiffTextWindowData::getLineInfo(
   const Diff3Line& d3l,
   int& lineIdx,
   DiffList*& pFineDiff1, DiffList*& pFineDiff2,   // return values
   int& changed, int& changed2
   )
{
   changed=0;
   changed2=0;
   bool bAEqB = d3l.bAEqB || ( d3l.bWhiteLineA && d3l.bWhiteLineB );
   bool bAEqC = d3l.bAEqC || ( d3l.bWhiteLineA && d3l.bWhiteLineC );
   bool bBEqC = d3l.bBEqC || ( d3l.bWhiteLineB && d3l.bWhiteLineC );
   if      ( m_winIdx == 1 ) {
      lineIdx=d3l.lineA;
      pFineDiff1=d3l.pFineAB;
      pFineDiff2=d3l.pFineCA;
      changed |= ((d3l.lineB==-1)!=(lineIdx==-1) ? 1 : 0) +
                 ((d3l.lineC==-1)!=(lineIdx==-1) && m_bTriple ? 2 : 0);
      changed2 |= ( bAEqB ? 0 : 1 ) + (bAEqC || !m_bTriple ? 0 : 2);
   }
   else if ( m_winIdx == 2 ) {
      lineIdx=d3l.lineB;
      pFineDiff1=d3l.pFineBC;
      pFineDiff2=d3l.pFineAB;
      changed |= ((d3l.lineC==-1)!=(lineIdx==-1) && m_bTriple ? 1 : 0) +
                 ((d3l.lineA==-1)!=(lineIdx==-1) ? 2 : 0);
      changed2 |= ( bBEqC || !m_bTriple ? 0 : 1 ) + (bAEqB ? 0 : 2);
   }
   else if ( m_winIdx == 3 ) {
      lineIdx=d3l.lineC;
      pFineDiff1=d3l.pFineCA;
      pFineDiff2=d3l.pFineBC;
      changed |= ((d3l.lineA==-1)!=(lineIdx==-1) ? 1 : 0) +
                 ((d3l.lineB==-1)!=(lineIdx==-1) ? 2 : 0);
      changed2 |= ( bAEqC ? 0 : 1 ) + (bBEqC ? 0 : 2);
   }
   else assert(false);
}



void DiffTextWindow::resizeEvent( QResizeEvent* e )
{
   QSize s = e->size();
   QFontMetrics fm = fontMetrics();
   int visibleLines = s.height()/fm.height()-2;
   int visibleColumns = s.width()/fm.width('W') - d->leftInfoWidth();
   emit resizeSignal( visibleColumns, visibleLines );
   QWidget::resizeEvent(e);
}

int DiffTextWindow::getNofVisibleLines()
{
   QFontMetrics fm = fontMetrics();
   int fmh = fm.height();
   int h = height();
   return h/fmh -1;//height()/fm.height()-2;
}

int DiffTextWindow::getNofVisibleColumns()
{
   QFontMetrics fm = fontMetrics();
   return width()/fm.width('W') - d->leftInfoWidth();
}

QString DiffTextWindow::getSelection()
{
   if ( d->m_pLineData==0 )
      return QString();

   QString selectionString;

   int line=0;
   int lineIdx=0;

   int it;
   int vectorSize = d->m_bWordWrap ? d->m_diff3WrapLineVector.size() : d->m_pDiff3LineVector->size();
   for( it=0; it<vectorSize; ++it )
   {
      const Diff3Line* d3l = d->m_bWordWrap ? d->m_diff3WrapLineVector[it].pD3L : (*d->m_pDiff3LineVector)[it];
      if      ( d->m_winIdx == 1 ) {    lineIdx=d3l->lineA;     }
      else if ( d->m_winIdx == 2 ) {    lineIdx=d3l->lineB;     }
      else if ( d->m_winIdx == 3 ) {    lineIdx=d3l->lineC;     }
      else assert(false);

      if( lineIdx != -1 )
      {
         const QChar* pLine = d->m_pLineData[lineIdx].pLine;
         int size = d->m_pLineData[lineIdx].size;
         QString lineString = QString( pLine, size );

         if ( d->m_bWordWrap )
         {
            size = d->m_diff3WrapLineVector[it].wrapLineLength;
            lineString = lineString.mid( d->m_diff3WrapLineVector[it].wrapLineOffset, size );
         }

         // Consider tabs
         int outPos = 0;
         for( int i=0; i<size; ++i )
         {
            int spaces = 1;
            if ( lineString[i]=='\t' )
            {
               spaces = tabber( outPos, d->m_pOptions->m_tabSize );
            }

            if( d->m_selection.within( line, outPos ) )
            {
               selectionString += lineString[i];
            }

            outPos += spaces;
         }

         if( d->m_selection.within( line, outPos ) &&
            !( d->m_bWordWrap && it+1<vectorSize && d3l == d->m_diff3WrapLineVector[it+1].pD3L ) 
           )
         {
            #if defined(_WIN32) || defined(Q_OS_OS2)
            selectionString += '\r';
            #endif
            selectionString += '\n';
         }
      }

      ++line;
   }

   return selectionString;
}

bool DiffTextWindow::findString( const QString& s, int& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive )
{
   int it = d3vLine;
   int endIt = bDirDown ? (int)d->m_pDiff3LineVector->size() : -1;
   int step =  bDirDown ? 1 : -1;
   int startPos = posInLine;

   for( ; it!=endIt; it+=step )
   {
      QString line = d->getString( it );
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

void DiffTextWindow::convertD3LCoordsToLineCoords( int d3LIdx, int d3LPos, int& line, int& pos )
{
   if( d->m_bWordWrap )
   {
      int wrapPos = d3LPos;
      int wrapLine = convertDiff3LineIdxToLine(d3LIdx);
      while ( wrapPos > d->m_diff3WrapLineVector[wrapLine].wrapLineLength )
      {
         wrapPos -= d->m_diff3WrapLineVector[wrapLine].wrapLineLength;
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
   if( d->m_bWordWrap )
   {
      d3LPos = pos;
      d3LIdx = convertLineToDiff3LineIdx( line );
      int wrapLine = convertDiff3LineIdxToLine(d3LIdx); // First wrap line belonging to this d3LIdx
      while ( wrapLine < line )
      {
         d3LPos += d->m_diff3WrapLineVector[wrapLine].wrapLineLength;
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
   d->m_selection.reset();
   if ( lastLine >= getNofLines() )
   {
      lastLine = getNofLines()-1;

      const Diff3Line* d3l = (*d->m_pDiff3LineVector)[convertLineToDiff3LineIdx(lastLine)];
      int line = -1;
      if ( d->m_winIdx==1 ) line = d3l->lineA;
      if ( d->m_winIdx==2 ) line = d3l->lineB;
      if ( d->m_winIdx==3 ) line = d3l->lineC;
      if (line>=0)
         endPos = d->m_pLineData[line].width( d->m_pOptions->m_tabSize);
   }

   if ( d->m_bWordWrap && d->m_pDiff3LineVector!=0 )
   {
      QString s1 = d->getString(firstLine);
      int firstWrapLine = convertDiff3LineIdxToLine(firstLine);
      int wrapStartPos = startPos;
      while ( wrapStartPos > d->m_diff3WrapLineVector[firstWrapLine].wrapLineLength )
      {
         wrapStartPos -= d->m_diff3WrapLineVector[firstWrapLine].wrapLineLength;
         s1 = s1.mid(d->m_diff3WrapLineVector[firstWrapLine].wrapLineLength);
         ++firstWrapLine;
      }

      QString s2 = d->getString(lastLine);
      int lastWrapLine = convertDiff3LineIdxToLine(lastLine);
      int wrapEndPos = endPos;
      while ( wrapEndPos > d->m_diff3WrapLineVector[lastWrapLine].wrapLineLength )
      {
         wrapEndPos -= d->m_diff3WrapLineVector[lastWrapLine].wrapLineLength;
         s2 = s2.mid(d->m_diff3WrapLineVector[lastWrapLine].wrapLineLength);
         ++lastWrapLine;
      }

      d->m_selection.start( firstWrapLine, convertToPosOnScreen( s1, wrapStartPos, d->m_pOptions->m_tabSize ) );
      d->m_selection.end( lastWrapLine, convertToPosOnScreen( s2, wrapEndPos, d->m_pOptions->m_tabSize ) );
      l=firstWrapLine;
      p=wrapStartPos;
   }
   else
   {
      d->m_selection.start( firstLine, convertToPosOnScreen( d->getString(firstLine), startPos, d->m_pOptions->m_tabSize ) );
      d->m_selection.end( lastLine, convertToPosOnScreen( d->getString(lastLine), endPos, d->m_pOptions->m_tabSize ) );
      l=firstLine;
      p=startPos;
   }
   update();
}

int DiffTextWindowData::convertLineOnScreenToLineInSource( int lineOnScreen, e_CoordType coordType, bool bFirstLine )
{
   int line=-1;
   if (lineOnScreen>=0)
   {
      if (coordType==eWrapCoords) return lineOnScreen;
      int d3lIdx = m_pDiffTextWindow->convertLineToDiff3LineIdx( lineOnScreen );
      if ( !bFirstLine && d3lIdx >= (int)m_pDiff3LineVector->size() )
         d3lIdx = m_pDiff3LineVector->size()-1;
      if (coordType==eD3LLineCoords) return d3lIdx;
      while ( line<0 && d3lIdx<(int)m_pDiff3LineVector->size() && d3lIdx>=0 )
      {
         const Diff3Line* d3l = (*m_pDiff3LineVector)[d3lIdx];
         if ( m_winIdx==1 ) line = d3l->lineA;
         if ( m_winIdx==2 ) line = d3l->lineB;
         if ( m_winIdx==3 ) line = d3l->lineC;
         if ( bFirstLine )
            ++d3lIdx;
         else
            --d3lIdx;
      }
      if (coordType==eFileCoords) return line;
   }
   return line;
}


void DiffTextWindow::getSelectionRange( int* pFirstLine, int* pLastLine, e_CoordType coordType )
{
   if (pFirstLine)
      *pFirstLine = d->convertLineOnScreenToLineInSource( d->m_selection.beginLine(), coordType, true );
   if (pLastLine)
      *pLastLine = d->convertLineOnScreenToLineInSource( d->m_selection.endLine(), coordType, false );
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
            int wsPos = max2( origLine.lastIndexOf( ' ', wrapPos ), origLine.lastIndexOf( '\t', wrapPos ) );

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
   if ( d->m_pDiff3LineVector==0 || ! d->m_bPaintingAllowed || !isVisible() || d->m_selection.isEmpty() )
   {
      return;
   }

   // convert the d->m_selection to unwrapped coordinates: Later restore to new coords
   int firstD3LIdx, firstD3LPos;
   QString s = d->getLineString( d->m_selection.beginLine() );
   int firstPosInText = convertToPosInText( s, d->m_selection.beginPos(), d->m_pOptions->m_tabSize );
   convertLineCoordsToD3LCoords( d->m_selection.beginLine(), firstPosInText, firstD3LIdx, firstD3LPos );

   int lastD3LIdx, lastD3LPos;
   s = d->getLineString( d->m_selection.endLine() );
   int lastPosInText = convertToPosInText( s, d->m_selection.endPos(), d->m_pOptions->m_tabSize );
   convertLineCoordsToD3LCoords( d->m_selection.endLine(), lastPosInText, lastD3LIdx, lastD3LPos );

   //d->m_selection.reset();
   d->m_selection.start( firstD3LIdx, firstD3LPos );
   d->m_selection.end( lastD3LIdx, lastD3LPos );
}

void DiffTextWindow::recalcWordWrap( bool bWordWrap, int wrapLineVectorSize, int nofVisibleColumns )
{
   if ( d->m_pDiff3LineVector==0 || ! d->m_bPaintingAllowed || !isVisible() )
   {
      d->m_bWordWrap = bWordWrap;
      if (!bWordWrap)  d->m_diff3WrapLineVector.resize( 0 );
      return;
   }

   d->m_bWordWrap = bWordWrap;

   if ( bWordWrap )
   {
      d->m_lineNumberWidth = d->m_pOptions->m_bShowLineNumbers ? (int)log10((double)qMax(d->m_size,1))+1 : 0;

      d->m_diff3WrapLineVector.resize( wrapLineVectorSize );

      if (nofVisibleColumns<0)
         nofVisibleColumns = getNofVisibleColumns();
      else
         nofVisibleColumns-= d->leftInfoWidth();
      int i;
      int wrapLineIdx = 0;
      int size = d->m_pDiff3LineVector->size();
      for( i=0; i<size; ++i )
      {
         QString s = d->getString( i );
         int linesNeeded = wordWrap( s, nofVisibleColumns, wrapLineVectorSize==0 ? 0 : &d->m_diff3WrapLineVector[wrapLineIdx] );
         Diff3Line& d3l = *(*d->m_pDiff3LineVector)[i];
         if ( d3l.linesNeededForDisplay<linesNeeded )
         {
            d3l.linesNeededForDisplay = linesNeeded;
         }

         if ( wrapLineVectorSize>0 )
         {
            int j;
            for( j=0; j<d3l.linesNeededForDisplay; ++j, ++wrapLineIdx )
            {
               Diff3WrapLine& d3wl = d->m_diff3WrapLineVector[wrapLineIdx];
               d3wl.diff3LineIndex = i;
               d3wl.pD3L = (*d->m_pDiff3LineVector)[i];
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
         d->m_firstLine = min2( d->m_firstLine, wrapLineVectorSize-1 );
         d->m_firstColumn = 0;
         d->m_pDiffTextWindowFrame->setFirstLine( d->m_firstLine );
      }
   }
   else
   {
      d->m_diff3WrapLineVector.resize( 0 );
   }

   if ( !d->m_selection.isEmpty() && ( !d->m_bWordWrap || wrapLineVectorSize>0 ) )
   {
      // Assume unwrapped coordinates 
      //( Why? ->Conversion to unwrapped coords happened a few lines above in this method. 
      //  Also see KDiff3App::recalcWordWrap() on the role of wrapLineVectorSize)

      // Wrap them now.

      // convert the d->m_selection to unwrapped coordinates.
      int firstLine, firstPos;
      convertD3LCoordsToLineCoords( d->m_selection.beginLine(), d->m_selection.beginPos(), firstLine, firstPos );

      int lastLine, lastPos;
      convertD3LCoordsToLineCoords( d->m_selection.endLine(), d->m_selection.endPos(), lastLine, lastPos );

      //d->m_selection.reset();
      d->m_selection.start( firstLine, convertToPosOnScreen( d->getLineString( firstLine ), firstPos, d->m_pOptions->m_tabSize ) );
      d->m_selection.end( lastLine, convertToPosOnScreen( d->getLineString( lastLine ),lastPos, d->m_pOptions->m_tabSize ) );
   }
}


class DiffTextWindowFrameData
{
public:
   DiffTextWindow* m_pDiffTextWindow;
   QLineEdit* m_pFileSelection;
   QPushButton* m_pBrowseButton;
   Options*      m_pOptions;
   QLabel*       m_pLabel;
   QLabel*       m_pTopLine;
   QLabel*       m_pEncoding;
   QLabel*       m_pLineEndStyle;
   QWidget*      m_pTopLineWidget;
   int           m_winIdx;

};

DiffTextWindowFrame::DiffTextWindowFrame( QWidget* pParent, QStatusBar* pStatusBar, Options* pOptions, int winIdx, SourceData* psd)
   : QWidget( pParent )
{
   d = new DiffTextWindowFrameData;
   d->m_winIdx = winIdx;
   setAutoFillBackground(true);
   d->m_pOptions = pOptions;
   d->m_pTopLineWidget = new QWidget(this);
   d->m_pFileSelection = new QLineEdit(d->m_pTopLineWidget);
   d->m_pBrowseButton = new QPushButton( "...",d->m_pTopLineWidget );
   d->m_pBrowseButton->setFixedWidth( 30 );
   connect(d->m_pBrowseButton,SIGNAL(clicked()), this, SLOT(slotBrowseButtonClicked()));
   connect(d->m_pFileSelection,SIGNAL(returnPressed()), this, SLOT(slotReturnPressed()));

   d->m_pLabel = new QLabel("A:",d->m_pTopLineWidget);
   d->m_pTopLine = new QLabel(d->m_pTopLineWidget);
   d->m_pDiffTextWindow = 0;
   d->m_pDiffTextWindow = new DiffTextWindow( this, pStatusBar, pOptions, winIdx );

   QVBoxLayout* pVTopLayout = new QVBoxLayout(d->m_pTopLineWidget);
   pVTopLayout->setMargin(2);
   pVTopLayout->setSpacing(0);
   QHBoxLayout* pHL = new QHBoxLayout();
   QHBoxLayout* pHL2 = new QHBoxLayout();
   pVTopLayout->addLayout(pHL);
   pVTopLayout->addLayout(pHL2);

   // Upper line:
   pHL->setMargin(0);
   pHL->setSpacing(2);

   pHL->addWidget( d->m_pLabel, 0 );
   pHL->addWidget( d->m_pFileSelection, 1 );
   pHL->addWidget( d->m_pBrowseButton, 0 );
   pHL->addWidget( d->m_pTopLine, 0 );

   // Lower line
   pHL2->setMargin(0);
   pHL2->setSpacing(2);
   pHL2->addWidget( d->m_pTopLine, 0 );
   d->m_pEncoding = new EncodingLabel(i18n("Encoding:"), this, psd, pOptions);

   d->m_pLineEndStyle = new QLabel(i18n("Line end style:"));
   pHL2->addWidget(d->m_pEncoding);
   pHL2->addWidget(d->m_pLineEndStyle);

   QVBoxLayout* pVL = new QVBoxLayout( this );
   pVL->setMargin(0);
   pVL->setSpacing(0);
   pVL->addWidget( d->m_pTopLineWidget, 0 );
   pVL->addWidget( d->m_pDiffTextWindow, 1 );

   d->m_pDiffTextWindow->installEventFilter( this );
   d->m_pFileSelection->installEventFilter( this );
   d->m_pBrowseButton->installEventFilter( this );
   init();
}

DiffTextWindowFrame::~DiffTextWindowFrame()
{
   delete d;
}

void DiffTextWindowFrame::init()
{
   DiffTextWindow* pDTW = d->m_pDiffTextWindow;
   if ( pDTW )
   {
      QString s = QDir::toNativeSeparators( pDTW->d->m_filename );
      d->m_pFileSelection->setText( s );
      QString winId = pDTW->d->m_winIdx==1 ? 
                             ( pDTW->d->m_bTriple?"A (Base)":"A") :
                             ( pDTW->d->m_winIdx==2 ? "B" : "C" );
      d->m_pLabel->setText( winId + ":" );
      d->m_pEncoding->setText( i18n("Encoding:") + " " + (pDTW->d->m_pTextCodec!=0 ? pDTW->d->m_pTextCodec->name() : QString()) );
      d->m_pLineEndStyle->setText( i18n("Line end style:") + " " + (pDTW->d->m_eLineEndStyle==eLineEndStyleDos ? i18n("DOS") : i18n("Unix")) );
   }
}

// Search for the first visible line (search loop needed when no line exist for this file.)
int DiffTextWindow::calcTopLineInFile( int firstLine )
{
   int l=-1;
   for ( int i = convertLineToDiff3LineIdx(firstLine); i<(int)d->m_pDiff3LineVector->size(); ++i )
   {
      const Diff3Line* d3l = (*d->m_pDiff3LineVector)[i];
      l = d3l->getLineInFile(d->m_winIdx);
      if (l!=-1) break;
   }
   return l;
}

void DiffTextWindowFrame::setFirstLine( int firstLine )
{
   DiffTextWindow* pDTW = d->m_pDiffTextWindow;
   if ( pDTW && pDTW->d->m_pDiff3LineVector )
   {
      QString s= i18n("Top line");
      int lineNumberWidth = (int)log10((double)qMax(pDTW->d->m_size,1))+1;

      int l=pDTW->calcTopLineInFile(firstLine);

      int w = d->m_pTopLine->fontMetrics().width(
            s+" "+QString().fill('0',lineNumberWidth));
      d->m_pTopLine->setMinimumWidth( w );

      if (l==-1)
         s = i18n("End");
      else
         s += " " + QString::number( l+1 );

      d->m_pTopLine->setText( s );
      d->m_pTopLine->repaint();
   }
}

DiffTextWindow* DiffTextWindowFrame::getDiffTextWindow()
{
   return d->m_pDiffTextWindow;
}

bool DiffTextWindowFrame::eventFilter( QObject* o, QEvent* e )
{
   DiffTextWindow* pDTW = d->m_pDiffTextWindow;
   if ( e->type()==QEvent::FocusIn || e->type()==QEvent::FocusOut )
   {
      QColor c1 = d->m_pOptions->m_bgColor;
      QColor c2;
      if      ( d->m_winIdx==1 ) c2 = d->m_pOptions->m_colorA;
      else if ( d->m_winIdx==2 ) c2 = d->m_pOptions->m_colorB;
      else if ( d->m_winIdx==3 ) c2 = d->m_pOptions->m_colorC;

      QPalette p = d->m_pTopLineWidget->palette();
      if ( e->type()==QEvent::FocusOut )
         std::swap(c1,c2);

      p.setColor(QPalette::Window, c2);
      setPalette( p );

      p.setColor(QPalette::WindowText, c1);
      d->m_pLabel->setPalette( p );
      d->m_pTopLine->setPalette( p );
      d->m_pEncoding->setPalette( p );
      d->m_pLineEndStyle->setPalette( p );
   }
   if (o == d->m_pFileSelection && e->type()==QEvent::Drop)
   {
      QDropEvent* d = static_cast<QDropEvent*>(e);

      if ( d->mimeData()->hasUrls() ) 
      {
         QList<QUrl> lst = d->mimeData()->urls();

         if ( lst.count() > 0 )
         {
            static_cast<QLineEdit*>(o)->setText( lst[0].toString() );
            static_cast<QLineEdit*>(o)->setFocus();
            emit fileNameChanged( lst[0].toString(), pDTW->d->m_winIdx );
            return true;
         }
      }
   }

   return false;
}

void DiffTextWindowFrame::slotReturnPressed()
{
   DiffTextWindow* pDTW = d->m_pDiffTextWindow;
   if ( pDTW->d->m_filename != d->m_pFileSelection->text() )
   {
      emit fileNameChanged( d->m_pFileSelection->text(), pDTW->d->m_winIdx );
   }
}

void DiffTextWindowFrame::slotBrowseButtonClicked()
{
    QString current = d->m_pFileSelection->text();

    KUrl newURL = KFileDialog::getOpenUrl( current, 0, this);
    if ( !newURL.isEmpty() )
    {
        DiffTextWindow* pDTW = d->m_pDiffTextWindow;
        emit fileNameChanged( newURL.url(), pDTW->d->m_winIdx );
    }
}

void DiffTextWindowFrame::sendEncodingChangedSignal(QTextCodec* c)
{
    emit encodingChanged(c);
}

EncodingLabel::EncodingLabel( const QString & text, DiffTextWindowFrame* pDiffTextWindowFrame, SourceData * pSD, Options* pOptions)
    : QLabel(text)
{
    m_pDiffTextWindowFrame = pDiffTextWindowFrame;
    m_pOptions = pOptions;
    m_pSourceData = pSD;
    m_pContextEncodingMenu = 0;
    setMouseTracking(true);
}

void EncodingLabel::mouseMoveEvent(QMouseEvent *)
{
    // When there is no data to display or it came from clipboard,
    // we will be use UTF-8 only,
    // in that case there is no possibility to change the encoding in the SourceData
    // so, we should hide the HandCursor and display usual ArrowCursor
    if (m_pSourceData->isFromBuffer()||m_pSourceData->isEmpty())
        setCursor(QCursor(Qt::ArrowCursor));
    else setCursor(QCursor(Qt::PointingHandCursor));
}


void EncodingLabel::mousePressEvent(QMouseEvent *)
{    
   if (!(m_pSourceData->isFromBuffer()||m_pSourceData->isEmpty()))
   {
      delete m_pContextEncodingMenu;
      m_pContextEncodingMenu = new QMenu(this);
      QMenu* pContextEncodingSubMenu = new QMenu(m_pContextEncodingMenu);

      int currentTextCodecEnum = m_pSourceData->getEncoding()->mibEnum(); // the codec that will be checked in the context menu
      QList<int> mibs = QTextCodec::availableMibs();
      QList<int> codecEnumList;

      // Adding "main" encodings
      insertCodec( i18n("Unicode, 8 bit"),  QTextCodec::codecForName("UTF-8"), codecEnumList, m_pContextEncodingMenu, currentTextCodecEnum);
      insertCodec( "", QTextCodec::codecForName("System"), codecEnumList, m_pContextEncodingMenu, currentTextCodecEnum);

      // Adding recent encodings
      if (m_pOptions!=0)
      {
         QStringList& recentEncodings = m_pOptions->m_recentEncodings;
         foreach(QString s, recentEncodings)
         {
            insertCodec("", QTextCodec::codecForName(s.toAscii()), codecEnumList, m_pContextEncodingMenu, currentTextCodecEnum);
         }
      }
      // Submenu to add the rest of available encodings
      pContextEncodingSubMenu->setTitle(i18n("Other"));
      foreach(int i, mibs)
      {
         QTextCodec* c = QTextCodec::codecForMib(i);
         if ( c!=0 ) 
            insertCodec("", c, codecEnumList, pContextEncodingSubMenu, currentTextCodecEnum);
      }

      m_pContextEncodingMenu->addMenu(pContextEncodingSubMenu);

      m_pContextEncodingMenu->exec(QCursor::pos());
   }
}

void EncodingLabel::insertCodec( const QString& visibleCodecName, QTextCodec* pCodec, QList<int> &codecEnumList, QMenu* pMenu, int currentTextCodecEnum)
{
   int CodecMIBEnum = pCodec->mibEnum();
   if ( pCodec!=0 && !codecEnumList.contains(CodecMIBEnum) )
   {
      QAction* pAction = new QAction( pMenu ); // menu takes ownership, so deleting the menu deletes the action too.
      pAction->setText( visibleCodecName.isEmpty() ? QString(pCodec->name()) : visibleCodecName+" ("+pCodec->name()+")" );
      pAction->setData(CodecMIBEnum);
      pAction->setCheckable(true);
      if (currentTextCodecEnum==CodecMIBEnum) 
         pAction->setChecked(true);
      pMenu->addAction(pAction);
      connect(pAction, SIGNAL(triggered()), this, SLOT(slotEncodingChanged()));
      codecEnumList.append(CodecMIBEnum);
   }
}

void EncodingLabel::slotEncodingChanged()
{
   QAction *pAction = qobject_cast<QAction *>(sender());
   if (pAction)
   {
      QTextCodec * pCodec = QTextCodec::codecForMib(pAction->data().toInt());
      if (pCodec!=0)
      {
         QString s( pCodec->name() );
         QStringList& recentEncodings = m_pOptions->m_recentEncodings;
         if ( !recentEncodings.contains(s) && s!="UTF-8" && s!="System" )
         {
            int itemsToRemove = recentEncodings.size() - m_maxRecentEncodings + 1;
            for (int i=0; i<itemsToRemove; ++i)
            {
               recentEncodings.removeFirst();
            }
            recentEncodings.append(s);
         }
      }
      m_pDiffTextWindowFrame->sendEncodingChangedSignal(pCodec);
   }
}
