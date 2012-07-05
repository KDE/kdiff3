/***************************************************************************
                          mergeresultwindow.h  -  description
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

#ifndef MERGERESULTWINDOW_H
#define MERGERESULTWINDOW_H

#include "diff.h"

#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QStatusBar>

class QPainter;

class Overview : public QWidget
{
   Q_OBJECT
public:
   Overview( Options* pOptions );

   void init( Diff3LineList* pDiff3LineList, bool bTripleDiff );
   void reset();
   void setRange( int firstLine, int pageHeight );
   void setPaintingAllowed( bool bAllowPainting );

   enum e_OverviewMode { eOMNormal, eOMAvsB, eOMAvsC, eOMBvsC };
   void setOverviewMode( e_OverviewMode eOverviewMode );
   e_OverviewMode getOverviewMode();

public slots:
   void setFirstLine(int firstLine);
   void slotRedraw();
signals:
   void setLine(int);
private:
   const Diff3LineList* m_pDiff3LineList;
   Options* m_pOptions;
   bool m_bTripleDiff;
   int m_firstLine;
   int m_pageHeight;
   QPixmap m_pixmap;
   bool m_bPaintingAllowed;
   e_OverviewMode m_eOverviewMode;
   int m_nofLines;

   virtual void paintEvent( QPaintEvent* e );
   virtual void mousePressEvent( QMouseEvent* e );
   virtual void mouseMoveEvent( QMouseEvent* e );
   void drawColumn( QPainter& p, e_OverviewMode eOverviewMode, int x, int w, int h, int nofLines );
};


enum e_MergeDetails
{
   eDefault,
   eNoChange,
   eBChanged,
   eCChanged,
   eBCChanged,         // conflict
   eBCChangedAndEqual, // possible conflict
   eBDeleted,
   eCDeleted,
   eBCDeleted,         // possible conflict

   eBChanged_CDeleted, // conflict
   eCChanged_BDeleted, // conflict
   eBAdded,
   eCAdded,
   eBCAdded,           // conflict
   eBCAddedAndEqual    // possible conflict
};

void mergeOneLine( const Diff3Line& d, e_MergeDetails& mergeDetails, bool& bConflict, bool& bLineRemoved, int& src, bool bTwoInputs );

enum e_MergeSrcSelector
{
   A=1,
   B=2,
   C=3
};

class MergeResultWindow : public QWidget
{
   Q_OBJECT
public:
   MergeResultWindow(
      QWidget* pParent,
      Options* pOptions,
      QStatusBar* pStatusBar
      );

   void init(
      const LineData* pLineDataA, int sizeA,
      const LineData* pLineDataB, int sizeB,
      const LineData* pLineDataC, int sizeC,
      const Diff3LineList* pDiff3LineList,
      TotalDiffStatus* pTotalDiffStatus
      );

   void reset();

   bool saveDocument( const QString& fileName, QTextCodec* pEncoding, e_LineEndStyle eLineEndStyle );
   int getNrOfUnsolvedConflicts(int* pNrOfWhiteSpaceConflicts=0);
   void choose(int selector);
   void chooseGlobal(int selector, bool bConflictsOnly, bool bWhiteSpaceOnly );

   int getNofColumns();
   int getNofLines();
   int getNofVisibleColumns();
   int getNofVisibleLines();
   QString getSelection();
   void resetSelection();
   void showNrOfConflicts();
   bool isDeltaAboveCurrent();
   bool isDeltaBelowCurrent();
   bool isConflictAboveCurrent();
   bool isConflictBelowCurrent();
   bool isUnsolvedConflictAtCurrent();
   bool isUnsolvedConflictAboveCurrent();
   bool isUnsolvedConflictBelowCurrent();
   bool findString( const QString& s, int& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive );
   void setSelection( int firstLine, int startPos, int lastLine, int endPos );
   void setOverviewMode( Overview::e_OverviewMode eOverviewMode );
   Overview::e_OverviewMode getOverviewMode();
public slots:
   void setFirstLine(int firstLine);
   void setFirstColumn(int firstCol);

   void slotGoCurrent();
   void slotGoTop();
   void slotGoBottom();
   void slotGoPrevDelta();
   void slotGoNextDelta();
   void slotGoPrevUnsolvedConflict();
   void slotGoNextUnsolvedConflict();
   void slotGoPrevConflict();
   void slotGoNextConflict();
   void slotAutoSolve();
   void slotUnsolve();
   void slotMergeHistory();
   void slotRegExpAutoMerge();
   void slotSplitDiff( int firstD3lLineIdx, int lastD3lLineIdx );
   void slotJoinDiffs( int firstD3lLineIdx, int lastD3lLineIdx );
   void slotSetFastSelectorLine(int);
   void setPaintingAllowed(bool);
   void updateSourceMask();

signals:
   void scroll( int deltaX, int deltaY );
   void modifiedChanged(bool bModified);
   void setFastSelectorRange( int line1, int nofLines );
   void sourceMask( int srcMask, int enabledMask );
   void resizeSignal();
   void selectionEnd();
   void newSelection();
   void updateAvailabilities();
   void showPopupMenu( const QPoint& point );
   void noRelevantChangesDetected();

private:
   void merge(bool bAutoSolve, int defaultSelector, bool bConflictsOnly=false, bool bWhiteSpaceOnly=false );
   QString getString( int lineIdx );

   Options* m_pOptions;

   const LineData* m_pldA;
   const LineData* m_pldB;
   const LineData* m_pldC;
   int m_sizeA;
   int m_sizeB;
   int m_sizeC;

   const Diff3LineList* m_pDiff3LineList;
   TotalDiffStatus* m_pTotalDiffStatus;

   bool m_bPaintingAllowed;
   int m_delayedDrawTimer;
   Overview::e_OverviewMode m_eOverviewMode;

private:
   class MergeEditLine
   {
   public:
      MergeEditLine(Diff3LineList::const_iterator i, int src=0){m_id3l=i; m_src=src; m_bLineRemoved=false; }
      void setConflict() { m_src=0; m_bLineRemoved=false; m_str=QString(); }
      bool isConflict()  { return  m_src==0 && !m_bLineRemoved && m_str.isNull(); }
      void setRemoved(int src=0)  { m_src=src; m_bLineRemoved=true; m_str=QString(); }
      bool isRemoved()   { return m_bLineRemoved; }
      bool isEditableText() { return !isConflict() && !isRemoved(); }
      void setString( const QString& s ){ m_str=s; m_bLineRemoved=false; m_src=0; }
      QString getString( const MergeResultWindow* );
      bool isModified() { return ! m_str.isNull() ||  (m_bLineRemoved && m_src==0); }

      void setSource( int src, bool bLineRemoved ) { m_src=src; m_bLineRemoved =bLineRemoved; }
      int src() { return m_src; }
      Diff3LineList::const_iterator id3l(){return m_id3l;}
      // getString() is implemented as MergeResultWindow::getString()
   private:
      Diff3LineList::const_iterator m_id3l;
      int m_src;         // 1, 2 or 3 for A, B or C respectively, or 0 when line is from neither source.
      QString m_str;    // String when modified by user or null-string when orig data is used.
      bool m_bLineRemoved;
   };

   class MergeEditLineList : private std::list<MergeEditLine>
   { // I want to know the size immediately!
   private:
      typedef std::list<MergeEditLine> BASE;
      int m_size;
      int* m_pTotalSize;
   public:
      typedef std::list<MergeEditLine>::iterator iterator;
      typedef std::list<MergeEditLine>::reverse_iterator reverse_iterator;
      typedef std::list<MergeEditLine>::const_iterator const_iterator;
      MergeEditLineList(){m_size=0; m_pTotalSize=0; }
      void clear()                             { ds(-m_size); BASE::clear();          }
      void push_back( const MergeEditLine& m)  { ds(+1); BASE::push_back(m);     }
      void push_front( const MergeEditLine& m) { ds(+1); BASE::push_front(m);    }
      void pop_back()                          { ds(-1); BASE::pop_back();    }
      iterator erase( iterator i )             { ds(-1); return BASE::erase(i);  }
      iterator insert( iterator i, const MergeEditLine& m ) { ds(+1); return BASE::insert(i,m); }
      int size(){ if (!m_pTotalSize) m_size = BASE::size(); return m_size; }
      iterator begin(){return BASE::begin();}
      iterator end(){return BASE::end();}
      reverse_iterator rbegin(){return BASE::rbegin();}
      reverse_iterator rend(){return BASE::rend();}
      MergeEditLine& front(){return BASE::front();}
      MergeEditLine& back(){return BASE::back();}
      bool empty() { return m_size==0; }
      void splice(iterator destPos, MergeEditLineList& srcList, iterator srcFirst, iterator srcLast)
      {
         int* pTotalSize = getTotalSizePtr() ? getTotalSizePtr() : srcList.getTotalSizePtr();
         srcList.setTotalSizePtr(0); // Force size-recalc after splice, because splice doesn't handle size-tracking
         setTotalSizePtr(0);
         BASE::splice( destPos, srcList, srcFirst, srcLast );
         srcList.setTotalSizePtr( pTotalSize );
         setTotalSizePtr( pTotalSize );
      }

      void setTotalSizePtr(int* pTotalSize)
      {
         if ( pTotalSize==0 && m_pTotalSize!=0 ) { *m_pTotalSize -= size(); }
         else if ( pTotalSize!=0 && m_pTotalSize==0 ) { *pTotalSize += size(); }
         m_pTotalSize = pTotalSize;
      }
      int* getTotalSizePtr()
      {
         return m_pTotalSize;
      }

   private:
      void ds(int deltaSize) 
      {
         m_size+=deltaSize; 
         if (m_pTotalSize!=0)  *m_pTotalSize+=deltaSize;
      }
   };

   friend class MergeEditLine;

   struct MergeLine
   {
      MergeLine()
      {
         srcSelect=0; mergeDetails=eDefault; d3lLineIdx = -1; srcRangeLength=0;
         bConflict=false; bDelta=false; bWhiteSpaceConflict=false;
      }
      Diff3LineList::const_iterator id3l;
      int d3lLineIdx;  // Needed to show the correct window pos.
      int srcRangeLength; // how many src-lines have this properties
      e_MergeDetails mergeDetails;
      bool bConflict;
      bool bWhiteSpaceConflict;
      bool bDelta;
      int srcSelect;
      MergeEditLineList mergeEditLineList;
      void split( MergeLine& ml2, int d3lLineIdx2 ) // The caller must insert the ml2 after this ml in the m_mergeLineList
      {
         if ( d3lLineIdx2<d3lLineIdx || d3lLineIdx2 >= d3lLineIdx + srcRangeLength ) 
            return; //Error
         ml2.mergeDetails = mergeDetails;
         ml2.bConflict = bConflict;
         ml2.bWhiteSpaceConflict = bWhiteSpaceConflict;
         ml2.bDelta = bDelta;
         ml2.srcSelect = srcSelect;

         ml2.d3lLineIdx = d3lLineIdx2;
         ml2.srcRangeLength = srcRangeLength - (d3lLineIdx2-d3lLineIdx);
         srcRangeLength = d3lLineIdx2-d3lLineIdx; // current MergeLine controls fewer lines
         ml2.id3l = id3l;
         for(int i=0; i<srcRangeLength; ++i)
            ++ml2.id3l;

         ml2.mergeEditLineList.clear();
         // Search for best place to splice
         for(MergeEditLineList::iterator i=mergeEditLineList.begin(); i!=mergeEditLineList.end();++i)
         {
            if (i->id3l()==ml2.id3l)
            {
               ml2.mergeEditLineList.splice( ml2.mergeEditLineList.begin(), mergeEditLineList, i, mergeEditLineList.end() );
               return;
            }
         }
         ml2.mergeEditLineList.setTotalSizePtr( mergeEditLineList.getTotalSizePtr() );
         ml2.mergeEditLineList.push_back(MergeEditLine(ml2.id3l));
      }
      void join( MergeLine& ml2 ) // The caller must remove the ml2 from the m_mergeLineList after this call
      {
         srcRangeLength += ml2.srcRangeLength;
         ml2.mergeEditLineList.clear();
         mergeEditLineList.clear();
         mergeEditLineList.push_back(MergeEditLine(id3l)); // Create a simple conflict
         if ( ml2.bConflict ) bConflict = true;
         if ( !ml2.bWhiteSpaceConflict ) bWhiteSpaceConflict = false;
         if ( ml2.bDelta ) bDelta = true;
      }
   };

private:
   static bool sameKindCheck( const MergeLine& ml1, const MergeLine& ml2 );
   struct HistoryMapEntry
   {
      MergeEditLineList mellA;
      MergeEditLineList mellB;
      MergeEditLineList mellC;
      MergeEditLineList& choice( bool bThreeInputs );
      bool staysInPlace( bool bThreeInputs, Diff3LineList::const_iterator& iHistoryEnd );
   };
   typedef std::map<QString,HistoryMapEntry> HistoryMap;
   void collectHistoryInformation( int src, Diff3LineList::const_iterator iHistoryBegin, Diff3LineList::const_iterator iHistoryEnd, HistoryMap& historyMap, std::list< HistoryMap::iterator >& hitList );

   typedef std::list<MergeLine> MergeLineList;
   MergeLineList m_mergeLineList;
   MergeLineList::iterator m_currentMergeLineIt;
   bool isItAtEnd( bool bIncrement, MergeLineList::iterator i )
   {
      if ( bIncrement ) return i!=m_mergeLineList.end();
      else              return i!=m_mergeLineList.begin();
   }

   int m_currentPos;
   bool checkOverviewIgnore(MergeLineList::iterator &i);

   enum e_Direction { eUp, eDown };
   enum e_EndPoint  { eDelta, eConflict, eUnsolvedConflict, eLine, eEnd };
   void go( e_Direction eDir, e_EndPoint eEndPoint );
   void calcIteratorFromLineNr(
      int line,
      MergeLineList::iterator& mlIt,
      MergeEditLineList::iterator& melIt
      );
   MergeLineList::iterator splitAtDiff3LineIdx( int d3lLineIdx );

   virtual void paintEvent( QPaintEvent* e );


   void myUpdate(int afterMilliSecs);
   virtual void timerEvent(QTimerEvent*);
   void writeLine(
      MyPainter& p, int line, const QString& str,
      int srcSelect, e_MergeDetails mergeDetails, int rangeMark, bool bUserModified, bool bLineRemoved, bool bWhiteSpaceConflict
      );
   void setFastSelector(MergeLineList::iterator i);
   void convertToLinePos( int x, int y, int& line, int& pos );
   bool event(QEvent*);
   virtual void mousePressEvent ( QMouseEvent* e );
   virtual void mouseDoubleClickEvent ( QMouseEvent* e );
   virtual void mouseReleaseEvent ( QMouseEvent * );
   virtual void mouseMoveEvent ( QMouseEvent * );
   virtual void resizeEvent( QResizeEvent* e );
   virtual void keyPressEvent( QKeyEvent* e );
   virtual void wheelEvent( QWheelEvent* e );
   virtual void focusInEvent( QFocusEvent* e );

   QPixmap m_pixmap;
   int m_firstLine;
   int m_firstColumn;
   int m_nofColumns;
   int m_nofLines;
   int m_totalSize; //Same as m_nofLines, but calculated differently
   bool m_bMyUpdate;
   bool m_bInsertMode;
   bool m_bModified;
   void setModified(bool bModified=true);

   int m_scrollDeltaX;
   int m_scrollDeltaY;
   int m_cursorXPos;
   int m_cursorYPos;
   int m_cursorOldXPos;
   bool m_bCursorOn; // blinking on and off each second
   QTimer m_cursorTimer;
   bool m_bCursorUpdate;
   QStatusBar* m_pStatusBar;

   Selection m_selection;

   bool deleteSelection2( QString& str, int& x, int& y,
                    MergeLineList::iterator& mlIt, MergeEditLineList::iterator& melIt );
   bool doRelevantChangesExist();
public slots:
   void deleteSelection();
   void pasteClipboard(bool bFromSelection);
private slots:
   void slotCursorUpdate();
};

class QLineEdit;
class QTextCodec;
class QComboBox;
class QLabel;
class WindowTitleWidget : public QWidget
{
   Q_OBJECT
private:
   QLabel*      m_pLabel;
   QLineEdit*   m_pFileNameLineEdit;
   //QPushButton* m_pBrowseButton;
   QLabel*      m_pModifiedLabel;
   QLabel*      m_pLineEndStyleLabel;
   QComboBox*   m_pLineEndStyleSelector;
   QLabel*      m_pEncodingLabel;
   QComboBox*   m_pEncodingSelector;
   Options*     m_pOptions;
public:
   WindowTitleWidget(Options* pOptions);
   QTextCodec* getEncoding();
   void        setFileName(const QString& fileName );
   QString     getFileName();
   void setEncodings( QTextCodec* pCodecForA, QTextCodec* pCodecForB, QTextCodec* pCodecForC );
   void setEncoding( QTextCodec* pCodec );
   void setLineEndStyles( e_LineEndStyle eLineEndStyleA, e_LineEndStyle eLineEndStyleB, e_LineEndStyle eLineEndStyleC);
   e_LineEndStyle getLineEndStyle();

   bool eventFilter( QObject* o, QEvent* e );
public slots:
   void slotSetModified( bool bModified );
//private slots:
//   void slotBrowseButtonClicked();

};

#endif

