/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kdiff3_part.h"

#include "fileaccess.h"
#include "kdiff3.h"
#include "Logging.h"

#include <QFile>
#include <QProcess>
#include <QTemporaryFile>
#include <QTextStream>

#include <KAboutData>
#include <KLocalizedString>
#include <KMessageBox>
#include <KParts/MainWindow>

#include <version.h>

KAboutData KDiff3Part::createAboutData()
{
    QString appVersion = QString(KDIFF3_VERSION_STRING);
    if(sizeof(void*) == 8)
        appVersion += " (64 bit)";
    else if(sizeof(void*) == 4)
        appVersion += " (32 bit)";

    KAboutData aboutData(QLatin1String("kdiff3part"), i18n("KDiff3 Part"),
                         appVersion, i18n("A KPart to display SVG images"),
                         KAboutLicense::GPL_V2,
                         i18n("Copyright 2007, Aurélien Gâteau <aurelien.gateau@free.fr>"));
    aboutData.addAuthor(i18n("Joachim Eibl"), QString(), QString("joachim.eibl at gmx.de"));
    return aboutData;
}

K_PLUGIN_FACTORY(KDiff3PartFactory, registerPlugin<KDiff3Part>();)

KDiff3Part::KDiff3Part(QWidget* parentWidget, QObject* parent, const QVariantList& args)
    : KParts::ReadWritePart(parent)
{
    //set AboutData
    setComponentData(createAboutData());
    if(!args.isEmpty())
    {
        const QString widgetName = args[0].toString();

        // this should be your custom internal widget
        m_widget = new KDiff3App(parentWidget, widgetName, this);
    }
    else
        m_widget = new KDiff3App(parentWidget, u8"KDiff3Part", this);

    // notify the part that this is our internal widget
    setWidget(m_widget);

    // create our actions
    //KStandardAction::open(this, &KDiff3Part:fileOpen, actionCollection());
    //KStandardAction::saveAs(this, &KDiff3Part:fileSaveAs, actionCollection());
    //KStandardAction::save(this, &KDiff3Part:save, actionCollection());

    setXMLFile("kdiff3_part.rc");

    // we are read-write by default
    setReadWrite(true);

    // we are not modified since we haven't done anything yet
    setModified(false);
}

KDiff3Part::~KDiff3Part()
{
    //TODO: Is parent check needed?
    if(m_widget != nullptr && qobject_cast<KParts::MainWindow*>(parent()) != nullptr )
    {
        m_widget->saveOptions(KSharedConfig::openConfig());
    }
}

void KDiff3Part::setReadWrite(bool /*rw*/)
{
    //    ReadWritePart::setReadWrite(rw);
}

void KDiff3Part::setModified(bool /*modified*/)
{
    /*
    // get a handle on our Save action and make sure it is valid
    QAction *save = actionCollection()->action(KStandardAction::stdName(KStandardAction::Save));
    if (!save)
        return;

    // if so, we either enable or disable it based on the current
    // state
    if (modified)
        save->setEnabled(true);
    else
        save->setEnabled(false);

    // in any event, we want our parent to do it's thing
    ReadWritePart::setModified(modified);
*/
}

void KDiff3Part::getNameAndVersion(const QString& str, const QString& lineStart, QString& fileName, QString& version)
{
    if(str.left(lineStart.length()) == lineStart && fileName.isEmpty())
    {
        int pos = lineStart.length();

        while(pos < str.length() && (str[pos] == ' ' || str[pos] == '\t')) ++pos;
        int pos2 = str.length() - 1;
        while(pos2 > pos)
        {
            while(pos2 > pos && str[pos2] != ' ' && str[pos2] != '\t') --pos2;
            fileName = str.mid(pos, pos2 - pos);
            qCDebug(kdiffMain) << "KDiff3Part::getNameAndVersion: fileName = " << fileName << "\n";

            if(FileAccess(fileName).exists()) break;
            --pos2;
        }

        int vpos = str.lastIndexOf("\t", -1);
        if(vpos > 0 && vpos > pos2)
        {
            version = str.mid(vpos + 1);
            while(!version.right(1)[0].isLetterOrNumber())
                version.truncate(version.length() - 1);
        }
    }
}

bool KDiff3Part::openFile()
{
    return false;
}

bool KDiff3Part::saveFile()
{
    /*    // if we aren't read-write, return immediately
    if (isReadWrite() == false)
        return false;

    // localFilePath() is always local, so we use QFile
    QFile file(localFilePath());
    if (file.open(IO_WriteOnly) == false)
        return false;

    // use QTextStream to dump the text to the file
    QTextStream stream(&file);
    //stream << m_widget->text();

    file.close();
    return true;
*/
    return false; // Not implemented
}

#include "kdiff3_part.moc"
