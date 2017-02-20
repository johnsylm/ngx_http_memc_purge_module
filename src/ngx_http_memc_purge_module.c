#include "nginx.h"
#include "ngx_config.h"
#include "ngx_core.h"
#include "ngx_http.h"
#include "ngx_http_memc_purge_memop.h"
#include "ngx_http_memc_purge_gp.h"
#include "ngx_http_memc_purge_crypt.h"


typedef struct {
    ngx_str_t mip; // Memcache ip, for lookup.
    ngx_str_t mpt; // Memcache port, for lookup.
    memcached_st *memc;
} ngx_http_memc_purge_memConn_t;

typedef struct {
    ngx_flag_t active;
    ngx_str_t mip; // Ip for memcache
    ngx_str_t mpt; // Port for memcache
    ngx_str_t mpx; // Prefix of key in memcache.
    ngx_http_memc_purge_memConn_t *memCon;
} ngx_http_memc_purge_loc_conf_t;

typedef struct {
    unsigned int done;
} ngx_http_memc_purge_ctx_t;


static ngx_int_t ngx_http_memc_purge_init_confs(ngx_conf_t *cf);
static ngx_int_t ngx_http_memc_purge_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_memc_purge_handler(ngx_http_request_t *r);

static void ngx_http_memc_purge_post_handler(ngx_http_request_t *r);
static void ngx_http_memc_purge_destroy(ngx_cycle_t *cycle);

static void * 
ngx_http_memc_purge_create_conf(ngx_conf_t *cf);

static char * 
ngx_http_memc_purge_merge_conf(ngx_conf_t* cf, void* parent, void* child);

static ngx_int_t ngx_http_memc_purge_init_connections(ngx_cycle_t *cycle);

