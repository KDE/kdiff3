/***************************************************************************
                          diff.h  -  description
                             -------------------
    begin                : Mon Mar 18 2002
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
 * Revision 1.1  2003/10/06 18:38:48  joachim99
 * KDiff3 version 0.9.70
 *                                                                   *
 ***************************************************************************/

#ifndef DIFF_H
#define DIFF_H

#include <qwidget.h>
#include <qpixmap.h>
#include <qtimer.h>
#include <qframe.h>
#include <list>
#include <vector>
#include <assert.h>
#include "common.h"
#include "fileaccess.h"



// Each range with matching elements is followed by a range with differences on either side.
// Then again range of matching elements should follow.
struct Diff
{
   int nofEquals;

   int diff1;
   int diff2;

   Diff(int eq, int d1, int d2){nofEquals=eq; diff1=d1; diff2=d2; }
};

typedef std::list<Diff> DiffList;

struct Diff3Line
{
   int lineA;
   int lineB;
   int lineC;

   bool bAEqC;                 // These are true if equal or only white-space changes exist.
   bool bBEqC;
   bool bAEqB;

   DiffList* pFineAB;          // These are 0 only if completely equal. 
   DiffList* pFineBC;
   DiffList* pFineCA;
   
   bool bWhiteLineA;
   bool bWhiteLineB;
   bool bWhiteLineC;

   Diff3Line()
   {
      lineA=-1; lineB=-1; lineC=-1;
      bAEqC=false; bAEqB=false; bBEqC=false;
      pFineAB=0; pFineBC=0; pFineCA=0;
   }

   ~Diff3Line()
   {
      if (pFineAB!=0) delete pFineAB;
      if (pFineBC!=0) delete pFineBC;
      if (pFineCA!=0) delete pFineCA;
      pFineAB=0; pFineBC=0; pFineCA=0;
   }

   bool operator==( const Diff3Line& d3l )
   {
      return lineA == d3l.lineA  &&  lineB == d3l.lineB  &&  lineC == d3l.lineC  
         && bAEqB == d3l.bAEqB  && bAEqC == d3l.bAEqC  && bBEqC == d3l.bBEqC;
   }
};

typedef std::list<Diff3Line> Diff3LineList;
typedef std::vector<const Diff3Line*> Diff3LineVector;

class TotalDiffStatus
{
public:
   void reset() {bBinaryAEqC=false; bBinaryBEqC=false; bBinaryAEqB=false;
                 bTextAEqC=false;   bTextBEqC=false;   bTextAEqB=false;}
   bool bBinaryAEqC;
   bool bBinaryBEqC;
   bool bBinaryAEqB;

   bool bTextAEqC;
   bool bTextBEqC;
   bool bTextAEqB;
};

void calcDiff3LineListUsingAB(       
   const DiffList* pDiffListAB,
   Diff3LineList& d3ll
   );

void calcDiff3LineListUsingAC(
   const DiffList* pDiffListBC,
   Diff3LineList& d3ll
   );

void calcDiff3LineListUsingBC(       
   const DiffList* pDiffListBC,
   Diff3LineList& d3ll
   );

struct LineData
{
   const char* pLine;
   const char* pFirstNonWhiteChar;
   int size;

   LineData(){ pLine=0; size=0; occurances=0; }
   int width();  // Calcs width considering tabs.
   int occurances;
   bool whiteLine(){ return pFirstNonWhiteChar-pLine == size; }
};

void prepareOccurances( LineData* p, int size );


