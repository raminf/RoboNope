#ifndef _NGX_HTTP_ROBONOPE_MODULE_H_INCLUDED_
#define _NGX_HTTP_ROBONOPE_MODULE_H_INCLUDED_

#include <stddef.h>
#include <stdint.h>  /* For uint32_t type */
#include <time.h>    /* For time_t */
#include <sys/types.h>  /* For off_t */

/* Module constants */
#define NGX_HTTP_ROBONOPE_MAX_CACHE 1000

/* Include NGINX headers */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_md5.h>

#ifdef ROBONOPE_USE_DUCKDB
typedef void* duckdb_connection;
#else
typedef struct sqlite3 sqlite3;
#endif

/* Module structs */
typedef struct {
    ngx_str_t user_agent;
    ngx_array_t *disallow;
    ngx_array_t *allow;
} ngx_http_robonope_robot_t;

typedef struct {
    ngx_str_t pattern;
    ngx_str_t user_agent;  /* User agent pattern */
    ngx_array_t *disallow;  /* Array of disallow patterns */
    ngx_array_t *allow;     /* Array of allow patterns */
} ngx_http_robonope_robot_entry_t;

typedef struct {
    ngx_array_t *cache;
    ngx_uint_t   cache_index;
    time_t       last_cleanup;
    ngx_array_t *robot_entries;
    void        *db;
    ngx_pool_t  *cache_pool;
} ngx_http_robonope_main_conf_t;

typedef struct {
    ngx_flag_t   enable;             /* Enable/disable the module */
    ngx_str_t    robots_path;        /* Path to robots.txt file */
    ngx_str_t    db_path;            /* Path to database file */
    ngx_str_t    static_content_path; /* Path to static content */
    ngx_flag_t   dynamic_content;    /* Generate dynamic content */
    ngx_uint_t   cache_ttl;          /* Cache TTL in seconds */
    ngx_uint_t   max_cache_entries;  /* Maximum number of cache entries */
    ngx_str_t    honeypot_class;     /* CSS class for honeypot elements */
    ngx_array_t *disallow_patterns;  /* Patterns to disallow */
    ngx_flag_t   use_lorem_ipsum;    /* Use Lorem Ipsum for content */
} ngx_http_robonope_loc_conf_t;

/* Function prototypes */
#ifdef NGINX_BUILD
static ngx_int_t ngx_http_robonope_load_robots(ngx_http_robonope_main_conf_t *mcf, ngx_str_t *robots_path);
static ngx_int_t ngx_http_robonope_init_db(ngx_http_robonope_main_conf_t *mcf, ngx_str_t *db_path);
static ngx_int_t ngx_http_robonope_init_cache(ngx_http_robonope_main_conf_t *mcf);
static ngx_int_t ngx_http_robonope_cache_lookup(ngx_http_robonope_main_conf_t *mcf, u_char *fingerprint);
static void ngx_http_robonope_cache_insert(ngx_http_robonope_main_conf_t *mcf, u_char *fingerprint);
static void ngx_http_robonope_cache_cleanup(ngx_http_robonope_main_conf_t *mcf);
static ngx_int_t ngx_http_robonope_log_request(
#ifdef ROBONOPE_USE_DUCKDB
    duckdb_connection conn,
#else
    sqlite3 *db,
#endif
    ngx_http_request_t *r,
    ngx_str_t *matched_pattern);
static u_char *ngx_http_robonope_generate_content(ngx_pool_t *pool, ngx_str_t *url, ngx_array_t *disallow_patterns);

/* Externals needed by the implementation */
extern ngx_module_t ngx_http_module;

#else /* !NGINX_BUILD */
/* Function declarations for testing */
ngx_int_t ngx_http_robonope_load_robots(ngx_http_robonope_main_conf_t *mcf, ngx_str_t *robots_path);
ngx_int_t ngx_http_robonope_is_blocked_url(ngx_str_t *url);
ngx_int_t ngx_http_robonope_init_db(ngx_http_robonope_main_conf_t *mcf, ngx_str_t *db_path);
#ifdef ROBONOPE_USE_DUCKDB
ngx_int_t ngx_http_robonope_log_request(duckdb_connection conn, ngx_http_request_t *r, ngx_str_t *matched_pattern);
#else
ngx_int_t ngx_http_robonope_log_request(sqlite3 *db, ngx_http_request_t *r, ngx_str_t *matched_pattern);
#endif
ngx_int_t ngx_http_robonope_init_cache(ngx_http_robonope_main_conf_t *mcf);
ngx_int_t ngx_http_robonope_cache_lookup(ngx_http_robonope_main_conf_t *mcf, u_char *fingerprint);
void ngx_http_robonope_cache_insert(ngx_http_robonope_main_conf_t *mcf, u_char *fingerprint);
u_char *ngx_http_robonope_generate_content(ngx_pool_t *pool, ngx_str_t *url, ngx_array_t *disallow_patterns);
#endif /* NGINX_BUILD */

#endif /* _NGX_HTTP_ROBONOPE_MODULE_H_INCLUDED_ */ 