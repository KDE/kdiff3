/***************************************************************************
                          diff.cpp  -  description
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
 * Revision 1.1  2002/08/18 16:24:15  joachim99
 * Initial revision
 *                                                                   *
 ***************************************************************************/

#include <stdio.h>
#include "diff.h"

#include <kmessagebox.h>
#include <klocale.h>
using namespace std;

int LineData::width()
{
   int w=0;
   int j=0;
   for( int i=0; i<size; ++i )
   {
      if ( pLine[i]=='\t' )
      {
         for(j %= g_tabSize; j<g_tabSize; ++j)
            ++w;
         j=0;
      }
      else
      {
         ++w;
         ++j;
      }
   }
   return w;
}


// The bStrict flag is true during the test where a nonmatching area ends.
// Then the equal()-function requires that the match has more than 2 nonwhite characters.
// This is to avoid matches on trivial lines (e.g. with white space only).
// This choice is good for C/C++.
bool equal( const LineData& l1, const LineData& l2, bool bStrict )
{
   if ( l1.pLine==0 || l2.pLine==0) return false;

   // Ignore white space diff
   const char* p1 = l1.pLine;
   const char* p1End = p1 + l1.size;

   const char* p2 = l2.pLine;
   const char* p2End = p2 + l2.size;

   if ( g_bIgnoreWhiteSpace )
   {
      int nonWhite = 0;
      for(;;)
      {
         while( (*p1==' ' || *p1=='\t') && p1!=p1End ) ++p1;
         while( (*p2==' ' || *p2=='\t') && p2!=p2End ) ++p2;

         if ( p1 == p1End  &&  p2 == p2End )
            return bStrict ? (nonWhite>2 || !g_bIgnoreTrivialMatches) : true;
         else if ( p1 == p1End || p2 == p2End )
            return false;

         if( *p1 != *p2 )
            return false;
         ++p1;
         ++p2;
         ++nonWhite;
      }
   }

   else
   {
      if ( l1.size==l2.size && memcmp(p1, p2, l1.size)==0)
         return true;
      else
         return false;
   }
}



/** Prepare the linedata vector for every input line.*/
int preprocess( const char* p, int size, vector<LineData>& v )
{
   int lines = 1;
   int i;
   for( i=0; i<size; ++i )
   {
      if (p[i]=='\n')
      {
         ++lines;
      }
   }

   v.resize( lines+5 );
   int lineIdx=0;
   int lineLength=0;
   for( i=0; i<=size; ++i )
   {

      // For WIN32
      if ( i==size || ( p[i]=='\r' && p[i+1]=='\n' ) || p[i]=='\n')        // The last line does not end with a linefeed.
      {
         v[lineIdx].pLine = &p[ i-lineLength ];
         v[lineIdx].size = lineLength;
         lineLength = 0;
         ++lineIdx;
         if ( i<size && p[i]=='\r') ++i;
      }
      else
      {
         ++lineLength;
      }
   }
   assert( lineIdx == lines );

   return lines;
}


const char* readFile( const char* filename, int& size )
{
   size = 0;
   if (filename==0 || filename[0]=='\0')   return 0;
   
   FILE* f = fopen( filename, "rb" );
   if ( f==0 )
   {
      cerr << "File open error for file: '"<< filename <<"': ";
      perror("");
      return 0;
   }

   fseek( f, 0, SEEK_END );

   size =  ftell(f);

   fseek( f, 0, SEEK_SET );

   char* buf = new char[size+100];
   int bytesRead = fread( buf, 1, size, f );
   if( bytesRead != size )
   {
      cerr << "File read error for file: '"<< filename <<"': ";


      perror("");
      exit(-1);
   }

   return buf;
}


// First step
void calcDiff3LineListUsingAB(       
   const DiffList* pDiffListAB,
   Diff3LineList& d3ll
   )
{
   // First make d3ll for AB (from pDiffListAB)

   DiffList::const_iterator i=pDiffListAB->begin();
   int lineA=0;
   int lineB=0;
   Diff d(0,0,0);

   for(;;)
   {
      if ( d.nofEquals==0 && d.diff1==0 && d.diff2==0 )
      {
         if ( i!=pDiffListAB->end() )
         {
            d=*i;
            ++i;
         }
         else
            break;
      }

      Diff3Line d3l;
      if( d.nofEquals>0 )
      {
         d3l.bAEqB = true;
         d3l.lineA = lineA;
         d3l.lineB = lineB;
         --d.nofEquals;
         ++lineA;
         ++lineB;
      }
      else if ( d.diff1>0 && d.diff2>0 )
      {
         d3l.lineA = lineA;
         d3l.lineB = lineB;
         --d.diff1;
         --d.diff2;
         ++lineA;
         ++lineB;
      }
      else if ( d.diff1>0 )
      {
         d3l.lineA = lineA;
         --d.diff1;
         ++lineA;
      }
      else if ( d.diff2>0 )
      {
         d3l.lineB = lineB;
         --d.diff2;
         ++lineB;
      }

      d3ll.push_back( d3l );      
   }
}


