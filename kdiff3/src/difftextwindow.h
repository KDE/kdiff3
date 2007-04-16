/***************************************************************************
                          difftextwindow.h  -  description
                             -------------------
    begin                : Mon Mar 18 2002
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

#ifndef DIFFTEXTWINDOW_H
#define DIFFTEXTWINDOW_H

#include "diff.h"

#include <qwidget.h>

class QStatusBar;
class OptionDialog;
class DiffTextWindowData;
class DiffTextWindowFrame;

class DiffTextWindow : public QWidget
{
   Q_OBJECT
public:
   DiffTextWindow(
      DiffTextWindowFrame* pParent,
      QStatusBar* pStatusBar,
      OptionDialog* pOptionDialog,
      int winIdx
      );
   ~DiffTextWindow();
   void init(
      const QString& fileName,
      const LineData* pLineData,
      int size,
      const Diff3LineVector* pDiff3LineVector,
      const ManualDiffHelpList* pManualDiffHelpList,
      bool bTriple
      );
   void reset();
   void convertToLinePos( int x, int y, int& line, int& pos );

   QString getSelection();
   int getFirstLine();
   int calcTopLineInFile( int firstLine );

   int getNofColumns();
   int getNofLines();
   int getNofVisibleLines();
   int getNofVisibleColumns();

   int convertLineToDiff3LineIdx( int line );
   int convertDiff3LineIdxToLine( int d3lIdx );

   void convertD3LCoordsToLineCoords( int d3LIdx, int d3LPos, int& line, int& pos );
   void convertLineCoordsToD3LCoords( int line, int pos, int& d3LIdx, int& d3LPos );

   void convertSelectionToD3LCoords();

   bool findString( const QString& s, int& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive );
   void setSelection( int firstLine, int startPos, int lastLine, int endPos, int& l, int& p );
   void getSelectionRange( int* firstLine, int* lastLine, e_CoordType coordType );

   void setPaintingAllowed( bool bAllowPainting );
   void recalcWordWrap( bool bWordWrap, int wrapLineVectorSize, int nofVisibleColumns );
   void print( MyPainter& painter, const QRect& r, int firstLine, int nofLinesPerPage );
signals:
   void resizeSignal( int nofVisibleColumns, int nofVisibleLines );
   void scroll( int deltaX, int deltaY );
   void newSelection();
   void selectionEnd();
   void setFastSelectorLine( int line );
   void gotFocus();
   void lineClicked( int winIdx, int line );

public slots:
   void setFirstLine( int line );
   void setFirstColumn( int col );
   void resetSelection();
   void setFastSelectorRange( int line1, int nofLines );

protected:
   virtual void mousePressEvent ( QMouseEvent * );
   virtual void mouseReleaseEvent ( QMouseEvent * );
   virtual void mouseMoveEvent ( QMouseEvent * );
   virtual void mouseDoubleClickEvent ( QMouseEvent * e );

   virtual void paintEvent( QPaintEvent*  );
   virtual void dragEnterEvent( QDragEnterEvent* e );
   virtual void focusInEvent( QFocusEvent* e );

   virtual void resizeEvent( QResizeEvent* );
   virtual void timerEvent(QTimerEvent*);

private:
   DiffTextWindowData* d;
   void showStatusLine( int line );
   friend class DiffTextWindowFrame;
};


class DiffTextWindowFrameData;

class DiffTextWindowFrame : public QWidget
{
   Q_OBJECT
public:
   DiffTextWindowFrame( QWidget* pParent, QStatusBar* pStatusBar, OptionDialog* pOptionDialog, int winIdx );
   ~DiffTextWindowFrame();
   DiffTextWindow* getDiffTextWindow();
   void init();
   void setFirstLine(int firstLine);
signals:
   void fileNameChanged(const QString&, int);
protected:
   bool eventFilter( QObject*, QEvent* );
private slots:
   void slotReturnPressed();
   void slotBrowseButtonClicked();
private:
   DiffTextWindowFrameData* d;
};


#endif

