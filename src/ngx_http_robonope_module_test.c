#include <string.h>
#include <stdlib.h>
#include <unity.h>
#include "ngx_http_robonope_module.h"

/* Test implementation of module functions */

/* Implementation of ngx_http_robonope_load_robots for testing */
ngx_int_t ngx_http_robonope_load_robots(ngx_str_t *robots_path) {
    return NGX_OK;
}

/* Implementation of ngx_http_robonope_is_blocked_url for testing */
ngx_int_t ngx_http_robonope_is_blocked_url(ngx_str_t *url) {
    if (url == NULL) {
        return NGX_ERROR;
    }
    
    /* For testing, block URLs containing "blocked" */
    if (strstr((char *)url->data, "blocked") != NULL) {
        return NGX_OK;
    }
    
    return NGX_DECLINED;
}

/* Implementation of ngx_http_robonope_init_db for testing */
ngx_int_t ngx_http_robonope_init_db(ngx_str_t *db_path) {
    return NGX_OK;
}

/* Implementation of ngx_http_robonope_generate_content for testing */
ngx_int_t ngx_http_robonope_generate_content(ngx_str_t *url, u_char **content, size_t *content_len) {
    const char *html = "<html><body><h1>RoboNope Test Page</h1></body></html>";
    size_t len = strlen(html);
    
    *content = malloc(len + 1);
    if (*content == NULL) {
        return NGX_ERROR;
    }
    
    memcpy(*content, html, len);
    (*content)[len] = '\0';
    *content_len = len;
    
    return NGX_OK;
}

/* Implementation of ngx_http_robonope_log_request for testing */
ngx_int_t ngx_http_robonope_log_request(ngx_str_t *url) {
    return NGX_OK;
}

/* Implementation of ngx_http_robonope_init_cache for testing */
ngx_int_t ngx_http_robonope_init_cache(void) {
    return NGX_OK;
}

/* Implementation of ngx_http_robonope_cache_lookup for testing */
ngx_int_t ngx_http_robonope_cache_lookup(u_char *fingerprint, ngx_int_t *found) {
    if (fingerprint == NULL || found == NULL) {
        return NGX_ERROR;
    }
    
    /* For testing, consider any fingerprint starting with 'test' as cached */
    if (strncmp((char *)fingerprint, "test", 4) == 0) {
        *found = 1;
    } else {
        *found = 0;
    }
    
    return NGX_OK;
}

/* Implementation of ngx_http_robonope_cache_insert for testing */
ngx_int_t ngx_http_robonope_cache_insert(u_char *fingerprint) {
    return NGX_OK;
} 