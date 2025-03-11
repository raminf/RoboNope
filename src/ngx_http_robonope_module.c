#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/* Optional modules configuration */
#ifndef NGX_HTTP_SSL
#define NGX_HTTP_SSL 0
#endif

#ifndef NGX_HTTP_SSI
#define NGX_HTTP_SSI 0
#endif

#if (NGX_HTTP_SSL)
#include <ngx_http_ssl_module.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>  /* For stat() */

#ifdef ROBONOPE_USE_DUCKDB
#include <duckdb.h>
#else
#include <sqlite3.h>
#endif

#include "ngx_http_robonope_module.h"

// Function declarations for internal use only
static void *ngx_http_robonope_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_robonope_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static void *ngx_http_robonope_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_robonope_init_main_conf(ngx_conf_t *cf, void *conf);
static ngx_int_t ngx_http_robonope_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_robonope_handler(ngx_http_request_t *r);
static void ngx_http_robonope_cleanup_db(void *data);
static void ngx_http_robonope_cache_cleanup(ngx_http_robonope_main_conf_t *mcf) __attribute__((unused));
static u_char *ngx_http_robonope_generate_random_text(ngx_pool_t *pool, ngx_uint_t words);
static ngx_str_t *ngx_http_robonope_generate_honeypot_link(ngx_pool_t *pool, ngx_str_t *base_url);
static ngx_int_t ngx_http_robonope_send_response(ngx_http_request_t *r, u_char *content);
static u_char *ngx_http_robonope_generate_class_name(ngx_pool_t *pool);

static ngx_command_t ngx_http_robonope_commands[] = {
    {
        ngx_string("robonope_enable"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_robonope_loc_conf_t, enable),
        NULL
    },
    {
        ngx_string("robonope_robots_path"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_robonope_loc_conf_t, robots_path),
        NULL
    },
    {
        ngx_string("robonope_db_path"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_robonope_loc_conf_t, db_path),
        NULL
    },
    {
        ngx_string("robonope_static_content_path"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_robonope_loc_conf_t, static_content_path),
        NULL
    },
    {
        ngx_string("robonope_dynamic_content"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_robonope_loc_conf_t, dynamic_content),
        NULL
    },
    {
        ngx_string("robonope_cache_ttl"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_robonope_loc_conf_t, cache_ttl),
        NULL
    },
    {
        ngx_string("robonope_max_cache_entries"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_num_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_robonope_loc_conf_t, max_cache_entries),
        NULL
    },
    {
        ngx_string("robonope_honeypot_class"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_robonope_loc_conf_t, honeypot_class),
        NULL
    },
    {
        ngx_string("robonope_use_lorem_ipsum"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_FLAG,
        ngx_conf_set_flag_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_robonope_loc_conf_t, use_lorem_ipsum),
        NULL
    },
    ngx_null_command
};

static ngx_http_module_t ngx_http_robonope_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_robonope_init,              /* postconfiguration */
    ngx_http_robonope_create_main_conf,  /* create main configuration */
    ngx_http_robonope_init_main_conf,    /* init main configuration */
    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */
    ngx_http_robonope_create_loc_conf,   /* create location configuration */
    ngx_http_robonope_merge_loc_conf     /* merge location configuration */
};

ngx_module_t ngx_http_robonope_module = {
    NGX_MODULE_V1,
    &ngx_http_robonope_module_ctx,    /* module context */
    ngx_http_robonope_commands,       /* module directives */
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
ngx_http_robonope_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_robonope_main_conf_t *mcf;

    mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_robonope_main_conf_t));
    if (mcf == NULL) {
        return NULL;
    }

    mcf->cache_pool = ngx_create_pool(4096, cf->log);
    if (mcf->cache_pool == NULL) {
        return NULL;
    }

    mcf->robot_entries = ngx_array_create(cf->pool, 10, sizeof(ngx_http_robonope_robot_entry_t));
    if (mcf->robot_entries == NULL) {
        ngx_destroy_pool(mcf->cache_pool);
        return NULL;
    }

    return mcf;
}

