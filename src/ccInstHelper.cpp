// uninstallHelper.cpp : Defines the entry point for the console application.
//
#include <string>
#include <vector>
#include <list>
#include <windows.h>
#include <string.h>
#include <io.h>

//#define __stdcall

#ifndef KREPLACEMENTS_H
// For compilation download the NSIS source package and modify the following
// line to point to the exdll.h-file
#include "C:/Programme/NSIS/Contrib/ExDll/exdll.h"
#endif

struct ReplacementItem
{ const char* fileType; const char* operationType; };

ReplacementItem g_replacementTable[] = {
   {"text_file_delta",   "xcompare"},
   {"text_file_delta",   "xmerge"},
   {"whole_copy",        "xcompare"},
   {"whole_copy",        "xmerge"},
   {"z_text_file_delta", "xcompare"},
   {"z_text_file_delta", "xmerge"},
   {"z_whole_copy",      "xcompare"},
   {"z_whole_copy",      "xmerge"},
   {"_xml",              "xcompare"},
   {"_xml",              "xmerge"},
   {"_xml2",             "xcompare"},
   {"_xml2",             "xmerge"},
   {"_rftdef",           "xcompare"},
   {"_rftmap",           "xcompare"},
   {"_rftvp",            "xcompare"},
   {"_xtools",           "xcompare"},
   {0,0}
};

struct LineItem
{
   std::string fileType;
   std::string opType;
   std::string command;
   std::string fileOpPart;
};

// Return true if successful, else false
bool readAndParseMapFile( const std::string& filename, std::list<LineItem>& lineItemList )
{
   // Read file
   FILE* pFile = fopen( filename.c_str(), "r" );
   if (pFile)
   {
      fseek(pFile,0,SEEK_END);
      int size = ftell(pFile);
      fseek(pFile,0,SEEK_SET);
      std::vector<char> buf( size );
      fread( &buf[0], 1, size, pFile );
      fclose( pFile );

      // Replace strings
      int lineStartPos=0;
      int wordInLine = 0;
      LineItem lineItem;
      for( int i=0; i<size; )
      {
         if( buf[i] == '\n' || buf[i] == '\r' )
         {
            ++i;
            wordInLine = 0;
            lineStartPos = i;
            continue;
         }
         if( buf[i] == ' ' || buf[i] == '\t' )
         {
            ++i;
            continue;
         }
         else
         {
            int wordStartPos = i;
            if (wordInLine<2)
            {
               while ( i<size && !( buf[i] == ' ' || buf[i] == '\t' ) )
                  ++i;

               std::string word( &buf[wordStartPos], i-wordStartPos );
               if (wordInLine==0)
                  lineItem.fileType = word;
               else
                  lineItem.opType = word;
               ++wordInLine;
            }
            else
            {
               lineItem.fileOpPart = std::string( &buf[lineStartPos], i-lineStartPos );
               while ( i<size && !( buf[i] == '\n' || buf[i] == '\r' ) )
                  ++i;

               std::string word( &buf[wordStartPos], i-wordStartPos );
               lineItem.command = word;
               lineItemList.push_back( lineItem );
            }
         }
      }
   }
   else
   {
      return false;
   }
   return true;
}

bool writeMapFile( const std::string& filename, const std::list<LineItem>& lineItemList )
{
   FILE* pFile = fopen( filename.c_str(), "w" );
   if (pFile)
   {
      std::list<LineItem>::const_iterator i = lineItemList.begin();
      for( ; i!=lineItemList.end(); ++i )
      {
         const LineItem& li = *i;
         fprintf( pFile, "%s%s\n", li.fileOpPart.c_str(), li.command.c_str() );
      }
      fclose( pFile );
   }
   else
   {
      return false;
   }
   return true;
}

std::string toUpper( const std::string& s )
{
   std::string s2 = s;

   for( unsigned int i=0; i<s.length(); ++i )
   {
      s2[i] = toupper( s2[i] );
   }
   return s2;
}

