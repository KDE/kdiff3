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
#include "stable.h"
#include <QStandardPaths>
#include <QApplication>
#include <KAboutData>
#include <KLocalizedString>
#include <KCrash/KCrash>
#include "kdiff3_shell.h"
#include "version.h"
#include <QTextCodec>
#include <QFile>
#include <QTextStream>
#include <QTranslator>
#include <QLocale>
#include <QFont>
//#include <QClipboard>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <vector>

#ifdef _WIN32
#include <process.h>
#endif

#ifdef KREPLACEMENTS_H
#include "optiondialog.h"
#endif
#include "common.h"

void initialiseCmdLineArgs( )
{
    QString configFileName = QStandardPaths::locate(QStandardPaths::GenericConfigLocation , "kdiff3rc" );
    QFile configFile( configFileName );
    QString ignorableOptionsLine = "-u;-query;-html;-abort";
    if( configFile.open( QIODevice::ReadOnly ) )
    {
        QTextStream ts( &configFile );
        while(!ts.atEnd())
        {
            QString line = ts.readLine();
            if( line.startsWith("IgnorableCmdLineOptions=") )
            {
                int pos = line.indexOf('=');
                if(pos>=0)
                {
                    ignorableOptionsLine = line.mid(pos+1);
                }
                break;
            }
        }
    }
    //support our own old preferances this is obsolete
    QStringList sl = ignorableOptionsLine.split( ',' );

    if(!sl.isEmpty())
    {
        QStringList ignorableOptions = sl.front().split( ';' );
        for(QStringList::iterator i=ignorableOptions.begin(); i!=ignorableOptions.end(); ++i)
        {
            (*i).remove('-');
            if(!(*i).isEmpty())
            {
                if( i->length()==1 ) {
                    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << i->toLatin1() << QLatin1String( "ignore" ), i18n( "Ignored. (User defined.)" ) ) );
                }
                else {
                    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << i->toLatin1(), i18n( "Ignored. (User defined.)" ) ) );
                }
            }
        }
    }
}

#ifdef _WIN32
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

