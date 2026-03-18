#include "data_feeds.h"
#include "vendor/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <unistd.h>
#include <libgen.h>

// Global feeds instance
DataFeeds g_feeds;

// Base directory (resolved from binary location)
static char g_base_dir[1024] = "";

// File paths for JSON data (live feeds in /tmp)
#define NEWS_PATH "/tmp/cric_news.json"
#define MASTODON_PATH "/tmp/cric_mastodon.json"
#define WIFI_PATH "/tmp/cric_wifi.json"

// Offline fallback filenames (relative to binary dir)
#define NEWS_OFFLINE_REL "data/news_offline.json"
#define MASTODON_OFFLINE_REL "data/mastodon_offline.json"

// Resolved offline paths (filled by feeds_set_base_dir)
static char news_offline_path[1024] = "";
static char mastodon_offline_path[1024] = "";

void feeds_set_base_dir(const char* exe_path) {
    // Try /proc/self/exe first (Linux)
    char resolved[1024];
    ssize_t len = readlink("/proc/self/exe", resolved, sizeof(resolved) - 1);
    if (len > 0) {
        resolved[len] = '\0';
    } else if (exe_path) {
        // Fallback: use realpath on argv[0]
        char* rp = realpath(exe_path, resolved);
        if (!rp) {
            strncpy(resolved, exe_path, sizeof(resolved) - 1);
            resolved[sizeof(resolved) - 1] = '\0';
        }
    } else {
        g_base_dir[0] = '\0';
        return;
    }

    // Get directory component
    char* dir = dirname(resolved);
    strncpy(g_base_dir, dir, sizeof(g_base_dir) - 1);
    g_base_dir[sizeof(g_base_dir) - 1] = '\0';

    // Build absolute offline paths
    snprintf(news_offline_path, sizeof(news_offline_path), "%s/%s", g_base_dir, NEWS_OFFLINE_REL);
    snprintf(mastodon_offline_path, sizeof(mastodon_offline_path), "%s/%s", g_base_dir, MASTODON_OFFLINE_REL);

    fprintf(stderr, "DEBUG: feeds base dir: %s\n", g_base_dir);
}

const char* feeds_get_base_dir(void) {
    return g_base_dir;
}

// Threat keywords for Mastodon post classification
static const char* threat_keywords_high[] = {
    "revolution", "hack", "exploit", "attack", "breach",
    "overthrow", "destroy", "weapon", "war", "bomb",
};
#define THREAT_HIGH_COUNT 10

static const char* threat_keywords_elevated[] = {
    "surveillance", "privacy", "resistance", "protest", "rebel",
    "encrypt", "anonymous", "leak", "whistleblow", "dissent",
    "activist", "censorship", "freedom", "liberation",
};
#define THREAT_ELEVATED_COUNT 14

