/***************************************************************************
                          kdiff.cpp  -  description
                             -------------------
    begin                : Mon Mär 18 20:04:50 CET 2002
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

#include "diff.h"
#include "directorymergewindow.h"

#include <iostream>
#include <algorithm>
#include <ctype.h>
#include <qaccel.h>

#include <kfiledialog.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kfontdialog.h>
#include <kstatusbar.h>
#include <kkeydialog.h>

#include <qclipboard.h>
#include <qscrollbar.h>
#include <qlayout.h>
#include <qsplitter.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qdir.h>
#include <qfile.h>
#include <qvbuttongroup.h>
#include <qradiobutton.h>
#include <qdragobject.h>
#include <assert.h>

#include "kdiff3.h"
#include "optiondialog.h"
#include "fileaccess.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "gnudiff_diff.h"

//using namespace std;

int g_tabSize = 8;
bool g_bIgnoreWhiteSpace = true;
bool g_bIgnoreTrivialMatches = true;


QString createTempFile( const LineData* p, int size, bool bIgnoreWhiteSpace, bool bIgnoreNumbers )
{
   QString fileName = FileAccess::tempFileName();

   QFile file( fileName );
   bool bSuccess = file.open( IO_WriteOnly );
   if ( !bSuccess ) return "";
   for (int l=0; l<size; ++l)
   {
      // We don't want any white space in the diff output
      QCString s( p[l].size+1 );
      int is=0;
      for(int j=0; j<p[l].size; ++j )
      {
         char c = p[l].pLine[j];
         if ( !( bIgnoreWhiteSpace && isWhite( c )  ||
                 bIgnoreNumbers    && (isdigit( c ) || c=='-' || c=='.' )
               )
            )
         {
            if ( c=='\0' )  // Replace zeros with something else.
               s[is]=(char)0xFF;
            else
               s[is]=c;
            ++is;
         }
      }
      s[is]='\n';
      ++is;
      if( is != file.writeBlock( &s[0], is ) )
         return "";
   }
   return fileName;
}

bool KDiff3App::runDiff( LineData* p1, int size1, LineData* p2, int size2, DiffList& diffList )
{
   static GnuDiff gnuDiff;
   
   g_pProgressDialog->setSubCurrent(0);

   diffList.clear();
   if ( p1[0].pLine==0 || p2[0].pLine==0 )
   {
      Diff d( 0,0,0);
      if ( p1[0].pLine==0 && p2[0].pLine==0 && size1 == size2 )
         d.nofEquals = size1;
      else 
      {
         d.diff1=size1;
         d.diff2=size2;
      }

      diffList.push_back(d);
   }
   else
   {   
      GnuDiff::comparison comparisonInput;
      memset( &comparisonInput, 0, sizeof(comparisonInput) );
      comparisonInput.parent = 0;
      comparisonInput.file[0].buffer = (word*)p1[0].pLine;//ptr to buffer
      comparisonInput.file[0].buffered = p1[size1-1].pLine-p1[0].pLine+p1[size1-1].size; // size of buffer
      comparisonInput.file[1].buffer = (word*)p2[0].pLine;//ptr to buffer
      comparisonInput.file[1].buffered = p2[size2-1].pLine-p2[0].pLine+p2[size2-1].size; // size of buffer
   
      gnuDiff.ignore_white_space = GnuDiff::IGNORE_ALL_SPACE;  // I think nobody needs anything else ...
      gnuDiff.bIgnoreWhiteSpace = true;
      gnuDiff.bIgnoreNumbers    = m_pOptionDialog->m_bIgnoreNumbers;
      gnuDiff.minimal = m_pOptionDialog->m_bTryHard;
      gnuDiff.ignore_case = false;  //  m_pOptionDialog->m_bUpCase is applied while reading.
      GnuDiff::change* script = gnuDiff.diff_2_files( &comparisonInput );
   
      int equalLinesAtStart =  comparisonInput.file[0].prefix_lines;
      int currentLine1 = 0;
      int currentLine2 = 0;
      GnuDiff::change* p=0;
      for (GnuDiff::change* e = script; e; e = p)
      {
         Diff d(0,0,0);
         d.nofEquals = e->line0 - currentLine1;
         assert( d.nofEquals == e->line1 - currentLine2 );
         d.diff1 = e->deleted;
         d.diff2 = e->inserted;
         currentLine1 += d.nofEquals + d.diff1;
         currentLine2 += d.nofEquals + d.diff2;
         diffList.push_back(d);
   
         p = e->link;
         free (e);
      }
   
      if ( !diffList.empty() )
      {
         diffList.front().nofEquals += equalLinesAtStart;
         currentLine1 += equalLinesAtStart;
         currentLine2 += equalLinesAtStart;
      }
   
      if (size1-currentLine1==size2-currentLine2 )
      {
         Diff d( size1-currentLine1,0,0);
         diffList.push_back(d);
      }
      else if ( !diffList.empty() )
      {  // Only necessary for a files that end with a newline
         int nofEquals = min2(size1-currentLine1,size2-currentLine2);
         if ( nofEquals==0 )
         {
            diffList.back().diff1 += size1-currentLine1;
            diffList.back().diff2 += size2-currentLine2;
         }
         else
         {
            Diff d( nofEquals,size1-currentLine1-nofEquals,size2-currentLine2-nofEquals);
            diffList.push_back(d);
         }
      }
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

   g_pProgressDialog->setSubCurrent(1.0);

   return true;
#if 0

   if ( m_pOptionDialog->m_bUseExternalDiff )
   {
      bool bSuccess=false;

      bool bIgnoreWhiteSpace = m_pOptionDialog->m_bIgnoreWhiteSpace;
      bool bIgnoreNumbers = m_pOptionDialog->m_bIgnoreNumbers;

      // Create two temp files (Remove all white spaces and carriage returns here)
      // (Make sure that no existing files are overwritten.)
      g_pProgressDialog->setSubCurrent(0);
      QString fileName1 = createTempFile( p1, size1, bIgnoreWhiteSpace, bIgnoreNumbers );
      g_pProgressDialog->setSubCurrent(0.25);
      QString fileName2 = createTempFile( p2, size2, bIgnoreWhiteSpace, bIgnoreNumbers );
      g_pProgressDialog->setSubCurrent(0.5);
      QString fileNameOut = FileAccess::tempFileName();

      if ( !fileName1.isEmpty() && !fileName2.isEmpty() )
      {
         QString cmd;
#ifdef _WIN32
         // Under Windows it's better to search for diff.exe
         char buf[200];
         int r= SearchPathA( 0, "diff.exe",  0, sizeof(buf), buf, 0 );

         if (r!=0) { cmd = buf; }
#else
         // under Un*x I assume that a diff command is in the PATH
         cmd = "diff";
#endif
         if ( !cmd.isEmpty() )
         {
            cmd += " -a ";     // -a = treat all files as text
            if ( m_pOptionDialog->m_bTryHard )
               cmd += "--minimal ";
            //if ( m_pOptionDialog->m_bIgnoreWhiteSpace ) This I do myself, see below
            //   cmd += "--ignore-all-space ";
            cmd += fileName1+" "+fileName2+" >"+fileNameOut;
            // Run diff
            //int status1 = ::system( 0 ); // ==0 if shell not found
            int status = ::system( cmd.ascii() );
#ifdef WEXITSTATUS
            status = WEXITSTATUS(status);
#endif
            //if (status<0)
            //{
            //   errorString = strerror( errno );
            //}
            bSuccess = status>=0 && status!=127;
         }
      }

      g_pProgressDialog->setSubCurrent(0.75);

      int currentLine1 = 0;
      int currentLine2 = 0;
      if ( bSuccess )
      {
         // Parse the output and create the difflist
         QFile f( fileNameOut );
         bSuccess = f.open(IO_ReadOnly);
         if (bSuccess)
         {
            const QByteArray buf = f.readAll();
            unsigned int bufSize=buf.size();
            unsigned int pos=0;
            for(pos=0;pos<bufSize;++pos)
            {
               unsigned int lineStart = pos;
               // Find end of line
               while( buf.at(pos)!='\n' && pos<bufSize )
                  ++pos;

               // parse it
               char c = buf.at(lineStart);
               if ( c == '>' || c == '-' || c=='<' )
                  continue;  // Not interested in the data
               else
               {
                  QCString line( &buf.at(lineStart), pos-lineStart+1 );
                  int pa = line.find('a'); // add
                  int pc = line.find('c'); // change
                  int pd = line.find('d'); // delete
                  int p = pa>0 ? pa : (pc > 0 ? pc : pd);
                  if (p<0) break; // Unexpected error
                  QCString left = line.left(p);
                  QCString right = line.mid(p+1);
                  int pcommaleft = left.find(',');
                  int pcommaright = right.find(',');
                  int l1top=-1, l1bottom=-1, l2top=-1, l2bottom=-1;
                  if (pcommaleft>0)
                  {
                     l1top    = left.left(pcommaleft).toInt();
                     l1bottom = left.mid(pcommaleft+1).toInt();
                  }
                  else
                  {
                     l1top    = left.toInt();
                     l1bottom = l1top;
                  }
                  if (pcommaright>0)
                  {
                     l2top    = right.left(pcommaright).toInt();
                     l2bottom = right.mid(pcommaright+1).toInt();
                  }
                  else
                  {
                     l2top    = right.toInt();
                     l2bottom = l2top;
                  }

                  Diff d(0,0,0);

                  if ( pa>0 )
                  {
                     d.nofEquals = l1top - currentLine1;
                     d.diff1 = 0;
                     d.diff2 = l2bottom - l2top + 1;
                     assert( d.nofEquals == l2top - 1 - currentLine2 );
                  }
                  else if ( pd>0 )
                  {
                     d.nofEquals = l2top - currentLine2;
                     d.diff1 = l1bottom - l1top + 1;
                     d.diff2 = 0;
                     assert( d.nofEquals == l1top - 1 - currentLine1 );
                  }
                  else if ( pc>0 )
                  {
                     d.nofEquals = l1top - 1 - currentLine1;
                     d.diff1 = l1bottom - l1top + 1;
                     d.diff2 = l2bottom - l2top + 1;
                     assert( d.nofEquals == l2top - 1 - currentLine2 );
                  }
                  else
                     assert(false);

                  currentLine1 += d.nofEquals + d.diff1;
                  currentLine2 += d.nofEquals + d.diff2;
                  diffList.push_back(d);
               }
            }
            if (size1-currentLine1==size2-currentLine2 )
            {
               Diff d( size1-currentLine1,0,0);
               diffList.push_back(d);
               currentLine1=size1;
               currentLine2=size2;
            }

            if ( currentLine1 == size1 && currentLine2 == size2 )
               bSuccess = true;
         }
         else
         {
            bSuccess = size1==size2;
         }
      }
      if ( currentLine1==0 && currentLine2==0 )
      {
         Diff d( 0, size1, size2 );
         diffList.push_back(d);
      }

      g_pProgressDialog->setSubCurrent(1.0);

      if ( !bSuccess )
      {
         KMessageBox::sorry(this,
            i18n("Running the external diff failed.\n"
                 "Check if the diff works, if the program can write in the temp folder or if the disk is full.\n"
                 "The external diff option will be disabled now and the internal diff will be used."),
            i18n("Warning"));
         m_pOptionDialog->m_bUseExternalDiff = false;
      }

      FileAccess::removeFile( fileName1 );
      FileAccess::removeFile( fileName2 );
      FileAccess::removeFile( fileNameOut );
   }

   if ( ! m_pOptionDialog->m_bUseExternalDiff )
   {
      g_pProgressDialog->setSubCurrent(0.0);
      if ( size1>0 && p1!=0 && p1->occurances==0 )
      {
         prepareOccurances( p1, size1 );
      }
      g_pProgressDialog->setSubCurrent(0.25);

      if ( size2>0 && p2!=0 && p2->occurances==0 )
      {
         prepareOccurances( p2, size2 );
      }
      g_pProgressDialog->setSubCurrent(0.5);

      calcDiff( p1, size1, p2, size2, diffList, 2, m_pOptionDialog->m_maxSearchLength );

      g_pProgressDialog->setSubCurrent(1.0);
   }
   return true;
#endif
}


void KDiff3App::init( bool bAuto )
{
   bool bPreserveCarriageReturn = m_pOptionDialog->m_bPreserveCarriageReturn;

   bool bVisibleMergeResultWindow = ! m_outputFilename.isEmpty();
   if ( bVisibleMergeResultWindow )
   {
      bPreserveCarriageReturn = false;
   }

   m_diff3LineList.clear();

   bool bUseLineMatchingPP = !m_pOptionDialog->m_LineMatchingPreProcessorCmd.isEmpty() ||
                             m_pOptionDialog->m_bIgnoreComments;
   bool bUpCase = m_pOptionDialog->m_bUpCase;

   if (m_pDiffTextWindow1) m_pDiffTextWindow1->setPaintingAllowed( false );
   if (m_pDiffTextWindow2) m_pDiffTextWindow2->setPaintingAllowed( false );
   if (m_pDiffTextWindow3) m_pDiffTextWindow3->setPaintingAllowed( false );
   if (m_pOverview)        m_pOverview->setPaintingAllowed( false );


   if( m_sd3.isEmpty() )
      g_pProgressDialog->setMaximum( 4 );  // Read 2 files, 1 comparison, 1 finediff
   else
      g_pProgressDialog->setMaximum( 9 );  // Read 3 files, 3 comparisons, 3 finediffs

   g_pProgressDialog->start();

   // First get all input data.
   g_pProgressDialog->setInformation(i18n("Loading A"));
   m_sd1.readPPFile( bPreserveCarriageReturn, m_pOptionDialog->m_PreProcessorCmd, bUpCase );
   m_sdlm1.readLMPPFile( &m_sd1, m_pOptionDialog->m_LineMatchingPreProcessorCmd, bUpCase, m_pOptionDialog->m_bIgnoreComments );
   g_pProgressDialog->step();

   g_pProgressDialog->setInformation(i18n("Loading B"));
   m_sd2.readPPFile( bPreserveCarriageReturn, m_pOptionDialog->m_PreProcessorCmd, bUpCase );
   m_sdlm2.readLMPPFile( &m_sd2, m_pOptionDialog->m_LineMatchingPreProcessorCmd, bUpCase, m_pOptionDialog->m_bIgnoreComments );
   g_pProgressDialog->step();

   m_sd3.m_bIsText = true;
   m_totalDiffStatus.reset();
   // Run the diff.
   if ( m_sd3.isEmpty() )
   {
      m_totalDiffStatus.bBinaryAEqB = (m_sd1.m_size!=0 && m_sd1.m_size==m_sd2.m_size  &&  memcmp(m_sd1.m_pBuf,m_sd2.m_pBuf,m_sd1.m_size)==0);
      g_pProgressDialog->setInformation(i18n("Diff: A <-> B"));
      if ( !bUseLineMatchingPP )
         runDiff( &m_sd1.m_v[0], m_sd1.m_vSize, &m_sd2.m_v[0], m_sd2.m_vSize, m_diffList12 );
      else
         runDiff( &m_sdlm1.m_v[0], m_sdlm1.m_vSize, &m_sdlm2.m_v[0], m_sdlm2.m_vSize, m_diffList12 );

      g_pProgressDialog->step();

      g_pProgressDialog->setInformation(i18n("Linediff: A <-> B"));
      calcDiff3LineListUsingAB( &m_diffList12, m_diff3LineList );
      fineDiff( m_diff3LineList, 1, &m_sd1.m_v[0], &m_sd2.m_v[0], m_totalDiffStatus.bTextAEqB );
      if ( m_sd1.m_size==0 ) m_totalDiffStatus.bTextAEqB=false;

      g_pProgressDialog->step();
   }
   else
   {
      g_pProgressDialog->setInformation(i18n("Loading C"));
      m_sd3.readPPFile( bPreserveCarriageReturn, m_pOptionDialog->m_PreProcessorCmd, bUpCase );
      m_sdlm3.readLMPPFile( &m_sd3, m_pOptionDialog->m_LineMatchingPreProcessorCmd, bUpCase, m_pOptionDialog->m_bIgnoreComments );
      g_pProgressDialog->step();

      m_totalDiffStatus.bBinaryAEqB = (m_sd1.m_size!=0 && m_sd1.m_size==m_sd2.m_size  &&  memcmp(m_sd1.m_pBuf,m_sd2.m_pBuf,m_sd1.m_size)==0);
      m_totalDiffStatus.bBinaryAEqC = (m_sd1.m_size!=0 && m_sd1.m_size==m_sd3.m_size  &&  memcmp(m_sd1.m_pBuf,m_sd3.m_pBuf,m_sd1.m_size)==0);
      m_totalDiffStatus.bBinaryBEqC = (m_sd3.m_size!=0 && m_sd3.m_size==m_sd2.m_size  &&  memcmp(m_sd3.m_pBuf,m_sd2.m_pBuf,m_sd3.m_size)==0);

      if ( !bUseLineMatchingPP )
      {
         g_pProgressDialog->setInformation(i18n("Diff: A <-> B"));
         runDiff( &m_sd1.m_v[0], m_sd1.m_vSize, &m_sd2.m_v[0], m_sd2.m_vSize, m_diffList12 );
         g_pProgressDialog->step();
         g_pProgressDialog->setInformation(i18n("Diff: B <-> C"));
         runDiff( &m_sd2.m_v[0], m_sd2.m_vSize, &m_sd3.m_v[0], m_sd3.m_vSize, m_diffList23 );
         g_pProgressDialog->step();
         g_pProgressDialog->setInformation(i18n("Diff: A <-> C"));
         runDiff( &m_sd1.m_v[0], m_sd1.m_vSize, &m_sd3.m_v[0], m_sd3.m_vSize, m_diffList13 );
         g_pProgressDialog->step();
      }
      else
      {
         g_pProgressDialog->setInformation(i18n("Diff: A <-> B"));
         runDiff( &m_sdlm1.m_v[0], m_sd1.m_vSize, &m_sdlm2.m_v[0], m_sd2.m_vSize, m_diffList12 );
         g_pProgressDialog->step();
         g_pProgressDialog->setInformation(i18n("Diff: B <-> C"));
         runDiff( &m_sdlm2.m_v[0], m_sd2.m_vSize, &m_sdlm3.m_v[0], m_sd3.m_vSize, m_diffList23 );
         g_pProgressDialog->step();
         g_pProgressDialog->setInformation(i18n("Diff: A <-> C"));
         runDiff( &m_sdlm1.m_v[0], m_sd1.m_vSize, &m_sdlm3.m_v[0], m_sd3.m_vSize, m_diffList13 );
         g_pProgressDialog->step();
      }

      calcDiff3LineListUsingAB( &m_diffList12, m_diff3LineList );
      calcDiff3LineListUsingAC( &m_diffList13, m_diff3LineList );
      calcDiff3LineListTrim( m_diff3LineList, &m_sd1.m_v[0], &m_sd2.m_v[0], &m_sd3.m_v[0] );

      calcDiff3LineListUsingBC( &m_diffList23, m_diff3LineList );
      calcDiff3LineListTrim( m_diff3LineList, &m_sd1.m_v[0], &m_sd2.m_v[0], &m_sd3.m_v[0] );
      debugLineCheck( m_diff3LineList, m_sd1.m_vSize, 1 );
      debugLineCheck( m_diff3LineList, m_sd2.m_vSize, 2 );
      debugLineCheck( m_diff3LineList, m_sd3.m_vSize, 3 );

      g_pProgressDialog->setInformation(i18n("Linediff: A <-> B"));
      fineDiff( m_diff3LineList, 1, &m_sd1.m_v[0], &m_sd2.m_v[0], m_totalDiffStatus.bTextAEqB );
      g_pProgressDialog->step();
      g_pProgressDialog->setInformation(i18n("Linediff: B <-> C"));
      fineDiff( m_diff3LineList, 2, &m_sd2.m_v[0], &m_sd3.m_v[0], m_totalDiffStatus.bTextBEqC );
      g_pProgressDialog->step();
      g_pProgressDialog->setInformation(i18n("Linediff: A <-> C"));
      fineDiff( m_diff3LineList, 3, &m_sd3.m_v[0], &m_sd1.m_v[0], m_totalDiffStatus.bTextAEqC );
      g_pProgressDialog->step();
      if ( m_sd1.m_size==0 ) { m_totalDiffStatus.bTextAEqB=false;  m_totalDiffStatus.bTextAEqC=false; }
      if ( m_sd2.m_size==0 ) { m_totalDiffStatus.bTextAEqB=false;  m_totalDiffStatus.bTextBEqC=false; }
   }
   calcWhiteDiff3Lines( m_diff3LineList, &m_sd1.m_v[0], &m_sd2.m_v[0], &m_sd3.m_v[0] );
   calcDiff3LineVector( m_diff3LineList, m_diff3LineVector );

   // Calc needed lines for display
   m_neededLines = m_diff3LineList.size();

   m_pDirectoryMergeWindow->allowResizeEvents(false);
   initView();
   m_pDirectoryMergeWindow->allowResizeEvents(true);

   m_bTripleDiff = ! m_sd3.isEmpty();

   m_pDiffTextWindow1->init( m_sd1.getAliasName(),
      &m_sd1.m_v[0], m_sd1.m_vSize, &m_diff3LineVector, 1, m_bTripleDiff );
   m_pDiffTextWindow2->init( m_sd2.getAliasName(),
      &m_sd2.m_v[0], m_sd2.m_vSize, &m_diff3LineVector, 2, m_bTripleDiff );
   m_pDiffTextWindow3->init( m_sd3.getAliasName(),
      &m_sd3.m_v[0], m_sd3.m_vSize, &m_diff3LineVector, 3, m_bTripleDiff );

   if (m_bTripleDiff) m_pDiffTextWindow3->show();
   else               m_pDiffTextWindow3->hide();

   m_bOutputModified = bVisibleMergeResultWindow;

   m_pMergeResultWindow->init(
      &m_sd1.m_v[0],
      &m_sd2.m_v[0],
      m_bTripleDiff ? &m_sd3.m_v[0] : 0,
      &m_diff3LineList,
      &m_totalDiffStatus,
      m_outputFilename.isEmpty() ? QString("unnamed.txt") : m_outputFilename
      );

   m_pOverview->init(&m_diff3LineList, m_bTripleDiff );
   m_pDiffVScrollBar->setValue( 0 );
   m_pHScrollBar->setValue( 0 );

   g_pProgressDialog->hide();
   m_pDiffTextWindow1->setPaintingAllowed( true );
   m_pDiffTextWindow2->setPaintingAllowed( true );
   m_pDiffTextWindow3->setPaintingAllowed( true );
   m_pOverview->setPaintingAllowed( true );

   if ( !bVisibleMergeResultWindow )
      m_pMergeWindowFrame->hide();
   else
      m_pMergeWindowFrame->show();

   // Calc max width for display
   m_maxWidth = max2( m_pDiffTextWindow1->getNofColumns(), m_pDiffTextWindow2->getNofColumns() );
   m_maxWidth = max2( m_maxWidth, m_pDiffTextWindow3->getNofColumns() );
   m_maxWidth += 5;

   if ( bVisibleMergeResultWindow && !bAuto )
      m_pMergeResultWindow->showNrOfConflicts();
   else if ( !bAuto )
   {
      QString totalInfo;
      if ( m_totalDiffStatus.bBinaryAEqB && m_totalDiffStatus.bBinaryAEqC )
         totalInfo += i18n("All input files are binary equal.");
      else  if ( m_totalDiffStatus.bTextAEqB && m_totalDiffStatus.bTextAEqC )
         totalInfo += i18n("All input files contain the same text.");
      else {
         if    ( m_totalDiffStatus.bBinaryAEqB ) totalInfo += i18n("Files A and B are binary equal.\n");
         else if ( m_totalDiffStatus.bTextAEqB ) totalInfo += i18n("Files A and B have equal text. \n");
         if    ( m_totalDiffStatus.bBinaryAEqC ) totalInfo += i18n("Files A and C are binary equal.\n");
         else if ( m_totalDiffStatus.bTextAEqC ) totalInfo += i18n("Files A and C have equal text. \n");
         if    ( m_totalDiffStatus.bBinaryBEqC ) totalInfo += i18n("Files B and C are binary equal.\n");
         else if ( m_totalDiffStatus.bTextBEqC ) totalInfo += i18n("Files B and C have equal text. \n");
      }

      if ( !totalInfo.isEmpty() )
         KMessageBox::information( this, totalInfo );
   }

   if ( bVisibleMergeResultWindow && (!m_sd1.m_bIsText || !m_sd2.m_bIsText || !m_sd3.m_bIsText) )
   {
      KMessageBox::information( this, i18n(
         "Some inputfiles don't seem to be pure textfiles.\n"
         "Note that the KDiff3-merge was not meant for binary data.\n"
         "Continue at your own risk.") );
/*      QDialog d(this);
      QVBoxLayout* l = new QVBoxLayout(&d);
      QVButtonGroup* bg = new QVButtonGroup(i18n("Some File(s) Don't Seem to be Text Files"),&d);
      l->addWidget(bg);
      new QRadioButton("A: " + m_sd1.getAliasName(), bg);
      new QRadioButton("B: " + m_sd2.getAliasName(), bg);
      if (!m_sd3.isEmpty()) new QRadioButton("C: " + m_sd3.getAliasName(), bg);
      bg->setButton(0);
      QHBoxLayout* hbl = new QHBoxLayout(l);
      QPushButton* pSaveButton = new QPushButton( i18n("Save Selection"), &d );
      connect( pSaveButton, SIGNAL(clicked()), &d, SLOT(accept()));
      hbl->addWidget( pSaveButton );
      QPushButton* pProceedButton = new QPushButton( i18n("Proceed with Text-Merge"), &d );
      connect( pProceedButton, SIGNAL(clicked()), &d, SLOT(reject()));
      hbl->addWidget( pProceedButton );

      int result = d.exec();
      if( result == QDialog::Accepted )
      {
         if      ( bg->find(0)->isOn() ) saveBuffer( m_sd1.m_pBuf, m_sd1.m_size, m_outputFilename );
         else if ( bg->find(1)->isOn() ) saveBuffer( m_sd2.m_pBuf, m_sd2.m_size, m_outputFilename );
         else                            saveBuffer( m_sd3.m_pBuf, m_sd3.m_size, m_outputFilename );
      }*/
   }

   QTimer::singleShot( 10, this, SLOT(slotAfterFirstPaint()) );
   
   if ( bVisibleMergeResultWindow && m_pMergeResultWindow )
   {
      m_pMergeResultWindow->setFocus();
   }
   else if(m_pDiffTextWindow1)
   {
      m_pDiffTextWindow1->setFocus();
   }
}


