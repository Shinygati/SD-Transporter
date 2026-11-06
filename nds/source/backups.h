/* Recovery history management. GPL-3.0-or-later. */
#ifndef WALKER_BACKUPS_H
#define WALKER_BACKUPS_H
int backupsScan(const char *walker);
int backupsCount(void);
int backupsLimited(void);
int backupsDescribe(int index,char *name,unsigned nameSize,char *detail,unsigned detailSize);
int backupsDelete(int index);
int backupsPreserveActive(const char *walker,const unsigned char *data);
int backupsInterruptedCount(const char *walker);
int backupsInterruptedDescribe(const char *walker,int index,char *name,unsigned size,char *detail,unsigned detailSize);
int backupsRestoreInterrupted(const char *walker,int index);
int backupsLoad(int index,unsigned char *data);
int backupsRestoreImage(const char *walker,const unsigned char *data,int (*replace)(const char *,const char *));
#endif
