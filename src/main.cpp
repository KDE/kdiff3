// clang-format off
/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/
// clang-format on

#include "kdiff3_shell.h"
#include "UTF8BOMCodec.h"
#include "version.h"

#include <stdio.h>  // for fileno, stderr
#include <stdlib.h> // for exit

#ifndef Q_OS_WIN
#include <unistd.h>
#endif

#include <KAboutData>
#include <KCrash/KCrash>
#include <KLocalizedString>
#include <KMessageBox>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFile>
#include <QPointer>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

void initialiseCmdLineArgs(QCommandLineParser* cmdLineParser)
{
    const QString configFileName = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, "kdiff3rc");
    QFile configFile(configFileName);
    QString ignorableOptionsLine = "-u;-query;-html;-abort";
    if(configFile.open(QIODevice::ReadOnly))
    {
        QTextStream ts(&configFile);
        while(!ts.atEnd())
        {
            const QString line = ts.readLine();
            if(line.startsWith(u8"IgnorableCmdLineOptions="))
            {
                const qint32 pos = line.indexOf('=');
                if(pos >= 0)
                {
                    ignorableOptionsLine = line.mid(pos + 1);
                }
                break;
            }
        }
    }
    //support our own old preferences this is obsolete
    const QStringList sl = ignorableOptionsLine.split(',');

    if(!sl.isEmpty())
    {
        const QStringList ignorableOptions = sl.front().split(';');
        for(QString ignorableOption: ignorableOptions)
        {
            ignorableOption.remove('-');
            if(!ignorableOption.isEmpty())
            {
                if(ignorableOption.length() == 1)
                {
                    cmdLineParser->addOption(QCommandLineOption({ignorableOption, u8"ignore"}, i18n("Ignored. (User defined.)")));
                }
                else
                {
                    cmdLineParser->addOption(QCommandLineOption(ignorableOption, i18n("Ignored. (User defined.)")));
                }
            }
        }
    }
}

qint32 main(qint32 argc, char* argv[])
{
    constexpr QLatin1String appName("kdiff3", sizeof("kdiff3") - 1);
    /*
        QTextCodec auto registers text codecs in its constructor.
        Do this now because we only need one codec object.
        This object is Qt's domain once created and must be on the heap.
    */
    const UTF8BOMCodec *textCodec = new UTF8BOMCodec();
    Q_UNUSED(textCodec);

    //Syncronize qt HDPI behavoir on all versions/platforms
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv); // KAboutData and QCommandLineParser depend on this being setup.
    KLocalizedString::setApplicationDomain(appName.data());

    KCrash::initialize();

    const QString i18nName = i18n("KDiff3");
    QString appVersion(KDIFF3_VERSION_STRING);

    if(sizeof(void*) == 8)
        appVersion += i18nc("Program version info.", " (64 bit)");
    else if(sizeof(void*) == 4)
        appVersion += i18nc("Program version info.", " (32 bit)");
    const QString description = i18n("Tool for Comparison and Merge of Files and Folders");
    const QString copyright = i18n("(c) 2002-2014 Joachim Eibl, (c) 2017 Michael Reeves KF5/Qt5 port");
    const QString homePage = QStringLiteral("https://kde.org/applications/development/kdiff3");

    KAboutData aboutData(appName, i18nName,
                         appVersion, description, KAboutLicense::GPL_V2, copyright, QString(),
                         homePage);

    KAboutData::setApplicationData(aboutData);

    /*
        The QCommandLineParser is a static scoped unique ptr. This is safe given that.
        As the distuctor will not be fired until main exits.
    */
    QCommandLineParser* cmdLineParser = KDiff3Shell::getParser().get();
    cmdLineParser->setApplicationDescription(aboutData.shortDescription());

    aboutData.setupCommandLine(cmdLineParser);

    initialiseCmdLineArgs(cmdLineParser);
    // ignorable command options
    cmdLineParser->addOption(QCommandLineOption({u8"m", u8"merge"}, i18n("Merge the input.")));
    cmdLineParser->addOption(QCommandLineOption({u8"b", u8"base"}, i18n("Explicit base file. For compatibility with certain tools."), u8"file"));
    cmdLineParser->addOption(QCommandLineOption({u8"o", u8"output"}, i18n("Output file. Implies -m. E.g.: -o newfile.txt"), u8"file"));
    cmdLineParser->addOption(QCommandLineOption(u8"out", i18n("Output file, again. (For compatibility with certain tools.)"), u8"file"));
