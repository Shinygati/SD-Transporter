#include <nds/ndstypes.h>
typedef struct { unsigned cluster; } aFile;
void getFileFromCluster(aFile*,unsigned,bool);
