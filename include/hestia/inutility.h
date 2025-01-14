#ifndef __INUTILITY_H__
#define __INUTILITY_H__

#include <unistd.h>

char** split_h(const char* str);

char* load_file(const char* fname, int exists);
int vercmp(const char *a, const char *b);
char* path_home_from_uid(unsigned uid);
char* path_home(char* path);
char* path_explode(const char* path);
int dir_exists(const char* path);
void mk_dir(const char* path, unsigned privilege);
void rm(const char* path);

void colorfg_set(unsigned color);
void colorbg_set(unsigned color);
void bold_set(void);
void print_repeats(unsigned count, const char* ch);
void print_repeat(unsigned count, const char ch);
int shell(const char* exec);
int readline_yesno(void);

#endif
