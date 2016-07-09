/***************************************************************************
 * Copyright (C) 2003-2007 Joachim Eibl <joachim.eibl at gmx.de>           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.           *
 ***************************************************************************/

#include "kdiff3_part.h"

#include <kcomponentdata.h>
#include <kaction.h>
#include <kstandardaction.h>
#include <kfiledialog.h>

#include <QFile>
#include <QTextStream>
#include <QProcess>

#include "kdiff3.h"
#include "fileaccess.h"

#include <kmessagebox.h>
#include <klocale.h>

#include "version.h"

KDiff3Part::KDiff3Part( QWidget *parentWidget, const char *widgetName,
                                  QObject *parent )
    : KParts::ReadWritePart(parent)
{
    // we need an instance
    setComponentData( *KDiff3PartFactory::instance() );

    // this should be your custom internal widget
    m_widget = new KDiff3App( parentWidget, widgetName, this );
    
    // This hack is necessary to avoid a crash when the program terminates.
    m_bIsShell = qobject_cast<KParts::MainWindow*>(parentWidget)!=0;

    // notify the part that this is our internal widget
    setWidget(m_widget);

    // create our actions
    //KStandardAction::open(this, SLOT(fileOpen()), actionCollection());
    //KStandardAction::saveAs(this, SLOT(fileSaveAs()), actionCollection());
    //KStandardAction::save(this, SLOT(save()), actionCollection());

    setXMLFile("kdiff3_part.rc");

    // we are read-write by default
    setReadWrite(true);

    // we are not modified since we haven't done anything yet
    setModified(false);
}

