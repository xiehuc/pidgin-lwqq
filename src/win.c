#ifdef WIN32
#include <stdio.h>
#include <stdlib.h>
#include <lwqq.h>

const char* wpurple_install_dir(void);

char* strsep(char** stringp, const char* delim)
{
   char* s;
   const char* spanp;
   int c, sc;
   char* tok;

   if ((s = *stringp) == NULL)
      return (NULL);
   for (tok = s;;) {
      c = *s++;
      spanp = delim;
      do {
         if ((sc = *spanp++) == c) {
            if (c == 0)
               s = NULL;
            else
               s[-1] = 0;
            *stringp = s;
            return (tok);
         }
      } while (sc != 0);
   }
   /* NOTREACHED */
}

const char* global_data_dir()
{
   static char dir[512];
   snprintf(dir, sizeof(dir), "%s" LWQQ_PATH_SEP "share",
            (char*)wpurple_install_dir());
   return dir;
}

#endif

// vim: tabstop=3 sw=3 sts=3 noexpandtab
