#include "libmemcached/memcached.h"

typedef struct {
    char *val;
    int len;
} ngx_http_memc_purge_memresult;

int memcache_connect(memcached_st **memc, char *ip, char *port);

ngx_http_memc_purge_memresult *
memcache_get_key(memcached_st *memc, char *pfx, char *key);

int memcache_connect_test(char *ip, char *port, ngx_conf_t *cf);