static char *
ngx_http_memc_purge_collect_conn_msg(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_int_t 
ngx_http_memc_purge_init_conf_connect(ngx_http_memc_purge_loc_conf_t *conf);

static ngx_array_t *ngx_http_memc_purge_confs = NULL;
static ngx_array_t *ngx_http_memc_purge_confs_conns = NULL;


static ngx_command_t ngx_http_memc_purge_module_commands[] = {
    { ngx_string("ngx_memc_purge_active"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_memc_purge_collect_conn_msg,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },
    { ngx_string("ngx_memc_purge_memcip"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_memc_purge_loc_conf_t, mip),
      NULL },
    { ngx_string("ngx_memc_purge_memcport"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_memc_purge_loc_conf_t, mpt),
      NULL },
    { ngx_string("ngx_memc_purge_memcpre"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_memc_purge_loc_conf_t, mpx),
      NULL },
      ngx_null_command
};

static ngx_http_module_t  ngx_http_memc_purge_module_ctx = { 
    ngx_http_memc_purge_init_confs,       /* preconfiguration */
    ngx_http_memc_purge_init,             /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_memc_purge_create_conf,      /* create location configuration */
    ngx_http_memc_purge_merge_conf        /* merge location configuration */
};

ngx_module_t ngx_http_memc_purge_module = {
    NGX_MODULE_V1,
    &ngx_http_memc_purge_module_ctx,       /* module context */
    ngx_http_memc_purge_module_commands,   /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    ngx_http_memc_purge_init_connections,  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    ngx_http_memc_purge_destroy,           /* exit process */
    ngx_http_memc_purge_destroy,           /* exit master */
    NGX_MODULE_V1_PADDING
};

static void * 
ngx_http_memc_purge_create_conf(ngx_conf_t *cf) 
{
    ngx_http_memc_purge_loc_conf_t *conf;
    conf = (ngx_http_memc_purge_loc_conf_t *)ngx_pcalloc(cf->pool, sizeof(ngx_http_memc_purge_loc_conf_t));
    if(conf == NULL) return NGX_CONF_ERROR;
    memset(conf, 0, sizeof(ngx_http_memc_purge_loc_conf_t));
    conf->active = NGX_CONF_UNSET;
    return conf;
}

static char * 
ngx_http_memc_purge_merge_conf(ngx_conf_t* cf, void* parent, void* child) 
{
    ngx_http_memc_purge_loc_conf_t *prev = (ngx_http_memc_purge_loc_conf_t *) parent;
    ngx_http_memc_purge_loc_conf_t *conf = (ngx_http_memc_purge_loc_conf_t *) child;

    ngx_conf_merge_value(conf->active, prev->active, 0);
    ngx_conf_merge_str_value(conf->mip, prev->mip, "127.0.0.1");
    ngx_conf_merge_str_value(conf->mpt, prev->mpt, "11211");
    ngx_conf_merge_str_value(conf->mpx, prev->mpx, "PLAYURL_");

    return NGX_CONF_OK;
}

static char *
ngx_http_memc_purge_collect_conn_msg(ngx_conf_t *cf, 
    ngx_command_t *cmd, void *conf) 
{
    ngx_http_memc_purge_loc_conf_t *lrcf = conf;
    ngx_str_t *value;

    value = cf->args->elts;

    if(ngx_strncmp(value[1].data, "on", 2) == 0) {
        lrcf->active = 1;
        void **ptr = (void **)ngx_array_push(ngx_http_memc_purge_confs);
        if(ptr == NULL) return NGX_CONF_ERROR;
        *ptr = conf;
    } else if(ngx_strncmp(value[1].data, "off", 3) == 0) {
        lrcf->active = 0;
        return NGX_CONF_OK;
    } else {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

static ngx_int_t 
ngx_http_memc_purge_handler(ngx_http_request_t *r) 
{
    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_POST))) {
        return NGX_DECLINED;
    }

    ngx_http_memc_purge_loc_conf_t *mplcf;
    ngx_int_t rc;

    mplcf = ngx_http_get_module_loc_conf(r, ngx_http_memc_purge_module);

    if(!mplcf->active) {
        return NGX_DECLINED;
    }

    if(r->headers_in.server.data == NULL) {
        return NGX_DECLINED;
    }

    // If request method is POST
    if (r->method & NGX_HTTP_POST) {
        ngx_http_memc_purge_ctx_t *ctx = ngx_http_get_module_ctx(r, ngx_http_memc_purge_module);
        
        if(ctx != NULL) {
            if(ctx->done) {
                return NGX_DECLINED;
            }
        }

        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_memc_purge_ctx_t));
        if (ctx == NULL) {
            return NGX_ERROR;
        }

        ngx_http_set_ctx(r, ctx, ngx_http_memc_purge_module);

        if (r->headers_in.content_type == NULL
            || r->headers_in.content_type->value.data == NULL)
        {
            return NGX_DECLINED;
        }

        ngx_str_t ctype = r->headers_in.content_type->value;
        if(ctype.len < 33) return NGX_DECLINED;

        if (ngx_strncasecmp(ctype.data,
                            (u_char *)"application/x-www-form-urlencoded",
                            33) != 0)
        {
            return NGX_DECLINED;
        }
        
        rc = ngx_http_read_client_request_body(r, ngx_http_memc_purge_post_handler);

        if (rc == NGX_ERROR 
            || rc >= NGX_HTTP_SPECIAL_RESPONSE) 
        {
            r->main->count--;
            return rc;
        }
        
        if(rc == NGX_AGAIN) {
            return NGX_DONE;
        }

        return NGX_HTTP_OK;
    }

    if(r->headers_in.server.data == NULL) {
        return NGX_DECLINED;
    }

    char *region = ngx_http_memc_purge_cookie_param(r, "region");
    char *lang = ngx_http_memc_purge_cookie_param(r, "lang");

    if (region == NULL
        || lang == NULL)
    {
        return NGX_DECLINED;
    }

    // Make str "http://{host}{uri} for function md5 .
    ngx_str_t *address = ngx_http_memc_purge_get_full_address(r);
    
    // Because Leak-check result of valgrind , fllowing code looks so weird ... "t_char = ..."
    u_char *md5str, *t_char = (u_char *) address->data;
    md5str = (u_char *) calloc(sizeof(u_char), address->len);

    memcpy((void *)md5str, (void *)t_char, sizeof(u_char) * address->len);
    char *md5 = ngx_http_memc_purge_md5(md5str, address->len);

    free(md5str);
    md5str = NULL;
    t_char = NULL;

    int keylen = strlen(md5) + strlen(region) + strlen(lang);
    char *key = (char *) ngx_pcalloc(r->pool, keylen + 1);

    strncat(key, md5, strlen(md5));
    strncat(key, region, strlen(region));
    strncat(key, lang, strlen(lang));
    key[keylen] = '\0';

    free(md5);
    md5 = NULL;

    ngx_http_memc_purge_memresult *response = memcache_get_key(mplcf->memCon->memc, (char *) mplcf->mpx.data, key);

    if(response == NULL) {
        return NGX_DECLINED;
    }

    int response_len = response->len;
    
    // Clean http request body first.
    rc = ngx_http_discard_request_body(r);
    if (rc != NGX_OK) {
        free(response->val);
        free(response);
        response = NULL;
        return rc;
    }
    
    ngx_buf_t *b;
    ngx_chain_t out;
    
    // Send header to client
    ngx_str_t type = ngx_string("text/html;charset=utf-8");
    r->headers_out.status = NGX_HTTP_OK; 
    r->headers_out.content_type = type;
    r->headers_out.content_length_n = response_len;
    
    // Message in header
    ngx_str_t processed_key = ngx_string("x-prowed-by");
    ngx_str_t processed_val = ngx_string("ngx_http_memc_purge_module/Cary");

    ngx_table_elt_t *h = ngx_list_push(& r->headers_out.headers);
    if(h == NULL) return NGX_HTTP_INTERNAL_SERVER_ERROR;

    h->hash = 1;
    h->key.len = processed_key.len;
    h->key.data = processed_key.data;
    h->value.len = processed_val.len;
    h->value.data = processed_val.data;
    
    b = ngx_create_temp_buf(r->pool, response_len);
    if(b == NULL) {
        free(response->val);
        free(response);
        response = NULL;
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    ngx_memcpy(b->pos, (u_char *)response->val, response_len);

    b->last = b->pos + response_len;
    b->memory = 1;
    b->last_buf = 1;
    b->last_in_chain = 1;

    free(response->val);
    free(response);
    response = NULL;
    
    rc = ngx_http_send_header(r);
    if(rc == NGX_ERROR || rc > NGX_OK || r->header_only) return rc;

    out.buf = b;
    out.next = NULL;

    rc = ngx_http_output_filter(r, &out);
    if(rc == NGX_OK) 
        return NGX_HTTP_OK;
    else
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
}

