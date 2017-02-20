#include "ngx_core.h"
#include "zlib.h"
#include "ngx_http_memc_purge_memop.h"


int memcache_connect_test(char *ip, char *port, ngx_conf_t *cf) 
{
    memcached_st *memc = NULL;
    int r = memcache_connect(&memc, ip, port);

    if(r == 0) {
        return 0;
    }
    
    memcached_free(memc);
    memc = NULL;

    return 1;
}


int memcache_connect(memcached_st **memc, char *ip, char *port) 
{
    int memc_port = atoi(port);
    memcached_return rc;
    memcached_server_st *server;

    *memc = memcached_create(NULL);
    server = memcached_server_list_append(NULL, ip, memc_port, &rc);

    if(rc != MEMCACHED_SUCCESS) {
        ngx_log_stderr(0, "Connect to memcache(ip: %s , port: %s) failed!",
                          ip, port);
        memcached_server_free(server);
        return 0;
    };

    rc = memcached_server_push(*memc, server);
    memcached_server_list_free(server);
    
    char key[] = "ngx_http_memc_purge_conntest", val[] = "success";
    int klen = strlen(key), vlen = strlen(val);

    rc = memcached_set(*memc, key, klen, val, vlen, (time_t)5, (uint32_t)0);

    if(rc == MEMCACHED_SUCCESS) {
        return 1;
    } else {
        ngx_log_stderr(0, "Connect to memcache(ip: %s , port: %s) failed!",
                          ip, port);
        memcached_free(*memc);
        *memc = NULL;
        return 0;
    }
}

ngx_http_memc_purge_memresult *
memcache_get_key(memcached_st *memc, char *pfx, char *key) 
{
    uint32_t flags = 2;
    memcached_return rc;
    char *cmd;
    int l = strlen(pfx) + strlen(key), vlen;

    cmd = (char *) calloc(sizeof(char), (l+5));
    snprintf(cmd, l+1, "%s%s", pfx, key);

    ngx_http_memc_purge_memresult *result = calloc(sizeof(ngx_http_memc_purge_memresult), 1);
    result->val = memcached_get(memc, cmd, l+1, (size_t *) &vlen, &flags, &rc);
    result->len = vlen;

    free(cmd);
    cmd = NULL;

    if (rc != MEMCACHED_SUCCESS) {
        free(result);
        result = NULL;

        return NULL;
    }
    
    if (result->val == NULL
        || result->len == 0) 
    {
        free(result);
        result = NULL;

        return NULL;
    }

    if (flags >= 1) {
        unsigned long tmpl = 5 * result->len;
        char *tmp  = calloc(sizeof(char), tmpl);
        int des = uncompress((Bytef *)tmp, &tmpl, (Bytef *)result->val, result->len);
        
        if(des != Z_OK) {
            free(tmp);
            tmp = NULL;

            free(result->val);
            free(result);
            result = NULL;

            return NULL;
        }
        
        free(result->val);
        result->len = (int) tmpl;

        result->val = calloc(sizeof(char), tmpl);
        strncpy(result->val, tmp, tmpl);

        free(tmp);
        tmp = NULL;
    }
    
    return result;
}
