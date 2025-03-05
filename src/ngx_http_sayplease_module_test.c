#include <string.h>
#include <stdlib.h>
#include "ngx_http_sayplease_module.h"

/* Test implementation of module functions */

/* Implementation of ngx_http_sayplease_load_robots for testing */
ngx_int_t ngx_http_sayplease_load_robots(ngx_str_t *robots_path) {
    /* For testing, just return success */
    return NGX_OK;
}

/* Implementation of ngx_http_sayplease_is_blocked_url for testing */
ngx_int_t ngx_http_sayplease_is_blocked_url(ngx_str_t *url) {
    /* For testing, check if URL contains "/norobots/" */
    if (url && url->data) {
        if (strstr((char *)url->data, "/norobots/") != NULL) {
            return NGX_ERROR; /* URL is blocked */
        }
    }
    return NGX_OK; /* URL is allowed */
}

/* Implementation of ngx_http_sayplease_init_db for testing */
ngx_int_t ngx_http_sayplease_init_db(ngx_str_t *db_path) {
    /* For testing, just return success */
    return NGX_OK;
}

/* Implementation of ngx_http_sayplease_generate_content for testing */
ngx_int_t ngx_http_sayplease_generate_content(ngx_str_t *url, u_char **content, size_t *content_len) {
    /* For testing, generate a simple HTML page */
    const char *html = "<html><body><h1>SayPlease Test Page</h1></body></html>";
    size_t html_len = strlen(html);
    
    *content = (u_char *)malloc(html_len + 1);
    if (*content == NULL) {
        return NGX_ERROR;
    }
    
    memcpy(*content, html, html_len);
    (*content)[html_len] = '\0';
    *content_len = html_len;
    
    return NGX_OK;
}

/* Implementation of ngx_http_sayplease_log_request for testing */
ngx_int_t ngx_http_sayplease_log_request(ngx_str_t *url) {
    /* For testing, just return success */
    return NGX_OK;
}

/* Implementation of ngx_http_sayplease_init_cache for testing */
ngx_int_t ngx_http_sayplease_init_cache(void) {
    /* For testing, just return success */
    return NGX_OK;
}

/* Implementation of ngx_http_sayplease_cache_lookup for testing */
ngx_int_t ngx_http_sayplease_cache_lookup(u_char *fingerprint, ngx_int_t *found) {
    /* For testing, check if fingerprint starts with "test_fingerprint" */
    if (fingerprint && strncmp((char *)fingerprint, "test_fingerprint", 15) == 0) {
        *found = 1; /* Found */
    } else {
        *found = 0; /* Not found */
    }
    return NGX_OK;
}

/* Implementation of ngx_http_sayplease_cache_insert for testing */
ngx_int_t ngx_http_sayplease_cache_insert(u_char *fingerprint) {
    /* For testing, just return success */
    return NGX_OK;
} 