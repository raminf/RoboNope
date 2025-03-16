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
static ngx_str_t *ngx_http_robonope_generate_honeypot_link(ngx_pool_t *pool, ngx_str_t *base_url, ngx_array_t *disallow_patterns, ngx_http_robonope_loc_conf_t *lcf);
static ngx_int_t ngx_http_robonope_send_response(ngx_http_request_t *r, u_char *content);
static u_char *ngx_http_robonope_generate_class_name(ngx_pool_t *pool);
static ngx_int_t ngx_http_robonope_serve_honeypot(ngx_http_request_t *r, ngx_http_robonope_loc_conf_t *lcf, ngx_str_t *honeypot_link);
static ngx_int_t ngx_http_robonope_load_robots_and_db(ngx_http_request_t *r, ngx_http_robonope_loc_conf_t *lcf, 
                                                    ngx_str_t **robots_content, ngx_array_t **disallow_patterns);
static ngx_int_t ngx_http_robonope_is_disallowed(ngx_http_request_t *r, ngx_array_t *disallow_patterns);
static ngx_int_t ngx_http_robonope_log_request(
#ifdef ROBONOPE_USE_DUCKDB
    duckdb_connection conn,
#else
    sqlite3 *db,
#endif
    ngx_http_request_t *r,
    ngx_str_t *matched_pattern);

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
    {
        ngx_string("robonope_instructions_url"),
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_robonope_loc_conf_t, instructions_url),
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
    
    conf->robots_path.data = NULL;
    conf->db_path.data = NULL;
    conf->static_content_path.data = NULL;
    conf->honeypot_class.data = NULL;
    conf->instructions_url.data = NULL;

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
    
    // Don't set a default value for instructions_url
    if (conf->instructions_url.data == NULL) {
        conf->instructions_url = prev->instructions_url;
    }

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
    ngx_http_robonope_loc_conf_t *lcf;
    ngx_str_t *robots_content = NULL;
    ngx_array_t *disallow_patterns = NULL;
    ngx_str_t *honeypot_link = NULL;
    ngx_str_t *user_agent = NULL;
    ngx_str_t ua_lowercase;
    size_t i;

    lcf = ngx_http_get_module_loc_conf(r, ngx_http_robonope_module);

    if (!lcf->enable) {
        return NGX_DECLINED;
    }

    /* Check if the User-Agent header exists and if it's a bot */
    if (r->headers_in.user_agent) {
        user_agent = &r->headers_in.user_agent->value;
        
        /* Convert user agent to lowercase for case-insensitive comparison */
        ua_lowercase.len = user_agent->len;
        ua_lowercase.data = ngx_pnalloc(r->pool, ua_lowercase.len);
        if (ua_lowercase.data == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
        
        for (i = 0; i < user_agent->len; i++) {
            ua_lowercase.data[i] = ngx_tolower(user_agent->data[i]);
        }
    }

    /* Load robots.txt and database */
    if (ngx_http_robonope_load_robots_and_db(r, lcf, &robots_content, &disallow_patterns) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* Check if the request URI is in the disallow patterns */
    if (!ngx_http_robonope_is_disallowed(r, disallow_patterns)) {
        return NGX_DECLINED;
    }

    /* Generate honeypot link - this will use instructions URL if redirect_to_instructions is enabled */
    honeypot_link = ngx_http_robonope_generate_honeypot_link(r->pool, &r->uri, disallow_patterns, lcf);
    if (honeypot_link == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    /* Serve honeypot content */
    return ngx_http_robonope_serve_honeypot(r, lcf, honeypot_link);
}

ngx_int_t
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
    // Check if db_path is valid
    if (db_path == NULL || db_path->data == NULL || db_path->len == 0) {
        return NGX_OK; // Return success but don't initialize the database
    }

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
ngx_http_robonope_generate_honeypot_link(ngx_pool_t *pool, ngx_str_t *base_url, ngx_array_t *disallow_patterns, ngx_http_robonope_loc_conf_t *lcf)
{
    ngx_str_t *link;
    u_char *p;
    size_t len;
    ngx_str_t *patterns;
    ngx_uint_t pattern_index;
    
    // Check if we should use the instructions URL for the honeypot link
    if (lcf != NULL && lcf->instructions_url.data != NULL && lcf->instructions_url.len > 0) {
        // Use the configured instructions URL as the honeypot link
        len = lcf->instructions_url.len;
        
        link = ngx_palloc(pool, sizeof(ngx_str_t));
        if (link == NULL) {
            return NULL;
        }
        
        p = ngx_palloc(pool, len + 1);
        if (p == NULL) {
            return NULL;
        }
        
        link->data = p;
        p = ngx_cpymem(p, lcf->instructions_url.data, len);
        *p = '\0';
        link->len = len;
        
        return link;
    }
    
    if (disallow_patterns == NULL || disallow_patterns->nelts == 0) {
        // Fallback to default honeypot link if no disallow patterns are available
        len = sizeof("/admin/index.html") - 1;
        
        link = ngx_palloc(pool, sizeof(ngx_str_t));
        if (link == NULL) {
            return NULL;
        }
        
        p = ngx_palloc(pool, len + 1);
        if (p == NULL) {
            return NULL;
        }
        
        link->data = p;
        p = ngx_cpymem(p, "/admin/index.html", len);
        *p = '\0';
        link->len = len;
        
        return link;
    }
    
    // Select a random pattern from the disallow list
    patterns = disallow_patterns->elts;
    pattern_index = ngx_random() % disallow_patterns->nelts;
    
    // Create a link based on the selected pattern
    len = patterns[pattern_index].len;
    
    // Add some randomness to the path
    static const char *extensions[] = {
        "index.html", "login.php", "data.json", "config.xml", "settings.html"
    };
    static const size_t num_extensions = sizeof(extensions) / sizeof(extensions[0]);
    const char *extension = extensions[ngx_random() % num_extensions];
    size_t ext_len = ngx_strlen(extension);
    
    link = ngx_palloc(pool, sizeof(ngx_str_t));
    if (link == NULL) {
        return NULL;
    }
    
    // Allocate memory for pattern + "/" + extension + null terminator
    p = ngx_palloc(pool, len + 1 + ext_len + 1);
    if (p == NULL) {
        return NULL;
    }
    
    link->data = p;
    
    // Copy pattern
    p = ngx_cpymem(p, patterns[pattern_index].data, patterns[pattern_index].len);
    
    // Add slash if the pattern doesn't end with one
    if (p > link->data && *(p-1) != '/') {
        *p++ = '/';
    }
    
    // Add random extension
    p = ngx_cpymem(p, extension, ext_len);
    
    *p = '\0';
    link->len = p - link->data;
    
    return link;
}

ngx_int_t 
ngx_http_robonope_init_cache(ngx_http_robonope_main_conf_t *mcf)
{
    // Implementation of ngx_http_robonope_init_cache function
    return NGX_OK; // Placeholder return, actual implementation needed
}

ngx_int_t 
ngx_http_robonope_cache_lookup(ngx_http_robonope_main_conf_t *mcf, u_char *fingerprint)
{
    // Implementation of ngx_http_robonope_cache_lookup function
    return NGX_OK; // Placeholder return, actual implementation needed
}

void 
ngx_http_robonope_cache_insert(ngx_http_robonope_main_conf_t *mcf, u_char *fingerprint)
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

static ngx_int_t
ngx_http_robonope_serve_honeypot(ngx_http_request_t *r, ngx_http_robonope_loc_conf_t *lcf, ngx_str_t *honeypot_link)
{
    u_char *content, *body;
    u_char *random_class;
    size_t total_len;

    // Generate random class name
    random_class = ngx_http_robonope_generate_class_name(r->pool);
    if (random_class == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // Generate body content - always use random text
    body = ngx_http_robonope_generate_random_text(r->pool, 50);
    if (body == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    // Calculate total length needed
    total_len = ngx_strlen(body) + honeypot_link->len + 500; // Extra space for HTML structure

    content = ngx_pcalloc(r->pool, total_len);
    if (content == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
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

    // Send the response
    return ngx_http_robonope_send_response(r, content);
}

static ngx_int_t
ngx_http_robonope_load_robots_and_db(ngx_http_request_t *r, ngx_http_robonope_loc_conf_t *lcf, 
                                    ngx_str_t **robots_content, ngx_array_t **disallow_patterns)
{
    ngx_http_robonope_main_conf_t *mcf;
    
    mcf = ngx_http_get_module_main_conf(r, ngx_http_robonope_module);
    if (mcf == NULL) {
        return NGX_ERROR;
    }
    
    // Initialize robots.txt if not already done
    if (mcf->robot_entries->nelts == 0) {
        if (ngx_http_robonope_load_robots(mcf, &lcf->robots_path) != NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "failed to load robots.txt");
            return NGX_ERROR;
        }
    }
    
    // Initialize database if not already done and db_path is set
    if (mcf->db == NULL && lcf->db_path.data != NULL && lcf->db_path.len > 0) {
        if (ngx_http_robonope_init_db(mcf, &lcf->db_path) != NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "failed to initialize database");
            // Continue even if database initialization fails
            // This allows the module to function without logging
        }
    }
    
    // Create a combined array of all disallow patterns
    *disallow_patterns = ngx_array_create(r->pool, 10, sizeof(ngx_str_t));
    if (*disallow_patterns == NULL) {
        return NGX_ERROR;
    }
    
    // Collect all disallow patterns from all user-agent entries
    for (ngx_uint_t i = 0; i < mcf->robot_entries->nelts; i++) {
        ngx_http_robonope_robot_entry_t *entry = &((ngx_http_robonope_robot_entry_t *)mcf->robot_entries->elts)[i];
        
        if (entry->disallow == NULL || entry->disallow->nelts == 0) {
            continue;
        }
        
        ngx_str_t *patterns = entry->disallow->elts;
        for (ngx_uint_t j = 0; j < entry->disallow->nelts; j++) {
            ngx_str_t *pattern = ngx_array_push(*disallow_patterns);
            if (pattern == NULL) {
                return NGX_ERROR;
            }
            
            pattern->len = patterns[j].len;
            pattern->data = patterns[j].data;
        }
    }
    
    return NGX_OK;
}

static ngx_int_t
ngx_http_robonope_is_disallowed(ngx_http_request_t *r, ngx_array_t *disallow_patterns)
{
    ngx_str_t *patterns;
    ngx_str_t matched_pattern;
    ngx_http_robonope_main_conf_t *mcf;
    ngx_http_robonope_loc_conf_t *lcf;
    ngx_md5_t md5;
    u_char fingerprint[16];
    
    if (disallow_patterns == NULL || disallow_patterns->nelts == 0) {
        return 0; // Not disallowed if no patterns
    }
    
    mcf = ngx_http_get_module_main_conf(r, ngx_http_robonope_module);
    lcf = ngx_http_get_module_loc_conf(r, ngx_http_robonope_module);
    if (mcf == NULL || lcf == NULL) {
        return 0;
    }
    
    patterns = disallow_patterns->elts;
    
    // Check each disallow pattern
    for (ngx_uint_t i = 0; i < disallow_patterns->nelts; i++) {
        if (ngx_strncmp(r->uri.data, patterns[i].data, patterns[i].len) == 0) {
            matched_pattern = patterns[i];
            
            // Generate MD5 fingerprint of client IP and user agent
            ngx_md5_init(&md5);
            ngx_md5_update(&md5, r->connection->addr_text.data, r->connection->addr_text.len);
            
            if (r->headers_in.user_agent != NULL) {
                ngx_md5_update(&md5, r->headers_in.user_agent->value.data, r->headers_in.user_agent->value.len);
            }
            
            ngx_md5_final(fingerprint, &md5);
            
            // Log request only if database path is set
            if (lcf->db_path.data != NULL && lcf->db_path.len > 0 && mcf->db != NULL) {
                ngx_http_robonope_log_request(
                    mcf->db,
                    r,
                    &matched_pattern
                );
            }
            
            // Add client to cache if not already there
            if (ngx_http_robonope_cache_lookup(mcf, fingerprint) != NGX_OK) {
                ngx_http_robonope_cache_insert(mcf, fingerprint);
            }
            
            return 1; // URL is disallowed
        }
    }
    
    return 0; // URL is not disallowed
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