void KDiff3App::resizeDiffTextWindow(int newWidth, int newHeight)
{
   m_DTWHeight = newHeight;
   m_pDiffVScrollBar->setRange(0, max2(0, m_neededLines+1 - newHeight) );
   m_pDiffVScrollBar->setPageStep( newHeight );
   m_pOverview->setRange( m_pDiffVScrollBar->value(), m_pDiffVScrollBar->pageStep() );

   // The second window has a somewhat inverse width

   m_pHScrollBar->setRange(0, max2(0, m_maxWidth - newWidth) );
   m_pHScrollBar->setPageStep( newWidth );
}

void KDiff3App::resizeMergeResultWindow()
{
   MergeResultWindow* p = m_pMergeResultWindow;
   m_pMergeVScrollBar->setRange(0, max2(0, p->getNofLines() - p->getNofVisibleLines()) );
   m_pMergeVScrollBar->setPageStep( p->getNofVisibleLines() );

   // The second window has a somewhat inverse width
//   m_pHScrollBar->setRange(0, max2(0, m_maxWidth - newWidth) );
//   m_pHScrollBar->setPageStep( newWidth );
}

void KDiff3App::scrollDiffTextWindow( int deltaX, int deltaY )
{
   if ( deltaY!= 0 )
   {
      m_pDiffVScrollBar->setValue( m_pDiffVScrollBar->value() + deltaY );
      m_pOverview->setRange( m_pDiffVScrollBar->value(), m_pDiffVScrollBar->pageStep() );
   }
   if ( deltaX!= 0)
      m_pHScrollBar->setValue( m_pHScrollBar->value() + deltaX );
}

