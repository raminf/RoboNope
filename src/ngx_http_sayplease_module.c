#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>  /* For stat() */

#ifdef SAYPLEASE_USE_DUCKDB
#include <duckdb.h>
#else
#include <sqlite3.h>
#endif

#include "ngx_http_sayplease_module.h"

// Function declarations for internal use only
static void *ngx_http_sayplease_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_sayplease_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static void *ngx_http_sayplease_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_sayplease_init_main_conf(ngx_conf_t *cf, void *conf);
static ngx_int_t ngx_http_sayplease_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_sayplease_handler(ngx_http_request_t *r);
static void ngx_http_sayplease_cleanup_db(void *data);
static void ngx_http_sayplease_cache_cleanup(ngx_http_sayplease_main_conf_t *mcf) __attribute__((unused));
static u_char *ngx_http_sayplease_generate_lorem_ipsum(ngx_pool_t *pool, ngx_uint_t paragraphs);
static u_char *ngx_http_sayplease_generate_random_text(ngx_pool_t *pool, ngx_uint_t words);
static ngx_str_t *ngx_http_sayplease_generate_honeypot_link(ngx_pool_t *pool, ngx_str_t *base_url);

static ngx_command_t ngx_http_sayplease_commands[] = {
    {
        ngx_string("sayplease_enable"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_sayplease_loc_conf_t, enable),
        NULL
    },
    {
        ngx_string("sayplease_robots_path"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_sayplease_loc_conf_t, robots_path),
        NULL
    },
    {
        ngx_string("sayplease_db_path"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_sayplease_loc_conf_t, db_path),
        NULL
    },
    {
        ngx_string("sayplease_static_content_path"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_sayplease_loc_conf_t, static_content_path),
        NULL
    },
    {
        ngx_string("sayplease_dynamic_content"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_sayplease_loc_conf_t, dynamic_content),
        NULL
    },
    {
        ngx_string("sayplease_cache_ttl"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_sayplease_loc_conf_t, cache_ttl),
        NULL
    },
    {
        ngx_string("sayplease_max_cache_entries"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_sayplease_loc_conf_t, max_cache_entries),
        NULL
    },
    {
        ngx_string("sayplease_honeypot_class"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_sayplease_loc_conf_t, honeypot_class),
        NULL
    },
    {
        ngx_string("sayplease_use_lorem_ipsum"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_sayplease_loc_conf_t, use_lorem_ipsum),
        NULL
    },
    ngx_null_command
};

static ngx_http_module_t ngx_http_sayplease_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_sayplease_init,              /* postconfiguration */
    ngx_http_sayplease_create_main_conf,  /* create main configuration */
    ngx_http_sayplease_init_main_conf,    /* init main configuration */
    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */
    ngx_http_sayplease_create_loc_conf,   /* create location configuration */
    ngx_http_sayplease_merge_loc_conf     /* merge location configuration */
};

ngx_module_t ngx_http_sayplease_module = {
    NGX_MODULE_V1,
    &ngx_http_sayplease_module_ctx,    /* module context */
    ngx_http_sayplease_commands,       /* module directives */
    NGX_HTTP_MODULE,                   /* module type */
    NULL,                              /* init master */
    NULL,                              /* init module */
    NULL,                              /* init process */
    NULL,                              /* init thread */
    NULL,                              /* exit thread */
    NULL,                              /* exit process */
    NULL,                              /* exit master */
    NGX_MODULE_V1_PADDING
};

static void *
ngx_http_sayplease_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_sayplease_main_conf_t *mcf;

    mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_sayplease_main_conf_t));
    if (mcf == NULL) {
        return NULL;
    }

    mcf->robot_entries = ngx_array_create(cf->pool, 10, sizeof(ngx_http_sayplease_robot_entry_t));
    if (mcf->robot_entries == NULL) {
        return NULL;
    }

    return mcf;
}

static char *
ngx_http_sayplease_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_sayplease_main_conf_t *mcf = conf;
    ngx_http_sayplease_loc_conf_t *lcf;

    lcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_sayplease_module);
    
    if (ngx_http_sayplease_load_robots(mcf, &lcf->robots_path) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "failed to load robots.txt");
        return NGX_CONF_ERROR;
    }

    if (ngx_http_sayplease_init_db(mcf, &lcf->db_path) != NGX_OK) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "failed to initialize database");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

