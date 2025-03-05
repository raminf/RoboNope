#ifndef _NGX_HTTP_SAYPLEASE_MODULE_H_INCLUDED_
#define _NGX_HTTP_SAYPLEASE_MODULE_H_INCLUDED_

#include <stddef.h>

/* Basic type definitions for testing */
typedef unsigned char u_char;
typedef unsigned int ngx_uint_t;
typedef int ngx_int_t;

/* Constants for testing */
#define NGX_OK 0
#define NGX_ERROR -1

/* Structure definitions for testing */
typedef struct {
    int dummy;  /* Placeholder for testing */
} ngx_log_t;

typedef struct {
    void *last;
    void *end;
    void *next;
    void *failed;
} ngx_pool_t;

typedef struct {
    size_t len;
    u_char *data;
} ngx_str_t;

typedef struct {
    void *elts;
    ngx_uint_t nelts;
    size_t size;
    ngx_uint_t nalloc;
    ngx_pool_t *pool;
} ngx_array_t;

/* SayPlease module structures */
typedef struct {
    ngx_str_t user_agent;
    ngx_array_t *disallow;
    ngx_array_t *allow;
} ngx_http_sayplease_robot_t;

/* Function declarations for testing */
ngx_int_t ngx_http_sayplease_load_robots(ngx_str_t *robots_path);
ngx_int_t ngx_http_sayplease_is_blocked_url(ngx_str_t *url);
ngx_int_t ngx_http_sayplease_init_db(ngx_str_t *db_path);
ngx_int_t ngx_http_sayplease_log_request(ngx_str_t *url);
ngx_int_t ngx_http_sayplease_init_cache(void);
ngx_int_t ngx_http_sayplease_cache_lookup(u_char *fingerprint, ngx_int_t *found);
ngx_int_t ngx_http_sayplease_cache_insert(u_char *fingerprint);
ngx_int_t ngx_http_sayplease_generate_content(ngx_str_t *url, u_char **content, size_t *content_len);

/* Nginx function declarations for testing */
ngx_pool_t *ngx_create_pool(size_t size);
void ngx_destroy_pool(ngx_pool_t *pool);
void *ngx_palloc(ngx_pool_t *pool, size_t size);
void *ngx_pcalloc(ngx_pool_t *pool, size_t size);
ngx_array_t *ngx_array_create(ngx_pool_t *pool, ngx_uint_t n, size_t size);
void *ngx_array_push(ngx_array_t *a);

#endif /* _NGX_HTTP_SAYPLEASE_MODULE_H_INCLUDED_ */ 