void KDiff3App::scrollMergeResultWindow( int deltaX, int deltaY )
{
   if ( deltaY!= 0 )
      m_pMergeVScrollBar->setValue( m_pMergeVScrollBar->value() + deltaY );
   if ( deltaX!= 0)
      m_pHScrollBar->setValue( m_pHScrollBar->value() + deltaX );
}

void KDiff3App::setDiff3Line( int line )
{
   m_pDiffVScrollBar->setValue( line );
}

void KDiff3App::sourceMask( int srcMask, int enabledMask )
{
   chooseA->setChecked( (srcMask & 1) != 0 );
   chooseB->setChecked( (srcMask & 2) != 0 );
   chooseC->setChecked( (srcMask & 4) != 0 );
   chooseA->setEnabled( (enabledMask & 1) != 0 );
   chooseB->setEnabled( (enabledMask & 2) != 0 );
   chooseC->setEnabled( (enabledMask & 4) != 0 );
}



// Function uses setMinSize( sizeHint ) before adding the widget.
// void addWidget(QBoxLayout* layout, QWidget* widget);
template <class W, class L>
void addWidget( L* layout, W* widget)
{
   QSize s = widget->sizeHint();
   widget->setMinimumSize( QSize(max2(s.width(),0),max2(s.height(),0) ) );
   layout->addWidget( widget );
}