#ifdef ENABLE_AUTO
    cmdLineParser->addOption(QCommandLineOption(u8"auto", i18n("No GUI if all conflicts are auto-solvable. (Needs -o file)")));
    cmdLineParser->addOption(QCommandLineOption(u8"noauto", i18n("Ignore --auto and always show GUI.")));
#else
    cmdLineParser->addOption(QCommandLineOption(u8"noauto", i18n("Ignored.")));
    cmdLineParser->addOption(QCommandLineOption(u8"auto", i18n("Ignored.")));
#endif
    cmdLineParser->addOption(QCommandLineOption(u8"L1", i18n("Visible name replacement for input file 1 (base)."), u8"alias1"));
    cmdLineParser->addOption(QCommandLineOption(u8"L2", i18n("Visible name replacement for input file 2."), u8"alias2"));
    cmdLineParser->addOption(QCommandLineOption(u8"L3", i18n("Visible name replacement for input file 3."), u8"alias3"));
    cmdLineParser->addOption(QCommandLineOption({u8"L", u8"fname"}, i18n("Alternative visible name replacement. Supply this once for every input."), u8"alias"));
    cmdLineParser->addOption(QCommandLineOption(u8"cs", i18n("Override a config setting. Use once for every setting. E.g.: --cs \"AutoAdvance=1\""), u8"string"));
    cmdLineParser->addOption(QCommandLineOption(u8"confighelp", i18n("Show list of config settings and current values.")));
    cmdLineParser->addOption(QCommandLineOption(u8"config", i18n("Use a different config file."), u8"file"));

    // other command options
    cmdLineParser->addPositionalArgument(u8"[File1]", i18n("file1 to open (base, if not specified via --base)"));
    cmdLineParser->addPositionalArgument(u8"[File2]", i18n("file2 to open"));
    cmdLineParser->addPositionalArgument(u8"[File3]", i18n("file3 to open"));

    bool isAtty = true;

#ifndef Q_OS_WIN
    isAtty = isatty(fileno(stderr)) == 1;//will be true for redirected output as well
#endif
    /*
        QCommandLineParser::process does what is expected on windows or when running from a commandline.
        However, it only accounts for a lack of terminal output on windows.
    */
    if(isAtty)
    {
        cmdLineParser->process(QCoreApplication::arguments());
    }
    else
    {
        /*
            There is no terminal connected so don't just exit mysteriously on error.
        */
        if(!cmdLineParser->parse(QCoreApplication::arguments()))
        {
            const QString errorMessage = cmdLineParser->errorText();

            KMessageBox::error(nullptr, "<html><head/><body><h2>" + errorMessage + "</h2><pre>" + i18n("See kdiff3 --help for supported options.") + "</pre></body></html>", aboutData.displayName());
            exit(1);
        }

        if(cmdLineParser->isSet(QStringLiteral("version")))
        {
            KMessageBox::information(nullptr,
                                    aboutData.displayName() + ' ' + aboutData.version(), aboutData.displayName());
            exit(0);
        }
        if(cmdLineParser->isSet(QStringLiteral("help")))
        {
            KMessageBox::information(nullptr, "<html><head/><body><pre>" + cmdLineParser->helpText() + "</pre></body></html>", aboutData.displayName());

            exit(0);
        }
    }

    aboutData.processCommandLine(cmdLineParser);

    /*
      Do not attempt to call show here that will be done later.
      This variable exists solely to insure the KDiff3Shell is deleted on exit.
    */
    const QPointer<KDiff3Shell> p(new KDiff3Shell());//QPointer will take it from here.
    Q_UNUSED(p);

    qint32 retVal = QApplication::exec();
    return retVal;
}
