/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

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

//A bit of a hack but I don't have control over the constructor for KDiff3Part since its a KPart plugin.
bool KDiff3Part::bNeedInit = true;

KAboutData KDiff3Part::createAboutData()
{
    QString appVersion = QString(KDIFF3_VERSION_STRING);
    if(sizeof(void*) == 8)
        appVersion += " (64 bit)";
    else if(sizeof(void*) == 4)
        appVersion += " (32 bit)";

    KAboutData aboutData(u8"kdiff3part", i18n("KDiff3 Part"),
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
    assert(parentWidget);
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

    if(bNeedInit)
        m_widget->completeInit();
}

KDiff3Part::~KDiff3Part()
{
    //TODO: Is parent check needed?
    if(m_widget != nullptr && qobject_cast<KParts::MainWindow*>(parent()) != nullptr)
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
    if(str.startsWith(lineStart) && fileName.isEmpty())
    {
        //Skip the start string
        qint32 pos = lineStart.length();
        //Skip white space if any after start string.
        while(pos < str.length() && (str[pos] == ' ' || str[pos] == '\t')) ++pos;
        qint32 pos2 = str.length() - 1;
        while(pos2 > pos)
        {
            //skip trailing whitespace
            while(pos2 > pos && str[pos2] != ' ' && str[pos2] != '\t') --pos2;
            fileName = str.mid(pos, pos2 - pos);
            qCDebug(kdiffMain) << "KDiff3Part::getNameAndVersion: fileName = " << fileName;
            //Always fails for cvs output this is a designed failure.
            if(FileAccess(fileName).exists()) break;
            --pos2;
        }

        qint32 vpos = str.lastIndexOf("\t", -1);
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
    // m_file is always local so we can use QFile on it
    //fprintf(stderr, "KDiff3: %s\n", localFilePath().toLatin1().constData());
    qCDebug(kdiffMain) << "KDiff3Part::openFile(): localFilePath() == " << localFilePath();
    QFile file(localFilePath());
    if(!file.open(QIODevice::ReadOnly))
        return false;

    //get version and name
    QTextStream stream(&file);
    QString str;
    QString fileName1;
    QString fileName2;
    QString version1;
    QString version2;
    QStringList errors;

    /*
        This assumes use of -u otherwise the patch file may not be recognized.
        Also assumes cvs or diff was used to gernerate the path.
    */
    while(!stream.atEnd() && (fileName1.isEmpty() || fileName2.isEmpty()))
    {
        str = stream.readLine() + '\n';
        getNameAndVersion(str, "---", fileName1, version1);
        getNameAndVersion(str, "+++", fileName2, version2);
    }

    file.close();

    if(fileName1.isEmpty() && fileName2.isEmpty())
    {
        KMessageBox::error(m_widget, i18n("Could not find files for comparison."));
        return false;
    }

    const FileAccess f1(fileName1);
    const FileAccess f2(fileName2);

    if(f1.exists() && f2.exists() && fileName1 != fileName2)
    {
        m_widget->slotFileOpen2(errors, fileName1, fileName2, "", "", "", "", "", nullptr);
        return true;
    }
    else if(version1.isEmpty() && f1.exists())
    {
        // Normal patch
        // patch -f -u --ignore-whitespace -i [inputfile] -o [outfile] [patchfile]
        QTemporaryFile tmpFile;
        FileAccess::createTempFile(tmpFile);
        const QString tempFileName = tmpFile.fileName();
        const QString cmd = "patch";
        QStringList args = {"-f", "-u", "--ignore-whitespace", "-i", '"' + localFilePath() + '"',
                    "-o", '"' + tempFileName + '"', '"' + fileName1 + '"'};

        QProcess process;
        process.start(cmd, args);
        process.waitForFinished(-1);

        m_widget->slotFileOpen2(errors, fileName1, tempFileName, "", "",
                                "", version2.isEmpty() ? fileName2 : "REV:" + version2 + ':' + fileName2, "", nullptr); // alias names                                                                                              //    std::cerr << "KDiff3: f1:" << fileName1.toLatin1() <<"<->"<<tempFileName.toLatin1()<< std::endl;
    }
    else if(version2.isEmpty() && f2.exists())
    {
        // Reverse patch
        // patch -f -u -R --ignore-whitespace -i [inputfile] -o [outfile] [patchfile]
        QTemporaryFile tmpFile;
        FileAccess::createTempFile(tmpFile);
        const QString tempFileName = tmpFile.fileName();
        const QString cmd = "patch";
        const QStringList args = {"-f", "-u", "-R", "--ignore-whitespace", "-i", '"' + localFilePath() + '"',
                      "-o", '"' + tempFileName + '"', '"' + fileName2 + '"'};

        QProcess process;
        process.start(cmd, args);
        process.waitForFinished(-1);

        m_widget->slotFileOpen2(errors, tempFileName, fileName2, "", "",
                                version1.isEmpty() ? fileName1 : "REV:" + version1 + ':' + fileName1, "", "", nullptr); // alias name
    }
    else if(!version1.isEmpty() && !version2.isEmpty())
    {
        qCDebug(kdiffMain) << "KDiff3Part::openFile():" << fileName1 << "<->" << fileName2 << "\n";
        // FIXME: Why must this be cvs?
        // Assuming that files are on CVS: Try to get them
        // cvs update -p -r [REV] [FILE] > [OUTPUTFILE]

        QTemporaryFile tmpFile1;
        FileAccess::createTempFile(tmpFile1);
        const QString tempFileName1 = tmpFile1.fileName();
        const QString cmd = "cvs";
        QStringList args = {"update", "-p", "-r", version1, '"' + fileName1 + '"'};
        QProcess process1;

        process1.setStandardOutputFile(tempFileName1);
        process1.start(cmd, args);
        process1.waitForFinished(-1);

        QTemporaryFile tmpFile2;
        FileAccess::createTempFile(tmpFile2);
        QString tempFileName2 = tmpFile2.fileName();

        args = QStringList{"update", "-p", "-r", version2, '"' + fileName2 + '"'};
        QProcess process2;
        process2.setStandardOutputFile(tempFileName2);
        process2.start(cmd, args);
        process2.waitForFinished(-1);

        m_widget->slotFileOpen2(errors, tempFileName1, tempFileName2, "", "",
                                "REV:" + version1 + ':' + fileName1,
                                "REV:" + version2 + ':' + fileName2,
                                "", nullptr);

        qCDebug(kdiffMain) << "KDiff3Part::openFile():" << tempFileName1 << "<->" << tempFileName2 << "\n";
        return true;
    }
    else
    {
        KMessageBox::error(m_widget, i18n("Could not find files for comparison."));
    }

    return true;
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
