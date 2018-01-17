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

bool verbose = false;
Options *m_pOptions = NULL;
ManualDiffHelpList m_manualDiffHelpList;

bool g_bIgnoreWhiteSpace = true;
bool g_bIgnoreTrivialMatches = true;


void printDiffList(const QString caption, const DiffList &diffList)
{
   QTextStream out(stdout);
   DiffList::const_iterator i;

   out << "Printing difflist " << caption << ":" << endl;
   out << "  nofEquals, diff1, diff2" << endl;

   for(i = diffList.begin(); i != diffList.end(); i++)
   {
      out << "  " << i->nofEquals << "," << i->diff1 << "," << i->diff2 << endl;
   }
}

void printDiff3List(const Diff3LineList &diff3LineList,
                   const SourceData &sd1,
                   const SourceData &sd2,
                   const SourceData &sd3,
                   bool forceVerbosity=false)
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
         lineAText.replace(QString("\r"), QString("\\r"));
         lineAText.replace(QString("\n"), QString("\\n"));
         lineAText = QString("%1 %2").arg(d3l.lineA, linenumsize).arg(lineAText.left(columnsize - linenumsize - 1));
      }

      if(d3l.lineB != -1)
      {
         const LineData *pLineData = &sd2.getLineDataForDiff()[d3l.lineB];
         lineBText = QString(pLineData->pLine, pLineData->size);
         lineBText.replace(QString("\r"), QString("\\r"));
         lineBText.replace(QString("\n"), QString("\\n"));
         lineBText = QString("%1 %2").arg(d3l.lineB, linenumsize).arg(lineBText.left(columnsize - linenumsize - 1));
      }

      if(d3l.lineC != -1)
      {
         const LineData *pLineData = &sd3.getLineDataForDiff()[d3l.lineC];
         lineCText = QString(pLineData->pLine, pLineData->size);
         lineCText.replace(QString("\r"), QString("\\r"));
         lineCText.replace(QString("\n"), QString("\\n"));
         lineCText = QString("%1 %2").arg(d3l.lineC, linenumsize).arg(lineCText.left(columnsize - linenumsize - 1));
      }

      out << QString("%1 %2 %3").arg(lineAText, -columnsize)
                                .arg(lineBText, -columnsize)
                                .arg(lineCText, -columnsize);
      if(verbose || forceVerbosity)
      {
         out << " " << d3l.bAEqB << " " << d3l.bBEqC << " " << d3l.bAEqC;
      }

      out << endl;
   }
}

void printDiff3List(QString caption,
                   const Diff3LineList &diff3LineList,
                   const SourceData &sd1,
                   const SourceData &sd2,
                   const SourceData &sd3,
                   bool forceVerbosity=false)
{
   QTextStream out(stdout);
   out << "Printing diff3list " << caption << ":" << endl;
   printDiff3List(diff3LineList, sd1, sd2, sd3, forceVerbosity);
}

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

      if (verbose)
      {
         printDiffList("m_diffList12", m_diffList12);
         printDiffList("m_diffList23", m_diffList23);
         printDiffList("m_diffList13", m_diffList13);
      }

      calcDiff3LineListUsingAB( &m_diffList12, m_diff3LineList );
      if (verbose) printDiff3List("after calcDiff3LineListUsingAB", m_diff3LineList, m_sd1, m_sd2, m_sd3);

      calcDiff3LineListUsingAC( &m_diffList13, m_diff3LineList );
      if (verbose) printDiff3List("after calcDiff3LineListUsingAC", m_diff3LineList, m_sd1, m_sd2, m_sd3);

      correctManualDiffAlignment( m_diff3LineList, &m_manualDiffHelpList );
      calcDiff3LineListTrim( m_diff3LineList, m_sd1.getLineDataForDiff(), m_sd2.getLineDataForDiff(), m_sd3.getLineDataForDiff(), &m_manualDiffHelpList );
      if (verbose) printDiff3List("after 1st calcDiff3LineListTrim", m_diff3LineList, m_sd1, m_sd2, m_sd3);

      if ( m_pOptions->m_bDiff3AlignBC )
      {
         calcDiff3LineListUsingBC( &m_diffList23, m_diff3LineList );
         if (verbose) printDiff3List("after calcDiff3LineListUsingBC", m_diff3LineList, m_sd1, m_sd2, m_sd3);
         correctManualDiffAlignment( m_diff3LineList, &m_manualDiffHelpList );
         calcDiff3LineListTrim( m_diff3LineList, m_sd1.getLineDataForDiff(), m_sd2.getLineDataForDiff(), m_sd3.getLineDataForDiff(), &m_manualDiffHelpList );
         if (verbose) printDiff3List("after 2nd calcDiff3LineListTrim", m_diff3LineList, m_sd1, m_sd2, m_sd3);
      }

      fineDiff( m_diff3LineList, 1, m_sd1.getLineDataForDisplay(), m_sd2.getLineDataForDisplay() );
      fineDiff( m_diff3LineList, 2, m_sd2.getLineDataForDisplay(), m_sd3.getLineDataForDisplay() );
      fineDiff( m_diff3LineList, 3, m_sd3.getLineDataForDisplay(), m_sd1.getLineDataForDisplay() );
   }
   calcWhiteDiff3Lines( m_diff3LineList, m_sd1.getLineDataForDiff(), m_sd2.getLineDataForDiff(), m_sd3.getLineDataForDiff() );
}