class SourceData
{
public:
   SourceData(){ m_pBuf=0;m_size=0;m_vSize=0;m_bIsText=false;m_bPreserve=false; }
   const char* m_pBuf;
   int m_size;
   int m_vSize; // Nr of lines in m_pBuf1 and size of m_v1, m_dv12 and m_dv13
   std::vector<LineData> m_v;
   bool m_bIsText;
   bool m_bPreserve;
   void reset();
   void readPPFile( bool bPreserveCR, const QString& ppCmd, bool bUpCase );
   void readLMPPFile( SourceData* pOrigSource, const QString& ppCmd, bool bUpCase );
   void readFile(const QString& filename, bool bFollowLinks, bool bUpCase );
   void preprocess(bool bPreserveCR );
   void setData( const QString& data, bool bUpCase );
   void setFilename(const QString& filename);
   void setFileAccess( const FileAccess& fa );
   FileAccess& getFileAccess();
   QString getFilename();
   void setAliasName(const QString& a);
   QString getAliasName();
   bool isEmpty() { return getFilename().isEmpty(); }
private:
   QString m_fileName;
   QString m_aliasName;
   FileAccess m_fileAccess;
};

void calcDiff3LineListTrim( Diff3LineList& d3ll, LineData* pldA, LineData* pldB, LineData* pldC );
void calcWhiteDiff3Lines(   Diff3LineList& d3ll, LineData* pldA, LineData* pldB, LineData* pldC );

void calcDiff3LineVector( const Diff3LineList& d3ll, Diff3LineVector& d3lv );

void debugLineCheck( Diff3LineList& d3ll, int size, int idx );

class QStatusBar;



class Selection
{
public:
   Selection(){ reset(); oldLastLine=-1; lastLine=-1; oldFirstLine=-1; }
   int firstLine;
   int firstPos;
   int lastLine;
   int lastPos;
   int oldLastLine;
   int oldFirstLine;
   bool bSelectionContainsData;
   bool isEmpty() { return firstLine==-1 || (firstLine==lastLine && firstPos==lastPos) || bSelectionContainsData==false;}
   void reset(){
      oldFirstLine=firstLine;
      oldLastLine =lastLine;
      firstLine=-1;
      bSelectionContainsData = false;
   }
   void start( int l, int p ) { firstLine = l; firstPos = p; }
   void end( int l, int p )  {
      if ( oldLastLine == -1 )
         oldLastLine = lastLine;
      lastLine  = l;
      lastPos  = p;
   }
   bool within( int l, int p );

   bool lineWithin( int l );
   int firstPosInLine(int l);
   int lastPosInLine(int l);
   int beginLine(){ return min2(firstLine,lastLine); }
   int endLine(){ return max2(firstLine,lastLine); }
   int beginPos() { return firstLine==lastLine ? min2(firstPos,lastPos) :
                           firstLine<lastLine ? firstPos : lastPos;      }
   int endPos()   { return firstLine==lastLine ? max2(firstPos,lastPos) :
                           firstLine<lastLine ? lastPos : firstPos;      }
};

class OptionDialog;

class DiffTextWindow : public QWidget
{
   Q_OBJECT
public:
   DiffTextWindow(
      QWidget* pParent,
      QStatusBar* pStatusBar,
      OptionDialog* pOptionDialog
      );
   void init(
      const QString& fileName,
      LineData* pLineData,
      int size,
      const Diff3LineVector* pDiff3LineVector,
      int winIdx,
      bool bTriple
      );
   virtual void mousePressEvent ( QMouseEvent * );
   virtual void mouseReleaseEvent ( QMouseEvent * );
   virtual void mouseMoveEvent ( QMouseEvent * );
   virtual void mouseDoubleClickEvent ( QMouseEvent * e );
   void convertToLinePos( int x, int y, int& line, int& pos );

   virtual void paintEvent( QPaintEvent*  );
   virtual void dragEnterEvent( QDragEnterEvent* e );
   //void setData( const char* pText);

   virtual void resizeEvent( QResizeEvent* );

   QString getSelection();
   int getFirstLine() { return m_firstLine; }

   int getNofColumns();
   int getNofLines();
   int getNofVisibleLines();
   int getNofVisibleColumns();

   bool findString( const QCString& s, int& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive );
   void setSelection( int firstLine, int startPos, int lastLine, int endPos );