static void *
ngx_http_sayplease_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_sayplease_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_sayplease_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->enable = NGX_CONF_UNSET;
    conf->dynamic_content = NGX_CONF_UNSET;
    conf->cache_ttl = NGX_CONF_UNSET_UINT;
    conf->max_cache_entries = NGX_CONF_UNSET_UINT;
    conf->use_lorem_ipsum = NGX_CONF_UNSET;

    return conf;
}

static char *
ngx_http_sayplease_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_sayplease_loc_conf_t *prev = parent;
    ngx_http_sayplease_loc_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_value(conf->dynamic_content, prev->dynamic_content, 1);
    ngx_conf_merge_str_value(conf->robots_path, prev->robots_path, "/etc/nginx/robots.txt");
    ngx_conf_merge_str_value(conf->db_path, prev->db_path, "/var/lib/nginx/sayplease.db");
    ngx_conf_merge_str_value(conf->static_content_path, prev->static_content_path, "/etc/nginx/sayplease_static");
    ngx_conf_merge_uint_value(conf->cache_ttl, prev->cache_ttl, 3600);
    ngx_conf_merge_uint_value(conf->max_cache_entries, prev->max_cache_entries, NGX_HTTP_SAYPLEASE_MAX_CACHE);
    ngx_conf_merge_str_value(conf->honeypot_class, prev->honeypot_class, "honeypot");
    ngx_conf_merge_value(conf->use_lorem_ipsum, prev->use_lorem_ipsum, 1);

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_sayplease_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt *h;
    ngx_http_core_main_conf_t *cmcf;
    ngx_pool_cleanup_t *cln;
    ngx_http_sayplease_main_conf_t *mcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_sayplease_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_sayplease_handler;

    // Initialize cache
    if (ngx_http_sayplease_init_cache(mcf) != NGX_OK) {
        return NGX_ERROR;
    }

    // Register cleanup handler
    cln = ngx_pool_cleanup_add(cf->pool, 0);
    if (cln == NULL) {
        return NGX_ERROR;
    }

    cln->handler = ngx_http_sayplease_cleanup_db;
    cln->data = mcf;

    return NGX_OK;
}

