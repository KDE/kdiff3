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
   ~DiffTextWindow() override;
   void init(
      const QString& fileName,
      QTextCodec* pCodec,
      e_LineEndStyle eLineEndStyle,
      const LineData* pLineData,
      LineRef size,
      const Diff3LineVector* pDiff3LineVector,
      const ManualDiffHelpList* pManualDiffHelpList,
      bool bTriple
      );
   void reset();
   void convertToLinePos( int x, int y, LineRef& line, int& pos );

   QString getSelection();
   int getFirstLine();
   LineRef calcTopLineInFile( int firstLine );

   int getMaxTextWidth();
   int getNofLines();
   int getNofVisibleLines();
   int getVisibleTextAreaWidth();

   LineRef convertLineToDiff3LineIdx( LineRef line );
   LineRef convertDiff3LineIdxToLine( LineRef d3lIdx );

   void convertD3LCoordsToLineCoords( LineRef d3LIdx, int d3LPos, LineRef& line, int& pos );
   void convertLineCoordsToD3LCoords( LineRef line, int pos, LineRef& d3LIdx, int& d3LPos );

   void convertSelectionToD3LCoords();

   bool findString( const QString& s, int& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive );
   void setSelection( LineRef firstLine, int startPos, LineRef lastLine, int endPos, LineRef& l, int& p );
   void getSelectionRange( LineRef* firstLine, LineRef* lastLine, e_CoordType coordType );

   void setPaintingAllowed( bool bAllowPainting );
   void recalcWordWrap( bool bWordWrap, int wrapLineVectorSize, int nofVisibleColumns);
   void recalcWordWrapHelper( int wrapLineVectorSize, int visibleTextWidth, int cacheListIdx);
   void print( MyPainter& painter, const QRect& r, int firstLine, int nofLinesPerPage );
Q_SIGNALS:
   void resizeHeightChangedSignal(int nofVisibleLines);
   void resizeWidthChangedSignal(int nofVisibleColumns);
   void newSelection();
   void selectionEnd();
   void setFastSelectorLine( int line );
   void gotFocus();
   void lineClicked( int winIdx, LineRef line );

public Q_SLOTS:
   void setFirstLine( LineRef line );
   void setHorizScrollOffset( int horizScrollOffset );
   void resetSelection();
   void setFastSelectorRange( LineRef line1, LineRef nofLines );

protected:
   void mousePressEvent ( QMouseEvent * ) override;
   void mouseReleaseEvent ( QMouseEvent * ) override;
   void mouseMoveEvent ( QMouseEvent * ) override;
   void mouseDoubleClickEvent ( QMouseEvent * e ) override;

   void paintEvent( QPaintEvent*  ) override;
   void dragEnterEvent( QDragEnterEvent* e ) override;
   void focusInEvent( QFocusEvent* e ) override;

   void resizeEvent( QResizeEvent* ) override;
   void timerEvent(QTimerEvent*) override;

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
   ~DiffTextWindowFrame() override;
   DiffTextWindow* getDiffTextWindow();
   void init();
   void setFirstLine(int firstLine);
   void sendEncodingChangedSignal(QTextCodec* c);
Q_SIGNALS:
   void fileNameChanged(const QString&, int);
   void encodingChanged(QTextCodec*);
protected:
   bool eventFilter( QObject*, QEvent* ) override;
   //void paintEvent(QPaintEvent*);
private Q_SLOTS:
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
   void mouseMoveEvent(QMouseEvent *ev) override;
   void mousePressEvent(QMouseEvent *ev) override;
private Q_SLOTS:
   void slotEncodingChanged();
private:
   DiffTextWindowFrame* m_pDiffTextWindowFrame; //To send "EncodingChanged" signal
   QMenu* m_pContextEncodingMenu;
   SourceData* m_pSourceData; //SourceData to get access to "isEmpty()" and "isFromBuffer()" functions
   static const int m_maxRecentEncodings  = 5;
   Options* m_pOptions;

   void insertCodec( const QString& visibleCodecName, QTextCodec* pCodec, QList<int> &CodecEnumList, QMenu* pMenu, int currentTextCodecEnum);
};

bool startRunnables();

#endif