// Second step
void calcDiff3LineListUsingAC(       
   const DiffList* pDiffListAC,
   Diff3LineList& d3ll
   )
{
   ////////////////
   // Now insert data from C using pDiffListAC

   DiffList::const_iterator i=pDiffListAC->begin();
   Diff3LineList::iterator i3 = d3ll.begin();
   int lineA=0;
   int lineC=0;
   Diff d(0,0,0);

   for(;;)
   {
      if ( d.nofEquals==0 && d.diff1==0 && d.diff2==0 )
      {
         if ( i!=pDiffListAC->end() )
         {
            d=*i;
            ++i;
         }
         else
            break;
      }

      Diff3Line d3l;
      if( d.nofEquals>0 )
      {
         // Find the corresponding lineA
         while( (*i3).lineA!=lineA ) 
            ++i3;
         (*i3).lineC = lineC;


         (*i3).bAEqC = true;
         (*i3).bBEqC = (*i3).bAEqB;
                  
         --d.nofEquals;
         ++lineA;
         ++lineC;
         ++i3;
      }
      else if ( d.diff1>0 && d.diff2>0 )
      {
         d3l.lineC = lineC;
         d3ll.insert( i3, d3l );
         --d.diff1;
         --d.diff2;
         ++lineA;
         ++lineC;
      }
      else if ( d.diff1>0 )
      {
         --d.diff1;
         ++lineA;
      }
      else if ( d.diff2>0 )
      {
         d3l.lineC = lineC;
         d3ll.insert( i3, d3l );
         --d.diff2;
         ++lineC;
      }
   }
}

