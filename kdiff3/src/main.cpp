/***************************************************************************
                          main.cpp  -  Where everything starts.
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
 * Revision 1.2  2003/10/07 08:41:06  friseb123
 * Placing the version into a separate include: version.h
 * version.c extract the version number
 *
 * Revision 1.1  2003/10/06 18:38:48  joachim99
 * KDiff3 version 0.9.70
 *                                                                   *
 ***************************************************************************/

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include "version.h"
#include "kdiff3_shell.h"


static const char *description =
   I18N_NOOP("Text Diff and Merge Tool");

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
  { "fname alias",  I18N_NOOP("Visible name replacement. Supply this once for every input."), 0 },
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
      "(c) 2002-2003 Joachim Eibl", 0, "http://kdiff3.sourceforge.net/", "joachim.eibl@gmx.de");
   aboutData.addAuthor("Joachim Eibl",0, "joachim.eibl@gmx.de");
   KCmdLineArgs::init( argc, argv, &aboutData );
   KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

  KApplication app;

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
