/***************************************************************************
                          main.cpp  -  description
                             -------------------
    begin                : Don Jul 11 12:31:29 CEST 2002
    copyright            : (C) 2002 by Joachim Eibl
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

/***************************************************************************
 * $Log$
 * Revision 1.1  2002/08/18 16:24:16  joachim99
 * Initial revision
 *                                                                   *
 ***************************************************************************/

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>

#include "kdiff3.h"

static const char *description =
   I18N_NOOP("Text Diff and Merge Tool");

static KCmdLineOptions options[] =
{
  { "m", 0, 0 },
  { "merge", I18N_NOOP("Automatically merge the input."), 0 },
  { "o", 0, 0 },
  { "output file", I18N_NOOP("Output file. Implies -m. E.g.: -o newfile.txt"), 0 },
  { "+[File1]", I18N_NOOP("file1 to open (base)"), 0 },
  { "+[File2]", I18N_NOOP("file2 to open"), 0 },
  { "+[File3]", I18N_NOOP("file3 to open"), 0 },
  { 0, 0, 0 }
  // INSERT YOUR COMMANDLINE OPTIONS HERE
};

int main(int argc, char *argv[])
{

   KAboutData aboutData( "kdiff3", I18N_NOOP("KDiff3"),
      VERSION, description, KAboutData::License_GPL,
      "(c) 2002 Joachim Eibl", 0, 0, "joachim.eibl@gmx.de");
   aboutData.addAuthor("Joachim Eibl",0, "joachim.eibl@gmx.de");
   KCmdLineArgs::init( argc, argv, &aboutData );
   KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

  KApplication app;
 
  if (app.isRestored())
  {
    RESTORE(KDiff3App);
  }
  else 
  {
    KDiff3App *kdiff3 = new KDiff3App();
    kdiff3->show();

   /* With the following lines when the application starts a small window
      flashes up and disappears so fast that one can't see what is happening.
      But I think, I don't need these lines anyway: */

   /*   KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

      if (args->count())
      {
        kdiff3->openDocumentFile(args->arg(0));
      }
      else
      {
        kdiff3->openDocumentFile();
      }
      args->clear();*/
  }

  return app.exec();
}  
