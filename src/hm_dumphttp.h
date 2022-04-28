#ifndef _HM_DUMPHTTP_H
#define _HM_DUMPHTTP_H

#include <stdio.h>

FILE * hmdump_init(int core_id);
void hmdump_append_line(FILE* fp,char *data, size_t len);

#endif