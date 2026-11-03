/* Minimal Windows host-test adapter. Production uses the platform dirent API. */
#ifndef TEST_DIRENT_H
#define TEST_DIRENT_H
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
struct dirent { char d_name[256]; };
typedef struct { HANDLE handle; WIN32_FIND_DATAA data; int first; struct dirent item; } DIR;
static DIR *opendir(const char *path) {
    char pattern[1000];snprintf(pattern,sizeof(pattern),"%s/*",path);
    DIR *dir=(DIR*)malloc(sizeof(*dir));if(!dir)return 0;
    dir->handle=FindFirstFileA(pattern,&dir->data);dir->first=1;
    if(dir->handle==INVALID_HANDLE_VALUE){free(dir);return 0;}return dir;
}
static struct dirent *readdir(DIR *dir) {
    if(!dir->first && !FindNextFileA(dir->handle,&dir->data))return 0;
    dir->first=0;snprintf(dir->item.d_name,sizeof(dir->item.d_name),"%s",dir->data.cFileName);return &dir->item;
}
static int closedir(DIR *dir){FindClose(dir->handle);free(dir);return 0;}
#endif
