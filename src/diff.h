/***************************************************************************
                          diff.h  -  description
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

#ifndef DIFF_H
#define DIFF_H

#include <QPainter>
#include <QLinkedList>
#include <QVector>
#include <assert.h>
#include "common.h"
#include "fileaccess.h"
#include "options.h"


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

struct LineData
{
   const QChar* pLine;
   const QChar* pFirstNonWhiteChar;
   int size;

   LineData(){ pLine=0; pFirstNonWhiteChar=0; size=0; /*occurances=0;*/ 
               bContainsPureComment=false; }
   int width(int tabSize) const;  // Calcs width considering tabs.
   //int occurances;
   bool whiteLine() const { return pFirstNonWhiteChar-pLine == size; }
   bool bContainsPureComment;
};

class Diff3LineList;
class Diff3LineVector;

struct DiffBufferInfo
{
   const LineData* m_pLineDataA;
   const LineData* m_pLineDataB;
   const LineData* m_pLineDataC;
   int m_sizeA;
   int m_sizeB;
   int m_sizeC;
   const Diff3LineList* m_pDiff3LineList;
   const Diff3LineVector* m_pDiff3LineVector;
   void init( Diff3LineList* d3ll, const Diff3LineVector* d3lv,
      const LineData* pldA, int sizeA, const LineData* pldB, int sizeB, const LineData* pldC, int sizeC );
};

struct Diff3Line
{
   int lineA;
   int lineB;
   int lineC;

   bool bAEqC : 1;             // These are true if equal or only white-space changes exist.
   bool bBEqC : 1;
   bool bAEqB : 1;

   bool bWhiteLineA : 1;
   bool bWhiteLineB : 1;
   bool bWhiteLineC : 1;

   DiffList* pFineAB;          // These are 0 only if completely equal or if either source doesn't exist.
   DiffList* pFineBC;
   DiffList* pFineCA;

   int linesNeededForDisplay; // Due to wordwrap
   int sumLinesNeededForDisplay; // For fast conversion to m_diff3WrapLineVector

   DiffBufferInfo* m_pDiffBufferInfo; // For convenience

   Diff3Line()
   {
      lineA=-1; lineB=-1; lineC=-1;
      bAEqC=false; bAEqB=false; bBEqC=false;
      pFineAB=0; pFineBC=0; pFineCA=0;
      linesNeededForDisplay=1;
      sumLinesNeededForDisplay=0;
      bWhiteLineA=false; bWhiteLineB=false; bWhiteLineC=false;
      m_pDiffBufferInfo=0;
   }

   ~Diff3Line()
   {
      if (pFineAB!=0) delete pFineAB;
      if (pFineBC!=0) delete pFineBC;
      if (pFineCA!=0) delete pFineCA;
      pFineAB=0; pFineBC=0; pFineCA=0;
   }

   bool operator==( const Diff3Line& d3l ) const
   {
      return lineA == d3l.lineA  &&  lineB == d3l.lineB  &&  lineC == d3l.lineC  
         && bAEqB == d3l.bAEqB  && bAEqC == d3l.bAEqC  && bBEqC == d3l.bBEqC;
   }

   const LineData* getLineData( int src ) const
   {
      assert( m_pDiffBufferInfo!=0 );
      if ( src == 1 && lineA >= 0 ) return &m_pDiffBufferInfo->m_pLineDataA[lineA];
      if ( src == 2 && lineB >= 0 ) return &m_pDiffBufferInfo->m_pLineDataB[lineB];
      if ( src == 3 && lineC >= 0 ) return &m_pDiffBufferInfo->m_pLineDataC[lineC];
      return 0;
   }
   QString getString( int src ) const
   {
      const LineData* pld = getLineData(src);
      if ( pld )
         return QString( pld->pLine, pld->size);
      else
         return QString();
   }
   int getLineInFile( int src ) const
   {
      if ( src == 1 ) return lineA;
      if ( src == 2 ) return lineB;
      if ( src == 3 ) return lineC;
      return -1;
   }
};


class Diff3LineList : public QLinkedList<Diff3Line>
{
};
class Diff3LineVector : public QVector<Diff3Line*>
{
};

class Diff3WrapLine
{
public:
   Diff3Line* pD3L;
   int diff3LineIndex;
   int wrapLineOffset;
   int wrapLineLength;
};

typedef QVector<Diff3WrapLine> Diff3WrapLineVector;


