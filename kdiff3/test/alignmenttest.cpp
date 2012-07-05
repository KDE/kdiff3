// vim:sw=3:ts=3:expandtab

#include <iostream>
#include <stdio.h>

#include <QDirIterator>
#include <QTextCodec>
#include <QTextStream>

#include "diff.h"
#include "gnudiff_diff.h"
#include "options.h"
#include "progress.h"

#define i18n(s) s


Options *m_pOptions = NULL;
ManualDiffHelpList m_manualDiffHelpList;

bool g_bIgnoreWhiteSpace = true;
bool g_bIgnoreTrivialMatches = true;


void determineFileAlignment(SourceData &m_sd1, SourceData &m_sd2, SourceData &m_sd3, Diff3LineList &m_diff3LineList)
{
   DiffList m_diffList12;
   DiffList m_diffList23;
   DiffList m_diffList13;

   m_diff3LineList.clear();

   // Run the diff.
   if ( m_sd3.isEmpty() )
   {
      runDiff( m_sd1.getLineDataForDiff(), m_sd1.getSizeLines(), m_sd2.getLineDataForDiff(), m_sd2.getSizeLines(), m_diffList12,1,2,
               &m_manualDiffHelpList, m_pOptions);
      calcDiff3LineListUsingAB( &m_diffList12, m_diff3LineList );
      fineDiff( m_diff3LineList, 1, m_sd1.getLineDataForDisplay(), m_sd2.getLineDataForDisplay() );
   }
   else
   {
      runDiff( m_sd1.getLineDataForDiff(), m_sd1.getSizeLines(), m_sd2.getLineDataForDiff(), m_sd2.getSizeLines(), m_diffList12,1,2,
               &m_manualDiffHelpList, m_pOptions);
      runDiff( m_sd2.getLineDataForDiff(), m_sd2.getSizeLines(), m_sd3.getLineDataForDiff(), m_sd3.getSizeLines(), m_diffList23,2,3,
               &m_manualDiffHelpList, m_pOptions);
      runDiff( m_sd1.getLineDataForDiff(), m_sd1.getSizeLines(), m_sd3.getLineDataForDiff(), m_sd3.getSizeLines(), m_diffList13,1,3,
               &m_manualDiffHelpList, m_pOptions);

      calcDiff3LineListUsingAB( &m_diffList12, m_diff3LineList );
      calcDiff3LineListUsingAC( &m_diffList13, m_diff3LineList );
      correctManualDiffAlignment( m_diff3LineList, &m_manualDiffHelpList );
      calcDiff3LineListTrim( m_diff3LineList, m_sd1.getLineDataForDiff(), m_sd2.getLineDataForDiff(), m_sd3.getLineDataForDiff(), &m_manualDiffHelpList );

      if ( m_pOptions->m_bDiff3AlignBC )
      {
         calcDiff3LineListUsingBC( &m_diffList23, m_diff3LineList );
         correctManualDiffAlignment( m_diff3LineList, &m_manualDiffHelpList );
         calcDiff3LineListTrim( m_diff3LineList, m_sd1.getLineDataForDiff(), m_sd2.getLineDataForDiff(), m_sd3.getLineDataForDiff(), &m_manualDiffHelpList );
      }

      fineDiff( m_diff3LineList, 1, m_sd1.getLineDataForDisplay(), m_sd2.getLineDataForDisplay() );
      fineDiff( m_diff3LineList, 2, m_sd2.getLineDataForDisplay(), m_sd3.getLineDataForDisplay() );
      fineDiff( m_diff3LineList, 3, m_sd3.getLineDataForDisplay(), m_sd1.getLineDataForDisplay() );
   }
   calcWhiteDiff3Lines( m_diff3LineList, m_sd1.getLineDataForDiff(), m_sd2.getLineDataForDiff(), m_sd3.getLineDataForDiff() );
}


void printDiffList(const Diff3LineList &diff3LineList,
                   const SourceData &sd1,
                   const SourceData &sd2,
                   const SourceData &sd3)
{
   const int columnsize = 30;
   const int linenumsize = 6;
   Diff3LineList::const_iterator i;
   for ( i=diff3LineList.begin(); i!=diff3LineList.end(); ++i )
   {
      QTextStream out(stdout);
      QString lineAText;
      QString lineBText;
      QString lineCText;

      const Diff3Line& d3l = *i;

      if(d3l.lineA != -1)
      {
         const LineData *pLineData = &sd1.getLineDataForDiff()[d3l.lineA];
         lineAText = QString(pLineData->pLine, pLineData->size);
         lineAText = QString("%1 %2").arg(d3l.lineA, linenumsize).arg(lineAText.left(columnsize - linenumsize - 1));
      }

      if(d3l.lineB != -1)
      {
         const LineData *pLineData = &sd2.getLineDataForDiff()[d3l.lineB];
         lineBText = QString(pLineData->pLine, pLineData->size);
         lineBText = QString("%1 %2").arg(d3l.lineB, linenumsize).arg(lineBText.left(columnsize - linenumsize - 1));
      }

      if(d3l.lineC != -1)
      {
         const LineData *pLineData = &sd3.getLineDataForDiff()[d3l.lineC];
         lineCText = QString(pLineData->pLine, pLineData->size);
         lineCText = QString("%1 %2").arg(d3l.lineC, linenumsize).arg(lineCText.left(columnsize - linenumsize - 1));
      }

      out << QString("%1 %2 %3").arg(lineAText, -columnsize)
                                .arg(lineBText, -columnsize)
                                .arg(lineCText, -columnsize) + "\n";
   }
}