void KDiff3App::initView()
{
   // set the main widget here
   QValueList<int> oldHeights;
   if ( m_pDirectoryMergeSplitter->isVisible() )
   {
      oldHeights = m_pMainSplitter->sizes();
   }

   if ( m_pMainWidget != 0 )
   {
      return;
      //delete m_pMainWidget;
   }
   m_pMainWidget = new QFrame(m_pMainSplitter);
   m_pMainWidget->setFrameStyle( QFrame::Panel | QFrame::Sunken );
   m_pMainWidget->setLineWidth(1);


   QVBoxLayout* pVLayout = new QVBoxLayout(m_pMainWidget);

   QSplitter* pVSplitter = new QSplitter( m_pMainWidget );
   pVSplitter->setOrientation( Vertical );
   pVLayout->addWidget( pVSplitter );

   QWidget* pDiffWindowFrame = new QFrame( pVSplitter );
   QHBoxLayout* pDiffHLayout = new QHBoxLayout( pDiffWindowFrame );

   m_pDiffWindowSplitter = new QSplitter( pDiffWindowFrame );
   m_pDiffWindowSplitter->setOrientation( m_pOptionDialog->m_bHorizDiffWindowSplitting ?  Horizontal : Vertical );
   pDiffHLayout->addWidget( m_pDiffWindowSplitter );

   m_pOverview = new Overview( pDiffWindowFrame, m_pOptionDialog );
   pDiffHLayout->addWidget(m_pOverview);
   connect( m_pOverview, SIGNAL(setLine(int)), this, SLOT(setDiff3Line(int)) );
   //connect( m_pOverview, SIGNAL(afterFirstPaint()), this, SLOT(slotAfterFirstPaint()));

   m_pDiffVScrollBar = new QScrollBar( Vertical, pDiffWindowFrame );
   pDiffHLayout->addWidget( m_pDiffVScrollBar );

   m_pDiffTextWindow1 = new DiffTextWindow( m_pDiffWindowSplitter, statusBar(), m_pOptionDialog );
   m_pDiffTextWindow2 = new DiffTextWindow( m_pDiffWindowSplitter, statusBar(), m_pOptionDialog );
   m_pDiffTextWindow3 = new DiffTextWindow( m_pDiffWindowSplitter, statusBar(), m_pOptionDialog );

   // Merge window
   m_pMergeWindowFrame = new QFrame( pVSplitter );
   QHBoxLayout* pMergeHLayout = new QHBoxLayout( m_pMergeWindowFrame );

   m_pMergeResultWindow = new MergeResultWindow( m_pMergeWindowFrame, m_pOptionDialog );
   pMergeHLayout->addWidget( m_pMergeResultWindow );

   m_pMergeVScrollBar = new QScrollBar( Vertical, m_pMergeWindowFrame );
   pMergeHLayout->addWidget( m_pMergeVScrollBar );

   autoAdvance->setEnabled(true);

   QValueList<int> sizes = pVSplitter->sizes();
   int total = sizes[0] + sizes[1];
   sizes[0]=total/2; sizes[1]=total/2;
   pVSplitter->setSizes( sizes );

   m_pMergeResultWindow->installEventFilter( this );       // for Cut/Copy/Paste-shortcuts

   QHBoxLayout* pHScrollBarLayout = new QHBoxLayout( pVLayout );
   m_pHScrollBar = new QScrollBar( Horizontal, m_pMainWidget );
   pHScrollBarLayout->addWidget( m_pHScrollBar );
   m_pCornerWidget = new QWidget( m_pMainWidget );
   pHScrollBarLayout->addWidget( m_pCornerWidget );


   connect( m_pDiffVScrollBar, SIGNAL(valueChanged(int)), m_pOverview, SLOT(setFirstLine(int)));
   connect( m_pDiffVScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow1, SLOT(setFirstLine(int)));
   connect( m_pHScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow1, SLOT(setFirstColumn(int)));
   connect( m_pDiffTextWindow1, SIGNAL(newSelection()), this, SLOT(slotSelectionStart()));
   connect( m_pDiffTextWindow1, SIGNAL(selectionEnd()), this, SLOT(slotSelectionEnd()));
   connect( m_pDiffTextWindow1, SIGNAL(scroll(int,int)), this, SLOT(scrollDiffTextWindow(int,int)));
   m_pDiffTextWindow1->installEventFilter( this );

   connect( m_pDiffVScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow2, SLOT(setFirstLine(int)));
   connect( m_pHScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow2, SLOT(setFirstColumn(int)));
   connect( m_pDiffTextWindow2, SIGNAL(newSelection()), this, SLOT(slotSelectionStart()));
   connect( m_pDiffTextWindow2, SIGNAL(selectionEnd()), this, SLOT(slotSelectionEnd()));
   connect( m_pDiffTextWindow2, SIGNAL(scroll(int,int)), this, SLOT(scrollDiffTextWindow(int,int)));
   m_pDiffTextWindow2->installEventFilter( this );

   connect( m_pDiffVScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow3, SLOT(setFirstLine(int)));
   connect( m_pHScrollBar, SIGNAL(valueChanged(int)), m_pDiffTextWindow3, SLOT(setFirstColumn(int)));
   connect( m_pDiffTextWindow3, SIGNAL(newSelection()), this, SLOT(slotSelectionStart()));
   connect( m_pDiffTextWindow3, SIGNAL(selectionEnd()), this, SLOT(slotSelectionEnd()));
   connect( m_pDiffTextWindow3, SIGNAL(scroll(int,int)), this, SLOT(scrollDiffTextWindow(int,int)));
   m_pDiffTextWindow3->installEventFilter( this );

   MergeResultWindow* p = m_pMergeResultWindow;
   connect( m_pMergeVScrollBar, SIGNAL(valueChanged(int)), p, SLOT(setFirstLine(int)));

   connect( m_pHScrollBar,      SIGNAL(valueChanged(int)), p, SLOT(setFirstColumn(int)));
   connect( p, SIGNAL(scroll(int,int)), this, SLOT(scrollMergeResultWindow(int,int)));
   connect( p, SIGNAL(sourceMask(int,int)), this, SLOT(sourceMask(int,int)));
   connect( p, SIGNAL( resizeSignal() ),this, SLOT(resizeMergeResultWindow()));
   connect( p, SIGNAL( selectionEnd() ), this, SLOT( slotSelectionEnd() ) );
   connect( p, SIGNAL( newSelection() ), this, SLOT( slotSelectionStart() ) );
   connect( p, SIGNAL( modified() ), this, SLOT( slotOutputModified() ) );
   connect( p, SIGNAL( updateAvailabilities() ), this, SLOT( slotUpdateAvailabilities() ) );
   connect( p, SIGNAL( showPopupMenu(const QPoint&) ), this, SLOT(showPopupMenu(const QPoint&)));
   sourceMask(0,0);


   connect( p, SIGNAL(setFastSelectorRange(int,int)), m_pDiffTextWindow1, SLOT(setFastSelectorRange(int,int)));
   connect( p, SIGNAL(setFastSelectorRange(int,int)), m_pDiffTextWindow2, SLOT(setFastSelectorRange(int,int)));
   connect( p, SIGNAL(setFastSelectorRange(int,int)), m_pDiffTextWindow3, SLOT(setFastSelectorRange(int,int)));
   connect(m_pDiffTextWindow1, SIGNAL(setFastSelectorLine(int)), p, SLOT(slotSetFastSelectorLine(int)));
   connect(m_pDiffTextWindow2, SIGNAL(setFastSelectorLine(int)), p, SLOT(slotSetFastSelectorLine(int)));
   connect(m_pDiffTextWindow3, SIGNAL(setFastSelectorLine(int)), p, SLOT(slotSetFastSelectorLine(int)));

   connect( m_pDiffTextWindow1, SIGNAL( resizeSignal(int,int) ),this, SLOT(resizeDiffTextWindow(int,int)));

   m_pDiffTextWindow1->setFocus();
   m_pMainWidget->setMinimumSize(50,50);
   if ( m_pDirectoryMergeSplitter->isVisible() )
   {
      if (oldHeights.count() < 2)
         oldHeights.append(0);
      if (oldHeights[1]==0)    // Distribute the available space evenly between the two widgets.
      {
         oldHeights[1] = oldHeights[0]/2;
         oldHeights[0] -= oldHeights[1];
      }
      m_pMainSplitter->setSizes( oldHeights );
   }
   m_pCornerWidget->setFixedSize( m_pDiffVScrollBar->width(), m_pHScrollBar->height() );
   //show();
   m_pMainWidget->show();
   showWindowA->setChecked( true );
   showWindowB->setChecked( true );
   showWindowC->setChecked( true );
}

void KDiff3App::slotAfterFirstPaint()
{
   int newHeight = m_pDiffTextWindow1->getNofVisibleLines();
   int newWidth  = m_pDiffTextWindow1->getNofVisibleColumns();
   m_DTWHeight = newHeight;
   m_pDiffVScrollBar->setRange(0, max2(0, m_neededLines+1 - newHeight) );
   m_pDiffVScrollBar->setPageStep( newHeight );
   m_pOverview->setRange( m_pDiffVScrollBar->value(), m_pDiffVScrollBar->pageStep() );

   // The second window has a somewhat inverse width
   m_pHScrollBar->setRange(0, max2(0, m_maxWidth - newWidth) );
   m_pHScrollBar->setPageStep( newWidth );

   m_pMergeResultWindow->slotGoTop();

   if (m_pCornerWidget)
      m_pCornerWidget->setFixedSize( m_pDiffVScrollBar->width(), m_pHScrollBar->height() );

   slotUpdateAvailabilities();
}

void KDiff3App::resizeEvent(QResizeEvent* e)
{
   QSplitter::resizeEvent(e);
   if (m_pCornerWidget)
      m_pCornerWidget->setFixedSize( m_pDiffVScrollBar->width(), m_pHScrollBar->height() );
}


bool KDiff3App::eventFilter( QObject* o, QEvent* e )
{
   if( o == m_pMergeResultWindow )
   {
      if ( e->type() == QEvent::KeyPress )
      {  // key press
         QKeyEvent *k = (QKeyEvent*)e;
         if (k->key()==Qt::Key_Insert &&  (k->state() & Qt::ControlButton)!=0 )
         {
            slotEditCopy();
            return true;
         }
         if (k->key()==Qt::Key_Insert &&  (k->state() & Qt::ShiftButton)!=0 )
         {
            slotEditPaste();
            return true;
         }
         if (k->key()==Qt::Key_Delete &&  (k->state() & Qt::ShiftButton)!=0 )
         {
            slotEditCut();
            return true;
         }
      }
      return QSplitter::eventFilter( o, e );    // standard event processing
   }

   if ( e->type() == QEvent::KeyPress )   // key press
   {
       QKeyEvent *k = (QKeyEvent*)e;

       bool bCtrl = (k->state() & Qt::ControlButton) != 0;
       if (k->key()==Qt::Key_Insert &&  bCtrl )
       {
          slotEditCopy();
          return true;
       }
       if (k->key()==Qt::Key_Insert &&  (k->state() & Qt::ShiftButton)!=0 )
       {
          slotEditPaste();
          return true;
       }
       int deltaX=0;
       int deltaY=0;
       int pageSize =  m_DTWHeight;
       switch( k->key() )
       {
       case Key_Down:   ++deltaY; break;
       case Key_Up:     --deltaY; break;
       case Key_PageDown: if (!bCtrl) deltaY+=pageSize; break;
       case Key_PageUp:   if (!bCtrl) deltaY-=pageSize; break;
       case Key_Left:     --deltaX;  break;
       case Key_Right:    ++deltaX;  break;
       case Key_Home: if ( bCtrl )  m_pDiffVScrollBar->setValue( 0 );
                      else          m_pHScrollBar->setValue( 0 );
                      break;
       case Key_End:  if ( bCtrl )  m_pDiffVScrollBar->setValue( m_pDiffVScrollBar->maxValue() );
                      else          m_pHScrollBar->setValue( m_pHScrollBar->maxValue() );
                      break;
       default: break;
       }

       scrollDiffTextWindow( deltaX, deltaY );

       return true;                        // eat event
   }
   else if (e->type() == QEvent::Wheel ) {  // wheel event
       QWheelEvent *w = (QWheelEvent*)e;
       w->accept();

       int deltaX=0;

       int d=w->delta();
       int deltaY = -d/120 * QApplication::wheelScrollLines();

       scrollDiffTextWindow( deltaX, deltaY );
       return true;
   }
   else if (e->type() == QEvent::Drop )
   {
      QDropEvent* pDropEvent = static_cast<QDropEvent*>(e);
      pDropEvent->accept();

      if ( QUriDrag::canDecode(pDropEvent) )
      {
#ifdef KREPLACEMENTS_H
         QStringList stringList;
         QUriDrag::decodeLocalFiles( pDropEvent, stringList );
         if ( canContinue() && !stringList.isEmpty() )
         {
            raise();
            QString filename = stringList.first();
            if      ( o == m_pDiffTextWindow1 ) m_sd1.setFilename( filename );
            else if ( o == m_pDiffTextWindow2 ) m_sd2.setFilename( filename );
            else if ( o == m_pDiffTextWindow3 ) m_sd3.setFilename( filename );
            init();
         }
#else
         KURL::List urlList;
         KURLDrag::decode( pDropEvent, urlList );
         if ( canContinue() && !urlList.isEmpty() )
         {
            raise();
            FileAccess fa( urlList.first().url() );
            if      ( o == m_pDiffTextWindow1 ) m_sd1.setFileAccess( fa );
            else if ( o == m_pDiffTextWindow2 ) m_sd2.setFileAccess( fa );
            else if ( o == m_pDiffTextWindow3 ) m_sd3.setFileAccess( fa );
            init();
         }
#endif
      }
      else if ( QTextDrag::canDecode(pDropEvent) )
      {
         QString text;
         bool bDecodeSuccess = QTextDrag::decode( pDropEvent, text );
         if ( bDecodeSuccess && canContinue() )
         {
            raise();
            bool bUpCase = m_pOptionDialog->m_bUpCase;
            if      ( o == m_pDiffTextWindow1 ) m_sd1.setData(text, bUpCase);
            else if ( o == m_pDiffTextWindow2 ) m_sd2.setData(text, bUpCase);
            else if ( o == m_pDiffTextWindow3 ) m_sd3.setData(text, bUpCase);
            init();
         }
      }

      return true;
   }
   return QSplitter::eventFilter( o, e );    // standard event processing
}