class TotalDiffStatus
{
public:
   TotalDiffStatus(){ reset(); }
   void reset() {bBinaryAEqC=false; bBinaryBEqC=false; bBinaryAEqB=false;
                 bTextAEqC=false;   bTextBEqC=false;   bTextAEqB=false;
                 nofUnsolvedConflicts=0; nofSolvedConflicts=0;
                 nofWhitespaceConflicts=0;
                }
   bool bBinaryAEqC : 1;
   bool bBinaryBEqC : 1;
   bool bBinaryAEqB : 1;

   bool bTextAEqC : 1;
   bool bTextBEqC : 1;
   bool bTextAEqB : 1;

   int nofUnsolvedConflicts;
   int nofSolvedConflicts;
   int nofWhitespaceConflicts;
};

// Three corresponding ranges. (Minimum size of a valid range is one line.)
class ManualDiffHelpEntry
{
public:
   ManualDiffHelpEntry() { lineA1=-1; lineA2=-1; 
                           lineB1=-1; lineB2=-1; 
                           lineC1=-1; lineC2=-1; }
   int lineA1;
   int lineA2;
   int lineB1;
   int lineB2;
   int lineC1;
   int lineC2;
   int& firstLine( int winIdx )
   {
      return winIdx==1 ? lineA1 : (winIdx==2 ? lineB1 : lineC1 );
   }
   int& lastLine( int winIdx )
   {
      return winIdx==1 ? lineA2 : (winIdx==2 ? lineB2 : lineC2 );
   }
   bool isLineInRange( int line, int winIdx )
   {
      return line>=0 && line>=firstLine(winIdx) && line<=lastLine(winIdx);
   }
   bool operator==(const ManualDiffHelpEntry& r) const
   {
      return lineA1 == r.lineA1   &&   lineB1 == r.lineB1   &&   lineC1 == r.lineC1  &&
             lineA2 == r.lineA2   &&   lineB2 == r.lineB2   &&   lineC2 == r.lineC2;
   }
};

// A list of corresponding ranges
typedef std::list<ManualDiffHelpEntry> ManualDiffHelpList;

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

void correctManualDiffAlignment( Diff3LineList& d3ll, ManualDiffHelpList* pManualDiffHelpList );

class SourceData
{
public:
   SourceData();
   ~SourceData();

   void setOptions( Options* pOptions );

   int getSizeLines() const;
   int getSizeBytes() const;
   const char* getBuf() const;
   const QString& getText() const;
   const LineData* getLineDataForDisplay() const;
   const LineData* getLineDataForDiff() const;

   void setFilename(const QString& filename);
   void setFileAccess( const FileAccess& fa );
   void setEncoding(QTextCodec* pEncoding);
   //FileAccess& getFileAccess();
   QString getFilename();
   void setAliasName(const QString& a);
   QString getAliasName();
   bool isEmpty();  // File was set
   bool hasData();  // Data was readable
   bool isText();   // is it pure text (vs. binary data)
   bool isIncompleteConversion(); // true if some replacement characters were found
   bool isFromBuffer();  // was it set via setData() (vs. setFileAccess() or setFilename())
   QStringList setData( const QString& data );
   bool isValid(); // Either no file is specified or reading was successful

   // Returns a list of error messages if anything went wrong
   QStringList readAndPreprocess(QTextCodec* pEncoding, bool bAutoDetectUnicode );
   bool saveNormalDataAs( const QString& fileName );

   bool isBinaryEqualWith( const SourceData& other ) const;

   void reset();

   QTextCodec* getEncoding() const { return m_pEncoding; }
   e_LineEndStyle getLineEndStyle() const { return m_normalData.m_eLineEndStyle; }

private:
   QTextCodec* detectEncoding( const QString& fileName, QTextCodec* pFallbackCodec );
   QString m_aliasName;
   FileAccess m_fileAccess;
   Options* m_pOptions;
   QString m_tempInputFileName;

   struct FileData
   {
      FileData(){ m_pBuf=0; m_size=0; m_vSize=0; m_bIsText=false; m_eLineEndStyle=eLineEndStyleUndefined; m_bIncompleteConversion=false;}
      ~FileData(){ reset(); }
      const char* m_pBuf;
      int m_size;
      int m_vSize; // Nr of lines in m_pBuf1 and size of m_v1, m_dv12 and m_dv13
      QString m_unicodeBuf;
      QVector<LineData> m_v;
      bool m_bIsText;
      bool m_bIncompleteConversion;
      e_LineEndStyle m_eLineEndStyle;
      bool readFile( const QString& filename );
      bool writeFile( const QString& filename );
      void preprocess(bool bPreserveCR, QTextCodec* pEncoding );
      void reset();
      void removeComments();
      void copyBufFrom( const FileData& src );
   };
   FileData m_normalData;
   FileData m_lmppData;  
   QTextCodec* m_pEncoding; 
};