KDiff3Part::~KDiff3Part()
{
   if ( m_widget!=0  && ! m_bIsShell )
   {
      m_widget->saveOptions( m_widget->isPart() ? componentData().config() : KGlobal::config() );
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
    KAction *save = actionCollection()->action(KStandardAction::stdName(KStandardAction::Save));
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

static void getNameAndVersion( const QString& str, const QString& lineStart, QString& fileName, QString& version )
{
   if ( str.left( lineStart.length() )==lineStart && fileName.isEmpty() )
   {
      int pos = lineStart.length();
      while ( pos<str.length() && (str[pos]==' ' || str[pos]=='\t') ) ++pos;
      int pos2 = str.length()-1;
      while ( pos2>pos )
      { 
         while (pos2>pos && str[pos2]!=' ' && str[pos2]!='\t') --pos2;
         fileName = str.mid( pos, pos2-pos );
         fprintf(stderr, "KDiff3: %s\n", fileName.toLatin1().constData());
         if ( FileAccess(fileName).exists() ) break;
         --pos2;
      }
      
      int vpos = str.lastIndexOf("\t", -1);
      if ( vpos>0 && vpos>(int)pos2 )
      {
         version = str.mid( vpos+1 );
         while( !version.right(1)[0].isLetterOrNumber() )
            version.truncate( version.length()-1 );
      }
   }
}


bool KDiff3Part::openFile()
{
   // m_file is always local so we can use QFile on it
   fprintf(stderr, "KDiff3: %s\n", localFilePath().toLatin1().constData());
   QFile file(localFilePath());
   if (file.open(QIODevice::ReadOnly) == false)
      return false;

   // our example widget is text-based, so we use QTextStream instead
   // of a raw QDataStream
   QTextStream stream(&file);
   QString str;
   QString fileName1;
   QString fileName2;
   QString version1;
   QString version2;
   while (!stream.atEnd() && (fileName1.isEmpty() || fileName2.isEmpty()) )
   {
      str = stream.readLine() + "\n";
      getNameAndVersion( str, "---", fileName1, version1 );
      getNameAndVersion( str, "+++", fileName2, version2 );
   }

   file.close();

   if ( fileName1.isEmpty() && fileName2.isEmpty() )
   {
      KMessageBox::sorry(m_widget, i18n("Couldn't find files for comparison."));
      return false;
   }

   FileAccess f1(fileName1);
   FileAccess f2(fileName2);

   if ( f1.exists() && f2.exists() && fileName1!=fileName2 )
   {
      m_widget->slotFileOpen2( fileName1, fileName2, "", "", "", "", "", 0 );
      return true;
   }
   else if ( version1.isEmpty() && f1.exists() )
   {
      // Normal patch
      // patch -f -u --ignore-whitespace -i [inputfile] -o [outfile] [patchfile]
      QString tempFileName = FileAccess::tempFileName();
      QString cmd = "patch -f -u --ignore-whitespace -i \"" + localFilePath() +
                  "\" -o \""+tempFileName + "\" \"" + fileName1+ "\"";

      QProcess process;
      process.start( cmd );
      process.waitForFinished(-1);

      m_widget->slotFileOpen2( fileName1, tempFileName, "", "",
                               "", version2.isEmpty() ? fileName2 : "REV:"+version2+":"+fileName2, "", 0 ); // alias names
//    std::cerr << "KDiff3: f1:" << fileName1.toLatin1() <<"<->"<<tempFileName.toLatin1()<< std::endl;
      FileAccess::removeTempFile( tempFileName );
   }
   else if ( version2.isEmpty() && f2.exists() )
   {
      // Reverse patch
      // patch -f -u -R --ignore-whitespace -i [inputfile] -o [outfile] [patchfile]
      QString tempFileName = FileAccess::tempFileName();
      QString cmd = "patch -f -u -R --ignore-whitespace -i \"" + localFilePath() +
                  "\" -o \""+tempFileName + "\" \"" + fileName2+"\"";

      QProcess process;
      process.start( cmd );
      process.waitForFinished(-1);

      m_widget->slotFileOpen2( tempFileName, fileName2, "", "",
                               version1.isEmpty() ? fileName1 : "REV:"+version1+":"+fileName1, "", "", 0 ); // alias name
//    std::cerr << "KDiff3: f2:" << fileName2.toLatin1() <<"<->"<<tempFileName.toLatin1()<< std::endl;
      FileAccess::removeTempFile( tempFileName );
   }
   else if ( !version1.isEmpty() && !version2.isEmpty() )
   {
      fprintf(stderr, "KDiff3: f1/2:%s<->%s\n", fileName1.toLatin1().constData(), fileName2.toLatin1().constData());
      // Assuming that files are on CVS: Try to get them
      // cvs update -p -r [REV] [FILE] > [OUTPUTFILE]

      QString tempFileName1 = FileAccess::tempFileName();
      QString cmd1 = "cvs update -p -r " + version1 + " \"" + fileName1 + "\" >\""+tempFileName1+"\"";
      QProcess process1;
      process1.start( cmd1 );
      process1.waitForFinished(-1);

      QString tempFileName2 = FileAccess::tempFileName();
      QString cmd2 = "cvs update -p -r " + version2 + " \"" + fileName2 + "\" >\""+tempFileName2+"\"";
      QProcess process2;
      process2.start( cmd2 );
      process2.waitForFinished(-1);

      m_widget->slotFileOpen2( tempFileName1, tempFileName2, "", "",
         "REV:"+version1+":"+fileName1,
         "REV:"+version2+":"+fileName2,
         "", 0
      );

//    std::cerr << "KDiff3: f1/2:" << tempFileName1.toLatin1() <<"<->"<<tempFileName2.toLatin1()<< std::endl;
      FileAccess::removeTempFile( tempFileName1 );
      FileAccess::removeTempFile( tempFileName2 );
      return true;
   }
   else
   {
      KMessageBox::sorry(m_widget, i18n("Couldn't find files for comparison."));
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
    return false;  // Not implemented
}


// It's usually safe to leave the factory code alone.. with the
// notable exception of the KAboutData data
#include <kaboutdata.h>
#include <klocale.h>
#include <kglobal.h>

KComponentData*  KDiff3PartFactory::s_instance = 0L;
KAboutData* KDiff3PartFactory::s_about = 0L;

KDiff3PartFactory::KDiff3PartFactory()
    : KParts::Factory()
{
}

KDiff3PartFactory::~KDiff3PartFactory()
{
    delete s_instance;
    delete s_about;

    s_instance = 0L;
}

KParts::Part* KDiff3PartFactory::createPartObject( QWidget *parentWidget,
                                                        QObject *parent,
                                                        const char *classname, const QStringList&/*args*/ )
{
    // Create an instance of our Part
    KDiff3Part* obj = new KDiff3Part( parentWidget, 0, parent );

    // See if we are to be read-write or not
    if (QString(classname) == "KParts::ReadOnlyPart")
        obj->setReadWrite(false);

    return obj;
}

KComponentData* KDiff3PartFactory::instance()
{
    if( !s_instance )
    {
        s_about = new KAboutData(QByteArray("kdiff3part"), QByteArray("kdiff3part"), ki18n("KDiff3Part"), QByteArray(VERSION));
        s_about->addAuthor(ki18n("Joachim Eibl"), KLocalizedString(), QByteArray("joachim.eibl at gmx.de"));
        s_instance = new KComponentData(s_about);
    }
    return s_instance;
}

extern "C"
{
    void* init_libkdiff3part()
    {
        return new KDiff3PartFactory;
    }
}

// Suppress warning with --enable-final
#undef VERSION

//#include "kdiff3_part.moc"