OpenDialog::OpenDialog(
   QWidget* pParent, const QString& n1, const QString& n2, const QString& n3,
   bool bMerge, const QString& outputName, const char* slotConfigure, OptionDialog* pOptions )
: QDialog( pParent, "OpenDialog", true /*modal*/ )
{
   m_pOptions = pOptions;

   QVBoxLayout* v = new QVBoxLayout( this, 5 );
   QGridLayout* h = new QGridLayout( v, 5, 4, 5 );
   h->setColStretch( 1, 10 );

   QLabel* label  = new QLabel( i18n("A (Base):"), this );

   m_lineA = new QComboBox( true, this );
   m_lineA->insertStringList( m_pOptions->m_recentAFiles );
   m_lineA->setEditText( KURL(n1).prettyURL() );
   m_lineA->setMinimumSize( 200, m_lineA->size().height() );
   QPushButton * button = new QPushButton( i18n("File..."), this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectFileA() ) );
   QPushButton * button2 = new QPushButton( i18n("Dir..."), this );
   connect( button2, SIGNAL(clicked()), this, SLOT( selectDirA() ) );

   h->addWidget( label,    0, 0 );
   h->addWidget( m_lineA,  0, 1 );
   h->addWidget( button,   0, 2 );
   h->addWidget( button2,  0, 3 );

   label    = new QLabel( "B:", this );
   m_lineB  = new QComboBox( true, this );
   m_lineB->insertStringList( m_pOptions->m_recentBFiles );
   m_lineB->setEditText( KURL(n2).prettyURL() );
   m_lineB->setMinimumSize( 200, m_lineB->size().height() );
   button   = new QPushButton( i18n("File..."), this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectFileB() ) );
   button2   = new QPushButton( i18n("Dir..."), this );
   connect( button2, SIGNAL(clicked()), this, SLOT( selectDirB() ) );

   h->addWidget( label,     1, 0 );
   h->addWidget( m_lineB,   1, 1 );
   h->addWidget( button,    1, 2 );
   h->addWidget( button2,   1, 3 );

   label  = new QLabel( i18n("C (Optional):"), this );
   m_lineC= new QComboBox( true, this );
   m_lineC->insertStringList( m_pOptions->m_recentCFiles );
   m_lineC->setEditText( KURL(n3).prettyURL() );
   m_lineC->setMinimumSize( 200, m_lineC->size().height() );
   button = new QPushButton( i18n("File..."), this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectFileC() ) );
   button2   = new QPushButton( i18n("Dir..."), this );
   connect( button2, SIGNAL(clicked()), this, SLOT( selectDirC() ) );

   h->addWidget( label,     2, 0 );
   h->addWidget( m_lineC,   2, 1 );
   h->addWidget( button,    2, 2 );
   h->addWidget( button2,   2, 3 );

   m_pMerge = new QCheckBox( i18n("Merge"), this );
   h->addWidget( m_pMerge, 3, 0 );

   label  = new QLabel( i18n("Output (optional):"), this );
   m_lineOut = new QComboBox( true, this );
   m_lineOut->insertStringList( m_pOptions->m_recentOutputFiles );
   m_lineOut->setEditText( KURL(outputName).prettyURL() );
   m_lineOut->setMinimumSize( 200, m_lineOut->size().height() );
   button = new QPushButton( i18n("File..."), this );
   connect( button, SIGNAL(clicked()), this, SLOT( selectOutputName() ) );
   button2   = new QPushButton( i18n("Dir..."), this );
   connect( button2, SIGNAL(clicked()), this, SLOT( selectOutputDir() ) );
   connect( m_pMerge, SIGNAL(stateChanged(int)), this, SLOT(internalSlot(int)) );
   connect( this, SIGNAL(internalSignal(bool)), m_lineOut, SLOT(setEnabled(bool)) );
   connect( this, SIGNAL(internalSignal(bool)), button, SLOT(setEnabled(bool)) );
   connect( this, SIGNAL(internalSignal(bool)), button2, SLOT(setEnabled(bool)) );

   m_pMerge->setChecked( !bMerge );
   m_pMerge->setChecked( bMerge );
//   m_lineOutput->setEnabled( bMerge );

//   button->setEnabled( bMerge );

   h->addWidget( label,          4, 0 );
   h->addWidget( m_lineOut,      4, 1 );
   h->addWidget( button,         4, 2 );
   h->addWidget( button2,        4, 3 );

   h->addColSpacing( 1, 200 );

   QHBoxLayout* l = new QHBoxLayout( v, 5 );

   button = new QPushButton( i18n("Configure..."), this );
   connect( button, SIGNAL(clicked()), pParent, slotConfigure );
   l->addWidget( button, 1 );

   l->addStretch(1);

   button = new QPushButton( i18n("&OK"), this );
   button->setDefault( true );
   connect( button, SIGNAL(clicked()), this, SLOT( accept() ) );
   l->addWidget( button, 1 );

   button = new QPushButton( i18n("&Cancel"), this );
   connect( button, SIGNAL(clicked()), this, SLOT( reject() ) );
   l->addWidget( button,1 );

   QSize sh = sizeHint();
   setFixedHeight( sh.height() );
}

void OpenDialog::selectURL( QComboBox* pLine, bool bDir, int i, bool bSave )
{
   QString current = pLine->currentText();
   if (current.isEmpty() && i>3 ){  current = m_lineC->currentText(); }
   if (current.isEmpty()        ){  current = m_lineB->currentText(); }
   if (current.isEmpty()        ){  current = m_lineA->currentText(); }
   KURL newURL = bDir ? KFileDialog::getExistingURL( current, this)
                      : bSave ? KFileDialog::getSaveURL( current, 0, this)
                              : KFileDialog::getOpenURL( current, 0, this);
   if ( !newURL.isEmpty() )
   {
      pLine->setEditText( newURL.url() );
   }
   // newURL won't be modified if nothing was selected.
}

void OpenDialog::selectFileA()     {  selectURL( m_lineA,    false, 1, false );  }
void OpenDialog::selectFileB()     {  selectURL( m_lineB,    false, 2, false );  }
void OpenDialog::selectFileC()     {  selectURL( m_lineC,    false, 3, false );  }
void OpenDialog::selectOutputName(){  selectURL( m_lineOut,  false, 4, true );  }
void OpenDialog::selectDirA()      {  selectURL( m_lineA,    true,  1, false );  }
void OpenDialog::selectDirB()      {  selectURL( m_lineB,    true,  2, false );  }
void OpenDialog::selectDirC()      {  selectURL( m_lineC,    true,  3, false );  }
void OpenDialog::selectOutputDir() {  selectURL( m_lineOut,  true,  4, true );  }

void OpenDialog::internalSlot(int i)
{
   emit internalSignal(i!=0);
}

void OpenDialog::accept()
{
   unsigned int maxNofRecentFiles = 10;

   QString s = m_lineA->currentText();
   s = KURL::fromPathOrURL(s).prettyURL();
   QStringList* sl = &m_pOptions->m_recentAFiles;
   // If an item exist, remove it from the list and reinsert it at the beginning.
   sl->remove(s);
   if ( !s.isEmpty() ) sl->prepend( s );
   if (sl->count()>maxNofRecentFiles) sl->erase( sl->at(maxNofRecentFiles), sl->end() );

   s = m_lineB->currentText();
   s = KURL::fromPathOrURL(s).prettyURL();
   sl = &m_pOptions->m_recentBFiles;
   sl->remove(s);
   if ( !s.isEmpty() ) sl->prepend( s );
   if (sl->count()>maxNofRecentFiles) sl->erase( sl->at(maxNofRecentFiles), sl->end() );

   s = m_lineC->currentText();
   s = KURL::fromPathOrURL(s).prettyURL();
   sl = &m_pOptions->m_recentCFiles;
   sl->remove(s);
   if ( !s.isEmpty() ) sl->prepend( s );
   if (sl->count()>maxNofRecentFiles) sl->erase( sl->at(maxNofRecentFiles), sl->end() );

   s = m_lineOut->currentText();
   s = KURL::fromPathOrURL(s).prettyURL();
   sl = &m_pOptions->m_recentOutputFiles;
   sl->remove(s);
   if ( !s.isEmpty() ) sl->prepend( s );
   if (sl->count()>maxNofRecentFiles) sl->erase( sl->at(maxNofRecentFiles), sl->end() );

   QDialog::accept();
}