static ngx_int_t 
ngx_http_memc_purge_init_confs(ngx_conf_t *cf) 
{
    ngx_http_memc_purge_confs = NULL;
    ngx_http_memc_purge_confs_conns = NULL;

    if(ngx_http_memc_purge_confs == NULL) {
        ngx_http_memc_purge_confs = ngx_array_create(cf->pool, 
                                                     1000, 
                                                     sizeof(ngx_http_memc_purge_loc_conf_t *));
        if(ngx_http_memc_purge_confs == NULL)
            return NGX_ERROR;
    }   

    if(ngx_http_memc_purge_confs_conns == NULL) {
        ngx_http_memc_purge_confs_conns = ngx_array_create(cf->pool, 
                                                           1000, 
                                                           sizeof(ngx_http_memc_purge_memConn_t));
        if(ngx_http_memc_purge_confs_conns == NULL) 
            return NGX_ERROR;
    }

    return NGX_OK;
}

static ngx_int_t 
ngx_http_memc_purge_init(ngx_conf_t *cf) 
{
    unsigned int i;
    ngx_http_memc_purge_loc_conf_t* ptr;

    for(i = 0; i < ngx_http_memc_purge_confs->nelts; i++) {
        ptr = ((ngx_http_memc_purge_loc_conf_t **) ngx_http_memc_purge_confs->elts)[i];
        int result = memcache_connect_test((char *) ptr->mip.data, 
                                           (char *) ptr->mpt.data, 
                                           cf);

        if(result != 1) {
            return NGX_ERROR;
        }

    }

    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) return NGX_ERROR;

    *h = ngx_http_memc_purge_handler;

    return NGX_OK;
}


static ngx_int_t 
ngx_http_memc_purge_init_connections(ngx_cycle_t *cycle) 
{
    unsigned int i;
    ngx_http_memc_purge_loc_conf_t* ptr;

    for(i = 0; i < ngx_http_memc_purge_confs->nelts; i++) {
        ptr = ((ngx_http_memc_purge_loc_conf_t **) ngx_http_memc_purge_confs->elts)[i];
        ngx_int_t result = ngx_http_memc_purge_init_conf_connect(ptr);

        if(result != NGX_OK) {
            return NGX_ERROR;
        }
    }

    return NGX_OK;
}

static ngx_int_t 
ngx_http_memc_purge_init_conf_connect(ngx_http_memc_purge_loc_conf_t *conf) 
{
    unsigned int i;
    ngx_http_memc_purge_memConn_t *ptr = NULL;
    for(i = 0; i < ngx_http_memc_purge_confs_conns->nelts; i++) {
        ptr = (ngx_http_memc_purge_memConn_t *)ngx_http_memc_purge_confs_conns->elts 
              +
              sizeof(ngx_http_memc_purge_memConn_t)*i;

        if (strcmp((char *) ptr->mip.data, (char *) conf->mip.data) == 0
            && strcmp((char *) ptr->mpt.data, (char *) conf->mpt.data) == 0) 
        {
            conf->memCon = ptr;
            return NGX_OK;
        }

    }
    
    ptr = ngx_array_push(ngx_http_memc_purge_confs_conns);
    ptr->mip = conf->mip;
    ptr->mpt = conf->mpt;

    int result = memcache_connect(&ptr->memc, (char *) ptr->mip.data, (char *) ptr->mpt.data);
    if(result == 0) return NGX_ERROR;
    
    conf->memCon = ptr;
    
    return NGX_OK;
}


