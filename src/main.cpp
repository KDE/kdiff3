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
#include "common.h"
#include "kdiff3_shell.h"
#include "version.h"

#include <KAboutData>
#include <KCrash/KCrash>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QFont>
#include <QLocale>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTextCodec>
#include <QTextStream>
#include <QTranslator>
//#include <QClipboard>
#include <vector>

#ifdef Q_OS_WIN
#include <process.h>
#include <qt_windows.h>
#endif

void initialiseCmdLineArgs(QCommandLineParser* cmdLineParser)
{
    QString configFileName = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, "kdiff3rc");
    QFile configFile(configFileName);
    QString ignorableOptionsLine = "-u;-query;-html;-abort";
    if(configFile.open(QIODevice::ReadOnly))
    {
        QTextStream ts(&configFile);
        while(!ts.atEnd())
        {
            QString line = ts.readLine();
            if(line.startsWith("IgnorableCmdLineOptions="))
            {
                int pos = line.indexOf('=');
                if(pos >= 0)
                {
                    ignorableOptionsLine = line.mid(pos + 1);
                }
                break;
            }
        }
    }
    //support our own old preferances this is obsolete
    QStringList sl = ignorableOptionsLine.split(',');

    if(!sl.isEmpty())
    {
        QStringList ignorableOptions = sl.front().split(';');
        for(QStringList::iterator i = ignorableOptions.begin(); i != ignorableOptions.end(); ++i)
        {
            (*i).remove('-');
            if(!(*i).isEmpty())
            {
                if(i->length() == 1) {
                    cmdLineParser->addOption(QCommandLineOption(QStringList() << i->toLatin1() << QLatin1String("ignore"), i18n("Ignored. (User defined.)")));
                }
                else
                {
                    cmdLineParser->addOption(QCommandLineOption(QStringList() << i->toLatin1(), i18n("Ignored. (User defined.)")));
                }
            }
        }
    }
}

#ifdef Q_OS_WIN
// This command checks the comm
static bool isOptionUsed(const QString& s, int argc, char* argv[])
{
    for(int j = 0; j < argc; ++j)
    {
        if(QString("-" + s) == argv[j] || QString("--" + s) == argv[j])
        {
            return true;
        }
    }
    return false;
}
#endif