#if defined(KREPLACEMENTS_H) && !defined(QT_NO_TRANSLATION)
class ContextFreeTranslator : public QTranslator
{
public:
   ContextFreeTranslator( QObject* pParent ) : QTranslator(pParent) {}
#if QT_VERSION>=0x050000
   QString translate(const char * context, const char * sourceText, const char * disambiguation, int /*n*/ ) const /*override*/
#else
   QString translate(const char* context, const char* sourceText, const char* disambiguation ) const /*override*/
#endif
   {
      if ( context != 0 )
         return QTranslator::translate(0,sourceText,disambiguation);
      else
         return QString();
   }
};
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv); // KAboutData and QCommandLineParser depend on this being setup.

    KCrash::initialize();
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

   const QByteArray& appName = QByteArray::fromRawData("kdiff3", 6);
   const QByteArray& appCatalog = appName;
   const QString i18nName = i18n("kdiff3");
   QByteArray appVersion = QByteArray::fromRawData( VERSION, sizeof(VERSION));
   if ( sizeof(void*)==8 )
      appVersion += " (64 bit)";
   else if ( sizeof(void*)==4 )
       appVersion += " (32 bit)";
   const QString description = i18n("Tool for Comparison and Merge of Files and Directories");
   const QString copyright = i18n("(c) 2002-2014 Joachim Eibl");
   const QString& homePage = QStringLiteral("http://kdiff3.sourceforge.net/");
   const QString& bugsAddress = QStringLiteral("joachim.eibl@gmx.de");
   
   KLocalizedString::setApplicationDomain(appCatalog);
   KAboutData aboutData( appName, i18nName, 
         appVersion, description, KAboutLicense::GPL_V2, copyright, description, 
         homePage, bugsAddress );

    KAboutData::setApplicationData( aboutData );
    
    KDiff3Shell::getParser()->setApplicationDescription(aboutData.shortDescription());
    KDiff3Shell::getParser()->addVersionOption();
    KDiff3Shell::getParser()->addHelpOption();
    initialiseCmdLineArgs( );
    // ignorable command options
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "m" ) << QLatin1String( "merge" ), i18n( "Merge the input." ) ) );
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "b" ) << QLatin1String( "base" ), i18n( "Explicit base file. For compatibility with certain tools." ), QLatin1String("file") ) );
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "o" ) << QLatin1String( "output" ), i18n( "Output file. Implies -m. E.g.: -o newfile.txt" ), QLatin1String("file") ) );
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "out" ), i18n( "Output file, again. (For compatibility with certain tools.)" ) , QLatin1String("file") ) );
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "auto" ), i18n( "No GUI if all conflicts are auto-solvable. (Needs -o file)" ) ) );
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "qall" ), i18n( "Don't solve conflicts automatically." ) ) );
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "L1" ), i18n( "Visible name replacement for input file 1 (base)." ), QLatin1String("alias1") ) );
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "L2" ), i18n( "Visible name replacement for input file 2." ), QLatin1String("alias2") ) );
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "L3" ), i18n( "Visible name replacement for input file 3." ), QLatin1String("alias3") ) );
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "L" ) << QLatin1String( "fname alias" ), i18n( "Alternative visible name replacement. Supply this once for every input." ) ) );
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "cs" ), i18n( "Override a config setting. Use once for every setting. E.g.: --cs \"AutoAdvance=1\"" ), QLatin1String("string") ) );
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "confighelp" ), i18n( "Show list of config settings and current values." ) ) );
    KDiff3Shell::getParser()->addOption( QCommandLineOption( QStringList() << QLatin1String( "config" ), i18n( "Use a different config file." ), QLatin1String("file") ) );

    // other command options
    KDiff3Shell::getParser()->addPositionalArgument( QLatin1String( "[File1]" ), i18n( "file1 to open (base, if not specified via --base)" ) );
    KDiff3Shell::getParser()->addPositionalArgument( QLatin1String( "[File2]" ), i18n( "file2 to open" ) );
    KDiff3Shell::getParser()->addPositionalArgument( QLatin1String( "[File3]" ), i18n( "file3 to open" ) );

    KDiff3Shell::getParser()->process( app ); // PORTING SCRIPT: move this to after any parser.addOption
    //must be after process or parse call
    aboutData.setupCommandLine( KDiff3Shell::getParser() );
    /**
     * take component name and org. name from KAboutData
     */
    app.setApplicationName(aboutData.componentName());
    app.setApplicationDisplayName(aboutData.displayName());
    app.setOrganizationDomain(aboutData.organizationDomain());
    app.setApplicationVersion(aboutData.version());
    
#if defined(KREPLACEMENTS_H) && !defined(QT_NO_TRANSLATION)
    QString locale;

    locale = app.config()->readEntry( "Language", "Auto" );
    int spacePos = locale.indexOf( ' ' );
    if( spacePos > 0 ) locale = locale.left( spacePos );
    ContextFreeTranslator kdiff3Translator( 0 );
    QTranslator qtTranslator( 0 );
    if( locale != "en_orig" )
    {
        QString translationDir = getTranslationDir( locale );
        kdiff3Translator.load(QLocale::system(), QString( "kdiff3_" ), translationDir );
        app.installTranslator( &kdiff3Translator );
	
        qtTranslator.load(QLocale::system(), QString("qt_"), translationDir );
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
     //p->setWindowState( p->windowState() | Qt::WindowActive ); // Patch for ubuntu: window not active on startup
  }
//app.installEventFilter( new CFilter );
  int retVal = app.exec();
/*  if (QApplication::clipboard()->text().size() == 0)
     QApplication::clipboard()->clear(); // Patch for Ubuntu: Fix issue with Qt clipboard*/
  return retVal;
}

// Suppress warning with --enable-final
#undef VERSION
