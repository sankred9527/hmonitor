#include "hm_dumphttp.h"


FILE * hmdump_init(int core_id)
{
    char fn[32];
    snprintf(fn,32,"httplog_%d.log", core_id);
    FILE *fp = fopen(fn, "w+");

    return fp;
}

void hmdump_append_line(FILE* fp,char *data, size_t len)
{
    fwrite(data, len, 1, fp);
    fwrite("\n", 1, 1, fp);
}