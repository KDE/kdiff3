/***************************************************************************
                          main.cpp  -  Where everything starts.
                             -------------------
    begin                : Don Jul 11 12:31:29 CEST 2002
    copyright            : (C) 2002-2004 by Joachim Eibl
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

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include "kdiff3_shell.h"
#include "version.h"
#include <qtextcodec.h>

#ifdef KREPLACEMENTS_H
#include "optiondialog.h"
#endif

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
  { "u",  I18N_NOOP("Has no effect. For compatibility with certain tools."), 0 },
#ifdef _WIN32
  { "query",       I18N_NOOP("For compatibility with certain tools."), 0 },
#endif
  { "+[File1]", I18N_NOOP("file1 to open (base, if not specified via --base)"), 0 },
  { "+[File2]", I18N_NOOP("file2 to open"), 0 },
  { "+[File3]", I18N_NOOP("file3 to open"), 0 },
  { 0, 0, 0 }
};


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

      The installation is very simple:
      1. In the Clearcase "bin"-directory rename "cleardiffmrg.exe" to "cleardiffmrg_orig.exe".
      2. Copy kdiff3.exe into that "bin"-directory and rename it to "cleardiffmrg.exe".
         (Also copy the other files that are needed by KDiff3 there.)

      Now when a file comparison or merge is done by Clearcase then of course KDiff3 will be
      run instead.
      If the commandline contains the option "-directory" then KDiff3 can't do it but will
      run "cleardiffmrg_orig.exe" instead.
   */

   /* // Write all args into a temporary file. Uncomment this for debugging purposes.
   FILE* f = fopen("c:\\t.txt","w");
   for(int i=0; i< argc; ++i)
      fprintf(f,"Arg %d: %s\n", i, argv[i]);
   fclose(f);
   */

   // KDiff3 can replace cleardiffmrg from clearcase. But not all functions.
   if ( isOptionUsed( "directory", argc,argv ) )
   {
      return ::_spawnvp(_P_WAIT , "cleardiffmrg_orig", argv );      
   }
   
#endif
   QApplication::setColorSpec( QApplication::ManyColor ); // Grab all 216 colors

   KAboutData aboutData( "kdiff3", I18N_NOOP("KDiff3"),
      VERSION, description, KAboutData::License_GPL,
      "(c) 2002-2005 Joachim Eibl", 0, "http://kdiff3.sourceforge.net/", "joachim.eibl@gmx.de");
   aboutData.addAuthor("Joachim Eibl",0, "joachim.eibl@gmx.de");
   aboutData.addCredit("Eike Sauer", "Bugfixes, Debian package maintainer" );
   aboutData.addCredit("Sebastien Fricker", "Windows installer" );
   aboutData.addCredit("Stephan Binner", "i18n-help", "binner@kde.org" );
   aboutData.addCredit("Stefan Partheymueller", "Clipboard-patch" );
   aboutData.addCredit("David Faure", "KIO-Help", "faure@kde.org" );
   aboutData.addCredit("Bernd Gehrmann", "Class CvsIgnoreList from Cervisia" );
   aboutData.addCredit("Andre Woebbeking", "Class StringMatcher" );
   aboutData.addCredit("Michael Denio", "Directory Equality-Coloring patch");
   aboutData.addCredit("Paul Eggert, Mike Haertel, David Hayes, Richard Stallman, Len Tower", "GNU-Diffutils");
   aboutData.addCredit(I18N_NOOP("+ Many thanks to those who reported bugs and contributed ideas!"));
   
   
   KCmdLineArgs::init( argc, argv, &aboutData );
   KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

   KApplication app;
#ifdef KREPLACEMENTS_H
   QString locale;
   
   locale = app.config()->readEntry("Language", "Auto");
   int spacePos = locale.find(' ');
   if (spacePos>0) locale = locale.left(spacePos);
   QTranslator kdiff3Translator( 0 );
   QTranslator qtTranslator( 0 );
   if (locale != "en_orig")
   {
      if ( locale == "Auto" || locale.isEmpty() )
         locale = QTextCodec::locale();
         
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

  return app.exec();
}

// Suppress warning with --enable-final
#undef VERSION
