-- RoboNope Bot Tracking Database Schema

-- Bot Requests Table
CREATE TABLE IF NOT EXISTS bot_requests (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    user_agent TEXT NOT NULL,
    ip_address TEXT NOT NULL,
    request_url TEXT NOT NULL,
    request_method TEXT NOT NULL,
    status_code INTEGER NOT NULL,
    response_size INTEGER NOT NULL,
    request_headers TEXT,
    matched_pattern TEXT,
    fingerprint TEXT UNIQUE
);

-- Create index on timestamp for faster queries
CREATE INDEX IF NOT EXISTS idx_bot_requests_timestamp ON bot_requests(timestamp);

-- Create index on user_agent for filtering by bot type
CREATE INDEX IF NOT EXISTS idx_bot_requests_user_agent ON bot_requests(user_agent);

-- Create index on fingerprint for duplicate detection
CREATE INDEX IF NOT EXISTS idx_bot_requests_fingerprint ON bot_requests(fingerprint);

-- Bot Statistics View
CREATE VIEW IF NOT EXISTS bot_statistics AS
SELECT 
    user_agent,
    COUNT(*) as total_requests,
    COUNT(DISTINCT ip_address) as unique_ips,
    COUNT(DISTINCT request_url) as unique_urls,
    MIN(timestamp) as first_seen,
    MAX(timestamp) as last_seen,
    AVG(response_size) as avg_response_size,
    COUNT(CASE WHEN status_code >= 400 THEN 1 END) as error_count
FROM bot_requests
GROUP BY user_agent;

-- Hourly Request Patterns View
CREATE VIEW IF NOT EXISTS hourly_patterns AS
SELECT 
    strftime('%Y-%m-%d %H:00:00', timestamp) as hour,
    user_agent,
    COUNT(*) as request_count
FROM bot_requests
GROUP BY hour, user_agent
ORDER BY hour DESC;

-- Pattern Violations View
CREATE VIEW IF NOT EXISTS pattern_violations AS
SELECT 
    matched_pattern,
    COUNT(*) as violation_count,
    COUNT(DISTINCT user_agent) as unique_bots,
    COUNT(DISTINCT ip_address) as unique_ips
FROM bot_requests
GROUP BY matched_pattern
ORDER BY violation_count DESC; 