static ngx_int_t
ngx_http_sayplease_handler(ngx_http_request_t *r)
{
    ngx_http_sayplease_loc_conf_t *lcf;
    ngx_http_sayplease_main_conf_t *mcf;
    ngx_str_t *patterns;
    ngx_uint_t i;
    ngx_str_t *pattern;
    u_char *content;
    ngx_buf_t *b;
    ngx_chain_t out;
    ngx_str_t matched_pattern = ngx_null_string;

    lcf = ngx_http_get_module_loc_conf(r, ngx_http_sayplease_module);
    if (!lcf->enable) {
        return NGX_DECLINED;
    }

    mcf = ngx_http_get_module_main_conf(r, ngx_http_sayplease_module);
    patterns = mcf->robot_entries->elts;

    // Check if URL matches any disallow pattern
    for (i = 0; i < mcf->robot_entries->nelts; i++) {
        ngx_str_t *pattern;
        ngx_http_sayplease_robot_entry_t *entry;
        
        entry = (ngx_http_sayplease_robot_entry_t *)mcf->robot_entries->elts;
        pattern = entry[i].disallow->elts;

        if (ngx_strncmp(r->uri.data, pattern->data, pattern->len) == 0) {
            matched_pattern = *pattern;
            break;
        }
    }

    if (matched_pattern.len == 0) {
        return NGX_DECLINED;
    }

    // Generate request fingerprint
    u_char fingerprint[32];
    ngx_md5_t md5;
    ngx_md5_init(&md5);
    ngx_md5_update(&md5, r->connection->addr_text.data, r->connection->addr_text.len);
    ngx_md5_update(&md5, r->headers_in.user_agent->value.data, r->headers_in.user_agent->value.len);
    ngx_md5_update(&md5, r->uri.data, r->uri.len);
    ngx_md5_final(fingerprint, &md5);

    // Check cache first
    if (ngx_http_sayplease_cache_lookup(mcf, fingerprint) == NGX_OK) {
        // Return cached response
        entry = (ngx_http_sayplease_robot_entry_t *)mcf->robot_entries->elts;
        content = ngx_http_sayplease_generate_content(r->pool, &r->uri, entry[i].disallow);
        if (content == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    } else {
        // Log the request and cache the fingerprint
        ngx_http_sayplease_log_request(
#ifdef SAYPLEASE_USE_DUCKDB
            mcf->conn,
#else
            mcf->db,
#endif
            r,
            &matched_pattern);

        ngx_http_sayplease_cache_insert(mcf, fingerprint);

        // Generate new content
        entry = (ngx_http_sayplease_robot_entry_t *)mcf->robot_entries->elts;
        content = ngx_http_sayplease_generate_content(r->pool, &r->uri, entry[i].disallow);
        if (content == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    // Set response headers
    r->headers_out.content_type.len = sizeof("text/html") - 1;
    r->headers_out.content_type.data = (u_char *) "text/html";
    r->headers_out.status = NGX_HTTP_OK;

    // Allocate response buffer
    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b->pos = content;
    b->last = content + ngx_strlen(content);
    b->memory = 1;
    b->last_buf = 1;

    out.buf = b;
    out.next = NULL;

    r->headers_out.content_length_n = b->last - b->pos;
    ngx_http_send_header(r);

    return ngx_http_output_filter(r, &out);
}

static ngx_int_t
ngx_http_sayplease_load_robots(ngx_http_sayplease_main_conf_t *mcf, ngx_str_t *robots_path)
{
    ngx_fd_t fd;
    ngx_file_t file;
    char *buf, *line, *directive, *value;
    ngx_http_sayplease_robot_entry_t *entry = NULL;
    size_t size;
    ssize_t n;

    fd = ngx_open_file(robots_path->data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));
    file.fd = fd;
    file.name = *robots_path;

    struct stat sb;
    if (stat((const char *)robots_path->data, &sb) == -1) {
        return NGX_ERROR;
    }
    size = sb.st_size;

    buf = ngx_palloc(mcf->cache_pool, size + 1);
    if (buf == NULL) {
        ngx_close_file(fd);
        return NGX_ERROR;
    }

    n = ngx_read_file(&file, (u_char *)buf, size, 0);
    ngx_close_file(fd);

    if (n == NGX_ERROR) {
        return NGX_ERROR;
    }

    buf[n] = '\0';
    line = strtok(buf, "\n");

    while (line != NULL) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') {
            line = strtok(NULL, "\n");
            continue;
        }

        directive = strtok(line, ":");
        value = strtok(NULL, "\n");

        if (directive && value) {
            // Trim whitespace
            while (*value == ' ') value++;

            if (ngx_strncasecmp((u_char *)"User-agent", (u_char *)directive, 10) == 0) {
                entry = ngx_array_push(mcf->robot_entries);
                if (entry == NULL) {
                    return NGX_ERROR;
                }
                entry->user_agent.len = ngx_strlen(value);
                entry->user_agent.data = ngx_pcalloc(mcf->cache_pool, entry->user_agent.len + 1);
                if (entry->user_agent.data == NULL) {
                    return NGX_ERROR;
                }
                ngx_memcpy(entry->user_agent.data, value, entry->user_agent.len);
                entry->allow = ngx_array_create(mcf->cache_pool, 4, sizeof(ngx_str_t));
                entry->disallow = ngx_array_create(mcf->cache_pool, 4, sizeof(ngx_str_t));
            }
            else if (entry != NULL) {
                ngx_str_t *pattern;
                if (ngx_strncasecmp((u_char *)"Allow", (u_char *)directive, 5) == 0) {
                    pattern = ngx_array_push(entry->allow);
                }
                else if (ngx_strncasecmp((u_char *)"Disallow", (u_char *)directive, 8) == 0) {
                    pattern = ngx_array_push(entry->disallow);
                }
                else {
                    line = strtok(NULL, "\n");
                    continue;
                }

                if (pattern == NULL) {
                    return NGX_ERROR;
                }

                pattern->len = ngx_strlen(value);
                pattern->data = ngx_pcalloc(mcf->cache_pool, pattern->len + 1);
                if (pattern->data == NULL) {
                    return NGX_ERROR;
                }
                ngx_memcpy(pattern->data, value, pattern->len);
            }
        }

        line = strtok(NULL, "\n");
    }

    return NGX_OK;
}

