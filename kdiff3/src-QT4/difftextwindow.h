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

#include <QLabel>

class QMenu;
class QStatusBar;
class Options;
class DiffTextWindowData;
class DiffTextWindowFrame;
class EncodingLabel;

class DiffTextWindow : public QWidget
{
   Q_OBJECT
public:
   DiffTextWindow(
      DiffTextWindowFrame* pParent,
      QStatusBar* pStatusBar,
      Options* pOptions,

      int winIdx
      );
   ~DiffTextWindow();
   void init(
      const QString& fileName,
      QTextCodec* pCodec,
      e_LineEndStyle eLineEndStyle,
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
   DiffTextWindowFrame( QWidget* pParent, QStatusBar* pStatusBar, Options* pOptions, int winIdx, SourceData* psd);
   ~DiffTextWindowFrame();
   DiffTextWindow* getDiffTextWindow();
   void init();
   void setFirstLine(int firstLine);
   void sendEncodingChangedSignal(QTextCodec* c);
signals:
   void fileNameChanged(const QString&, int);
   void encodingChanged(QTextCodec*);
protected:
   bool eventFilter( QObject*, QEvent* );
   //void paintEvent(QPaintEvent*);
private slots:
   void slotReturnPressed();
   void slotBrowseButtonClicked();
private:
   DiffTextWindowFrameData* d;
};

class EncodingLabel : public QLabel
{
   Q_OBJECT
public:
   EncodingLabel( const QString & text, DiffTextWindowFrame* pDiffTextWindowFrame, SourceData* psd, Options* pOptions);
protected:
   void mouseMoveEvent(QMouseEvent *ev);
   void mousePressEvent(QMouseEvent *ev);
private slots:
   void slotEncodingChanged();
private:
   DiffTextWindowFrame* m_pDiffTextWindowFrame; //To send "EncodingChanged" signal
   QMenu* m_pContextEncodingMenu;
   SourceData* m_pSourceData; //SourceData to get access to "isEmpty()" and "isFromBuffer()" functions
   static const int m_maxRecentEncodings  = 5;
   Options* m_pOptions;

   void insertCodec( const QString& visibleCodecName, QTextCodec* pCodec, QList<int> &CodecEnumList, QMenu* pMenu, int currentTextCodecEnum);
};

#endif

