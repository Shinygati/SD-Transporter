/* Verified HGSS special-course unlocks. GPL-3.0-or-later. */
#ifndef HGSS_COURSES_H
#define HGSS_COURSES_H
#include <stdint.h>
int hgssUnlockCourses(uint8_t *save,unsigned bytes,uint32_t *added);
int hgssUnlockCourseFile(const char *walker,int (*replace)(const char *,const char *),char *,unsigned);
#endif
