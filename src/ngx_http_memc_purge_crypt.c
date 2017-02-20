#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "openssl/md5.h"
#include "ngx_http_memc_purge_crypt.h"

char *
ngx_http_memc_purge_md5(unsigned char *s, int len) 
{
    unsigned char md[16];
    int i;
    char tmp[3]={'\0'}, *buf = calloc(sizeof(char), 33);
    MD5(s, len, md);

    for (i = 0; i < 16; i++){
        sprintf(tmp, "%2.2x", md[i]);
        strcat(buf, tmp);
    }

    return buf;
}
