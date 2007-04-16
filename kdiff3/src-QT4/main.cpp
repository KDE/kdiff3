/***************************************************************************
                          main.cpp  -  Where everything starts.
                             -------------------
    begin                : Don Jul 11 12:31:29 CEST 2002
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

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include "kdiff3_shell.h"
#include <kstandarddirs.h>
#include "version.h"
#include <qtextcodec.h>
#include <qfile.h>
#include <qtextstream.h>
#include <QTranslator>
#include <QLocale>
#include <vector>

#ifdef KREPLACEMENTS_H
#include "optiondialog.h"
#endif
#include "common.h"

static const char *description =
   I18N_NOOP("Tool for Comparison and Merge of Files and Directories");

static KCmdLineOptions options[] =
{
  { "m", 0, 0 },
  { "merge", I18N_NOOP("Merge the input."), 0 },
  { "b", 0, 0 },
  { "base file", I18N_NOOP("Explicit base file. For compatibility with certain tools."), 0 },
  { "o", 0, 0 },
  { "output file", I18N_NOOP("Output file. Implies -m. E.g.: -o newfile.txt"), 0 },
  { "out file",    I18N_NOOP("Output file, again. (For compatibility with certain tools.)"), 0 },
  { "auto",        I18N_NOOP("No GUI if all conflicts are auto-solvable. (Needs -o file)"), 0 },
  { "qall",        I18N_NOOP("Don't solve conflicts automatically. (For compatibility...)"), 0 },
  { "L1 alias1",   I18N_NOOP("Visible name replacement for input file 1 (base)."), 0 },
  { "L2 alias2",   I18N_NOOP("Visible name replacement for input file 2."), 0 },
  { "L3 alias3",   I18N_NOOP("Visible name replacement for input file 3."), 0 },
  { "L", 0, 0 },
  { "fname alias", I18N_NOOP("Alternative visible name replacement. Supply this once for every input."), 0 },
  { "cs string",   I18N_NOOP("Override a config setting. Use once for every setting. E.g.: --cs \"AutoAdvance=1\""), 0 },
  { "confighelp",  I18N_NOOP("Show list of config settings and current values."), 0 },
  { "config file", I18N_NOOP("Use a different config file."), 0 }
};
static KCmdLineOptions options2[] =
{
  { "+[File1]", I18N_NOOP("file1 to open (base, if not specified via --base)"), 0 },
  { "+[File2]", I18N_NOOP("file2 to open"), 0 },
  { "+[File3]", I18N_NOOP("file3 to open"), 0 }
};


void initialiseCmdLineArgs(std::vector<KCmdLineOptions>& vOptions, QStringList& ignorableOptions)
{
   vOptions.insert( vOptions.end(), options, (KCmdLineOptions*)((char*)options+sizeof(options)));
   QString configFileName = KStandardDirs().findResource("config","kdiff3rc");
   QFile configFile( configFileName );
   if ( configFile.open( QIODevice::ReadOnly ) )
   {
      QTextStream ts( &configFile );
      while(!ts.atEnd())
      {
         QString line = ts.readLine();
         if ( line.startsWith("IgnorableCmdLineOptions=") )
         {
            int pos = line.indexOf('=');
            if (pos>=0)
            {
               QString s = line.mid(pos+1);
               QStringList sl = s.split( '|' );
               if (!sl.isEmpty())
               {
                  ignorableOptions = sl.front().split( ';' );
                  for (QStringList::iterator i=ignorableOptions.begin(); i!=ignorableOptions.end(); ++i)
                  {
                     KCmdLineOptions ignoreOption;
                     (*i).remove('-');
                     if (!(*i).isEmpty())
                     {
                        ignoreOption.name = (new QByteArray( (*i).toLatin1() ))->constData();
                        ignoreOption.description = I18N_NOOP("Ignored. (User defined.)");
                        ignoreOption.def = 0;
                        vOptions.push_back(ignoreOption);
                     }
                  }
               }
            }
            break;
         }
      }
   }
   vOptions.insert(vOptions.end(),options2,(KCmdLineOptions*)((char*)options2+sizeof(options2)));

   KCmdLineOptions last = KCmdLineLastOption;
   vOptions.push_back(last);
   KCmdLineArgs::addCmdLineOptions( &vOptions[0] ); // Add our own options.
}


#ifdef _WIN32
#include <process.h>
// This command checks the comm
static bool isOptionUsed(const QString& s, int argc, char* argv[])
{
   for(int j=0; j<argc; ++j )
   {
      if( "-"+s == argv[j] || "--"+s==argv[j] )
      {
         return true;
      }
   }
   return false;
}
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
   /* KDiff3 can be used as replacement for the text-diff and merge tool provided by
      Clearcase. This is experimental and so far has only been tested under Windows.

      There are two ways to use KDiff3 with clearcase
      -  The file lib/mgrs/map contains the list of compare/merge tasks on one side and 
         the tool on the other. Originally this contains only clearcase tools, but you can
         edit this file and put kdiff3 there instead. (Recommended method)
      -  Exchange the original program with KDiff3: (Hackish, no fine control)
         1. In the Clearcase "bin"-directory rename "cleardiffmrg.exe" to "cleardiffmrg_orig.exe".
         2. Copy kdiff3.exe into that "bin"-directory and rename it to "cleardiffmrg.exe".
            (Also copy the other files that are needed by KDiff3 there.)
         Now when a file comparison or merge is done by Clearcase then of course KDiff3 will be
         run instead.
         If the commandline contains the option "-directory" then KDiff3 can't do it but will
         run "cleardiffmrg_orig.exe" instead.
   */

   // Write all args into a temporary file. Uncomment this for debugging purposes.
   /*
   FILE* f = fopen("c:\\t.txt","w");
   for(int i=0; i< argc; ++i)
      fprintf(f,"Arg %d: %s\n", i, argv[i]);

   // Call orig cleardiffmrg.exe to see what result it returns.
   int result=0;
   result = ::_spawnvp(_P_WAIT , "C:\\Programme\\Rational\\ClearCase\\bin\\cleardiffmrg.exe", argv );
   fprintf(f,"Result: %d\n", result );
   fclose(f);
   return result;
   */

   // KDiff3 can replace cleardiffmrg from clearcase. But not all functions.
   if ( isOptionUsed( "directory", argc,argv ) )
   {
      return ::_spawnvp(_P_WAIT , "cleardiffmrg_orig", argv );      
   }