static char *
ngx_http_robonope_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_robonope_loc_conf_t *lcf;

    lcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_robonope_module);
    if (lcf == NULL) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}

static void *
ngx_http_robonope_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_robonope_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_robonope_loc_conf_t));
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
ngx_http_robonope_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_robonope_loc_conf_t *prev = parent;
    ngx_http_robonope_loc_conf_t *conf = child;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_value(conf->dynamic_content, prev->dynamic_content, 1);
    ngx_conf_merge_str_value(conf->robots_path, prev->robots_path, "/etc/nginx/robots.txt");
    ngx_conf_merge_str_value(conf->db_path, prev->db_path, "/var/lib/nginx/robonope.db");
    ngx_conf_merge_str_value(conf->static_content_path, prev->static_content_path, "/etc/nginx/robonope_static");
    ngx_conf_merge_uint_value(conf->cache_ttl, prev->cache_ttl, 3600);
    ngx_conf_merge_uint_value(conf->max_cache_entries, prev->max_cache_entries, NGX_HTTP_ROBONOPE_MAX_CACHE);
    ngx_conf_merge_str_value(conf->honeypot_class, prev->honeypot_class, "honeypot");
    ngx_conf_merge_value(conf->use_lorem_ipsum, prev->use_lorem_ipsum, 1);

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_robonope_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt *h;
    ngx_http_core_main_conf_t *cmcf;
    ngx_pool_cleanup_t *cln;
    ngx_http_robonope_main_conf_t *mcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_robonope_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_robonope_handler;

    // Initialize cache
    if (ngx_http_robonope_init_cache(mcf) != NGX_OK) {
        return NGX_ERROR;
    }

    // Register cleanup handler
    cln = ngx_pool_cleanup_add(cf->pool, 0);
    if (cln == NULL) {
        return NGX_ERROR;
    }

    cln->handler = ngx_http_robonope_cleanup_db;
    cln->data = mcf;

    return NGX_OK;
}