   void setPaintingAllowed( bool bAllowPainting );
signals:
   void resizeSignal( int nofVisibleColumns, int nofVisibleLines );
   void scroll( int deltaX, int deltaY );
   void newSelection();
   void selectionEnd();
   void setFastSelectorLine( int line );

public slots:
   void setFirstLine( int line );
   void setFirstColumn( int col );
   void resetSelection();
   void setFastSelectorRange( int line1, int nofLines );

private:
   bool m_bPaintingAllowed;
   LineData* m_pLineData;
   int m_size;
   QString m_filename;

   const Diff3LineVector* m_pDiff3LineVector;

   OptionDialog* m_pOptionDialog;
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

   QCString getString( int line );

   void writeLine(
      QPainter& p, const LineData* pld,
      const DiffList* pLineDiff1, const DiffList* pLineDiff2, int line,
      int whatChanged, int whatChanged2, int srcLineNr );

   QStatusBar* m_pStatusBar;

   Selection selection;

   int m_scrollDeltaX;
   int m_scrollDeltaY;
   virtual void timerEvent(QTimerEvent*);
   bool m_bMyUpdate;
   void myUpdate(int afterMilliSecs );

   QRect m_invalidRect;
};



class Overview : public QWidget
{
   Q_OBJECT
public:
   Overview( QWidget* pParent, OptionDialog* pOptions );
             
   void init( Diff3LineList* pDiff3LineList, bool bTripleDiff );
   void setRange( int firstLine, int pageHeight );
   void setPaintingAllowed( bool bAllowPainting );

public slots:
   void setFirstLine(int firstLine);
signals:
   void setLine(int);
private:
   const Diff3LineList* m_pDiff3LineList;
   OptionDialog* m_pOptions;
   bool m_bTripleDiff;
   int m_firstLine;
   int m_pageHeight;
   QPixmap m_pixmap;
   bool m_bPaintingAllowed;

   virtual void paintEvent( QPaintEvent* e );
   virtual void mousePressEvent( QMouseEvent* e );
   virtual void mouseMoveEvent( QMouseEvent* e );
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


class MergeResultWindow : public QWidget
{
   Q_OBJECT
public:
   MergeResultWindow(
      QWidget* pParent,
      OptionDialog* pOptionDialog
      );

   void init(
      const LineData* pLineDataA,
      const LineData* pLineDataB,
      const LineData* pLineDataC,
      const Diff3LineList* pDiff3LineList,
      const TotalDiffStatus* pTotalDiffStatus,
      QString fileName
      );

   bool saveDocument( const QString& fileName );
   int getNrOfUnsolvedConflicts();
   void choose(int selector);

   int getNofColumns();
   int getNofLines();
   int getNofVisibleColumns();
   int getNofVisibleLines();
   virtual void resizeEvent( QResizeEvent* e );
   virtual void keyPressEvent( QKeyEvent* e );
   virtual void wheelEvent( QWheelEvent* e );
   QString getSelection();
   void resetSelection();
   void showNrOfConflicts();
   bool isDeltaAboveCurrent();
   bool isDeltaBelowCurrent();
   bool isConflictAboveCurrent();
   bool isConflictBelowCurrent();
   bool isUnsolvedConflictAboveCurrent();
   bool isUnsolvedConflictBelowCurrent();
   bool findString( const QCString& s, int& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive );
   void setSelection( int firstLine, int startPos, int lastLine, int endPos );
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
   void slotChooseA();
   void slotChooseB();
   void slotChooseC();
   void slotChooseAEverywhere();
   void slotChooseBEverywhere();
   void slotChooseCEverywhere();
   void slotAutoSolve();
   void slotUnsolve();
   void slotSetFastSelectorLine(int);

signals:
   void scroll( int deltaX, int deltaY );
   void modified();
   void setFastSelectorRange( int line1, int nofLines );
   void sourceMask( int srcMask, int enabledMask );
   void resizeSignal();
   void selectionEnd();
   void newSelection();
   void updateAvailabilities();
   void showPopupMenu( const QPoint& point );

private:
   void merge(bool bAutoSolve, int defaultSelector);
   QCString getString( int lineIdx );

