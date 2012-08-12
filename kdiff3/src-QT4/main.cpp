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
#include <QTextCodec>
#include <QFile>
#include <QTextStream>
#include <QTranslator>
#include <QLocale>
#include <QFont>
#include <QClipboard>
#include <vector>

#ifdef KREPLACEMENTS_H
#include "optiondialog.h"
#endif
#include "common.h"

void initialiseCmdLineArgs( KCmdLineOptions& options )
{
   QString configFileName = KStandardDirs().findResource("config","kdiff3rc");
   QFile configFile( configFileName );
   QString ignorableOptionsLine = "-u;-query;-html;-abort";
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
               ignorableOptionsLine = line.mid(pos+1);
            }
            break;
         }
      }
   }

#ifdef KREPLACEMENTS_H
   QStringList sl = ignorableOptionsLine.split( '|' );
#else
   QStringList sl = ignorableOptionsLine.split( ',' );
#endif
   if (!sl.isEmpty())
   {
      QStringList ignorableOptions = sl.front().split( ';' );
      for (QStringList::iterator i=ignorableOptions.begin(); i!=ignorableOptions.end(); ++i)
      {
         (*i).remove('-');
         if (!(*i).isEmpty())
         {
            if ( i->length()==1 )
               options.add( i->toLatin1() ).add("ignore", ki18n("Ignored. (User defined.)") );
            else
               options.add( i->toLatin1(), ki18n("Ignored. (User defined.)") );
         }
      }
   }
}

#ifdef _WIN32
#include <process.h>
// This command checks the comm
static bool isOptionUsed(const QString& s, int argc, char* argv[])
{
   for(int j=0; j<argc; ++j )
   {
      if( QString("-"+s) == argv[j] || QString("--"+s)==argv[j] )
      {
         return true;
      }
   }
   return false;
}
#endif

class ContextFreeTranslator : public QTranslator
{
public:
   ContextFreeTranslator( QObject* pParent ) : QTranslator(pParent) {}
   QString translate(const char* context, const char* sourceText, const char* comment ) const
   {
      if ( context != 0 )
         return QTranslator::translate(0,sourceText,comment);
      else
         return QString();
   }
};

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
   FILE* f = fopen(QDir::toNativeSeparators(QDir::homePath()+"//kdiff3_call_args.txt").toLatin1().data(),"w");
   for(int i=0; i< argc; ++i)
      fprintf(f,"Arg %d: %s\n", i, argv[i]);
   fclose(f);
   
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
#ifdef Q_OS_OS2
   // expand wildcards on the command line
   _wildcard(&argc, &argv);