// Third step
void calcDiff3LineListUsingBC(       
   const DiffList* pDiffListBC,
   Diff3LineList& d3ll
   )
{
   ////////////////
   // Now improve the position of data from C using pDiffListBC
   // If a line from C equals a line from A then it is in the
   // same Diff3Line already.
   // If a line from C equals a line from B but not A, this
   // information will be used here.

   DiffList::const_iterator i=pDiffListBC->begin();
   Diff3LineList::iterator i3b = d3ll.begin();
   Diff3LineList::iterator i3c = d3ll.begin();
   int lineB=0;
   int lineC=0;
   Diff d(0,0,0);

   for(;;)
   {
      if ( d.nofEquals==0 && d.diff1==0 && d.diff2==0 )
      {
         if ( i!=pDiffListBC->end() )
         {
            d=*i;
            ++i;
         }
         else
            break;
      }

      Diff3Line d3l;
      if( d.nofEquals>0 )
      {
         // Find the corresponding lineB and lineC
         while( i3b!=d3ll.end() && (*i3b).lineB!=lineB ) 
            ++i3b;

         while( i3c!=d3ll.end() && (*i3c).lineC!=lineC )
            ++i3c;

         assert(i3b!=d3ll.end());
         assert(i3c!=d3ll.end());

         if ( i3b==i3c )
         {
            assert( (*i3b).lineC == lineC );
            (*i3b).bBEqC = true;
         }
         else //if ( !(*i3b).bAEqB )
         {
            // Is it possible to move this line up?
            // Test if no other B's are used between i3c and i3b

            // First test which is before: i3c or i3b ?
            Diff3LineList::iterator i3c1 = i3c;
            Diff3LineList::iterator i3b1 = i3b;
            while( i3c1!=i3b  &&  i3b1!=i3c )
            {
               assert(i3b1!=d3ll.end() || i3c1!=d3ll.end());
               if( i3c1!=d3ll.end() ) ++i3c1;
               if( i3b1!=d3ll.end() ) ++i3b1;
            }

            if( i3c1==i3b  &&  !(*i3b).bAEqB ) // i3c before i3b
            {
               Diff3LineList::iterator i3 = i3c;
               int nofDisturbingLines = 0;
               while( i3 != i3b && i3!=d3ll.end() )
               {
                  if ( (*i3).lineB != -1 ) 
                     ++nofDisturbingLines;
                  ++i3;
               }

               if ( nofDisturbingLines>0 && nofDisturbingLines < d.nofEquals )
               {
                  // Move the disturbing lines up, out of sight.
                  i3 = i3c;
                  while( i3 != i3b )
                  {
                     if ( (*i3).lineB != -1 ) 
                     {
                        Diff3Line d3l;
                        d3l.lineB = (*i3).lineB;
                        (*i3).lineB = -1;


                        (*i3).bAEqB = false;
                        (*i3).bBEqC = false;
                        d3ll.insert( i3c, d3l );
                     }
                     ++i3;
                  }
                  nofDisturbingLines=0;
               }

               if ( nofDisturbingLines == 0 )
               {
                  // Yes, the line from B can be moved.
                  (*i3b).lineB = -1;   // This might leave an empty line: removed later.
                  (*i3b).bAEqB = false;
                  (*i3b).bAEqC = false;
                  (*i3b).bBEqC = false;
                  //(*i3b).lineC = -1;
                  (*i3c).lineB = lineB;
                  (*i3c).bBEqC = true;
               }
            }

            else if( i3b1==i3c  &&  !(*i3b).bAEqC)
            {
               Diff3LineList::iterator i3 = i3b;
               int nofDisturbingLines = 0;
               while( i3 != i3c && i3!=d3ll.end() )
               {
                  if ( (*i3).lineC != -1 ) 
                     ++nofDisturbingLines;
                  ++i3;
               }

               if ( nofDisturbingLines>0 && nofDisturbingLines < d.nofEquals )
               {
                  // Move the disturbing lines up, out of sight.
                  i3 = i3b;
                  while( i3 != i3c )
                  {
                     if ( (*i3).lineC != -1 ) 
                     {
                        Diff3Line d3l;
                        d3l.lineC = (*i3).lineC;
                        (*i3).lineC = -1;
                        (*i3).bAEqC = false;
                        (*i3).bBEqC = false;
                        d3ll.insert( i3b, d3l );
                     }
                     ++i3;

                  }
                  nofDisturbingLines=0;
               }

               if ( nofDisturbingLines == 0 )
               {
                  // Yes, the line from C can be moved.
                  (*i3c).lineC = -1;   // This might leave an empty line: removed later.
                  (*i3c).bAEqC = false;
                  (*i3c).bBEqC = false;
                  //(*i3c).lineB = -1;
                  (*i3b).lineC = lineC;
                  (*i3b).bBEqC = true;
               }
            }
         }
                  
         --d.nofEquals;
         ++lineB;
         ++lineC;
         ++i3b;
         ++i3c;
      }
      else if ( d.diff1>0 )
      {
         Diff3LineList::iterator i3 = i3b;
         while( (*i3).lineB!=lineB ) 
            ++i3;
         if( i3 != i3b  &&  (*i3).bAEqB==false )
         {
            // Take this line and move it up as far as possible
            d3l.lineB = lineB;
            d3ll.insert( i3b, d3l );
            (*i3).lineB = -1;
         }
         else
         {
            i3b=i3;
         }
         --d.diff1;
         ++lineB;
         ++i3b;

         if( d.diff2>0 )
         {
            --d.diff2;
            ++lineC;
         }
      }
      else if ( d.diff2>0 )
      {
         --d.diff2;
         ++lineC;
      }
   }
/*
   Diff3LineList::iterator it = d3ll.begin();
   int li=0;
   for( ; it!=d3ll.end(); ++it, ++li )
   {
      printf( "%4d %4d %4d %4d  A%c=B A%c=C B%c=C\n",  
         li, (*it).lineA, (*it).lineB, (*it).lineC, 
         (*it).bAEqB ? '=' : '!', (*it).bAEqC ? '=' : '!', (*it).bBEqC ? '=' : '!' );
   }
   printf("\n");*/
}