void KDiff3App::slotFileOpen()
{
   if ( !canContinue() ) return;

   if ( m_pDirectoryMergeWindow->isDirectoryMergeInProgress() )
   {
      int result = KMessageBox::warningYesNo(this,
         i18n("You are currently doing a directory merge. Are you sure, you want to abort?"),
         i18n("Warning"), i18n("Abort"), i18n("Continue Merging") );
      if ( result!=KMessageBox::Yes )
         return;
   }


   slotStatusMsg(i18n("Opening files..."));

   for(;;)
   {
      OpenDialog d(this,
         m_sd1.m_bPreserve ? QString("") : m_sd1.getAliasName(),
         m_sd2.m_bPreserve ? QString("") : m_sd2.getAliasName(),
         m_sd3.m_bPreserve ? QString("") : m_sd3.getAliasName(),
         !m_outputFilename.isEmpty(),
         m_bDefaultFilename ? QString("") : m_outputFilename,
         SLOT(slotConfigure()), m_pOptionDialog );
      int status = d.exec();
      if ( status == QDialog::Accepted )
      {
         m_sd1.setFilename( d.m_lineA->currentText() );
         m_sd2.setFilename( d.m_lineB->currentText() );
         m_sd3.setFilename( d.m_lineC->currentText() );

         if( d.m_pMerge->isChecked() )
         {
            if ( d.m_lineOut->currentText().isEmpty() )
            {
               m_outputFilename = "unnamed.txt";
               m_bDefaultFilename = true;
            }
            else
            {
               m_outputFilename = d.m_lineOut->currentText();
               m_bDefaultFilename = false;
            }
         }
         else
            m_outputFilename = "";

         m_bDirCompare = improveFilenames();
         if ( m_bDirCompare )
         {
            m_pDirectoryMergeSplitter->show();
            if ( m_pMainWidget!=0 )
            {
               m_pMainWidget->hide();
            }
            break;
         }
         else
         {
            m_pDirectoryMergeSplitter->hide();
            init();

            if ( ! m_sd1.isEmpty() && m_sd1.m_pBuf==0  ||
                 ! m_sd2.isEmpty() && m_sd2.m_pBuf==0  ||
                 ! m_sd3.isEmpty() && m_sd3.m_pBuf==0 )
            {
               QString text( i18n("Opening of these files failed:") );
               text += "\n\n";
               if ( ! m_sd1.isEmpty() && m_sd1.m_pBuf==0 )
                  text += " - " + m_sd1.getAliasName() + "\n";
               if ( ! m_sd2.isEmpty() && m_sd2.m_pBuf==0 )
                  text += " - " + m_sd2.getAliasName() + "\n";
               if ( ! m_sd3.isEmpty() && m_sd3.m_pBuf==0 )
                  text += " - " + m_sd3.getAliasName() + "\n";

               KMessageBox::sorry( this, text, i18n("File open error") );
               continue;
            }
         }
      }
      break;
   }

   slotUpdateAvailabilities();
   slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotFileOpen2(QString fn1, QString fn2, QString fn3, QString ofn,
                              QString an1, QString an2, QString an3 )
{
   if ( !canContinue() ) return;

   slotStatusMsg(i18n("Opening files..."));

   m_sd1.setFilename( fn1 );
   m_sd2.setFilename( fn2 );
   m_sd3.setFilename( fn3 );

   m_sd1.setAliasName( an1 );
   m_sd2.setAliasName( an2 );
   m_sd3.setAliasName( an3 );

   if ( ! ofn.isEmpty() )
   {
      m_outputFilename = ofn;
      m_bDefaultFilename = false;
   }
   else
   {
      m_outputFilename = "";
      m_bDefaultFilename = true;
   }

   bool bDirCompare = improveFilenames();  // Using local bDirCompare is intended, because
                                           // this method is called from the directory-merge-window.

   if( bDirCompare )
   {
   }
   else
   {
      init();

      if ( ! m_sd1.isEmpty() && m_sd1.m_pBuf==0  ||
           ! m_sd2.isEmpty() && m_sd2.m_pBuf==0  ||
           ! m_sd3.isEmpty() && m_sd3.m_pBuf==0 )
      {
         QString text( i18n("Opening of these files failed:") );
         text += "\n\n";
         if ( ! m_sd1.isEmpty() && m_sd1.m_pBuf==0 )
            text += " - " + m_sd1.getAliasName() + "\n";
         if ( ! m_sd2.isEmpty() && m_sd2.m_pBuf==0 )
            text += " - " + m_sd2.getAliasName() + "\n";
         if ( ! m_sd3.isEmpty() && m_sd3.m_pBuf==0 )
            text += " - " + m_sd3.getAliasName() + "\n";

         KMessageBox::sorry( this, text, i18n("File open error") );
      }
      else
      {
         if ( m_pDirectoryMergeWindow!=0 && m_pDirectoryMergeWindow->isVisible() && ! dirShowBoth->isChecked() )
         {
            slotDirViewToggle();
         }
      }
   }
   slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditCut()
{
   slotStatusMsg(i18n("Cutting selection..."));

   QString s;
   if ( m_pMergeResultWindow!=0 )
   {
      s = m_pMergeResultWindow->getSelection();
      m_pMergeResultWindow->deleteSelection();

      m_pMergeResultWindow->update();
   }

   if ( !s.isNull() )
   {
      QApplication::clipboard()->setText( s );
   }

   slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditCopy()
{
   slotStatusMsg(i18n("Copying selection to clipboard..."));
   QString s;
   if (               m_pDiffTextWindow1!=0 )   s = m_pDiffTextWindow1->getSelection();
   if ( s.isNull() && m_pDiffTextWindow2!=0 )   s = m_pDiffTextWindow2->getSelection();
   if ( s.isNull() && m_pDiffTextWindow3!=0 )   s = m_pDiffTextWindow3->getSelection();
   if ( s.isNull() && m_pMergeResultWindow!=0 ) s = m_pMergeResultWindow->getSelection();
   if ( !s.isNull() )
   {
      QApplication::clipboard()->setText( s );
   }

   slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditPaste()
{
   slotStatusMsg(i18n("Inserting clipboard contents..."));

   if ( m_pMergeResultWindow!=0 && m_pMergeResultWindow->isVisible() )
   {
      m_pMergeResultWindow->pasteClipboard();
   }
   else if ( canContinue() )
   {
      bool bUpCase = m_pOptionDialog->m_bUpCase;
      if ( m_pDiffTextWindow1->hasFocus() )
      {
         m_sd1.setData( QApplication::clipboard()->text(), bUpCase );
         init();
      }
      else if ( m_pDiffTextWindow2->hasFocus() )
      {
         m_sd2.setData( QApplication::clipboard()->text(), bUpCase );
         init();
      }
      else if ( m_pDiffTextWindow3->hasFocus() )
      {
         m_sd3.setData( QApplication::clipboard()->text(), bUpCase );
         init();
      }
   }

   slotStatusMsg(i18n("Ready."));
}


void KDiff3App::slotGoCurrent()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoCurrent();
}
void KDiff3App::slotGoTop()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoTop();
}
void KDiff3App::slotGoBottom()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoBottom();
}
void KDiff3App::slotGoPrevUnsolvedConflict()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoPrevUnsolvedConflict();
}
void KDiff3App::slotGoNextUnsolvedConflict()
{
   m_bTimerBlock = false;
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoNextUnsolvedConflict();
}
void KDiff3App::slotGoPrevConflict()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoPrevConflict();
}
void KDiff3App::slotGoNextConflict()
{
   m_bTimerBlock = false;
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoNextConflict();
}
void KDiff3App::slotGoPrevDelta()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoPrevDelta();
}
void KDiff3App::slotGoNextDelta()
{
   if (m_pMergeResultWindow)  m_pMergeResultWindow->slotGoNextDelta();
}

void KDiff3App::choose( int choice )
{
   if (!m_bTimerBlock )
   {
      if ( m_pDirectoryMergeWindow && m_pDirectoryMergeWindow->hasFocus() )
      {
         if (choice==A) m_pDirectoryMergeWindow->slotCurrentChooseA();
         if (choice==B) m_pDirectoryMergeWindow->slotCurrentChooseB();
         if (choice==C) m_pDirectoryMergeWindow->slotCurrentChooseC();
         
         chooseA->setChecked(false);
         chooseB->setChecked(false);
         chooseC->setChecked(false);
      }
      else if ( m_pMergeResultWindow )
      {
         m_pMergeResultWindow->choose( choice );
         if ( autoAdvance->isChecked() )
         {
            m_bTimerBlock = true;
            QTimer::singleShot( m_pOptionDialog->m_autoAdvanceDelay, this, SLOT( slotGoNextUnsolvedConflict() ) );
         }
      }
   }
}

void KDiff3App::slotChooseA() { choose( A ); }
void KDiff3App::slotChooseB() { choose( B ); }
void KDiff3App::slotChooseC() { choose( C ); }

// bConflictsOnly automatically choose for conflicts only (true) or for everywhere
static void mergeChooseGlobal( KDiff3App* pThis, MergeResultWindow* pMRW, int selector, bool bConflictsOnly, bool bWhiteSpaceOnly )
{
   if ( pMRW )
   {
      pThis->slotGoTop();
      pMRW->chooseGlobal(selector, bConflictsOnly, bWhiteSpaceOnly );
   }
}

void KDiff3App::slotChooseAEverywhere() {  mergeChooseGlobal( this, m_pMergeResultWindow, A, false, false ); }
void KDiff3App::slotChooseBEverywhere() {  mergeChooseGlobal( this, m_pMergeResultWindow, B, false, false ); }
void KDiff3App::slotChooseCEverywhere() {  mergeChooseGlobal( this, m_pMergeResultWindow, C, false, false ); }
void KDiff3App::slotChooseAForUnsolvedConflicts() {  mergeChooseGlobal( this, m_pMergeResultWindow, A, true, false ); }
void KDiff3App::slotChooseBForUnsolvedConflicts() {  mergeChooseGlobal( this, m_pMergeResultWindow, B, true, false ); }
void KDiff3App::slotChooseCForUnsolvedConflicts() {  mergeChooseGlobal( this, m_pMergeResultWindow, C, true, false ); }
void KDiff3App::slotChooseAForUnsolvedWhiteSpaceConflicts() {  mergeChooseGlobal( this, m_pMergeResultWindow, A, true, true ); }
void KDiff3App::slotChooseBForUnsolvedWhiteSpaceConflicts() {  mergeChooseGlobal( this, m_pMergeResultWindow, B, true, true ); }
void KDiff3App::slotChooseCForUnsolvedWhiteSpaceConflicts() {  mergeChooseGlobal( this, m_pMergeResultWindow, C, true, true ); }


void KDiff3App::slotAutoSolve()
{
   if (m_pMergeResultWindow )
   {
      slotGoTop();
      m_pMergeResultWindow->slotAutoSolve();
      // m_pMergeWindowFrame->show(); incompatible with bPreserveCarriageReturn
      m_pMergeResultWindow->showNrOfConflicts();
      slotUpdateAvailabilities();
   }
}

void KDiff3App::slotUnsolve()
{
   if (m_pMergeResultWindow )
   {
      slotGoTop();
      m_pMergeResultWindow->slotUnsolve();
   }
}


void KDiff3App::slotConfigure()
{
   m_pOptionDialog->setState();
   m_pOptionDialog->incInitialSize ( QSize(0,40) );
   m_pOptionDialog->exec();
   slotRefresh();
}

void KDiff3App::slotConfigureKeys()
{
    KKeyDialog::configure(actionCollection(), this);
}

void KDiff3App::slotRefresh()
{
   g_tabSize = m_pOptionDialog->m_tabSize;
   if (m_pDiffTextWindow1!=0)
   {
      m_pDiffTextWindow1->setFont(m_pOptionDialog->m_font);
      m_pDiffTextWindow1->update();
   }
   if (m_pDiffTextWindow2!=0)
   {
      m_pDiffTextWindow2->setFont(m_pOptionDialog->m_font);
      m_pDiffTextWindow2->update();
   }
   if (m_pDiffTextWindow3!=0)
   {
      m_pDiffTextWindow3->setFont(m_pOptionDialog->m_font);
      m_pDiffTextWindow3->update();
   }
   if (m_pMergeResultWindow!=0)
   {
      m_pMergeResultWindow->setFont(m_pOptionDialog->m_font);
      m_pMergeResultWindow->update();
   }

   if ( m_pDiffWindowSplitter!=0 )
   {
       m_pDiffWindowSplitter->setOrientation( m_pOptionDialog->m_bHorizDiffWindowSplitting ? Horizontal : Vertical );
   }
}

void KDiff3App::slotSelectionStart()
{
   //editCopy->setEnabled( false );
   //editCut->setEnabled( false );

   const QObject* s = sender();
   if (m_pDiffTextWindow1 && s!=m_pDiffTextWindow1)   m_pDiffTextWindow1->resetSelection();
   if (m_pDiffTextWindow2 && s!=m_pDiffTextWindow2)   m_pDiffTextWindow2->resetSelection();
   if (m_pDiffTextWindow3 && s!=m_pDiffTextWindow3)   m_pDiffTextWindow3->resetSelection();
   if (m_pMergeResultWindow && s!=m_pMergeResultWindow) m_pMergeResultWindow->resetSelection();
}