#endif

   //QApplication::setColorSpec( QApplication::ManyColor ); // Grab all 216 colors

   const QByteArray& appName = QByteArray("kdiff3");
   const QByteArray& appCatalog = appName;
   const KLocalizedString i18nName = ki18n("kdiff3");
   QByteArray appVersion = QByteArray( VERSION );
   if ( sizeof(void*)==8 )
      appVersion += " (64 bit)";
   else if ( sizeof(void*)==4 )
       appVersion += " (32 bit)";
   const KLocalizedString description = ki18n("Tool for Comparison and Merge of Files and Directories");
   const KLocalizedString copyright = ki18n("(c) 2002-2012 Joachim Eibl");
   const QByteArray& homePage = "http://kdiff3.sourceforge.net/";
   const QByteArray& bugsAddress = "joachim.eibl" "@" "gmx.de";
   KAboutData aboutData( appName, appCatalog, i18nName, 
         appVersion, description, KAboutData::License_GPL_V2, copyright, description, 
         homePage, bugsAddress );

   aboutData.addAuthor(ki18n("Joachim Eibl"), KLocalizedString(), QByteArray("joachim.eibl" "@" "gmx.de"));
   aboutData.addCredit(ki18n("Eike Sauer"), ki18n("Bugfixes, Debian package maintainer") );
   aboutData.addCredit(ki18n("Sebastien Fricker"), ki18n("Windows installer") );
   aboutData.addCredit(ki18n("Stephan Binner"), ki18n("i18n-help"), QByteArray("binner" "@" "kde.org") );
   aboutData.addCredit(ki18n("Stefan Partheymueller"), ki18n("Clipboard-patch" ));
   aboutData.addCredit(ki18n("David Faure"), ki18n("KIO-Help"), QByteArray("faure" "@" "kde.org" ));
   aboutData.addCredit(ki18n("Bernd Gehrmann"), ki18n("Class CvsIgnoreList from Cervisia" ));
   aboutData.addCredit(ki18n("Andre Woebbeking"), ki18n("Class StringMatcher" ));
   aboutData.addCredit(ki18n("Michael Denio"), ki18n("Directory Equality-Coloring patch"));
   aboutData.addCredit(ki18n("Manfred Koehler"), ki18n("Fix for slow startup on Windows"));
   aboutData.addCredit(ki18n("Sergey Zorin"), ki18n("Diff Ext for Windows"));
   aboutData.addCredit(ki18n("Paul Eggert, Mike Haertel, David Hayes, Richard Stallman, Len Tower"), ki18n("GNU-Diffutils"));
   aboutData.addCredit(ki18n("Tino Boellsterling, Timothy Mee"), ki18n("Intensive test, use and feedback"));
   aboutData.addCredit(ki18n("Michael Schmidt"), ki18n("Mac support"));
   aboutData.addCredit(ki18n("Valentin Rusu"), ki18n("KDE4 porting"), QByteArray("kde" "@" "rusu.info"));
   aboutData.addCredit(ki18n("Albert Astals Cid"), ki18n("KDE4 porting"), QByteArray("aacid" "@" "kde.org"));
   aboutData.addCredit(ki18n("Silvan Scherrer"), ki18n("OS2 port") );
   aboutData.addCredit(ki18n("Nigel Stewart"), ki18n("Cygwin support") );
   aboutData.addCredit(ki18n("Maurice van der Pot"), ki18n("Framework for automated tests") );

   aboutData.addCredit(ki18n("+ Many thanks to those who reported bugs and contributed ideas!"));

   KCmdLineArgs::init( argc, argv, &aboutData );

   KCmdLineOptions options;
   // ignorable command options
   options.add( "m" ).add( "merge", ki18n("Merge the input."));
   options.add( "b" ).add( "base file", ki18n("Explicit base file. For compatibility with certain tools.") );
   options.add( "o" ).add( "output file", ki18n("Output file. Implies -m. E.g.: -o newfile.txt"));
   options.add( "out file",    ki18n("Output file, again. (For compatibility with certain tools.)") );
   options.add( "auto",        ki18n("No GUI if all conflicts are auto-solvable. (Needs -o file)") );
   options.add( "qall",        ki18n("Don't solve conflicts automatically.") );
   options.add( "L1 alias1",   ki18n("Visible name replacement for input file 1 (base).") );
   options.add( "L2 alias2",   ki18n("Visible name replacement for input file 2.") );
   options.add( "L3 alias3",   ki18n("Visible name replacement for input file 3.") );
   options.add( "L" ).add( "fname alias", ki18n("Alternative visible name replacement. Supply this once for every input.") );
   options.add( "cs string",   ki18n("Override a config setting. Use once for every setting. E.g.: --cs \"AutoAdvance=1\"") );
   options.add( "confighelp",  ki18n("Show list of config settings and current values.") );
   options.add( "config file", ki18n("Use a different config file.") );

   // other command options
   options.add( "+[File1]", ki18n("file1 to open (base, if not specified via --base)") );
   options.add( "+[File2]", ki18n("file2 to open") );
   options.add( "+[File3]", ki18n("file3 to open") );

   initialiseCmdLineArgs( options );

   KCmdLineArgs::addCmdLineOptions( options );

   KApplication app;

#ifdef KREPLACEMENTS_H
   QString locale;

   locale = app.config()->readEntry("Language", "Auto");
   int spacePos = locale.indexOf(' ');
   if (spacePos>0) locale = locale.left(spacePos);
   ContextFreeTranslator kdiff3Translator( 0 );
   QTranslator qtTranslator( 0 );
   if (locale != "en_orig")
   {
      if ( locale == "Auto" || locale.isEmpty() )
         locale = locale = QLocale::system().name().left(2);
         
      QString translationDir = getTranslationDir(locale);
      kdiff3Translator.load( QString("kdiff3_")+locale, translationDir );
      app.installTranslator( &kdiff3Translator );
      
      qtTranslator.load( QString("qt_")+locale, translationDir );
      app.installTranslator( &qtTranslator );
   }
#endif

#ifndef QT_NO_SESSIONMANAGER
  if (app.isSessionRestored())
  {
     RESTORE(KDiff3Shell);
  }
  else
#endif
  {
     KDiff3Shell* p = new KDiff3Shell();
     p->show();
     p->setWindowState( p->windowState() | Qt::WindowActive ); // Patch for ubuntu: window not active on startup
  }
//app.installEventFilter( new CFilter );
  int retVal = app.exec();
  if (QApplication::clipboard()->text().size() == 0)
     QApplication::clipboard()->clear(); // Patch for Ubuntu: Fix issue with Qt clipboard
  return retVal;
}

// Suppress warning with --enable-final
#undef VERSION
