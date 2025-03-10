#include <string.h>
#include "../../deps/Unity/src/unity.h"
#include "../../src/ngx_http_robonope_module.h"

ngx_pool_t *pool;
ngx_log_t *ngx_log;

void setUp(void) {
    /* Create a memory pool for tests */
    pool = ngx_create_pool(1024);
    ngx_log = NULL;
    TEST_ASSERT_NOT_NULL(pool);
}

void tearDown(void) {
    /* Clean up memory pool */
    if (pool) {
        ngx_destroy_pool(pool);
        pool = NULL;
    }
}

void test_is_blocked_url(void) {
    ngx_str_t clean_url;
    ngx_str_t blocked_url;
    
    clean_url.data = (u_char *)"http://example.com/allowed";
    clean_url.len = strlen((char *)clean_url.data);
    
    blocked_url.data = (u_char *)"http://example.com/norobots/blocked";
    blocked_url.len = strlen((char *)blocked_url.data);
    
    TEST_ASSERT_EQUAL(NGX_OK, ngx_http_robonope_is_blocked_url(&clean_url));
    TEST_ASSERT_EQUAL(NGX_ERROR, ngx_http_robonope_is_blocked_url(&blocked_url));
}

void test_load_robots(void) {
    ngx_str_t robots_path;
    
    robots_path.data = (u_char *)"/path/to/robots.txt";
    robots_path.len = strlen((char *)robots_path.data);
    
    TEST_ASSERT_EQUAL(NGX_OK, ngx_http_robonope_load_robots(&robots_path));
}

void test_generate_content(void) {
    ngx_str_t url;
    u_char *content;
    size_t content_len;
    
    url.data = (u_char *)"http://example.com/test";
    url.len = strlen((char *)url.data);
    
    TEST_ASSERT_EQUAL(NGX_OK, ngx_http_robonope_generate_content(&url, &content, &content_len));
    TEST_ASSERT_NOT_NULL(content);
    TEST_ASSERT_TRUE(content_len > 0);
    TEST_ASSERT_TRUE(strstr((char *)content, "RoboNope") != NULL);
}

void test_cache_operations(void) {
    u_char test_fingerprint[16] = "test_fingerprint";
    u_char nonexistent[16] = "nonexistent_test";
    ngx_int_t found;
    
    TEST_ASSERT_EQUAL(NGX_OK, ngx_http_robonope_init_cache());
    
    // Test lookup for existing entry
    TEST_ASSERT_EQUAL(NGX_OK, ngx_http_robonope_cache_lookup(test_fingerprint, &found));
    TEST_ASSERT_TRUE(found);
    
    // Test lookup for non-existent entry
    TEST_ASSERT_EQUAL(NGX_OK, ngx_http_robonope_cache_lookup(nonexistent, &found));
    TEST_ASSERT_FALSE(found);
    
    // Test insertion
    TEST_ASSERT_EQUAL(NGX_OK, ngx_http_robonope_cache_insert(nonexistent));
}

void test_database_operations(void) {
    ngx_str_t db_path;
    ngx_str_t url;
    
    db_path.data = (u_char *)"/path/to/database.db";
    db_path.len = strlen((char *)db_path.data);
    
    url.data = (u_char *)"http://example.com/test";
    url.len = strlen((char *)url.data);
    
    TEST_ASSERT_EQUAL(NGX_OK, ngx_http_robonope_init_db(&db_path));
    TEST_ASSERT_EQUAL(NGX_OK, ngx_http_robonope_log_request(&url));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_is_blocked_url);
    RUN_TEST(test_load_robots);
    RUN_TEST(test_generate_content);
    RUN_TEST(test_cache_operations);
    RUN_TEST(test_database_operations);
    return UNITY_END();
} 