// Fourth step
void calcDiff3LineListTrim(       
   Diff3LineList& d3ll, LineData* pldA, LineData* pldB, LineData* pldC
   )
{
   const Diff3Line d3l_empty;
   d3ll.remove( d3l_empty );

   Diff3LineList::iterator i3 = d3ll.begin();
   Diff3LineList::iterator i3A = d3ll.begin();
   Diff3LineList::iterator i3B = d3ll.begin();
   Diff3LineList::iterator i3C = d3ll.begin();


   int line=0;
   int lineA=0;
   int lineB=0;
   int lineC=0;

   // The iterator i3 and the variable line look ahead.
   // The iterators i3A, i3B, i3C and corresponding lineA, lineB and lineC stop at empty lines, if found.
   // If possible, then the texts from the look ahead will be moved back to the empty places.

   for( ; i3!=d3ll.end(); ++i3, ++line )
   {

      if( line>lineA && (*i3).lineA != -1 && (*i3A).lineB!=-1 && (*i3A).bBEqC  &&

          equal( pldA[(*i3).lineA], pldB[(*i3A).lineB], false ))
      {
         // Empty space for A. A matches B and C in the empty line. Move it up.
         (*i3A).lineA = (*i3).lineA;
         (*i3A).bAEqB = true;
         (*i3A).bAEqC = true;
         (*i3).lineA = -1;
         (*i3).bAEqB = false;
         (*i3).bAEqC = false;
         ++i3A;
         ++lineA;
      }

      if( line>lineB && (*i3).lineB != -1 && (*i3B).lineA!=-1 && (*i3B).bAEqC  &&
          equal( pldB[(*i3).lineB], pldA[(*i3B).lineA], false ))
      {
         // Empty space for B. B matches A and C in the empty line. Move it up.
         (*i3B).lineB = (*i3).lineB;
         (*i3B).bAEqB = true;
         (*i3B).bBEqC = true;
         (*i3).lineB = -1;
         (*i3).bAEqB = false;
         (*i3).bBEqC = false;
         ++i3B;
         ++lineB;
      }

      if( line>lineC && (*i3).lineC != -1 && (*i3C).lineA!=-1 && (*i3C).bAEqB  &&
          equal( pldC[(*i3).lineC], pldA[(*i3C).lineA], false ))
      {
         // Empty space for C. C matches A and B in the empty line. Move it up.
         (*i3C).lineC = (*i3).lineC;
         (*i3C).bAEqC = true;
         (*i3C).bBEqC = true;
         (*i3).lineC = -1;
         (*i3).bAEqC = false;
         (*i3).bBEqC = false;
         ++i3C;
         ++lineC;
      }

      if( line>lineA && (*i3).lineA != -1 && !(*i3).bAEqB && !(*i3).bAEqC )
      {
         // Empty space for A. A doesn't match B or C. Move it up.
         (*i3A).lineA = (*i3).lineA;
         (*i3).lineA = -1;
         ++i3A;
         ++lineA;
      }

      if( line>lineB && (*i3).lineB != -1 && !(*i3).bAEqB && !(*i3).bBEqC )
      {
         // Empty space for B. B matches neither A nor C. Move B up.
         (*i3B).lineB = (*i3).lineB;
         (*i3).lineB = -1;
         ++i3B;
         ++lineB;
      }

      if( line>lineC && (*i3).lineC != -1 && !(*i3).bAEqC && !(*i3).bBEqC )
      {
         // Empty space for C. C matches neither A nor B. Move C up.
         (*i3C).lineC = (*i3).lineC;
         (*i3).lineC = -1;
         ++i3C;
         ++lineC;
      }

      if( line>lineA && line>lineB && (*i3).lineA != -1 && (*i3).bAEqB && !(*i3).bAEqC )
      {
         // Empty space for A and B. A matches B, but not C. Move A & B up.
         Diff3LineList::iterator i = lineA > lineB ? i3A   : i3B;
         int                     l = lineA > lineB ? lineA : lineB;
         (*i).lineA = (*i3).lineA;
         (*i).lineB = (*i3).lineB;
         (*i).bAEqB = true;
                  
         (*i3).lineA = -1;
         (*i3).lineB = -1;
         (*i3).bAEqB = false;
         i3A = i;
         i3B = i;
         ++i3A;
         ++i3B;
         lineA=l+1;
         lineB=l+1;
      }
      else if( line>lineA && line>lineC && (*i3).lineA != -1 && (*i3).bAEqC && !(*i3).bAEqB )
      {
         // Empty space for A and C. A matches C, but not B. Move A & C up.
         Diff3LineList::iterator i = lineA > lineC ? i3A   : i3C;
         int                     l = lineA > lineC ? lineA : lineC;
         (*i).lineA = (*i3).lineA;
         (*i).lineC = (*i3).lineC;
         (*i).bAEqC = true;
                  
         (*i3).lineA = -1;
         (*i3).lineC = -1;
         (*i3).bAEqC = false;
         i3A = i;
         i3C = i;
         ++i3A;
         ++i3C;
         lineA=l+1;
         lineC=l+1;
      }
      else if( line>lineB && line>lineC && (*i3).lineB != -1 && (*i3).bBEqC && !(*i3).bAEqC )
      {
         // Empty space for B and C. B matches C, but not A. Move B & C up.
         Diff3LineList::iterator i = lineB > lineC ? i3B   : i3C;
         int                     l = lineB > lineC ? lineB : lineC;
         (*i).lineB = (*i3).lineB;
         (*i).lineC = (*i3).lineC;
         (*i).bBEqC = true;
                  
         (*i3).lineB = -1;
         (*i3).lineC = -1;
         (*i3).bBEqC = false;
         i3B = i;
         i3C = i;
         ++i3B;
         ++i3C;
         lineB=l+1;
         lineC=l+1;
      }

      if ( (*i3).lineA != -1 )
      {
         lineA = line+1;
         i3A = i3;
         ++i3A;
      }
      if ( (*i3).lineB != -1 )
      {
         lineB = line+1;
         i3B = i3;
         ++i3B;
      }
      if ( (*i3).lineC != -1 )
      {
         lineC = line+1;
         i3C = i3;
         ++i3C;
      }
   }

   d3ll.remove( d3l_empty );

/*

   Diff3LineList::iterator it = d3ll.begin();
   int li=0;
   for( ; it!=d3ll.end(); ++it, ++li )
   {
      printf( "%4d %4d %4d %4d  A%c=B A%c=C B%c=C\n",  
         li, (*it).lineA, (*it).lineB, (*it).lineC, 
         (*it).bAEqB ? '=' : '!', (*it).bAEqC ? '=' : '!', (*it).bBEqC ? '=' : '!' );

   }
*/
}


