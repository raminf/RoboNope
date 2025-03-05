#include <stdlib.h>
#include <string.h>
#include "ngx_http_sayplease_module.h"

/* Mock implementations of Nginx functions */

ngx_pool_t *ngx_create_pool(size_t size) {
    ngx_pool_t *pool;
    
    pool = (ngx_pool_t *)malloc(sizeof(ngx_pool_t));
    if (!pool) {
        return NULL;
    }
    
    pool->last = NULL;
    pool->end = NULL;
    pool->next = NULL;
    pool->failed = NULL;
    
    return pool;
}

void ngx_destroy_pool(ngx_pool_t *pool) {
    if (pool) {
        free(pool);
    }
}

void *ngx_palloc(ngx_pool_t *pool, size_t size) {
    if (!pool) {
        return NULL;
    }
    
    return malloc(size);
}

void *ngx_pcalloc(ngx_pool_t *pool, size_t size) {
    void *p;
    
    p = ngx_palloc(pool, size);
    if (p) {
        memset(p, 0, size);
    }
    
    return p;
}

ngx_array_t *ngx_array_create(ngx_pool_t *pool, ngx_uint_t n, size_t size) {
    ngx_array_t *a;
    
    a = ngx_palloc(pool, sizeof(ngx_array_t));
    if (a == NULL) {
        return NULL;
    }
    
    a->elts = ngx_palloc(pool, n * size);
    if (a->elts == NULL) {
        return NULL;
    }
    
    a->nelts = 0;
    a->size = size;
    a->nalloc = n;
    a->pool = pool;
    
    return a;
}

void *ngx_array_push(ngx_array_t *a) {
    void *elt;
    
    if (a->nelts >= a->nalloc) {
        /* In a real implementation, this would resize the array */
        return NULL;
    }
    
    elt = (char *)a->elts + a->size * a->nelts;
    a->nelts++;
    
    return elt;
}

/* Add any other Nginx functions needed for testing */ 