#endif
   //QApplication::setColorSpec( QApplication::ManyColor ); // Grab all 216 colors

   KAboutData aboutData( "kdiff3", I18N_NOOP("KDiff3"),
      VERSION, description, KAboutData::License_GPL,
      "(c) 2002-2007 Joachim Eibl", 0, "http://kdiff3.sourceforge.net/", "joachim.eibl" "@" "gmx.de");
   aboutData.addAuthor("Joachim Eibl",0, "joachim.eibl" "@" "gmx.de");
   aboutData.addCredit("Eike Sauer", "Bugfixes, Debian package maintainer" );
   aboutData.addCredit("Sebastien Fricker", "Windows installer" );
   aboutData.addCredit("Stephan Binner", "i18n-help", "binner" "@" "kde.org" );
   aboutData.addCredit("Stefan Partheymueller", "Clipboard-patch" );
   aboutData.addCredit("David Faure", "KIO-Help", "faure" "@" "kde.org" );
   aboutData.addCredit("Bernd Gehrmann", "Class CvsIgnoreList from Cervisia" );
   aboutData.addCredit("Andre Woebbeking", "Class StringMatcher" );
   aboutData.addCredit("Michael Denio", "Directory Equality-Coloring patch");
   aboutData.addCredit("Manfred Koehler", "Fix for slow startup on Windows");
   aboutData.addCredit("Sergey Zorin", "Diff Ext for Windows");
   aboutData.addCredit("Paul Eggert, Mike Haertel, David Hayes, Richard Stallman, Len Tower", "GNU-Diffutils");
   aboutData.addCredit("Tino Boellsterling, Timothy Mee", "Intensive test, use and feedback");
   aboutData.addCredit("Michael Schmidt", "Mac support");

   aboutData.addCredit(I18N_NOOP("+ Many thanks to those who reported bugs and contributed ideas!"));

   KCmdLineArgs::init( argc, argv, &aboutData );
   std::vector<KCmdLineOptions> vOptions;
   QStringList ignorableOptions;
   initialiseCmdLineArgs(vOptions, ignorableOptions);

   KApplication app;

#ifdef KREPLACEMENTS_H
   QString locale;

   locale = app.config()->readEntry("Language", "Auto");
   int spacePos = locale.indexOf(' ');
   if (spacePos>0) locale = locale.left(spacePos);
   QTranslator kdiff3Translator( 0 );
   QTranslator qtTranslator( 0 );
   if (locale != "en_orig")
   {
      if ( locale == "Auto" || locale.isEmpty() )
         locale = locale = QLocale::system().name().left(2);
         
      QString translationDir = getTranslationDir();
      kdiff3Translator.load( QString("kdiff3_")+locale, translationDir );
      app.installTranslator( &kdiff3Translator );
      
      qtTranslator.load( QString("qt_")+locale, translationDir );
      app.installTranslator( &qtTranslator );
   }
#endif

  if (app.isRestored())
  {
     RESTORE(KDiff3Shell);
  }
  else
  {
     new KDiff3Shell();
  }

  int retVal = app.exec();
  return retVal;
}

// Suppress warning with --enable-final
#undef VERSION
