#include <stdio.h>
#include "version.h"
main(int argv,char** argc)
{
  if (argv<=1)
  {
    printf("%s\n",VERSION);
  }
  else if (strcmp(argc[1],"dos")==0)
  {
    printf("set MUI_VERSION=\"%s\"\n",VERSION);
  }
  else if (strcmp(argc[1],"nsis")==0)
  {
    printf("!define MUI_VERSION \"%s\"\n",VERSION);
  }
  else if (strcmp(argc[1],"unix")==0)
  {
    printf("#define MUI_VERSION=\"%s\"\n",VERSION);
  }
}