int integrateWithClearCase( const char* subCommand, const char* kdiff3CommandPath )
{
   std::string installCommand = subCommand; // "install" or "uninstall" or "existsClearCase"
   std::string kdiff3Command  = kdiff3CommandPath;

   /*
   std::wstring installCommand = subCommand; // "install" or "uninstall"
   std::wstring wKDiff3Command  = kdiff3CommandPath;
   std::string kdiff3Command;
   kdiff3Command.reserve( wKDiff3Command.length()+1 );
   kdiff3Command.resize( wKDiff3Command.length() );
   BOOL bUsedDefaultChar = FALSE;
   int successLen = WideCharToMultiByte(  CP_ACP, 0, 
      wKDiff3Command.c_str(), int(wKDiff3Command.length()),
      &kdiff3Command[0], int(kdiff3Command.length()), 0, &bUsedDefaultChar );

   if ( successLen != kdiff3Command.length() || bUsedDefaultChar )
   {
      std::cerr << "KDiff3 command contains characters that don't map to ansi code page.\n"
          "Aborting clearcase installation.\n"
          "Try to install KDiff3 in another path that doesn't require special characters.\n";
      return -1;
   }
   */

   // Try to locate cleartool, the clearcase tool in the path
   char buffer[1000];
   char* pLastPart = 0;
   int len = SearchPathA(0, "cleartool.exe", 0, sizeof(buffer)/sizeof(buffer[0]), 
                         buffer, &pLastPart );
   if ( len>0 && len+1<int(sizeof(buffer)/sizeof(buffer[0])) && pLastPart )
   {
      pLastPart[-1] = 0;
      pLastPart = strrchr( buffer, '\\' ); // cd up (because cleartool.exe is in bin subdir)
      if ( pLastPart )
         pLastPart[1]=0;

      std::string path( buffer );
      path += "lib\\mgrs\\map";
      std::string bakName = path + ".preKDiff3Install";
      
      if ( installCommand == "existsClearCase") 
      {
         return 1;
      }
      else if ( installCommand == "install") 
      {
         std::list<LineItem> lineItemList;
         bool bSuccess = readAndParseMapFile( path, lineItemList );
         if ( !bSuccess )
         {
            fprintf(stderr, "Error reading original map file.\n");
            return -1;
         }

         // Create backup
         if ( access( bakName.c_str(), 0 )!=0 ) // Create backup only if not exists yet
         {
            if ( rename( path.c_str(), bakName.c_str() ) )
            {
               fprintf(stderr, "Error renaming original map file.\n");
               return -1;
            }
         }

         std::list<LineItem>::iterator i = lineItemList.begin();
         for( ; i!=lineItemList.end(); ++i )
         {
            LineItem& li = *i;
            for (int j=0;;++j)
            {
               ReplacementItem& ri = g_replacementTable[j];
               if ( ri.fileType==0 || ri.operationType==0 )
                  break;
               if ( li.fileType == ri.fileType && li.opType == ri.operationType )
               {
                  li.command = kdiff3Command.c_str();
                  break;
               }
            }
         }

         bSuccess = writeMapFile( path, lineItemList );
         if ( !bSuccess )
         {
            if ( rename( bakName.c_str(), path.c_str() ) )
               fprintf(stderr, "Error writing new map file, restoring old file also failed.\n");
            else
               fprintf(stderr, "Error writing new map file, old file restored.\n");

            return -1;
         }
      }
      else if ( installCommand == "uninstall" )
      {
         std::list<LineItem> lineItemList;
         bool bSuccess = readAndParseMapFile( path, lineItemList );
         if ( !bSuccess )
         {
            fprintf(stderr, "Error reading original map file\n.");
            return -1;
         }

         std::list<LineItem> lineItemListBak;
         bSuccess = readAndParseMapFile( bakName, lineItemListBak );
         if ( !bSuccess )
         {
            fprintf(stderr, "Error reading backup map file.\n");
            return -1;
         }

         std::list<LineItem>::iterator i = lineItemList.begin();
         for( ; i!=lineItemList.end(); ++i )
         {
            LineItem& li = *i;
            if ((int)toUpper(li.command).find("KDIFF3")>=0)
            {
               std::list<LineItem>::const_iterator j = lineItemListBak.begin();
               for (;j!=lineItemListBak.end();++j)
               {
                  const LineItem& bi = *j;  // backup iterator
                  if ( li.fileType == bi.fileType && li.opType == bi.opType )
                  {
                     li.command = bi.command;
                     break;
                  }
               }
            }
         }

         bSuccess = writeMapFile( path, lineItemList );
         if ( !bSuccess )
         {
            fprintf(stderr, "Error writing map file.");
            return -1;
         }
      }
   }
   return 0;
}

#ifndef KREPLACEMENTS_H

extern "C"
void __declspec(dllexport) nsisPlugin(HWND hwndParent, int string_size, 
                                      char *variables, stack_t **stacktop,
                                      extra_parameters *extra)
{
   //g_hwndParent=hwndParent;

   EXDLL_INIT();
   {
      std::string param1( g_stringsize, ' ' );
      int retVal = popstring( &param1[0] );
      if ( retVal == 0 )
      {
         std::string param2( g_stringsize, ' ' );
         retVal = popstring( &param2[0] );
         if ( retVal == 0 )
            install( param1.c_str(), param2.c_str() );
         return;
      }
      fprintf(stderr, "Not enough parameters.\n");
   }
}

#endif
/*
int _tmain(int argc, _TCHAR* argv[])
{
   if ( argc<3 ) 
   {
      std::cout << "This program is needed to install/uninstall KDiff3 for clearcase.\n"
                   "It tries to patch the map file (clearcase-subdir\\lib\\mgrs\\map)\n"
                   "Usage 1: ccInstHelper install pathToKdiff3.exe\n"
                   "Usage 2: ccInstHelper uninstall pathToKdiff3.exe\n"
                   "Backups of the original map files are created in the dir of the map file.\n";
   }
   else
   {
      return install( argv[1], argv[2] );
   }
   
   return 0;
}
*/