static void 
ngx_http_memc_purge_destroy(ngx_cycle_t *cycle) 
{
    unsigned int i;
    ngx_http_memc_purge_loc_conf_t *ptr = NULL;

    for(i = 0; i < ngx_http_memc_purge_confs->nelts; i++) {
        ptr = ((ngx_http_memc_purge_loc_conf_t **)ngx_http_memc_purge_confs->elts)[i];
        ptr->memCon = NULL;
    }

    ngx_http_memc_purge_memConn_t *ptr2 = NULL;

    for(i = 0; i < ngx_http_memc_purge_confs_conns->nelts; i++) {

        ptr2 = (ngx_http_memc_purge_memConn_t *)ngx_http_memc_purge_confs_conns->elts
               +
               sizeof(ngx_http_memc_purge_memConn_t)*i;

        if(ptr2->memc != NULL) {
            memcached_free(ptr2->memc);
            ptr2->memc = NULL;
        }

    }

    ptr = NULL;
    ptr2 = NULL;

    ngx_array_destroy(ngx_http_memc_purge_confs);
    ngx_array_destroy(ngx_http_memc_purge_confs_conns);

    ngx_http_memc_purge_confs = NULL;
    ngx_http_memc_purge_confs_conns = NULL;
}

static void 
ngx_http_memc_purge_post_handler(ngx_http_request_t *r)
{
    ngx_http_memc_purge_loc_conf_t *mplcf;
    ngx_http_memc_purge_ctx_t *ctx;

    mplcf = ngx_http_get_module_loc_conf(r, ngx_http_memc_purge_module);
    ctx = ngx_http_get_module_ctx(r, ngx_http_memc_purge_module);

    ctx->done = 1;

    ngx_int_t rc;
    ngx_buf_t *b;
    ngx_chain_t out;
    ngx_str_t *post = ngx_http_memc_purge_postparam(r);

    // Make str "http://{host}{uri}{post} for function md5 .
    ngx_str_t *address = ngx_http_memc_purge_get_full_address(r);

    u_char *md5str, *t_char = (u_char *) address->data;
    md5str = (u_char *) calloc(sizeof(u_char), post->len + address->len);

    memcpy((void *)md5str, (void *)t_char, sizeof(u_char) * address->len);
    t_char = (u_char *)post->data;

    memcpy((void *) (md5str + address->len), (void *)t_char, 
                                             sizeof(u_char) * post->len);

    char *md5 = ngx_http_memc_purge_md5(md5str, post->len + address->len);

    free(md5str);
    md5str = NULL;
    t_char = NULL;

    int keylen = strlen(md5);
    char *key = (char *) ngx_pcalloc(r->pool, keylen + 1);
    strncat(key, md5, strlen(md5));
    key[keylen] = '\0';

    free(md5);
    md5 = NULL;

    ngx_http_memc_purge_memresult *response;
    response = memcache_get_key(mplcf->memCon->memc, 
                                (char *) mplcf->mpx.data, key);

    if(response == NULL) {
        ngx_http_core_run_phases(r);
        return;
    }

    ngx_str_t type = ngx_string("text/html;charset=utf-8");
    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_type = type;
    r->headers_out.content_length_n = response->len;

    ngx_str_t processed_key = ngx_string("x-prowed-by");
    ngx_str_t processed_val = ngx_string("ngx_http_memc_purge_module/Cary");
    ngx_table_elt_t *h = ngx_list_push(& r->headers_out.headers);

    if(h == NULL) {
        free(response->val);
        free(response);
        response = NULL;
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    h->hash = 1;
    h->key.len = processed_key.len;
    h->key.data = processed_key.data;
    h->value.len = processed_val.len;
    h->value.data = processed_val.data;

    b = ngx_create_temp_buf(r->pool, response->len);

    if(b == NULL) {
        free(response->val);
        free(response);
        response = NULL;
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    ngx_memcpy(b->pos, response->val, response->len);
    b->last = b->pos + response->len;
    b->memory = 1;
    b->last_buf = 1;
    b->last_in_chain = 1;

    free(response->val);
    free(response);
    response = NULL;

    rc = ngx_http_send_header(r);
    if (rc == NGX_ERROR 
        || rc > NGX_OK 
        || r->header_only) 
    { 
        ngx_http_finalize_request(r, rc);
        return;
    }

    out.buf = b;
    out.next = NULL;

    rc = ngx_http_output_filter(r, &out);

    if(rc == NGX_OK) {
        ngx_http_finalize_request(r, NGX_HTTP_OK);
        return;
    } else {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }
}