static ngx_int_t
ngx_http_robonope_handler(ngx_http_request_t *r)
{
    ngx_http_robonope_main_conf_t *mcf;
    ngx_http_robonope_loc_conf_t *lcf;
    ngx_md5_t md5;
    u_char fingerprint[16];

    mcf = ngx_http_get_module_main_conf(r, ngx_http_robonope_module);
    lcf = ngx_http_get_module_loc_conf(r, ngx_http_robonope_module);

    if (!lcf->enable) {
        return NGX_DECLINED;
    }

    // Check if User-Agent header exists
    if (r->headers_in.user_agent == NULL) {
        return NGX_DECLINED;
    }

    // Initialize robots.txt and DB if not already done
    if (mcf->robot_entries->nelts == 0) {
        if (ngx_http_robonope_load_robots(mcf, &lcf->robots_path) != NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "failed to load robots.txt");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    if (mcf->db == NULL) {
        if (ngx_http_robonope_init_db(mcf, &lcf->db_path) != NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "failed to initialize database");
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    // Check if URL matches any disallow pattern
    for (ngx_uint_t i = 0; i < mcf->robot_entries->nelts; i++) {
        ngx_http_robonope_robot_entry_t *entry = &((ngx_http_robonope_robot_entry_t *)mcf->robot_entries->elts)[i];
        
        // Skip if no disallow patterns
        if (entry->disallow == NULL || entry->disallow->nelts == 0) {
            continue;
        }
        
        ngx_str_t *pattern = entry->disallow->elts;
        
        // Check each disallow pattern
        for (ngx_uint_t j = 0; j < entry->disallow->nelts; j++) {
            if (ngx_strncmp(r->uri.data, pattern[j].data, pattern[j].len) == 0) {
                ngx_str_t matched_pattern = pattern[j];
                
                // Generate MD5 fingerprint of client IP
                ngx_md5_init(&md5);
                ngx_md5_update(&md5, r->connection->addr_text.data, r->connection->addr_text.len);
                ngx_md5_update(&md5, r->headers_in.user_agent->value.data, r->headers_in.user_agent->value.len);
                ngx_md5_final(fingerprint, &md5);
                
                // Check if client is already in cache
                if (ngx_http_robonope_cache_lookup(mcf, fingerprint) == NGX_OK) {
                    // Return cached response
                    u_char *content = ngx_http_robonope_generate_content(r->pool, &r->uri, entry->disallow);
                    if (content == NULL) {
                        return NGX_HTTP_INTERNAL_SERVER_ERROR;
                    }
                    
                    // Log request
                    ngx_http_robonope_log_request(
                        mcf->db,
                        r,
                        &matched_pattern
                    );
                    
                    // Send response
                    return ngx_http_robonope_send_response(r, content);
                } else {
                    // Add client to cache
                    ngx_http_robonope_cache_insert(mcf, fingerprint);
                    
                    // Generate new content
                    u_char *content = ngx_http_robonope_generate_content(r->pool, &r->uri, entry->disallow);
                    if (content == NULL) {
                        return NGX_HTTP_INTERNAL_SERVER_ERROR;
                    }
                    
                    // Log request
                    ngx_http_robonope_log_request(
                        mcf->db,
                        r,
                        &matched_pattern
                    );
                    
                    // Send response
                    return ngx_http_robonope_send_response(r, content);
                }
            }
        }
    }
    
    // No match found, pass to next handler
    return NGX_DECLINED;
}

static ngx_int_t
ngx_http_robonope_load_robots(ngx_http_robonope_main_conf_t *mcf, ngx_str_t *robots_path)
{
    ngx_fd_t fd;
    ngx_file_t file;
    char *buf, *line, *directive, *value;
    ngx_http_robonope_robot_entry_t *entry = NULL;
    size_t size;
    ssize_t n;

    if (mcf == NULL || robots_path == NULL || mcf->cache_pool == NULL) {
        return NGX_ERROR;
    }

    fd = ngx_open_file(robots_path->data, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        return NGX_ERROR;
    }

    ngx_memzero(&file, sizeof(ngx_file_t));
    file.fd = fd;
    file.name = *robots_path;

    struct stat sb;
    if (stat((const char *)robots_path->data, &sb) == -1) {
        ngx_close_file(fd);
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
    char *saveptr1 = NULL, *saveptr2 = NULL;
    line = strtok_r(buf, "\n", &saveptr1);

    while (line != NULL) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0') {
            line = strtok_r(NULL, "\n", &saveptr1);
            continue;
        }

        directive = strtok_r(line, ":", &saveptr2);
        if (directive == NULL) {
            line = strtok_r(NULL, "\n", &saveptr1);
            continue;
        }

        value = strtok_r(NULL, "\n", &saveptr2);
        if (value == NULL) {
            line = strtok_r(NULL, "\n", &saveptr1);
            continue;
        }

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
            if (entry->allow == NULL) {
                return NGX_ERROR;
            }
            entry->disallow = ngx_array_create(mcf->cache_pool, 4, sizeof(ngx_str_t));
            if (entry->disallow == NULL) {
                return NGX_ERROR;
            }
        }
        else if (entry != NULL) {
            ngx_str_t *pattern = NULL;
            if (ngx_strncasecmp((u_char *)"Allow", (u_char *)directive, 5) == 0) {
                pattern = ngx_array_push(entry->allow);
            }
            else if (ngx_strncasecmp((u_char *)"Disallow", (u_char *)directive, 8) == 0) {
                pattern = ngx_array_push(entry->disallow);
            }
            else {
                line = strtok_r(NULL, "\n", &saveptr1);
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

        line = strtok_r(NULL, "\n", &saveptr1);
    }

    return NGX_OK;
}

static ngx_int_t
ngx_http_robonope_init_db(ngx_http_robonope_main_conf_t *mcf, ngx_str_t *db_path)
{
#ifdef ROBONOPE_USE_DUCKDB
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
ngx_http_robonope_generate_content(ngx_pool_t *pool, ngx_str_t *url, ngx_array_t *disallow_patterns)
{
    /* We can't access the location configuration here, so we'll use default behavior */
    u_char *content, *body;
    ngx_str_t *honeypot_link;
    size_t total_len;
    u_char *random_class;

    // Generate random class name
    random_class = ngx_http_robonope_generate_class_name(pool);
    if (random_class == NULL) {
        return NULL;
    }

    // Generate body content - always use random text now
    body = ngx_http_robonope_generate_random_text(pool, 50);
    if (body == NULL) {
        return NULL;
    }

    // Generate honeypot link
    honeypot_link = ngx_http_robonope_generate_honeypot_link(pool, url);
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
        "<div class=\"content\">\n"
        "%s\n"
        "</div>\n"
        "<a href=\"%s\" class=\"%s\">Important Information</a>\n"
        "</body>\n"
        "</html>",
        random_class,
        body,
        honeypot_link->data,
        random_class);

    return content;
}

static ngx_int_t
ngx_http_robonope_log_request(
#ifdef ROBONOPE_USE_DUCKDB
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

#ifdef ROBONOPE_USE_DUCKDB
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
ngx_http_robonope_generate_random_text(ngx_pool_t *pool, ngx_uint_t words)
{
    static const char *subjects[] = {
        "the system", "our network", "the server", "this page", "the website",
        "the database", "the service", "the platform", "the application", "the interface"
    };
    
    static const char *verbs[] = {
        "processes", "manages", "handles", "analyzes", "monitors",
        "validates", "updates", "maintains", "controls", "optimizes"
    };
    
    static const char *objects[] = {
        "data requests", "user sessions", "network traffic", "system resources",
        "security protocols", "access permissions", "configuration settings",
        "database connections", "cache entries", "service endpoints"
    };
    
    static const char *adjectives[] = {
        "secure", "efficient", "reliable", "dynamic", "automated",
        "integrated", "optimized", "scalable", "robust", "advanced"
    };
    
    static const char *adverbs[] = {
        "automatically", "efficiently", "securely", "dynamically", "continuously",
        "reliably", "seamlessly", "actively", "intelligently", "effectively"
    };
    
    static const char *conjunctions[] = {
        "while", "and", "as", "because", "although",
        "however", "therefore", "moreover", "furthermore", "additionally"
    };

    static const size_t num_subjects = sizeof(subjects) / sizeof(subjects[0]);
    static const size_t num_verbs = sizeof(verbs) / sizeof(verbs[0]);
    static const size_t num_objects = sizeof(objects) / sizeof(objects[0]);
    static const size_t num_adjectives = sizeof(adjectives) / sizeof(adjectives[0]);
    static const size_t num_adverbs = sizeof(adverbs) / sizeof(adverbs[0]);
    static const size_t num_conjunctions = sizeof(conjunctions) / sizeof(conjunctions[0]);

    size_t total_len = words * 20; // Average word length + space + punctuation
    u_char *content, *p;
    ngx_uint_t i, sentences;
    
    content = ngx_pcalloc(pool, total_len + 1);
    if (content == NULL) {
        return NULL;
    }
    
    p = content;
    sentences = words / 10 + 1; // Create a new sentence every ~10 words
    
    for (i = 0; i < sentences; i++) {
        // Basic sentence patterns
        switch (ngx_random() % 3) {
            case 0: // Pattern: Subject + Verb + Object
                p = ngx_sprintf(p, "%s %s %s. ",
                    subjects[ngx_random() % num_subjects],
                    verbs[ngx_random() % num_verbs],
                    objects[ngx_random() % num_objects]);
                break;
                
            case 1: // Pattern: Subject + Adverb + Verb + Adjective + Object
                p = ngx_sprintf(p, "%s %s %s %s %s. ",
                    subjects[ngx_random() % num_subjects],
                    adverbs[ngx_random() % num_adverbs],
                    verbs[ngx_random() % num_verbs],
                    adjectives[ngx_random() % num_adjectives],
                    objects[ngx_random() % num_objects]);
                break;
                
            case 2: // Pattern: Conjunction + Subject + Verb + Object
                p = ngx_sprintf(p, "%s %s %s %s. ",
                    conjunctions[ngx_random() % num_conjunctions],
                    subjects[ngx_random() % num_subjects],
                    verbs[ngx_random() % num_verbs],
                    objects[ngx_random() % num_objects]);
                break;
        }
    }
    
    return content;
}

static ngx_str_t *
ngx_http_robonope_generate_honeypot_link(ngx_pool_t *pool, ngx_str_t *base_url)
{
    ngx_str_t *link;
    u_char *p;
    size_t len;
    
    // Create a honeypot link based on the base URL
    len = base_url->len + sizeof("/admin/login") - 1;
    
    link = ngx_palloc(pool, sizeof(ngx_str_t));
    if (link == NULL) {
        return NULL;
    }
    
    p = ngx_palloc(pool, len + 1);
    if (p == NULL) {
        return NULL;
    }
    
    link->data = p;
    
    // Copy base URL
    p = ngx_cpymem(p, base_url->data, base_url->len);
    
    // Add honeypot path
    p = ngx_cpymem(p, "/admin/login", sizeof("/admin/login") - 1);
    
    *p = '\0';
    link->len = p - link->data;
    
    return link;
}

static ngx_int_t ngx_http_robonope_init_cache(ngx_http_robonope_main_conf_t *mcf)
{
    // Implementation of ngx_http_robonope_init_cache function
    return NGX_OK; // Placeholder return, actual implementation needed
}

static ngx_int_t ngx_http_robonope_cache_lookup(ngx_http_robonope_main_conf_t *mcf, u_char *fingerprint)
{
    // Implementation of ngx_http_robonope_cache_lookup function
    return NGX_OK; // Placeholder return, actual implementation needed
}

static void ngx_http_robonope_cache_insert(ngx_http_robonope_main_conf_t *mcf, u_char *fingerprint)
{
    // Implementation of ngx_http_robonope_cache_insert function
}

static void ngx_http_robonope_cache_cleanup(ngx_http_robonope_main_conf_t *mcf)
{
    // Implementation of ngx_http_robonope_cache_cleanup function
}

static void ngx_http_robonope_cleanup_db(void *data)
{
    // Implementation of ngx_http_robonope_cleanup_db function
}

/* Helper function to convert char* to ngx_str_t */
static ngx_str_t *
ngx_http_robonope_str_create(ngx_pool_t *pool, const char *src) __attribute__((unused));

static ngx_str_t *
ngx_http_robonope_str_create(ngx_pool_t *pool, const char *src)
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

static ngx_int_t
ngx_http_robonope_send_response(ngx_http_request_t *r, u_char *content)
{
    ngx_buf_t *b;
    ngx_chain_t out;
    
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

/* Helper function to generate a random CSS class name */
static u_char *
ngx_http_robonope_generate_class_name(ngx_pool_t *pool)
{
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const size_t charset_size = sizeof(charset) - 1;
    static const size_t class_length = 12; // Random length between 8-16 chars
    
    u_char *class_name = ngx_pcalloc(pool, class_length + 1);
    if (class_name == NULL) {
        return NULL;
    }
    
    // First character must be a letter
    class_name[0] = charset[ngx_random() % 52];
    
    // Rest can be any character from charset
    for (size_t i = 1; i < class_length; i++) {
        class_name[i] = charset[ngx_random() % charset_size];
    }
    
    return class_name;
} 