   OptionDialog* m_pOptionDialog;

   const LineData* m_pldA;
   const LineData* m_pldB;
   const LineData* m_pldC;

   const Diff3LineList* m_pDiff3LineList;
   const TotalDiffStatus* m_pTotalDiffStatus;

private:
   class MergeEditLine
   {
   public:
      MergeEditLine(){ m_src=0; m_bLineRemoved=false; }
      void setConflict() { m_src=0; m_bLineRemoved=false; m_str=QCString(); }
      bool isConflict()  { return  m_src==0 && !m_bLineRemoved && m_str.isNull(); }
      void setRemoved(int src=0)  { m_src=src; m_bLineRemoved=true; m_str=QCString(); }
      bool isRemoved()   { return m_bLineRemoved; }
      bool isEditableText() { return !isConflict() && !isRemoved(); }
      void setString( const QCString& s ){ m_str=s; m_bLineRemoved=false; m_src=0; }
      const char* getString( const MergeResultWindow*, int& size );
      bool isModified() { return ! m_str.isNull() ||  (m_bLineRemoved && m_src==0); }

      void setSource( int src, Diff3LineList::const_iterator i, bool bLineRemoved )
      {
         m_src=src; m_id3l=i; m_bLineRemoved =bLineRemoved;
      }
      int src() { return m_src; }
      Diff3LineList::const_iterator id3l(){return m_id3l;}
      // getString() is implemented as MergeResultWindow::getString()
   private:
      Diff3LineList::const_iterator m_id3l;
      int m_src;         // 1, 2 or 3 for A, B or C respectively, or 0 when line is from neither source.
      QCString m_str;    // String when modified by user or null-string when orig data is used.
      bool m_bLineRemoved;
   };

   class MergeEditLineList : private std::list<MergeEditLine>
   { // I want to know the size immediately!
   private:
      typedef std::list<MergeEditLine> BASE;
      int m_size;
   public:
      typedef std::list<MergeEditLine>::iterator iterator;
      typedef std::list<MergeEditLine>::const_iterator const_iterator;
      MergeEditLineList(){m_size=0;}
      void clear()                             { m_size=0; BASE::clear();          }
      void push_back( const MergeEditLine& m)  { ++m_size; BASE::push_back(m);     }
      void push_front( const MergeEditLine& m) { ++m_size; BASE::push_front(m);    }
      iterator erase( iterator i )             { --m_size; return BASE::erase(i);  }
      iterator insert( iterator i, const MergeEditLine& m ) { ++m_size; return BASE::insert(i,m); }
      int size(){ /*assert(int(BASE::size())==m_size);*/ return m_size;}
      iterator begin(){return BASE::begin();}
      iterator end(){return BASE::end();}
      bool empty() { return m_size==0; }
   };

   friend class MergeEditLine;

   struct MergeLine
   {
      MergeLine()
      { srcSelect=0; mergeDetails=eDefault; d3lLineIdx = -1; srcRangeLength=0; bConflict=false; bDelta=false;}
      Diff3LineList::const_iterator id3l;
      e_MergeDetails mergeDetails;
      int d3lLineIdx;  // Needed to show the correct window pos.
      int srcRangeLength; // how many src-lines have this properties
      bool bConflict;
      bool bDelta;
      int srcSelect;
      MergeEditLineList mergeEditLineList;
   };

private:
   static bool sameKindCheck( const MergeLine& ml1, const MergeLine& ml2 );

   typedef std::list<MergeLine> MergeLineList;
   MergeLineList m_mergeLineList;
   MergeLineList::iterator m_currentMergeLineIt;
   int m_currentPos;