static ngx_int_t
ngx_http_sayplease_init_db(ngx_http_sayplease_main_conf_t *mcf, ngx_str_t *db_path)
{
#ifdef SAYPLEASE_USE_DUCKDB
    if (duckdb_open((char *)db_path->data, &mcf->db) != DuckDBSuccess) {
        return NGX_ERROR;
    }

    if (duckdb_connect(mcf->db, &mcf->conn) != DuckDBSuccess) {
        duckdb_close(&mcf->db);
        return NGX_ERROR;
    }

    const char *sql = "CREATE TABLE IF NOT EXISTS requests ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                     "ip VARCHAR,"
                     "user_agent VARCHAR,"
                     "url VARCHAR,"
                     "matched_pattern VARCHAR"
                     ");";

    duckdb_state state;
    state = duckdb_query(mcf->conn, sql, NULL);
    if (state != DuckDBSuccess) {
        duckdb_disconnect(&mcf->conn);
        duckdb_close(&mcf->db);
        return NGX_ERROR;
    }

#else
    sqlite3 *sqlite_db;
    if (sqlite3_open((char *)db_path->data, &sqlite_db) != SQLITE_OK) {
        return NGX_ERROR;
    }
    mcf->db = sqlite_db;

    char *err_msg = NULL;
    char *sql = "CREATE TABLE IF NOT EXISTS requests ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
                "ip TEXT,"
                "user_agent TEXT,"
                "url TEXT,"
                "matched_pattern TEXT"
                ");";

    if (sqlite3_exec(mcf->db, sql, NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        return NGX_ERROR;
    }
#endif

    return NGX_OK;
}

static u_char *
ngx_http_sayplease_generate_content(ngx_pool_t *pool, ngx_str_t *url, ngx_array_t *disallow_patterns)
{
    /* We can't access the location configuration here, so we'll use default behavior */
    ngx_int_t use_lorem_ipsum = 1; /* Default to using Lorem Ipsum */
    u_char *content, *body;
    ngx_str_t *honeypot_link;
    size_t total_len;

    // Generate body content
    if (use_lorem_ipsum) {
        body = ngx_http_sayplease_generate_lorem_ipsum(pool, 3);
    } else {
        body = ngx_http_sayplease_generate_random_text(pool, 100);
    }

    if (body == NULL) {
        return NULL;
    }

    // Generate honeypot link
    honeypot_link = ngx_http_sayplease_generate_honeypot_link(pool, url);
    if (honeypot_link == NULL) {
        return NULL;
    }

    // Calculate total length needed
    total_len = ngx_strlen(body) + honeypot_link->len + 500; // Extra space for HTML structure

    content = ngx_pcalloc(pool, total_len);
    if (content == NULL) {
        return NULL;
    }

    // Construct the full HTML
    ngx_sprintf(content,
        "<html>\n"
        "<head>\n"
        "<style>\n"
        ".%s { opacity: 0; position: absolute; top: -9999px; }\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "%s\n"
        "<a href=\"%s\" class=\"%s\">Important Information</a>\n"
        "</body>\n"
        "</html>",
        "honeypot",
        body,
        honeypot_link->data,
        "honeypot");

    return content;
}

static ngx_int_t
ngx_http_sayplease_log_request(
#ifdef SAYPLEASE_USE_DUCKDB
    duckdb_connection conn,
#else
    sqlite3 *db,
#endif
    ngx_http_request_t *r,
    ngx_str_t *matched_pattern)
{
    ngx_str_t ip;
    ip.data = r->connection->addr_text.data;
    ip.len = r->connection->addr_text.len;

#ifdef SAYPLEASE_USE_DUCKDB
    char *sql = ngx_sprintf("INSERT INTO requests (ip, user_agent, url, matched_pattern) "
                           "VALUES ('%s', '%s', '%s', '%s');",
                           ip.data,
                           r->headers_in.user_agent->value.data,
                           r->uri.data,
                           matched_pattern->data);

    duckdb_state state;
    state = duckdb_query(conn, sql, NULL);
    if (state != DuckDBSuccess) {
        return NGX_ERROR;
    }

#else
    char *sql;
    char *err_msg = NULL;

    sql = sqlite3_mprintf("INSERT INTO requests (ip, user_agent, url, matched_pattern) "
                         "VALUES ('%q', '%q', '%q', '%q');",
                         ip.data,
                         r->headers_in.user_agent->value.data,
                         r->uri.data,
                         matched_pattern->data);

    if (sqlite3_exec(db, sql, NULL, NULL, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        sqlite3_free(sql);
        return NGX_ERROR;
    }

    sqlite3_free(sql);
#endif

    return NGX_OK;
}

static u_char *
ngx_http_sayplease_generate_lorem_ipsum(ngx_pool_t *pool, ngx_uint_t paragraphs)
{
    static const char *lorem_paragraphs[] = {
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.",
        "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.",
        "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur.",
        "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
    };
    static const size_t num_paragraphs = sizeof(lorem_paragraphs) / sizeof(lorem_paragraphs[0]);

    size_t total_len = 0;
    ngx_uint_t i;
    u_char *content, *p;

    // Calculate total length needed
    for (i = 0; i < paragraphs && i < num_paragraphs; i++) {
        total_len += ngx_strlen(lorem_paragraphs[i]) + 5; // +5 for <p> tags and newline
    }

    content = ngx_pcalloc(pool, total_len + 1);
    if (content == NULL) {
        return NULL;
    }

    p = content;
    for (i = 0; i < paragraphs && i < num_paragraphs; i++) {
        p = ngx_sprintf(p, "<p>%s</p>\n", lorem_paragraphs[i]);
    }

    return content;
}

static u_char *
ngx_http_sayplease_generate_random_text(ngx_pool_t *pool, ngx_uint_t words)
{
    static const char *word_list[] = {
        "the", "be", "to", "of", "and", "a", "in", "that", "have", "I",
        "it", "for", "not", "on", "with", "he", "as", "you", "do", "at"
    };
    static const size_t num_words = sizeof(word_list) / sizeof(word_list[0]);

    size_t total_len = words * 10; // Average word length + space
    u_char *content, *p;
    ngx_uint_t i;

    content = ngx_pcalloc(pool, total_len + 1);
    if (content == NULL) {
        return NULL;
    }

    p = content;
    for (i = 0; i < words; i++) {
        size_t word_index = ngx_random() % num_words;
        p = ngx_sprintf(p, "%s ", word_list[word_index]);
    }

    return content;
}

static ngx_str_t *
ngx_http_sayplease_generate_honeypot_link(ngx_pool_t *pool, ngx_str_t *base_url)
{
    static const char *honeypot_paths[] = {
        "admin", "login", "private", "secret", "config",
        "backup", "db", "wp-admin", "administrator", "console"
    };
    static const size_t num_paths = sizeof(honeypot_paths) / sizeof(honeypot_paths[0]);

    ngx_str_t *link = ngx_pcalloc(pool, sizeof(ngx_str_t));
    if (link == NULL) {
        return NULL;
    }

    // Generate random path
    size_t path_index = ngx_random() % num_paths;
    
    // Allocate memory for the full URL
    link->data = ngx_pcalloc(pool, base_url->len + ngx_strlen(honeypot_paths[path_index]) + 2);
    if (link->data == NULL) {
        return NULL;
    }

    // Construct the URL
    ngx_sprintf(link->data, "%s/%s", base_url->data, honeypot_paths[path_index]);
    link->len = ngx_strlen(link->data);

    return link;
}

static ngx_int_t ngx_http_sayplease_init_cache(ngx_http_sayplease_main_conf_t *mcf)
{
    // Implementation of ngx_http_sayplease_init_cache function
    return NGX_OK; // Placeholder return, actual implementation needed
}

static ngx_int_t ngx_http_sayplease_cache_lookup(ngx_http_sayplease_main_conf_t *mcf, u_char *fingerprint)
{
    // Implementation of ngx_http_sayplease_cache_lookup function
    return NGX_OK; // Placeholder return, actual implementation needed
}

static void ngx_http_sayplease_cache_insert(ngx_http_sayplease_main_conf_t *mcf, u_char *fingerprint)
{
    // Implementation of ngx_http_sayplease_cache_insert function
}

static void ngx_http_sayplease_cache_cleanup(ngx_http_sayplease_main_conf_t *mcf)
{
    // Implementation of ngx_http_sayplease_cache_cleanup function
}

static void ngx_http_sayplease_cleanup_db(void *data)
{
    // Implementation of ngx_http_sayplease_cleanup_db function
}

/* Helper function to convert char* to ngx_str_t */
static ngx_str_t *
ngx_http_sayplease_str_create(ngx_pool_t *pool, const char *src) __attribute__((unused));

static ngx_str_t *
ngx_http_sayplease_str_create(ngx_pool_t *pool, const char *src)
{
    ngx_str_t *dst;
    size_t len;
    
    dst = ngx_palloc(pool, sizeof(ngx_str_t));
    if (dst == NULL) {
        return NULL;
    }
    
    len = ngx_strlen(src);
    dst->len = len;
    dst->data = ngx_pcalloc(pool, len + 1);
    if (dst->data == NULL) {
        return NULL;
    }
    
    ngx_memcpy(dst->data, src, len);
    
    return dst;
} 