void loadExpectedAlignmentFile(QString expectedResultFileName, Diff3LineList &expectedDiff3LineList)
{
   Diff3Line d3l;

   expectedDiff3LineList.clear();

   QFile file(expectedResultFileName);
   QString line;
   if ( file.open(QIODevice::ReadOnly) )
   {
      QTextStream t( &file );
      while ( !t.atEnd() )
      {
         QStringList lst = t.readLine().split(QRegExp("\\s+"));
         d3l.lineA = lst.at(0).toInt();
         d3l.lineB = lst.at(1).toInt();
         d3l.lineC = lst.at(2).toInt();

         expectedDiff3LineList.push_back( d3l );
      }
      file.close();
   }
}

void writeActualAlignmentFile(QString actualResultFileName, const Diff3LineList &actualDiff3LineList)
{
   Diff3LineList::const_iterator p_d3l;

   QFile file(actualResultFileName);
   if ( file.open(QIODevice::WriteOnly) )
   {
      {
         QTextStream t( &file );

         for(p_d3l = actualDiff3LineList.begin(); p_d3l != actualDiff3LineList.end(); p_d3l++)
         {
            t << p_d3l->lineA << " " << p_d3l->lineB << " " << p_d3l->lineC << endl;
         }
      }
      file.close();
   }
}

bool runTest(QString file1, QString file2, QString file3, QString expectedResultFile, QString actualResultFile, int maxLength)
{
   Options options;
   Diff3LineList actualDiff3LineList, expectedDiff3LineList;
   QTextCodec *p_codec = QTextCodec::codecForName("UTF-8");
   QTextStream out(stdout);

   options.m_bIgnoreCase = false;
   options.m_bDiff3AlignBC = true;

   m_pOptions = &options;

   SourceData m_sd1, m_sd2, m_sd3;

   QString msgprefix = "Running test with ";
   QString filepattern = QString(file1).replace("_base.", "_*.");
   QString msgsuffix = QString("...%1").arg("", maxLength - filepattern.length());
   out << msgprefix << filepattern << msgsuffix;
   out.flush();

   m_sd1.setOptions(&options);
   m_sd1.setFilename(file1);
   m_sd1.readAndPreprocess(p_codec, false);

   m_sd2.setOptions(&options);
   m_sd2.setFilename(file2);
   m_sd2.readAndPreprocess(p_codec, false);

   m_sd3.setOptions(&options);
   m_sd3.setFilename(file3);
   m_sd3.readAndPreprocess(p_codec, false);

   determineFileAlignment(m_sd1, m_sd2, m_sd3, actualDiff3LineList);

   loadExpectedAlignmentFile(expectedResultFile, expectedDiff3LineList);

   Diff3LineList::iterator p_actual = actualDiff3LineList.begin();
   Diff3LineList::iterator p_expected = expectedDiff3LineList.begin();
   bool equal = true;

   equal = (actualDiff3LineList.size() == expectedDiff3LineList.size());

   while(equal && (p_actual != actualDiff3LineList.end()))
   {
      equal = (p_actual->lineA == p_expected->lineA) &&
              (p_actual->lineB == p_expected->lineB) &&
              (p_actual->lineC == p_expected->lineC);
      p_actual++;
      p_expected++;
   }

   if(equal)
   {
      out << "OK" << endl;
   }
   else
   {
      out << "NOK" << endl;

      writeActualAlignmentFile(actualResultFile, actualDiff3LineList);

      out << "Actual result (written to " << actualResultFile << "):" << endl;
      out << "----------------------------------------------------------------------------------------------" << endl;
      printDiffList(actualDiff3LineList, m_sd1, m_sd2, m_sd3);
      out << "----------------------------------------------------------------------------------------------" << endl;
      out << "Expected result:" << endl;
      out << "----------------------------------------------------------------------------------------------" << endl;
      printDiffList(expectedDiff3LineList, m_sd1, m_sd2, m_sd3);
      out << "----------------------------------------------------------------------------------------------" << endl;
   }

   return equal;
}

int main()
{
   bool allOk = true;
   int maxLength = 0;
   QTextStream out(stdout);
   QDir testdatadir("testdata");

   QStringList nameFilter;
   nameFilter << "*_base.*";

   QStringList baseFiles = testdatadir.entryList(nameFilter, QDir::Files, QDir::Name);
   QListIterator<QString> it(baseFiles);

   for (int i = 0; i < baseFiles.size(); i++)
   {
      maxLength = std::max(baseFiles.at(i).length(), maxLength);
   }
   maxLength += testdatadir.path().length() + 1;

   while (it.hasNext())
   {
      QString fileName = testdatadir.path() + QDir::separator() + it.next();

      QRegExp baseFileRegExp("(.*)_base\\.(.*)");
      baseFileRegExp.exactMatch(fileName);

      QString prefix = baseFileRegExp.cap(1);
      QString suffix = baseFileRegExp.cap(2);

      QString contrib1FileName(prefix + "_contrib1." + suffix);
      QString contrib2FileName(prefix + "_contrib2." + suffix);
      QString expectedResultFileName(prefix + "_expected_result." + suffix);
      QString actualResultFileName(prefix + "_actual_result." + suffix);

      if(QFile(contrib1FileName).exists() &&
         QFile(contrib2FileName).exists() &&
         QFile(expectedResultFileName).exists())
      {
         bool ok = runTest(fileName, contrib1FileName, contrib2FileName, expectedResultFileName, actualResultFileName, maxLength);

         allOk = allOk && ok;
      }
      else
      {
         out << "Skipping " << fileName << " " << contrib1FileName << " " << contrib2FileName << " " << expectedResultFileName << " " << endl;
      }
   }

   return allOk ? 0 : -1;
}
