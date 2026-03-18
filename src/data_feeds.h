#ifndef DATA_FEEDS_H
#define DATA_FEEDS_H

#include <time.h>

#define MAX_HEADLINES 64
#define MAX_POSTS 256
#define MAX_WIFI_DEVICES 64
#define MAX_TEXT_LEN 256
#define MAX_LORE_FRAGMENTS 128

typedef struct {
    char text[MAX_TEXT_LEN];
    char source[64];
    float bias;           // -1 to 1
    float sentiment;      // -1 to 1
    char emotion[32];
} NewsHeadline;

typedef struct {
    char handle[64];
    char content[512];
    char threat_level[16]; // NORMAL, ELEVATED, HIGH, CRITICAL
} MastodonPost;

typedef struct {
    char ssid[64];
    char mac[18];
    int signal;
} WifiDevice;

typedef struct {
    NewsHeadline headlines[MAX_HEADLINES];
    int headline_count;
    time_t headlines_loaded_at;

    MastodonPost posts[MAX_POSTS];
    int post_count;
    time_t posts_loaded_at;

    WifiDevice devices[MAX_WIFI_DEVICES];
    int device_count;
    time_t devices_loaded_at;

    char lore_fragments[MAX_LORE_FRAGMENTS][MAX_TEXT_LEN];
    int lore_count;
} DataFeeds;

// Global feeds instance
extern DataFeeds g_feeds;

void feeds_set_base_dir(const char* exe_path); // call once with argv[0] or resolved path
const char* feeds_get_base_dir(void); // returns resolved binary directory
void feeds_init(DataFeeds* feeds);
void feeds_poll(DataFeeds* feeds);  // check file mtimes, reload if changed
void feeds_load_lore(DataFeeds* feeds, const char* path);

#endif // DATA_FEEDS_H