   enum e_Direction { eUp, eDown };
   enum e_EndPoint  { eDelta, eConflict, eUnsolvedConflict, eLine, eEnd };
   void go( e_Direction eDir, e_EndPoint eEndPoint );
   void calcIteratorFromLineNr(
      int line,
      MergeLineList::iterator& mlIt,
      MergeEditLineList::iterator& melIt
      );

   virtual void paintEvent( QPaintEvent* e );


   void myUpdate(int afterMilliSecs);
   virtual void timerEvent(QTimerEvent*);
   void writeLine(
      QPainter& p, int line, const char* pStr, int size,
      int srcSelect, e_MergeDetails mergeDetails, int rangeMark, bool bUserModified, bool bLineRemoved
      );
   void setFastSelector(MergeLineList::iterator i);
   void convertToLinePos( int x, int y, int& line, int& pos );
   virtual void mousePressEvent ( QMouseEvent* e );
   virtual void mouseDoubleClickEvent ( QMouseEvent* e );
   virtual void mouseReleaseEvent ( QMouseEvent * );
   virtual void mouseMoveEvent ( QMouseEvent * );

   QPixmap m_pixmap;
   int m_firstLine;
   int m_firstColumn;
   int m_nofColumns;
   int m_nofLines;
   bool m_bMyUpdate;
   bool m_bInsertMode;
   QString m_fileName;
   bool m_bModified;
   void setModified();

   int m_scrollDeltaX;
   int m_scrollDeltaY;
   int m_cursorXPos;
   int m_cursorYPos;
   int m_cursorOldXPos;
   bool m_bCursorOn; // blinking on and off each second
   QTimer m_cursorTimer;

   Selection m_selection;