// Case-insensitive substring search
static bool str_contains_ci(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    int hlen = (int)strlen(haystack);
    int nlen = (int)strlen(needle);
    if (nlen > hlen) return false;

    for (int i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (int j = 0; j < nlen; j++) {
            char h = haystack[i + j];
            char n = needle[j];
            if (h >= 'A' && h <= 'Z') h += 32;
            if (n >= 'A' && n <= 'Z') n += 32;
            if (h != n) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

// Classify Mastodon post threat level based on content keywords
static void classify_threat(MastodonPost* post) {
    // Check high-threat keywords
    for (int i = 0; i < THREAT_HIGH_COUNT; i++) {
        if (str_contains_ci(post->content, threat_keywords_high[i])) {
            strncpy(post->threat_level, "HIGH", sizeof(post->threat_level) - 1);
            return;
        }
    }
    // Check elevated keywords
    for (int i = 0; i < THREAT_ELEVATED_COUNT; i++) {
        if (str_contains_ci(post->content, threat_keywords_elevated[i])) {
            strncpy(post->threat_level, "ELEVATED", sizeof(post->threat_level) - 1);
            return;
        }
    }
    // Multiple keywords = CRITICAL
    int keyword_count = 0;
    for (int i = 0; i < THREAT_HIGH_COUNT; i++) {
        if (str_contains_ci(post->content, threat_keywords_high[i])) keyword_count++;
    }
    if (keyword_count >= 2) {
        strncpy(post->threat_level, "CRITICAL", sizeof(post->threat_level) - 1);
        return;
    }

    strncpy(post->threat_level, "NORMAL", sizeof(post->threat_level) - 1);
}

// Get file modification time, returns 0 if file doesn't exist
static time_t file_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return st.st_mtime;
}

// Read entire file into malloc'd buffer (caller frees)
static char* read_file(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 10 * 1024 * 1024) { // max 10MB
        fclose(f);
        return NULL;
    }

    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t read = fread(buf, 1, size, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

// Strip HTML tags from a string in-place
static void strip_html(char* str) {
    if (!str) return;
    char* read = str;
    char* write = str;
    bool in_tag = false;

    while (*read) {
        if (*read == '<') {
            in_tag = true;
        } else if (*read == '>') {
            in_tag = false;
        } else if (!in_tag) {
            *write++ = *read;
        }
        read++;
    }
    *write = '\0';
}

static void load_news(DataFeeds* feeds) {
    char* json_str = read_file(NEWS_PATH);
    if (!json_str && news_offline_path[0]) json_str = read_file(news_offline_path);
    if (!json_str) return;

    cJSON* root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) return;

    feeds->headline_count = 0;

    // Expect array of objects
    if (cJSON_IsArray(root)) {
        int count = cJSON_GetArraySize(root);
        if (count > MAX_HEADLINES) count = MAX_HEADLINES;

        for (int i = 0; i < count; i++) {
            cJSON* item = cJSON_GetArrayItem(root, i);
            if (!cJSON_IsObject(item)) continue;

            NewsHeadline* h = &feeds->headlines[feeds->headline_count];
            memset(h, 0, sizeof(NewsHeadline));

            cJSON* text = cJSON_GetObjectItem(item, "text");
            if (text && cJSON_IsString(text)) {
                strncpy(h->text, text->valuestring, MAX_TEXT_LEN - 1);
            }

            cJSON* source = cJSON_GetObjectItem(item, "source");
            if (source && cJSON_IsString(source)) {
                strncpy(h->source, source->valuestring, sizeof(h->source) - 1);
            }

            cJSON* bias = cJSON_GetObjectItem(item, "bias");
            if (bias && cJSON_IsNumber(bias)) {
                h->bias = (float)bias->valuedouble;
            }

            cJSON* sentiment = cJSON_GetObjectItem(item, "sentiment");
            if (sentiment && cJSON_IsNumber(sentiment)) {
                h->sentiment = (float)sentiment->valuedouble;
            }

            cJSON* emotion = cJSON_GetObjectItem(item, "emotion");
            if (emotion && cJSON_IsString(emotion)) {
                strncpy(h->emotion, emotion->valuestring, sizeof(h->emotion) - 1);
            }

            feeds->headline_count++;
        }
    }

    cJSON_Delete(root);
    feeds->headlines_loaded_at = time(NULL);
}

static void load_mastodon(DataFeeds* feeds) {
    char* json_str = read_file(MASTODON_PATH);
    if (!json_str && mastodon_offline_path[0]) json_str = read_file(mastodon_offline_path);
    if (!json_str) return;

    cJSON* root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) return;

    feeds->post_count = 0;

    if (cJSON_IsArray(root)) {
        int count = cJSON_GetArraySize(root);
        if (count > MAX_POSTS) count = MAX_POSTS;

        for (int i = 0; i < count; i++) {
            cJSON* item = cJSON_GetArrayItem(root, i);
            if (!cJSON_IsObject(item)) continue;

            MastodonPost* p = &feeds->posts[feeds->post_count];
            memset(p, 0, sizeof(MastodonPost));

            // Extract content (may have HTML)
            cJSON* content = cJSON_GetObjectItem(item, "content");
            if (content && cJSON_IsString(content)) {
                strncpy(p->content, content->valuestring, sizeof(p->content) - 1);
                strip_html(p->content);
            }

            // Extract account username
            cJSON* account = cJSON_GetObjectItem(item, "account");
            if (account && cJSON_IsObject(account)) {
                cJSON* username = cJSON_GetObjectItem(account, "username");
                if (username && cJSON_IsString(username)) {
                    strncpy(p->handle, username->valuestring, sizeof(p->handle) - 1);
                }
            }

            // Classify threat level
            classify_threat(p);

            feeds->post_count++;
        }
    }

    cJSON_Delete(root);
    feeds->posts_loaded_at = time(NULL);
}

static void load_wifi(DataFeeds* feeds) {
    char* json_str = read_file(WIFI_PATH);
    if (!json_str) return;

    cJSON* root = cJSON_Parse(json_str);
    free(json_str);
    if (!root) return;

    feeds->device_count = 0;

    if (cJSON_IsArray(root)) {
        int count = cJSON_GetArraySize(root);
        if (count > MAX_WIFI_DEVICES) count = MAX_WIFI_DEVICES;

        for (int i = 0; i < count; i++) {
            cJSON* item = cJSON_GetArrayItem(root, i);
            if (!cJSON_IsObject(item)) continue;

            WifiDevice* d = &feeds->devices[feeds->device_count];
            memset(d, 0, sizeof(WifiDevice));

            cJSON* ssid = cJSON_GetObjectItem(item, "ssid");
            if (ssid && cJSON_IsString(ssid)) {
                strncpy(d->ssid, ssid->valuestring, sizeof(d->ssid) - 1);
            }

            cJSON* mac = cJSON_GetObjectItem(item, "mac");
            if (mac && cJSON_IsString(mac)) {
                strncpy(d->mac, mac->valuestring, sizeof(d->mac) - 1);
            }

            cJSON* signal = cJSON_GetObjectItem(item, "signal");
            if (signal && cJSON_IsNumber(signal)) {
                d->signal = signal->valueint;
            }

            feeds->device_count++;
        }
    }

    cJSON_Delete(root);
    feeds->devices_loaded_at = time(NULL);
}

void feeds_init(DataFeeds* feeds) {
    memset(feeds, 0, sizeof(DataFeeds));

    // Attempt initial load
    load_news(feeds);
    load_mastodon(feeds);
    load_wifi(feeds);
}

void feeds_poll(DataFeeds* feeds) {
    // Check file modification times and reload if changed
    time_t news_mtime = file_mtime(NEWS_PATH);
    if (news_mtime > 0 && news_mtime != feeds->headlines_loaded_at) {
        load_news(feeds);
    }

    time_t mastodon_mtime = file_mtime(MASTODON_PATH);
    if (mastodon_mtime > 0 && mastodon_mtime != feeds->posts_loaded_at) {
        load_mastodon(feeds);
    }

    time_t wifi_mtime = file_mtime(WIFI_PATH);
    if (wifi_mtime > 0 && wifi_mtime != feeds->devices_loaded_at) {
        load_wifi(feeds);
    }
}

void feeds_load_lore(DataFeeds* feeds, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) return;

    feeds->lore_count = 0;
    char line[MAX_TEXT_LEN];
    char current_fragment[MAX_TEXT_LEN] = "";

    while (fgets(line, sizeof(line), f) && feeds->lore_count < MAX_LORE_FRAGMENTS) {
        // Remove trailing newline
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        if (len == 0) {
            // Empty line = fragment separator
            if (strlen(current_fragment) > 0) {
                strncpy(feeds->lore_fragments[feeds->lore_count], current_fragment, MAX_TEXT_LEN - 1);
                feeds->lore_count++;
                current_fragment[0] = '\0';
            }
        } else {
            // Append line to current fragment
            if (strlen(current_fragment) > 0) {
                strncat(current_fragment, "\n", MAX_TEXT_LEN - strlen(current_fragment) - 1);
            }
            strncat(current_fragment, line, MAX_TEXT_LEN - strlen(current_fragment) - 1);
        }
    }

    // Don't forget last fragment
    if (strlen(current_fragment) > 0 && feeds->lore_count < MAX_LORE_FRAGMENTS) {
        strncpy(feeds->lore_fragments[feeds->lore_count], current_fragment, MAX_TEXT_LEN - 1);
        feeds->lore_count++;
    }

    fclose(f);
}