void calcDiff3LineListTrim( Diff3LineList& d3ll, const LineData* pldA, const LineData* pldB, const LineData* pldC, ManualDiffHelpList* pManualDiffHelpList );
void calcWhiteDiff3Lines(   Diff3LineList& d3ll, const LineData* pldA, const LineData* pldB, const LineData* pldC );

void calcDiff3LineVector( Diff3LineList& d3ll, Diff3LineVector& d3lv );



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
      lastLine=-1;
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
   int beginLine(){ 
      if (firstLine<0 && lastLine<0) return -1;
      return max2(0,min2(firstLine,lastLine)); 
   }
   int endLine(){ 
      if (firstLine<0 && lastLine<0) return -1;
      return max2(firstLine,lastLine); 
   }
   int beginPos() { return firstLine==lastLine ? min2(firstPos,lastPos) :
                           firstLine<lastLine ? (firstLine<0?0:firstPos) : (lastLine<0?0:lastPos);  }
   int endPos()   { return firstLine==lastLine ? max2(firstPos,lastPos) :
                           firstLine<lastLine ? lastPos : firstPos;      }
};


// Helper class that swaps left and right for some commands.
class MyPainter : public QPainter
{
   int m_factor;
   int m_xOffset;
   int m_fontWidth;
public:
   MyPainter(QPaintDevice* pd, bool bRTL, int width, int fontWidth) 
   : QPainter(pd)
   {
      if (bRTL) 
      {
         m_fontWidth = fontWidth;
         m_factor = -1;
         m_xOffset = width-1;
      }
      else
      {
         m_fontWidth = 0;
         m_factor = 1;
         m_xOffset = 0;
      }
   }

   void fillRect( int x, int y, int w, int h, const QBrush& b )
   {
      if (m_factor==1)
         QPainter::fillRect( m_xOffset + x    , y, w, h, b );
      else
         QPainter::fillRect( m_xOffset - x - w, y, w, h, b );
   }

   void drawText( int x, int y, const QString& s, bool bAdapt=false )
   {
      Qt::LayoutDirection ld = (m_factor==1 || bAdapt == false) ? Qt::LeftToRight : Qt::RightToLeft;
      //QPainter::setLayoutDirection( ld );
      if ( ld==Qt::RightToLeft ) // Reverse the text
      {
         QString s2;
         for( int i=s.length()-1; i>=0; --i )
         {
            s2 += s[i];
         }
         QPainter::drawText( m_xOffset-m_fontWidth*s.length() + m_factor*x, y, s2 );
         return;
      }
      QPainter::drawText( m_xOffset-m_fontWidth*s.length() + m_factor*x, y, s );
   }

   void drawLine( int x1, int y1, int x2, int y2 )
   {
      QPainter::drawLine( m_xOffset + m_factor*x1, y1, m_xOffset + m_factor*x2, y2 );
   }
};

bool runDiff( const LineData* p1, int size1, const LineData* p2, int size2, DiffList& diffList, int winIdx1, int winIdx2,
              ManualDiffHelpList *pManualDiffHelpList, Options *pOptions);

bool fineDiff(
   Diff3LineList& diff3LineList,
   int selector,
   const LineData* v1,
   const LineData* v2
   );


bool equal( const LineData& l1, const LineData& l2, bool bStrict );




inline bool isWhite( QChar c )
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

extern bool g_bIgnoreWhiteSpace;
extern bool g_bIgnoreTrivialMatches;
extern int g_bAutoSolve;

// Cursor conversions that consider g_tabSize.
int convertToPosInText( const QString& s, int posOnScreen, int tabSize );
int convertToPosOnScreen( const QString& s, int posInText, int tabSize );

enum e_CoordType { eFileCoords, eD3LLineCoords, eWrapCoords };

void calcTokenPos( const QString&, int posOnScreen, int& pos1, int& pos2, int tabSize );

QString calcHistorySortKey( const QString& keyOrder, QRegExp& matchedRegExpr, const QStringList& parenthesesGroupList );
bool findParenthesesGroups( const QString& s, QStringList& sl );
#endif

