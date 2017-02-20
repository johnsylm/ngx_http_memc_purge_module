#include "ngx_http.h"
#include "ngx_http_memc_purge_gp.h"


char *
ngx_http_memc_purge_cookie_param(ngx_http_request_t *r, char *s) 
{
    char *ptr;
    char *arg;

    if(r->headers_in.cookies.nelts == 0) {
        return NULL;
    }

    ngx_str_t cookie = ((ngx_table_elt_t **) r->headers_in.cookies.elts) [0]
                                              -> value;
    
    if (cookie.len == 0) {
        return NULL;
    }
    
    arg = (char *)calloc(sizeof(u_char), cookie.len+1);
    snprintf(arg, cookie.len+1, "%s", cookie.data);

    ptr = strstr((char *) arg, (char *)s);
    if (ptr == NULL) {
        free(arg);
        arg = NULL;
        return NULL;
    }
    
    int i = 0, l = 0;
    while(ptr[i] && ptr[i] != '=') ptr++;
    ptr++;
    while(ptr[l] && ptr[l] != '\0' && ptr[l] != ';') l++;
    
    char *result = (char *)ngx_pcalloc(r->pool, sizeof(char)*l+1);
    strncpy(result, ptr, l); 
    result[l] = '\0';

    free(arg);
    ptr = NULL;
    arg = NULL;

    return result;
}

ngx_str_t *
ngx_http_memc_purge_postparam(ngx_http_request_t *r) 
{

    if (r->request_body == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 
                                   0, 
                                   "reqeust_body:null");
        return NULL;
    }

    ngx_chain_t* bufs = r->request_body->bufs;
    ngx_buf_t* buf = NULL;
    uint8_t* data_buf = NULL;
    size_t content_length = 0;
    size_t body_length = 0;

    if ( r->headers_in.content_length == NULL ) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 
                                   0, 
                                   "r->headers_in.content_length == NULL");
        return NULL;
    }

    content_length = atoi((char*) (r->headers_in.content_length->value.data));
    data_buf = (uint8_t *) ngx_palloc(r->pool, content_length + 1);
    size_t buf_length = 0;

    while ( bufs ) {
        buf = bufs->buf;
        bufs = bufs->next;
        buf_length = buf->last - buf->pos ;
        if( body_length + buf_length > content_length ) {
            memcpy( data_buf + body_length, buf->pos, content_length - body_length);
            body_length = content_length;
            break;
        }
        memcpy(data_buf + body_length, buf->pos, buf->last - buf->pos);
        body_length += buf->last - buf->pos;
    }

    ngx_str_t *post = ngx_pcalloc(r->pool, sizeof(ngx_str_t));
    post->len = body_length;
    post->data = (u_char *) data_buf;
    return post;

}

ngx_str_t *
ngx_http_memc_purge_get_full_address(ngx_http_request_t *r)
{
    int nlen = 7;
    ngx_str_t *address = ngx_pcalloc(r->pool, sizeof(ngx_str_t));

    address->len = r->headers_in.server.len + r->uri.len + 7;
    if((r->method & NGX_HTTP_GET) && r->args.data != NULL) {
        address->len += r->args.len + 1;
    }

    address->data = (u_char *) ngx_pcalloc(r->pool, sizeof(u_char) * address->len);
    ngx_snprintf(address->data, nlen, "%s", "http://");

    ngx_snprintf(address->data + nlen, r->headers_in.server.len, 
                                       "%s", 
                                       (char *) r->headers_in.server.data);
    nlen += r->headers_in.server.len;

    ngx_snprintf(address->data + nlen, r->uri.len, "%s", (char *) r->uri.data);
    nlen += r->uri.len;

    if((r->method & NGX_HTTP_GET) 
       && r->args.len > 0
       && r->args.data != NULL) 
    {
        ngx_snprintf(address->data + nlen, r->args.len + 1,
                                           "?%s", 
                                           (char *) r->args.data);
    }

    return address;
}