void KDiff3App::slotSelectionEnd()
{
   //const QObject* s = sender();
   //editCopy->setEnabled(true);
   //editCut->setEnabled( s==m_pMergeResultWindow );
   if ( m_pOptionDialog->m_bAutoCopySelection )
   {
      slotEditCopy();
   }
}

void KDiff3App::slotClipboardChanged()
{
   QString s = QApplication::clipboard()->text();
   //editPaste->setEnabled(!s.isEmpty());
}

void KDiff3App::slotOutputModified()
{
   if ( !m_bOutputModified )
   {
      m_bOutputModified=true;
      slotUpdateAvailabilities();
   }
}

void KDiff3App::slotAutoAdvanceToggled()
{
   m_pOptionDialog->m_bAutoAdvance = autoAdvance->isChecked();
}

void KDiff3App::slotShowWhiteSpaceToggled()
{
   m_pOptionDialog->m_bShowWhiteSpaceCharacters = showWhiteSpaceCharacters->isChecked();
   m_pOptionDialog->m_bShowWhiteSpace = showWhiteSpace->isChecked();
   showWhiteSpaceCharacters->setEnabled( showWhiteSpace->isChecked() );
   if ( m_pDiffTextWindow1!=0 )
      m_pDiffTextWindow1->update();
   if ( m_pDiffTextWindow2!=0 )
      m_pDiffTextWindow2->update();
   if ( m_pDiffTextWindow3!=0 )
      m_pDiffTextWindow3->update();
   if ( m_pOverview!=0 )
      m_pOverview->slotRedraw();
}

void KDiff3App::slotShowLineNumbersToggled()
{
   m_pOptionDialog->m_bShowLineNumbers = showLineNumbers->isChecked();
   if ( m_pDiffTextWindow1!=0 )
      m_pDiffTextWindow1->update();
   if ( m_pDiffTextWindow2!=0 )
      m_pDiffTextWindow2->update();
   if ( m_pDiffTextWindow3!=0 )
      m_pDiffTextWindow3->update();
}

/// Return true for directory compare, else false
bool KDiff3App::improveFilenames()
{
   FileAccess f1(m_sd1.getFilename());
   FileAccess f2(m_sd2.getFilename());
   FileAccess f3(m_sd3.getFilename());
   FileAccess f4(m_outputFilename);

   if ( f1.isFile()  &&  f1.exists() )
   {
      if ( f2.isDir() )
      {
         f2.addPath( f1.fileName() );
         if ( f2.isFile() && f2.exists() )
            m_sd2.setFileAccess( f2 );
      }
      if ( f3.isDir() )
      {
         f3.addPath( f1.fileName() );
         if ( f3.isFile() && f3.exists() )
            m_sd3.setFileAccess( f3 );
      }
      if ( f4.isDir() )
      {
         f4.addPath( f1.fileName() );
         if ( f4.isFile() && f4.exists() )
            m_outputFilename = f4.absFilePath();
      }
   }
   else if ( f1.isDir() )
   {
      FileAccess destDir;
      if (!m_bDefaultFilename) destDir = f4;
      m_pDirectoryMergeSplitter->show();
      m_pDirectoryMergeWindow->init(
         f1, f2, f3,
         destDir,  // Destdirname
         !m_outputFilename.isEmpty()
         );

      if (m_pMainWidget!=0) m_pMainWidget->hide();
      m_sd1.reset();
      if (m_pDiffTextWindow1!=0) m_pDiffTextWindow1->init(0,0,0,0,1,false);
      m_sd2.reset();
      if (m_pDiffTextWindow2!=0) m_pDiffTextWindow2->init(0,0,0,0,2,false);
      m_sd3.reset();
      if (m_pDiffTextWindow3!=0) m_pDiffTextWindow3->init(0,0,0,0,3,false);
      slotUpdateAvailabilities();
      return true;
   }
   return false;
}

void KDiff3App::slotReload()
{
   if ( !canContinue() ) return;

   init();
}

bool KDiff3App::canContinue()
{
   // First test if anything must be saved.
   if(m_bOutputModified)
   {
      int result = KMessageBox::warningYesNoCancel(this,
         i18n("The merge result hasn't been saved."),
         i18n("Warning"), i18n("Save && Continue"), i18n("Continue Without Saving") );
      if ( result==KMessageBox::Cancel )
         return false;
      else if ( result==KMessageBox::Yes )
      {
         slotFileSave();
         if ( m_bOutputModified )
         {
            KMessageBox::sorry(this, i18n("Saving the merge result failed."), i18n("Warning") );
            return false;
         }
      }
   }

   m_bOutputModified = false;
   return true;
}

void KDiff3App::slotCheckIfCanContinue( bool* pbContinue )
{
   if (pbContinue!=0) *pbContinue = canContinue();
}


void KDiff3App::slotDirShowBoth()
{
   if( dirShowBoth->isChecked() )
   {
      if ( m_bDirCompare )
         m_pDirectoryMergeSplitter->show();
      else
         m_pDirectoryMergeSplitter->hide();

      if ( m_pMainWidget!=0 )
         m_pMainWidget->show();
   }
   else
   {
      if ( m_pMainWidget!=0 )
      {
         m_pMainWidget->show();
         m_pDirectoryMergeSplitter->hide();
      }
      else if ( m_bDirCompare )
      {
         m_pDirectoryMergeSplitter->show();
      }
   }

   slotUpdateAvailabilities();
}


void KDiff3App::slotDirViewToggle()
{
   if ( m_bDirCompare )
   {
      if( ! m_pDirectoryMergeSplitter->isVisible() )
      {
         m_pDirectoryMergeSplitter->show();
         if (m_pMainWidget!=0)
            m_pMainWidget->hide();
      }
      else
      {
         if (m_pMainWidget!=0)
         {
            m_pDirectoryMergeSplitter->hide();
            m_pMainWidget->show();
         }
      }
   }
   slotUpdateAvailabilities();
}

void KDiff3App::slotShowWindowAToggled()
{
   if ( m_pDiffTextWindow1!=0 )
   {
      if ( showWindowA->isChecked() ) m_pDiffTextWindow1->show();
      else                            m_pDiffTextWindow1->hide();
      slotUpdateAvailabilities();
   }
}

void KDiff3App::slotShowWindowBToggled()
{
   if ( m_pDiffTextWindow2!=0 )
   {
      if ( showWindowB->isChecked() ) m_pDiffTextWindow2->show();
      else                            m_pDiffTextWindow2->hide();
      slotUpdateAvailabilities();
   }
}

void KDiff3App::slotShowWindowCToggled()
{
   if ( m_pDiffTextWindow3!=0 )
   {
      if ( showWindowC->isChecked() ) m_pDiffTextWindow3->show();
      else                            m_pDiffTextWindow3->hide();
      slotUpdateAvailabilities();
   }
}

void KDiff3App::slotEditFind()
{
   m_pFindDialog->currentLine = 0;
   m_pFindDialog->currentPos = 0;
   m_pFindDialog->currentWindow = 1;
   
   if ( QDialog::Accepted == m_pFindDialog->exec() )
   {
      slotEditFindNext();
   }
}

void KDiff3App::slotEditFindNext()
{
   QCString s = m_pFindDialog->m_pSearchString->text().utf8();
   if ( s.isEmpty() )
   {
      slotEditFind();
      return;
   }

   bool bDirDown = true;
   bool bCaseSensitive = m_pFindDialog->m_pCaseSensitive->isChecked();

   int d3vLine = m_pFindDialog->currentLine;
   int posInLine = m_pFindDialog->currentPos;
   if ( m_pFindDialog->currentWindow == 1 )
   {
      if ( m_pFindDialog->m_pSearchInA->isChecked() && m_pDiffTextWindow1!=0  &&
           m_pDiffTextWindow1->findString( s, d3vLine, posInLine, bDirDown, bCaseSensitive ) )
      {
         m_pDiffTextWindow1->setSelection( d3vLine, posInLine, d3vLine, posInLine+s.length() );
         m_pDiffVScrollBar->setValue(d3vLine-m_pDiffVScrollBar->pageStep()/2);
         m_pHScrollBar->setValue( max2( 0, posInLine+(int)s.length()-m_pHScrollBar->pageStep()) );
         m_pFindDialog->currentLine = d3vLine;
         m_pFindDialog->currentPos = posInLine + 1;
         return;
      }
      m_pFindDialog->currentWindow = 2;
      m_pFindDialog->currentLine = 0;
      m_pFindDialog->currentPos = 0;
   }

   d3vLine = m_pFindDialog->currentLine;
   posInLine = m_pFindDialog->currentPos;
   if ( m_pFindDialog->currentWindow == 2 )
   {
      if ( m_pFindDialog->m_pSearchInB->isChecked() && m_pDiffTextWindow2!=0 &&
           m_pDiffTextWindow2->findString( s, d3vLine, posInLine, bDirDown, bCaseSensitive ) )
      {
         m_pDiffTextWindow2->setSelection( d3vLine, posInLine, d3vLine, posInLine+s.length() );
         m_pDiffVScrollBar->setValue(d3vLine-m_pDiffVScrollBar->pageStep()/2);
         m_pHScrollBar->setValue( max2( 0, posInLine+(int)s.length()-m_pHScrollBar->pageStep()) );
         m_pFindDialog->currentLine = d3vLine;
         m_pFindDialog->currentPos = posInLine + 1;
         return;
      }
      m_pFindDialog->currentWindow = 3;
      m_pFindDialog->currentLine = 0;
      m_pFindDialog->currentPos = 0;
   }

   d3vLine = m_pFindDialog->currentLine;
   posInLine = m_pFindDialog->currentPos;
   if ( m_pFindDialog->currentWindow == 3 )
   {
      if ( m_pFindDialog->m_pSearchInC->isChecked() && m_pDiffTextWindow3!=0 &&
           m_pDiffTextWindow3->findString( s, d3vLine, posInLine, bDirDown, bCaseSensitive ) )
      {
         m_pDiffTextWindow3->setSelection( d3vLine, posInLine, d3vLine, posInLine+s.length() );
         m_pDiffVScrollBar->setValue(d3vLine-m_pDiffVScrollBar->pageStep()/2);
         m_pHScrollBar->setValue( max2( 0, posInLine+(int)s.length()-m_pHScrollBar->pageStep()) );
         m_pFindDialog->currentLine = d3vLine;
         m_pFindDialog->currentPos = posInLine + 1;
         return;
      }
      m_pFindDialog->currentWindow = 4;
      m_pFindDialog->currentLine = 0;
      m_pFindDialog->currentPos = 0;
   }  

   d3vLine = m_pFindDialog->currentLine;
   posInLine = m_pFindDialog->currentPos;
   if ( m_pFindDialog->currentWindow == 4 )
   {
      if ( m_pFindDialog->m_pSearchInOutput->isChecked() && m_pMergeResultWindow!=0 && m_pMergeResultWindow->isVisible() &&
           m_pMergeResultWindow->findString( s, d3vLine, posInLine, bDirDown, bCaseSensitive ) )
      {
         m_pMergeResultWindow->setSelection( d3vLine, posInLine, d3vLine, posInLine+s.length() );
         m_pMergeVScrollBar->setValue(d3vLine - m_pMergeVScrollBar->pageStep()/2);
         m_pHScrollBar->setValue( max2( 0, posInLine+(int)s.length()-m_pHScrollBar->pageStep()) );
         m_pFindDialog->currentLine = d3vLine;
         m_pFindDialog->currentPos = posInLine + 1;
         return;
      }
      m_pFindDialog->currentWindow = 5;
      m_pFindDialog->currentLine = 0;
      m_pFindDialog->currentPos = 0;
   }

   KMessageBox::information(this,i18n("Search complete."),i18n("Search Complete"));
   m_pFindDialog->currentWindow = 1;
   m_pFindDialog->currentLine = 0;
   m_pFindDialog->currentPos = 0;
}

