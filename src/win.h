#ifndef WEBQQ_WIN_H_H
#define WEBQQ_WIN_H_H
#ifdef WIN32

char *  strsep(char **stringp, const char* delim);
const char *wpurple_locale_dir(void);


#undef  LOCALEDIR
#define LOCALEDIR wpurple_locale_dir()

#undef GLOBAL_DATADIR
#define GLOBAL_DATADIR global_data_dir()

#endif
#endif