QString getLineFromSourceData(const SourceData &sd, int line)
{
   const LineData *pLineData = &sd.getLineDataForDiff()[line];
   QString lineText = QString(pLineData->pLine, pLineData->size);
   lineText.replace(QString("\r"), QString("\\r"));
   lineText.replace(QString("\n"), QString("\\n"));
   return lineText;
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

bool dataIsConsistent(int line1, QString &line1Text, int line2, QString &line2Text, bool equal)
{
   bool consistent = false;

   if(line1 == -1 || line2 == -1)
   {
      consistent = !equal;
   }
   else
   {
      /* If the equal boolean is true the line content must be the same,
       * if the line content is different the boolean should be false,
       * but other than that we can't be sure:
       * - if the line content is the same the boolean may not be true because
       *   GNU diff may have put that line as a removal in the first file and
       *   an addition in the second.
       * - also the comparison this test does between lines considers all
       *   whitespace equal, while GNU diff doesn't (for instance U+0020 vs U+00A0)
       */
      if(equal)
      {
         consistent = (line1Text == line2Text);
      }
      else if (line1Text != line2Text)
      {
         consistent = !equal;
      }
      else
      {
         consistent = true;
      }

   }
   return consistent;
}

bool runTest(QString file1, QString file2, QString file3, QString expectedResultFile, QString actualResultFile, int maxLength)
{
   Options options;
   Diff3LineList actualDiff3LineList, expectedDiff3LineList;
   QTextCodec *p_codec = QTextCodec::codecForName("UTF-8");
   QTextStream out(stdout);

   options.m_bIgnoreCase = false;
   options.m_bPreserveCarriageReturn = false;
   options.m_bDiff3AlignBC = true;

   m_pOptions = &options;

   SourceData m_sd1, m_sd2, m_sd3;

   QString msgprefix = "Running test with ";
   QString filepattern = QString(file1).replace("_base.", "_*.");
   QString msgsuffix = QString("...%1").arg("", maxLength - filepattern.length());
   out << msgprefix << filepattern << msgsuffix;
   if(verbose)
   {
      out << endl;
   }
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
   bool sequenceError = false;
   bool consistencyError = false;

   equal = (actualDiff3LineList.size() == expectedDiff3LineList.size());

   int latestLineA = -1;
   int latestLineB = -1;
   int latestLineC = -1;
   while(equal && (p_actual != actualDiff3LineList.end()))
   {
      /* Check if all line numbers are in sequence */
      if(p_actual->lineA != -1)
      {
         if(p_actual->lineA <= latestLineA)
         {
            sequenceError = true;
         }
         else
         {
            latestLineA = p_actual->lineA;
         }
      }
      if(p_actual->lineB != -1)
      {
         if(p_actual->lineB <= latestLineB)
         {
            sequenceError = true;
         }
         else
         {
            latestLineB = p_actual->lineB;
         }
      }
      if(p_actual->lineC != -1)
      {
         if(p_actual->lineC <= latestLineC)
         {
            sequenceError = true;
         }
         else
         {
            latestLineC = p_actual->lineC;
         }
      }

      /* Check if the booleans that indicate if lines are equal are consistent with the content of the lines */
      QString lineAText = (p_actual->lineA == -1) ? "" : getLineFromSourceData(m_sd1, p_actual->lineA).simplified().replace(" ", "");
      QString lineBText = (p_actual->lineB == -1) ? "" : getLineFromSourceData(m_sd2, p_actual->lineB).simplified().replace(" ", "");
      QString lineCText = (p_actual->lineC == -1) ? "" : getLineFromSourceData(m_sd3, p_actual->lineC).simplified().replace(" ", "");

      if(!dataIsConsistent(p_actual->lineA, lineAText, p_actual->lineB, lineBText, p_actual->bAEqB))
      {
         if(verbose) out << "inconsistency: line " << p_actual->lineA << " of A vs line " << p_actual->lineB << " of B" << endl;
         consistencyError = true;
      }
      if(!dataIsConsistent(p_actual->lineB, lineBText, p_actual->lineC, lineCText, p_actual->bBEqC))
      {
         if(verbose) out << "inconsistency: line " << p_actual->lineB << " of B vs line " << p_actual->lineC << " of C" << endl;
         consistencyError = true;
      }
      if(!dataIsConsistent(p_actual->lineA, lineAText, p_actual->lineC, lineCText, p_actual->bAEqC))
      {
         if(verbose) out << "inconsistency: line " << p_actual->lineA << " of A vs line " << p_actual->lineC << " of C" << endl;
         consistencyError = true;
      }

      /* Check if the actual output of the algorithm is equal to the expected output */
      equal = (p_actual->lineA == p_expected->lineA) &&
              (p_actual->lineB == p_expected->lineB) &&
              (p_actual->lineC == p_expected->lineC);
      p_actual++;
      p_expected++;
   }

   if(sequenceError)
   {
      out << "NOK" << endl;

      out << "Actual result has incorrectly sequenced line numbers:" << endl;
      out << "----------------------------------------------------------------------------------------------" << endl;
      printDiff3List(actualDiff3LineList, m_sd1, m_sd2, m_sd3);
   }
   else if(consistencyError)
   {
      out << "NOK" << endl;

      out << "Actual result has inconsistent equality booleans:" << endl;
      out << "----------------------------------------------------------------------------------------------" << endl;
      printDiff3List(actualDiff3LineList, m_sd1, m_sd2, m_sd3, true);
   }
   else if(equal)
   {
      out << "OK" << endl;
   }
   else
   {
      out << "NOK" << endl;

      writeActualAlignmentFile(actualResultFile, actualDiff3LineList);

      out << "Actual result (written to " << actualResultFile << "):" << endl;
      out << "----------------------------------------------------------------------------------------------" << endl;
      printDiff3List(actualDiff3LineList, m_sd1, m_sd2, m_sd3);
      out << "----------------------------------------------------------------------------------------------" << endl;
      out << "Expected result:" << endl;
      out << "----------------------------------------------------------------------------------------------" << endl;
      printDiff3List(expectedDiff3LineList, m_sd1, m_sd2, m_sd3);
      out << "----------------------------------------------------------------------------------------------" << endl;
   }

   return equal;
}


QStringList gettestdatafiles(QString testdir)
{
   QStringList baseFilePaths;
   QTextStream out(stdout);
   QStringList nameFilter;
   nameFilter << "*_base.*";

   QDir testdatadir(testdir);

   QStringList baseFileNames = testdatadir.entryList(nameFilter, QDir::Files, QDir::Name);
   QListIterator<QString> file_it(baseFileNames);
   while(file_it.hasNext())
   {
      baseFilePaths.append(testdir + "/" + file_it.next());
   }
   out << testdir << ": " << baseFilePaths.size() << " files" << endl;


   QStringList subdirs = testdatadir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
   QListIterator<QString> dir_it(subdirs);

   while (dir_it.hasNext())
   {
      QString subdir = dir_it.next();
      QStringList subdirBaseFilePaths = gettestdatafiles(testdir + "/" + subdir);

      baseFilePaths.append(subdirBaseFilePaths);
   }

   return baseFilePaths;
}


int main(int argc, char *argv[])
{
   bool allOk = true;
   int maxLength = 0;
   QTextStream out(stdout);
   QDir testdatadir("testdata");

   /* Print data at various steps in the algorithm to get an idea where to look for the root cause of a failing test */
   if((argc == 2) && (!strcmp(argv[1], "-v")))
   {
      verbose = true;
   }

   QStringList baseFiles = gettestdatafiles("testdata");
   QListIterator<QString> it(baseFiles);

   for (int i = 0; i < baseFiles.size(); i++)
   {
      maxLength = std::max(baseFiles.at(i).length(), maxLength);
   }
   maxLength += testdatadir.path().length() + 1;

   while (it.hasNext())
   {
      QString fileName = it.next();

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

   out << (allOk ? "All OK" : "Not all OK") << endl;

   return allOk ? 0 : -1;
}
