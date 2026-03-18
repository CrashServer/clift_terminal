#!/bin/bash
# CRIC Installation Feed Fetcher
# Runs in a loop, fetches data from external sources, writes JSON to /tmp/
# for the clift installation engine to consume.
#
# Usage: ./feed_fetcher.sh
# Configure via environment variables or edit defaults below.

set -euo pipefail

# Configuration
MASTODON_INSTANCE="${MASTODON_INSTANCE:-mastodon.social}"
MASTODON_KEYWORDS="${MASTODON_KEYWORDS:-cyberpunk,technology,resistance,surveillance,hacking}"
NEWS_DB="${NEWS_DB:-}"  # Path to NEWS aggregator SQLite DB (optional)
INTERVAL_MASTODON="${INTERVAL_MASTODON:-300}"   # 5 min
INTERVAL_NEWS="${INTERVAL_NEWS:-900}"           # 15 min
INTERVAL_WIFI="${INTERVAL_WIFI:-60}"            # 1 min
ENABLE_WIFI="${ENABLE_WIFI:-false}"

# Output paths
MASTODON_OUT="/tmp/cric_mastodon.json"
NEWS_OUT="/tmp/cric_news.json"
WIFI_OUT="/tmp/cric_wifi.json"

log() {
    echo "[$(date '+%H:%M:%S')] $*" >&2
}

fetch_mastodon() {
    log "Fetching Mastodon posts..."
    local tmpfile
    tmpfile=$(mktemp)
    echo "[" > "$tmpfile"
    local first=true

    for kw in $(echo "$MASTODON_KEYWORDS" | tr ',' ' '); do
        local result
        result=$(curl -sf --max-time 15 \
            "https://${MASTODON_INSTANCE}/api/v1/timelines/tag/${kw}?limit=10" 2>/dev/null || echo "[]")

        if [ "$result" != "[]" ] && [ -n "$result" ]; then
            # Strip outer brackets and append
            local inner
            inner=$(echo "$result" | sed 's/^\[//;s/\]$//')
            if [ -n "$inner" ]; then
                if [ "$first" = true ]; then
                    first=false
                else
                    echo "," >> "$tmpfile"
                fi
                echo "$inner" >> "$tmpfile"
            fi
        fi
    done

    echo "]" >> "$tmpfile"

    # Strip HTML from content fields using sed (basic cleanup)
    if command -v python3 &>/dev/null; then
        python3 -c "
import json, re, sys
try:
    with open('$tmpfile') as f:
        posts = json.load(f)
    # Deduplicate by id
    seen = set()
    unique = []
    for p in posts:
        pid = p.get('id', '')
        if pid not in seen:
            seen.add(pid)
            # Strip HTML from content
            content = p.get('content', '')
            content = re.sub(r'<[^>]+>', '', content)
            content = content.replace('&amp;', '&').replace('&lt;', '<').replace('&gt;', '>')
            p['content'] = content
            unique.append(p)
    json.dump(unique[:32], sys.stdout, ensure_ascii=False)
except Exception as e:
    print(f'[]', file=sys.stdout)
    print(f'Error: {e}', file=sys.stderr)
" > "${MASTODON_OUT}.tmp" && mv "${MASTODON_OUT}.tmp" "$MASTODON_OUT"
    else
        mv "$tmpfile" "$MASTODON_OUT"
    fi

    rm -f "$tmpfile"
    log "Mastodon: loaded $(python3 -c "import json; print(len(json.load(open('$MASTODON_OUT'))))" 2>/dev/null || echo '?') posts"
}

fetch_news() {
    if [ -z "$NEWS_DB" ] || [ ! -f "$NEWS_DB" ]; then
        return
    fi

    log "Fetching news headlines from $NEWS_DB..."
    sqlite3 -json "$NEWS_DB" \
        "SELECT a.title as text, a.source_name as source,
                COALESCE(a.source_bias, 0) as bias,
                COALESCE(aa.sentiment_score, 0) as sentiment,
                COALESCE(aa.dominant_emotion, 'neutral') as emotion
         FROM articles a
         LEFT JOIN article_analysis aa ON a.id = aa.article_id
         WHERE a.published_at > strftime('%s','now','-48 hours')
         ORDER BY a.published_at DESC LIMIT 50" \
        > "${NEWS_OUT}.tmp" 2>/dev/null && mv "${NEWS_OUT}.tmp" "$NEWS_OUT"

    log "News: loaded $(python3 -c "import json; print(len(json.load(open('$NEWS_OUT'))))" 2>/dev/null || echo '?') headlines"
}

fetch_wifi() {
    if [ "$ENABLE_WIFI" != "true" ]; then
        return
    fi

    log "Scanning WiFi..."
    # Requires sudo for scan, or can use cached scan results
    if command -v iw &>/dev/null; then
        sudo iw dev wlan0 scan 2>/dev/null | python3 -c "
import sys, json
devices = []
current = {}
for line in sys.stdin:
    line = line.strip()
    if line.startswith('BSS '):
        if current:
            devices.append(current)
        mac = line.split()[1].rstrip('(')
        current = {'mac': mac, 'ssid': '', 'signal': -100}
    elif 'SSID:' in line:
        current['ssid'] = line.split('SSID:', 1)[1].strip()
    elif 'signal:' in line:
        try:
            current['signal'] = int(float(line.split('signal:', 1)[1].split()[0]))
        except:
            pass
if current:
    devices.append(current)
json.dump(devices, sys.stdout)
" > "${WIFI_OUT}.tmp" 2>/dev/null && mv "${WIFI_OUT}.tmp" "$WIFI_OUT"
        log "WiFi: found $(python3 -c "import json; print(len(json.load(open('$WIFI_OUT'))))" 2>/dev/null || echo '?') devices"
    fi
}

# Track separate timers
last_mastodon=0
last_news=0
last_wifi=0

log "CRIC Feed Fetcher starting"
log "  Mastodon: ${MASTODON_INSTANCE} (keywords: ${MASTODON_KEYWORDS})"
log "  News DB: ${NEWS_DB:-not configured}"
log "  WiFi: ${ENABLE_WIFI}"

while true; do
    now=$(date +%s)

    if [ $((now - last_mastodon)) -ge "$INTERVAL_MASTODON" ]; then
        fetch_mastodon || log "Mastodon fetch failed"
        last_mastodon=$now
    fi

    if [ $((now - last_news)) -ge "$INTERVAL_NEWS" ]; then
        fetch_news || log "News fetch failed"
        last_news=$now
    fi

    if [ $((now - last_wifi)) -ge "$INTERVAL_WIFI" ]; then
        fetch_wifi || log "WiFi fetch failed"
        last_wifi=$now
    fi

    sleep 30
done
