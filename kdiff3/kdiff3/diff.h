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
 * Revision 1.1  2002/08/18 16:24:17  joachim99
 * Initial revision
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

typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;


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

   bool bAEqC;
   bool bBEqC;
   bool bAEqB;

   DiffList* pFineAB;
   DiffList* pFineBC;
   DiffList* pFineCA;

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
   int size;

   LineData(){ pLine=0; size=0; }
   int width();  // Calcs width considering tabs.
};

void calcDiff3LineListTrim( Diff3LineList& d3ll, LineData* pldA, LineData* pldB, LineData* pldC );

void debugLineCheck( Diff3LineList& d3ll, int size, int idx );

class QStatusBar;






class Selection
{
public:
   Selection(){ reset(); oldLastLine=-1; oldFirstLine=-1; }
   int firstLine;
   int firstPos;
   int lastLine;
   int lastPos;
   int oldLastLine;
   int oldFirstLine;
   bool bSelectionContainsData;
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
   int beginLine(){ return min(firstLine,lastLine); }
   int endLine(){ return max(firstLine,lastLine); }
   int beginPos() { return firstLine==lastLine ? min(firstPos,lastPos) :
                           firstLine<lastLine ? firstPos : lastPos;      }
   int endPos()   { return firstLine==lastLine ? max(firstPos,lastPos) :
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
      const char* pFilename,
      LineData* pLineData,
      int size,
      const Diff3LineList* pDiff3LineList,
      int winIdx,
      bool bTriple
      );
   virtual void mousePressEvent ( QMouseEvent * );
   virtual void mouseReleaseEvent ( QMouseEvent * );
   virtual void mouseMoveEvent ( QMouseEvent * );
   virtual void mouseDoubleClickEvent ( QMouseEvent * e );
   void convertToLinePos( int x, int y, int& line, int& pos );

   virtual void paintEvent( QPaintEvent*  );

   //void setData( const char* pText);

   virtual void resizeEvent( QResizeEvent* );

   QString getSelection();
   int getFirstLine() { return m_firstLine; }

   int getNofColumns();
   int getNofLines();

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
   LineData* m_pLineData;
   int m_size;
   const char* m_pFilename;

   const Diff3LineList* m_pDiff3LineList;

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

   int m_visibleColumns;
   int m_visibleLines;

   void getLineInfo(
      const Diff3Line& d,
      int& lineIdx,
      DiffList*& pFineDiff1, DiffList*& pFineDiff2,   // return values
      int& changed, int& changed  );

   QCString getString( int line );

   void writeLine(
      QPainter& p, const LineData* pld,
      const DiffList* pLineDiff1, const DiffList* pLineDiff2, int line,
      int whatChanged, int whatChanged2 );

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
   Overview( QWidget* pParent, Diff3LineList* pDiff3LineList,
             OptionDialog* pOptions, bool bTripleDiff );
   void setRange( int firstLine, int pageHeight );

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
      const LineData* pLineDataA,
      const LineData* pLineDataB,
      const LineData* pLineDataC,
      const Diff3LineList* pDiff3LineList,
      QString fileName,
      OptionDialog* pOptionDialog
      );

   bool saveDocument( const QString& fileName );
   void choose(int selector);

   int getNofColumns();
   int getNofLines();
   int getNofVisibleColumns();
   int getNofVisibleLines();
   virtual void resizeEvent( QResizeEvent* e );
   virtual void keyPressEvent( QKeyEvent* e );
   QString getSelection();
   void resetSelection();
public slots:
   void setFirstLine(int firstLine);
   void setFirstColumn(int firstCol);

   void slotGoTop();
   void slotGoBottom();
   void slotGoPrevDelta();
   void slotGoNextDelta();
   void slotGoPrevConflict();

   void slotGoNextConflict();
   void slotChooseA();
   void slotChooseB();
   void slotChooseC();

   void slotSetFastSelectorLine(int);


signals:
   void scroll( int deltaX, int deltaY );
   void modified();
   void savable( bool bSavable );
   void setFastSelectorRange( int line1, int nofLines );

   void sourceMask( int srcMask, int enabledMask );
   void resizeSignal();
   void selectionEnd();
   void newSelection();

private:
   void merge();

   OptionDialog* m_pOptionDialog;

   const LineData* m_pldA;
   const LineData* m_pldB;
   const LineData* m_pldC;

   const Diff3LineList* m_pDiff3LineList;

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
   typedef list<MergeEditLine> MergeEditLineList;

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


   typedef list<MergeLine> MergeLineList;
   MergeLineList m_mergeLineList;


   MergeLineList::iterator m_currentMergeLineIt;
   int m_currentPos;

   enum e_Direction { eUp, eDown };
   enum e_EndPoint  { eDelta, eConflict, eLine, eEnd };
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
   LineData* v2
   );

const char* readFile( const char* filename, int& size );

int preprocess( const char* p, int size, std::vector<LineData>& v );

bool equal( const LineData& l1, const LineData& l2, bool bStrict );



inline bool equal( char c1, char c2, bool bStrict )
{
   // If bStrict then white space doesn't match
   if ( bStrict &&  ( c1==' ' || c1=='\t' ) )
      return false;

   return c1==c2;
}


template <class T>
void calcDiff( const T* p1, int size1, const T* p2, int size2, DiffList& diffList, int match=1 )
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
         for(i2=0;;++i2)
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


template <class T>
T min3( T d1, T d2, T d3 )
{
   if ( d1 < d2  &&  d1 < d3 ) return d1;
   if ( d2 < d3 ) return d2;
   return d3;
}

template <class T>
T max3( T d1, T d2, T d3 )
{

   if ( d1 > d2  &&  d1 > d3 ) return d1;

   if ( d2 > d3 ) return d2;
   return d3;

}

template <class T>
T minMaxLimiter( T d, T minimum, T maximum )
{
   assert(minimum<=maximum);
   if ( d < minimum ) return minimum;
   if ( d > maximum ) return maximum;
   return d;
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

// Cursor conversions that consider g_tabSize.
int convertToPosInText( const char* p, int size, int posOnScreen );
int convertToPosOnScreen( const char* p, int posInText );
void calcTokenPos( const char* p, int size, int posOnScreen, int& pos1, int& pos2 );
#endif