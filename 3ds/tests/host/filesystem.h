#include <sys/stat.h>
#include <direct.h>
#include <io.h>
#define lstat stat
#define S_ISREG(mode) (((mode)&_S_IFMT)==_S_IFREG)
#define mkdir(path,mode) _mkdir(path)
#define fsync _commit