// Just make sure that all input lines are in the output too, exactly once.
void debugLineCheck( Diff3LineList& d3ll, int size, int idx )
{
   Diff3LineList::iterator it = d3ll.begin();

   
   int i=0;

   for ( it = d3ll.begin(); it!= d3ll.end(); ++it )
   {
      int l;
      if      (idx==1) l=(*it).lineA;
      else if (idx==2) l=(*it).lineB;
      else if (idx==3) l=(*it).lineC;
      else assert(false);

      if ( l!=-1 )
      {
         if( l!=i )
         {
            KMessageBox::error(0, i18n(
               "Data loss error:\n"
               "If it is reproducable please contact the author.\n"
               ), "Severe internal Error" );
            assert(false);
            cerr << "Severe Internal Error.\n";
            exit(-1);
         }
         ++i;
      }
   }

   if( size!=i )
   {
      KMessageBox::error(0, i18n(
         "Data loss error:\n"
         "If it is reproducable please contact the author.\n"
         ), "Severe internal Error" );
      assert(false);
      cerr << "Severe Internal Error.\n";
      exit(-1);
   }
}


void fineDiff(
   Diff3LineList& diff3LineList,
   int selector,
   LineData* v1,
   LineData* v2
   )
{
   // Finetuning: Diff each line with deltas
   Diff3LineList::iterator i;
   int k1=0;
   int k2=0;
   for( i= diff3LineList.begin(); i!= diff3LineList.end(); ++i)
   {
      if      (selector==1){ k1=i->lineA; k2=i->lineB; }
      else if (selector==2){ k1=i->lineB; k2=i->lineC; }
      else if (selector==3){ k1=i->lineC; k2=i->lineA; }
      else assert(false);

      if( k1!=-1 && k2!=-1 )
      {
         if ( v1[k1].size != v2[k2].size || memcmp( v1[k1].pLine, v2[k2].pLine, v1[k1].size)!=0 )
         {
            DiffList* pDiffList = new DiffList;
            calcDiff( v1[k1].pLine, v1[k1].size, v2[k2].pLine, v2[k2].size, *pDiffList, 2 );

            // Optimize the diff list.
            DiffList::iterator dli;
            for( dli = pDiffList->begin(); dli!=pDiffList->end(); ++dli)
            {
               if( dli->nofEquals < 4  &&  (dli->diff1>0 || dli->diff2>0)  )
               {
                  dli->diff1 += dli->nofEquals;
                  dli->diff2 += dli->nofEquals;
                  dli->nofEquals = 0;
               }
            }

            if      (selector==1){ delete (*i).pFineAB; (*i).pFineAB = pDiffList; }
            else if (selector==2){ delete (*i).pFineBC; (*i).pFineBC = pDiffList; }
            else if (selector==3){ delete (*i).pFineCA; (*i).pFineCA = pDiffList; }
            else assert(false);
         }
      }
   }
}