void KDiff3App::slotMergeCurrentFile()
{
   if ( m_bDirCompare  &&  m_pDirectoryMergeWindow->isVisible()  &&  m_pDirectoryMergeWindow->isFileSelected() )
   {
      m_pDirectoryMergeWindow->mergeCurrentFile();
   }
   else if ( m_pMainWidget != 0 && m_pMainWidget->isVisible() )
   {
      if ( !canContinue() ) return;
      if ( m_outputFilename.isEmpty() )
      {
         if ( !m_sd3.isEmpty() && !m_sd3.m_bPreserve )
         {
            m_outputFilename =  m_sd3.getFilename();
         }
         else if ( !m_sd2.isEmpty() && !m_sd2.m_bPreserve )
         {
            m_outputFilename =  m_sd2.getFilename();
         }
         else if ( !m_sd1.isEmpty() && !m_sd1.m_bPreserve )
         {
            m_outputFilename =  m_sd1.getFilename();
         }
         else
         {
            m_outputFilename = "unnamed.txt";
            m_bDefaultFilename = true;
         }
      }
      init();
   }
}

void KDiff3App::slotWinFocusNext()
{
   QWidget* focus = qApp->focusWidget();
   if ( focus == m_pDirectoryMergeWindow && m_pDirectoryMergeWindow->isVisible() && ! dirShowBoth->isChecked() )
   {
      slotDirViewToggle();
   }

   std::list<QWidget*> visibleWidgetList;
   if ( m_pDiffTextWindow1->isVisible() ) visibleWidgetList.push_back(m_pDiffTextWindow1);
   if ( m_pDiffTextWindow2->isVisible() ) visibleWidgetList.push_back(m_pDiffTextWindow2);
   if ( m_pDiffTextWindow3->isVisible() ) visibleWidgetList.push_back(m_pDiffTextWindow3);
   if ( m_pMergeResultWindow->isVisible() ) visibleWidgetList.push_back(m_pMergeResultWindow);
   if ( m_bDirCompare /*m_pDirectoryMergeWindow->isVisible()*/ ) visibleWidgetList.push_back(m_pDirectoryMergeWindow);
   //if ( m_pDirectoryMergeInfo->isVisible() ) visibleWidgetList.push_back(m_pDirectoryMergeInfo->getInfoList());

   std::list<QWidget*>::iterator i = std::find( visibleWidgetList.begin(),  visibleWidgetList.end(), focus);
   ++i;
   if ( i==visibleWidgetList.end() ) ++i;   // if at end goto begin of list
   if ( i!=visibleWidgetList.end() )
   {
      if ( *i == m_pDirectoryMergeWindow  && ! dirShowBoth->isChecked() )
      {
         slotDirViewToggle();
      }
      (*i)->setFocus();
   }
}

void KDiff3App::slotWinFocusPrev()
{
   QWidget* focus = qApp->focusWidget();
   if ( focus == m_pDirectoryMergeWindow  && m_pDirectoryMergeWindow->isVisible() && ! dirShowBoth->isChecked() )
   {
      slotDirViewToggle();
   }

   std::list<QWidget*> visibleWidgetList;
   if ( m_pDiffTextWindow1->isVisible() ) visibleWidgetList.push_back(m_pDiffTextWindow1);
   if ( m_pDiffTextWindow2->isVisible() ) visibleWidgetList.push_back(m_pDiffTextWindow2);
   if ( m_pDiffTextWindow3->isVisible() ) visibleWidgetList.push_back(m_pDiffTextWindow3);
   if ( m_pMergeResultWindow->isVisible() ) visibleWidgetList.push_back(m_pMergeResultWindow);
   if (m_bDirCompare /* m_pDirectoryMergeWindow->isVisible() */ ) visibleWidgetList.push_back(m_pDirectoryMergeWindow);
   //if ( m_pDirectoryMergeInfo->isVisible() ) visibleWidgetList.push_back(m_pDirectoryMergeInfo->getInfoList());

   std::list<QWidget*>::iterator i = std::find( visibleWidgetList.begin(),  visibleWidgetList.end(), focus);
   --i;
   if ( i==visibleWidgetList.end() ) --i;
   if ( i!=visibleWidgetList.end() )
   {
      if ( *i == m_pDirectoryMergeWindow  && ! dirShowBoth->isChecked() )
      {
         slotDirViewToggle();
      }
      (*i)->setFocus();
   }
}

void KDiff3App::slotWinToggleSplitterOrientation()
{
   if ( m_pDiffWindowSplitter!=0 )
   {
      m_pDiffWindowSplitter->setOrientation(
         m_pDiffWindowSplitter->orientation()==Vertical ? Horizontal : Vertical
         );

      m_pOptionDialog->m_bHorizDiffWindowSplitting = m_pDiffWindowSplitter->orientation()==Horizontal;
   }
}

void KDiff3App::slotUpdateAvailabilities()
{
   bool bTextDataAvailable = (m_sd1.m_pBuf != 0 || m_sd2.m_pBuf != 0 || m_sd3.m_pBuf != 0 );
   if( dirShowBoth->isChecked() )
   {
      if ( m_bDirCompare )
         m_pDirectoryMergeSplitter->show();
      else
         m_pDirectoryMergeSplitter->hide();

      if ( m_pMainWidget!=0 && !m_pMainWidget->isVisible() &&
           bTextDataAvailable
         )
         m_pMainWidget->show();
   }

   bool bDiffWindowVisible = m_pMainWidget != 0 && m_pMainWidget->isVisible();
   bool bMergeEditorVisible = m_pMergeWindowFrame !=0  &&  m_pMergeWindowFrame->isVisible();

   m_pDirectoryMergeWindow->updateAvailabilities( m_bDirCompare, bDiffWindowVisible, chooseA, chooseB, chooseC );

   dirShowBoth->setEnabled( m_bDirCompare );
   dirViewToggle->setEnabled(
      m_bDirCompare &&
      (!m_pDirectoryMergeSplitter->isVisible()  &&  m_pMainWidget!=0 && m_pMainWidget->isVisible() ||
        m_pDirectoryMergeSplitter->isVisible()  &&  m_pMainWidget!=0 && !m_pMainWidget->isVisible() && bTextDataAvailable )
      );
      
   bool bDirWindowHasFocus = m_pDirectoryMergeSplitter->isVisible() && m_pDirectoryMergeWindow->hasFocus();

   showWhiteSpaceCharacters->setEnabled( bDiffWindowVisible );
   autoAdvance->setEnabled( bMergeEditorVisible );
   autoSolve->setEnabled( bMergeEditorVisible  &&  m_bTripleDiff );
   unsolve->setEnabled( bMergeEditorVisible );
   if ( !bDirWindowHasFocus )
   {
      chooseA->setEnabled( bMergeEditorVisible );
      chooseB->setEnabled( bMergeEditorVisible );
      chooseC->setEnabled( bMergeEditorVisible  &&  m_bTripleDiff );
   }
   chooseAEverywhere->setEnabled( bMergeEditorVisible );
   chooseBEverywhere->setEnabled( bMergeEditorVisible );
   chooseCEverywhere->setEnabled( bMergeEditorVisible  &&  m_bTripleDiff );
   chooseAForUnsolvedConflicts->setEnabled( bMergeEditorVisible );
   chooseBForUnsolvedConflicts->setEnabled( bMergeEditorVisible );
   chooseCForUnsolvedConflicts->setEnabled( bMergeEditorVisible  &&  m_bTripleDiff );
   chooseAForUnsolvedWhiteSpaceConflicts->setEnabled( bMergeEditorVisible );
   chooseBForUnsolvedWhiteSpaceConflicts->setEnabled( bMergeEditorVisible );
   chooseCForUnsolvedWhiteSpaceConflicts->setEnabled( bMergeEditorVisible  &&  m_bTripleDiff );
   showWindowA->setEnabled( bDiffWindowVisible && ( m_pDiffTextWindow2->isVisible() || m_pDiffTextWindow3->isVisible() ) );
   showWindowB->setEnabled( bDiffWindowVisible && ( m_pDiffTextWindow1->isVisible() || m_pDiffTextWindow3->isVisible() ));
   showWindowC->setEnabled( bDiffWindowVisible &&  m_bTripleDiff && ( m_pDiffTextWindow1->isVisible() || m_pDiffTextWindow2->isVisible() ) );
   editFind->setEnabled( bDiffWindowVisible );
   editFindNext->setEnabled( bDiffWindowVisible );
   m_pFindDialog->m_pSearchInC->setEnabled( m_bTripleDiff );
   m_pFindDialog->m_pSearchInOutput->setEnabled( bMergeEditorVisible );

   bool bSavable = bMergeEditorVisible && m_pMergeResultWindow->getNrOfUnsolvedConflicts()==0;
   fileSave->setEnabled( m_bOutputModified && bSavable );
   fileSaveAs->setEnabled( bSavable );

   goTop->setEnabled( bDiffWindowVisible &&  m_pMergeResultWindow->isDeltaAboveCurrent() );
   goBottom->setEnabled( bDiffWindowVisible &&  m_pMergeResultWindow->isDeltaBelowCurrent() );
   goCurrent->setEnabled( bDiffWindowVisible );
   goPrevUnsolvedConflict->setEnabled( bMergeEditorVisible &&  m_pMergeResultWindow->isUnsolvedConflictAboveCurrent() );
   goNextUnsolvedConflict->setEnabled( bMergeEditorVisible &&  m_pMergeResultWindow->isUnsolvedConflictBelowCurrent() );
   goPrevConflict->setEnabled( bDiffWindowVisible &&  m_pMergeResultWindow->isConflictAboveCurrent() );
   goNextConflict->setEnabled( bDiffWindowVisible &&  m_pMergeResultWindow->isConflictBelowCurrent() );
   goPrevDelta->setEnabled( bDiffWindowVisible &&  m_pMergeResultWindow->isDeltaAboveCurrent() );
   goNextDelta->setEnabled( bDiffWindowVisible &&  m_pMergeResultWindow->isDeltaBelowCurrent() );

   winToggleSplitOrientation->setEnabled( bDiffWindowVisible && m_pDiffWindowSplitter!=0 );
}