   bool deleteSelection2( const char*& ps, int& stringLength, int& x, int& y,
                    MergeLineList::iterator& mlIt, MergeEditLineList::iterator& melIt );
public slots:
   void deleteSelection();
   void pasteClipboard();
private slots:
   void slotCursorUpdate();
};

void fineDiff(
   Diff3LineList& diff3LineList,
   int selector,
   LineData* v1,
   LineData* v2,
   int maxSearchLength,
   bool& bTextsTotalEqual
   );


bool equal( const LineData& l1, const LineData& l2, bool bStrict );

inline bool equal( char c1, char c2, bool /*bStrict*/ )
{
   // If bStrict then white space doesn't match

   //if ( bStrict &&  ( c1==' ' || c1=='\t' ) )
   //   return false;

   return c1==c2;
}


// My own diff-invention:
template <class T>
void calcDiff( const T* p1, int size1, const T* p2, int size2, DiffList& diffList, int match, int maxSearchRange )
{
   diffList.clear();

   const T* p1start = p1;
   const T* p2start = p2;
   const T* p1end=p1+size1;
   const T* p2end=p2+size2;
   for(;;)
   {
      int nofEquals = 0;
      while( p1!=p1end &&  p2!=p2end && equal(*p1, *p2, false) )
      {


         ++p1;
         ++p2;
         ++nofEquals;
      }

      bool bBestValid=false;
      int bestI1=0;
      int bestI2=0;
      int i1=0;
      int i2=0;
      for( i1=0; ; ++i1 )
      {
         if ( &p1[i1]==p1end || ( bBestValid && i1>= bestI1+bestI2))
         {
            break;
         }
         for(i2=0;i2<maxSearchRange;++i2)
         {
            if( &p2[i2]==p2end ||  ( bBestValid && i1+i2>=bestI1+bestI2) )
            {
               break;
            }
            else if(  equal( p2[i2], p1[i1], true ) &&
                      ( match==1 ||  abs(i1-i2)<3  || ( &p2[i2+1]==p2end  &&  &p1[i1+1]==p1end ) ||
                         ( &p2[i2+1]!=p2end  &&  &p1[i1+1]!=p1end  && equal( p2[i2+1], p1[i1+1], false ))
                      )
                   )
            {
               if ( i1+i2 < bestI1+bestI2 || bBestValid==false )
               {
                  bestI1 = i1;
                  bestI2 = i2;
                  bBestValid = true;
                  break;
               }
            }
         }
      }

      // The match was found using the strict search. Go back if there are non-strict
      // matches.
      while( bestI1>=1 && bestI2>=1 && equal( p1[bestI1-1], p2[bestI2-1], false ) )
      {
         --bestI1;
         --bestI2;
      }


      bool bEndReached = false;
      if (bBestValid)
      {
         // continue somehow
         Diff d(nofEquals, bestI1, bestI2);
         diffList.push_back( d );

         p1 += bestI1;
         p2 += bestI2;
      }
      else
      {
         // Nothing else to match.
         Diff d(nofEquals, p1end-p1, p2end-p2);
         diffList.push_back( d );

         bEndReached = true; //break;
      }

      // Sometimes the algorithm that chooses the first match unfortunately chooses
      // a match where later actually equal parts don't match anymore.
      // A different match could be achieved, if we start at the end.
      // Do it, if it would be a better match.
      int nofUnmatched = 0;
      const T* pu1 = p1-1;
      const T* pu2 = p2-1;
      while ( pu1>=p1start && pu2>=p2start && equal( *pu1, *pu2, false ) )
      {
         ++nofUnmatched;
         --pu1;
         --pu2;
      }

      Diff d = diffList.back();
      if ( nofUnmatched > 0 )
      {
         // We want to go backwards the nofUnmatched elements and redo
         // the matching
         d = diffList.back();
         Diff origBack = d;
         diffList.pop_back();

         while (  nofUnmatched > 0 )
         {
            if ( d.diff1 > 0  &&  d.diff2 > 0 )
            {
               --d.diff1;
               --d.diff2;
               --nofUnmatched;
            }
            else if ( d.nofEquals > 0 )
            {
               --d.nofEquals;
               --nofUnmatched;
            }

            if ( d.nofEquals==0 && (d.diff1==0 || d.diff2==0) &&  nofUnmatched>0 )
            {
               if ( diffList.empty() )
                  break;
               d.nofEquals += diffList.back().nofEquals;
               d.diff1 += diffList.back().diff1;
               d.diff2 += diffList.back().diff2;
               diffList.pop_back();
               bEndReached = false;
            }
         }

         if ( bEndReached )
            diffList.push_back( origBack );
         else
         {

            p1 = pu1 + 1 + nofUnmatched;
            p2 = pu2 + 1 + nofUnmatched;
            diffList.push_back( d );
         }
      }
      if ( bEndReached )
         break;
   }

#ifndef NDEBUG
   // Verify difflist
   {
      int l1=0;
      int l2=0;
      DiffList::iterator i;
      for( i = diffList.begin(); i!=diffList.end(); ++i )
      {
         l1+= i->nofEquals + i->diff1;
         l2+= i->nofEquals + i->diff2;
      }

      //if( l1!=p1-p1start || l2!=p2-p2start )
      if( l1!=size1 || l2!=size2 )
         assert( false );
   }
#endif
}



inline bool isWhite( char c )
{
   return c==' ' || c=='\t' ||  c=='\r';
}

/** Returns the number of equivalent spaces at position outPos.
*/
inline int tabber( int outPos, int tabSize )
{
   return tabSize - ( outPos % tabSize );
}

/** Returns a line number where the linerange [line, line+nofLines] can
    be displayed best. If it fits into the currently visible range then
    the returned value is the current firstLine.
*/
int getBestFirstLine( int line, int nofLines, int firstLine, int visibleLines );

extern int g_tabSize;
extern bool g_bIgnoreWhiteSpace;
extern bool g_bIgnoreTrivialMatches;
extern int g_bAutoSolve;

// Cursor conversions that consider g_tabSize.
int convertToPosInText( const char* p, int size, int posOnScreen );
int convertToPosOnScreen( const char* p, int posInText );
void calcTokenPos( const char* p, int size, int posOnScreen, int& pos1, int& pos2 );
#endif

