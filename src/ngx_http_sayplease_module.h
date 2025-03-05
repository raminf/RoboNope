#ifndef _NGX_HTTP_SAYPLEASE_MODULE_H_INCLUDED_
#define _NGX_HTTP_SAYPLEASE_MODULE_H_INCLUDED_

#include <stddef.h>

/* 
 * Define types only for testing, not when building with NGINX 
 * NGINX_BUILD is defined in the module's config file when building with NGINX
 */
#ifndef NGINX_BUILD

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

#endif /* !NGINX_BUILD */

/* SayPlease module structures */
typedef struct {
    ngx_str_t user_agent;
    ngx_array_t *disallow;
    ngx_array_t *allow;
} ngx_http_sayplease_robot_t;

#ifdef NGINX_BUILD
/* Additional structures needed for NGINX build */
typedef struct {
    ngx_str_t pattern;
    ngx_array_t *disallow;  /* Array of disallow patterns */
} ngx_http_sayplease_robot_entry_t;

/* Module configuration structures for NGINX build */
typedef struct {
    ngx_array_t *cache;         /* Cache for bot fingerprints */
    ngx_uint_t   cache_index;   /* Current index in the cache */
    time_t       last_cleanup;  /* Time of last cache cleanup */
    ngx_array_t *robot_entries; /* Array of robot entries */
    void        *db;            /* Database connection */
} ngx_http_sayplease_main_conf_t;

typedef struct {
    ngx_flag_t   enable;             /* Enable/disable the module */
    ngx_str_t    robots_path;        /* Path to robots.txt file */
    ngx_str_t    db_path;            /* Path to database file */
    ngx_str_t    static_content_path; /* Path to static content */
    ngx_flag_t   dynamic_content;    /* Generate dynamic content */
    ngx_int_t    cache_ttl;          /* Cache TTL in seconds */
    ngx_int_t    max_cache_entries;  /* Maximum number of cache entries */
    ngx_str_t    honeypot_class;     /* CSS class for honeypot elements */
    ngx_array_t *disallow_patterns;  /* Patterns to disallow */
    ngx_flag_t   use_lorem_ipsum;    /* Use Lorem Ipsum for content */
} ngx_http_sayplease_loc_conf_t;

/* Function prototypes for functions used in the implementation */
static ngx_int_t ngx_http_sayplease_load_robots(ngx_http_sayplease_main_conf_t *mcf, ngx_str_t *robots_path);
static ngx_int_t ngx_http_sayplease_init_db(ngx_http_sayplease_main_conf_t *mcf, ngx_str_t *db_path);
static ngx_int_t ngx_http_sayplease_init_cache(ngx_http_sayplease_main_conf_t *mcf);
static ngx_int_t ngx_http_sayplease_cache_lookup(ngx_http_sayplease_main_conf_t *mcf, u_char *fingerprint);
static void ngx_http_sayplease_cache_insert(ngx_http_sayplease_main_conf_t *mcf, u_char *fingerprint);
static void ngx_http_sayplease_cache_cleanup(ngx_http_sayplease_main_conf_t *mcf);
static ngx_int_t ngx_http_sayplease_log_request(
#ifdef SAYPLEASE_USE_DUCKDB
    duckdb_connection conn,
#else
    sqlite3 *db,
#endif
    ngx_http_request_t *r,
    ngx_str_t *matched_pattern);
static u_char *ngx_http_sayplease_generate_content(ngx_pool_t *pool, ngx_str_t *url, ngx_array_t *disallow_patterns);

#endif /* NGINX_BUILD */

/* Function declarations */
#ifndef NGINX_BUILD
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
#endif /* !NGINX_BUILD */

#endif /* _NGX_HTTP_SAYPLEASE_MODULE_H_INCLUDED_ */ 