int main(int argc, char* argv[])
{
    QApplication app(argc, argv); // KAboutData and QCommandLineParser depend on this being setup.
    KLocalizedString::setApplicationDomain("kdiff3");

    KCrash::initialize();

    const QByteArray& appName = QByteArray::fromRawData("kdiff3", 6);
    const QString i18nName = i18n("KDiff3");
    QByteArray appVersion = QByteArray::fromRawData(VERSION, sizeof(VERSION));
    if(sizeof(void*) == 8)
        appVersion += " (64 bit)";
    else if(sizeof(void*) == 4)
        appVersion += " (32 bit)";
    const QString description = i18n("Tool for Comparison and Merge of Files and Directories");
    const QString copyright = i18n("(c) 2002-2014 Joachim Eibl, (c) 2017 Michael Reeves KF5/Qt5 port");
    const QString& homePage = QStringLiteral("");
    const QString& bugsAddress = QStringLiteral("reeves.87""@""gmail.com");

    KAboutData aboutData(appName, i18nName,
                         appVersion, description, KAboutLicense::GPL_V2, copyright, description,
                         homePage, bugsAddress);

    KAboutData::setApplicationData(aboutData);

    QCommandLineParser* cmdLineParser = KDiff3Shell::getParser();
    cmdLineParser->setApplicationDescription(aboutData.shortDescription());
    cmdLineParser->addVersionOption();
    cmdLineParser->addHelpOption();

    aboutData.setupCommandLine(cmdLineParser);
    
    initialiseCmdLineArgs(cmdLineParser);
    // ignorable command options
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("m") << QLatin1String("merge"), i18n("Merge the input.")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("b") << QLatin1String("base"), i18n("Explicit base file. For compatibility with certain tools."), QLatin1String("file")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("o") << QLatin1String("output"), i18n("Output file. Implies -m. E.g.: -o newfile.txt"), QLatin1String("file")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("out"), i18n("Output file, again. (For compatibility with certain tools.)"), QLatin1String("file")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("auto"), i18n("No GUI if all conflicts are auto-solvable. (Needs -o file)")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("qall"), i18n("Do not solve conflicts automatically.")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("L1"), i18n("Visible name replacement for input file 1 (base)."), QLatin1String("alias1")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("L2"), i18n("Visible name replacement for input file 2."), QLatin1String("alias2")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("L3"), i18n("Visible name replacement for input file 3."), QLatin1String("alias3")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("L") << QLatin1String("fname alias"), i18n("Alternative visible name replacement. Supply this once for every input.")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("cs"), i18n("Override a config setting. Use once for every setting. E.g.: --cs \"AutoAdvance=1\""), QLatin1String("string")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("confighelp"), i18n("Show list of config settings and current values.")));
    cmdLineParser->addOption(QCommandLineOption(QStringList() << QLatin1String("config"), i18n("Use a different config file."), QLatin1String("file")));

    // other command options
    cmdLineParser->addPositionalArgument(QLatin1String("[File1]"), i18n("file1 to open (base, if not specified via --base)"));
    cmdLineParser->addPositionalArgument(QLatin1String("[File2]"), i18n("file2 to open"));
    cmdLineParser->addPositionalArgument(QLatin1String("[File3]"), i18n("file3 to open"));

    /*
        Don't use QCommandLineParser::process as it auto terminates the program if an option is not reconized.
        Further more errors are directed to the console alone if not running on windows. This makes for a bad
        user experiance when run from a graphical interface such as kde. Don't assume that this only happens
        when running from a commandline.
    */
    if(!cmdLineParser->parse(QCoreApplication::arguments())) {
        QString errorMessage = cmdLineParser->errorText();
        QString helpText = cmdLineParser->helpText();
        QMessageBox::warning(nullptr, aboutData.displayName(), "<html><head/><body><h2>" + errorMessage + "</h2><pre>" + helpText + "</pre></body></html>");
#if !defined(Q_OS_WIN) 
        fputs(qPrintable(errorMessage), stderr);
        fputs("\n\n", stderr);
        fputs(qPrintable(helpText + "\n"), stderr);
        fputs("\n", stderr);
#endif
        exit(1);
    }

    if(cmdLineParser->isSet(QStringLiteral("version"))) {
        QMessageBox::information(nullptr, aboutData.displayName(),
                                 aboutData.displayName() + ' ' + aboutData.version());
#if !defined(Q_OS_WIN) 
        printf("%s %s\n", appName.constData(), appVersion.constData());
#endif
        exit(0);
    }
    if(cmdLineParser->isSet(QStringLiteral("help"))) {
        QMessageBox::warning(nullptr, aboutData.displayName(), "<html><head/><body><pre>" + cmdLineParser->helpText() + "</pre></body></html>");
#if !defined(Q_OS_WIN) 
        fputs(qPrintable(cmdLineParser->helpText()), stdout);
#endif
        exit(0);
    }

    aboutData.processCommandLine(cmdLineParser);
    /**
     * take component name and org. name from KAboutData
     */
    app.setApplicationName(aboutData.componentName());
    app.setApplicationDisplayName(aboutData.displayName());
    app.setOrganizationDomain(aboutData.organizationDomain());
    app.setApplicationVersion(aboutData.version());

    KDiff3Shell* p = new KDiff3Shell();
    p->show();
    //p->setWindowState( p->windowState() | Qt::WindowActive ); // Patch for ubuntu: window not active on startup
    //app.installEventFilter( new CFilter );
    int retVal = app.exec();
    /*  if (QApplication::clipboard()->text().size() == 0)
     QApplication::clipboard()->clear(); // Patch for Ubuntu: Fix issue with Qt clipboard*/
    return retVal;
}

// Suppress warning with --enable-final
#undef VERSION
