#!/bin/sh
# Probes which 9GAG app-API endpoints answer, to find how interest feeds, tag feeds and
# tag search work today. Run it on the phone in Terminal:  sh probe-9gag-api.sh
#
# Uses GagBook's device id and, if you're logged in, its session token from
# ~/.config/harbour-gagbook/harbour-gagbook.conf. It prints HTTP status codes, post
# counts and post titles, never tokens or passwords.

API=${API:-https://api.9gag.com}
WEB=${WEB:-https://9gag.com}
APP=com.ninegag.android.app
VER=80240700
UA="9GAG/$VER (Jolla Sailfish OS;Android 10)"
TYPES=animated,photo,video,article
BODY=$(mktemp 2>/dev/null || echo /tmp/probe9gag.$$)
trap 'rm -f "$BODY" "$BODY.img"' EXIT

# GagBook's settings; also found when this runs in a devel-su root shell
CONF=$(find "$HOME/.config" "$HOME/.local/share" /home/*/.config /home/*/.local/share \
            -name 'harbour-gagbook.conf' 2>/dev/null | head -1)

# QSettings stores byte arrays as @ByteArray(...)
conf_value() {
    [ -n "$CONF" ] || return
    sed -n "s/^$1=//p" "$CONF" | head -1 | sed -e 's/^@ByteArray(//' -e 's/)$//'
}

UUID=$(conf_value deviceUuid)
[ ${#UUID} -eq 32 ] || UUID=$(tr -d '-' < /proc/sys/kernel/random/uuid)
USER_TOKEN=$(conf_value userToken)

have_python=0
command -v python3 >/dev/null 2>&1 && have_python=1

now_ms() {
    if [ $have_python = 1 ]; then python3 -c 'import time; print(int(time.time() * 1000))'
    else echo "$(date +%s)000"; fi
}

# api_get URL TOKEN -> prints the HTTP status, body in $BODY
api_get() {
    ts=$(now_ms)
    sig=$(printf '*%s_._%s._.%s9GAG' "$ts" "$APP" "$UUID" | sha1sum | cut -d' ' -f1)
    curl -s -o "$BODY" -w '%{http_code}' --max-time 20 \
        -H "9GAG-9GAG_TOKEN: $2" -H "9GAG-TIMESTAMP: $ts" \
        -H "9GAG-APP_ID: $APP" -H "X-Package-ID: $APP" \
        -H "9GAG-DEVICE_UUID: $UUID" -H "X-Device-UUID: $UUID" \
        -H "9GAG-DEVICE_TYPE: android" -H "9GAG-BUCKET_NAME: MAIN_RELEASE" \
        -H "9GAG-REQUEST-SIGNATURE: $sig" \
        -H "User-Agent: $UA" -H "X-Package-Version: $VER" "$1"
}

web_get() {
    curl -s -o "$BODY" -w '%{http_code}' --max-time 20 \
        -H 'User-Agent: Mozilla/5.0 (X11; Linux aarch64; rv:128.0) Gecko/20100101 Firefox/128.0' \
        -H 'Accept: application/json' "$1"
}

summary() {
    if [ $have_python = 1 ]; then
        python3 - "$BODY" <<'PY'
import json, sys
raw = open(sys.argv[1], 'rb').read()
try:
    d = json.loads(raw)
except Exception:
    print("    not JSON:", raw[:120].decode('utf-8', 'replace').replace('\n', ' '))
    sys.exit()
if not isinstance(d, dict):
    print("    JSON", type(d).__name__); sys.exit()
meta = d.get('meta') or {}
if meta.get('status') not in (None, 'Success'):
    print("    API error:", meta.get('errorCode'), meta.get('errorMessage'))
data = d.get('data')
if not isinstance(data, dict):
    print("    keys:", list(d.keys())[:10]); sys.exit()
posts = data.get('posts')
if isinstance(posts, list):
    nsfw = sum(1 for p in posts if p.get('nsfw'))
    print("    posts: %d (nsfw %d), didEndOfList=%s, nextCursor=%s"
          % (len(posts), nsfw, data.get('didEndOfList'), data.get('nextCursor')))
    for p in posts[:3]:
        img = ((p.get('images') or {}).get('image700') or {}).get('url', '')
        print("      - %s %-8s nsfw=%s %s | image: %s"
              % (p.get('id'), p.get('type'), p.get('nsfw'), (p.get('title') or '')[:40],
                 img.split('/')[2] + '/...' + img[-14:] if img else 'MISSING'))
    print("    ids:", ",".join(p.get('id', '?') for p in posts))
    if posts:
        img = ((posts[0].get('images') or {}).get('image700') or {}).get('url', '')
        open(sys.argv[1] + '.img', 'w').write(img)
for key in ('groups', 'tags', 'interests', 'items', 'results'):
    v = data.get(key)
    if isinstance(v, list):
        print("    %s: %d" % (key, len(v)))
        if v and isinstance(v[0], dict):
            print("      item keys:", sorted(v[0].keys()))
        for g in v[:60]:
            if isinstance(g, dict):
                print("      -", {k: g.get(k) for k in ('id', 'name', 'key', 'url', 'listType', 'isSensitive', 'nsfw',
                                                       'value', 'count') if k in g})
            else:
                print("      -", g)
other = [k for k in data.keys() if k not in ('posts', 'groups', 'tags', 'interests', 'items', 'results')]
if other:
    print("    other data keys:", other[:15])
PY
    else
        printf '    %s\n' "$(head -c 160 "$BODY" | tr '\n' ' ')"
    fi
}

probe() { # LABEL URL TOKEN
    rm -f "$BODY.img"
    code=$(api_get "$2" "$3")
    echo "[$code] $1"
    summary
}

# check_image: does the first post's image of the last probe download?
check_image() {
    [ -s "$BODY.img" ] || return
    img=$(cat "$BODY.img")
    for ua in "GagBook/1.5.1" "$UA"; do
        r=$(curl -s -o /dev/null -w '%{http_code} %{content_type} %{size_download} bytes' --max-time 20 \
                 -H "User-Agent: $ua" -H 'Accept: image/*' "$img")
        echo "    image download ($ua): $r"
    done
}

echo "GagBook API probe, $(date -u +%Y-%m-%dT%H:%MZ)"
echo "config: ${CONF:-not found}, logged-in token: $([ -n "$USER_TOKEN" ] && echo yes || echo no)"

code=$(api_get "$API/v2/guest-token" "")
GUEST=$( [ $have_python = 1 ] && python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["data"]["userToken"])' "$BODY" 2>/dev/null \
         || sed -n 's/.*"userToken":"\([^"]*\)".*/\1/p' "$BODY")
echo "[$code] guest token: $([ -n "$GUEST" ] && echo ok || echo FAILED)"
[ -n "$GUEST" ] || { summary; exit 1; }

echo; echo "== Section list"
probe "group-list" "$API/v2/group-list/entryTypes/$TYPES" "$GUEST"

echo; echo "== Feeds (guest)"
probe "hot, group default"            "$API/v2/post-list/group/default/type/hot/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "fresh, group default"          "$API/v2/post-list/group/default/type/fresh/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "old section, group 3 (cute)"   "$API/v2/post-list/group/3/type/hot/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "interest as group: humor"      "$API/v2/post-list/group/humor/type/hot/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "interest param: humor"         "$API/v2/post-list/interest/humor/type/hot/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "type interest: humor"          "$API/v2/post-list/type/interest/interest/humor/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "interest-posts: humor"         "$API/v2/interest-posts/interest/humor/type/hot/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "interest query: humor"         "$API/v2/post-list?group=1&type=hot&interest=humor&itemCount=10&entryTypes=$TYPES" "$GUEST"

probe "trending, group default"       "$API/v2/post-list/group/default/type/trending/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "interest humor, fresh"         "$API/v2/post-list/interest/humor/type/fresh/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "interest humor, page 2"        "$API/v2/post-list/interest/humor/type/hot/itemCount/10/entryTypes/$TYPES/olderThan/a5VpEDL" "$GUEST"

echo; echo "== Tags (guest)"
probe "tag-search: cat"               "$API/v2/tag-search/query/cat/fromIndex/0/itemCount/10/entryTypes/$TYPES/offset/10/sort/asc" "$GUEST"
probe "post-list tag: cat"            "$API/v2/post-list/tag/cat/type/hot/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "type tag: cat"                 "$API/v2/post-list/type/tag/tag/cat/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "tag-posts: cat"                "$API/v2/tag-posts/tag/cat/type/hot/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "search-posts: cat"             "$API/v2/search-posts/query/cat/itemCount/10/entryTypes/$TYPES" "$GUEST"
probe "tag-search: cat, from 10"      "$API/v2/tag-search/query/cat/fromIndex/10/itemCount/10/entryTypes/$TYPES/offset/10/sort/asc" "$GUEST"
probe "tag-search: old meme (space)"  "$API/v2/tag-search/query/old%20meme/fromIndex/0/itemCount/10/entryTypes/$TYPES/offset/10/sort/asc" "$GUEST"
probe "tag-search: old-meme (slug)"   "$API/v2/tag-search/query/old-meme/fromIndex/0/itemCount/10/entryTypes/$TYPES/offset/10/sort/asc" "$GUEST"
probe "tag suggestions: ca"           "$API/v2/tag-suggest/query/ca" "$GUEST"
probe "tag list"                      "$API/v2/tag-list" "$GUEST"

echo; echo "== Sensitive feeds"
[ -n "$USER_TOKEN" ] || echo "    (not logged in in GagBook, or config not found: only guest results)"
for who in guest user; do
    if [ $who = guest ]; then tok=$GUEST; else tok=$USER_TOKEN; [ -n "$tok" ] || continue; fi
    for interest in girlnsfw animensfw guys girlsfw; do
        probe "interest $interest ($who)" "$API/v2/post-list/interest/$interest/type/hot/itemCount/10/entryTypes/$TYPES" "$tok"
        check_image
    done
    probe "old NSFW section, group 12 ($who)" "$API/v2/post-list/group/12/type/hot/itemCount/10/entryTypes/$TYPES" "$tok"
    check_image
    probe "interest humor ($who)" "$API/v2/post-list/interest/humor/type/hot/itemCount/10/entryTypes/$TYPES" "$tok"
    check_image
done

echo; echo "== Website API (no login)"
for u in "$WEB/v1/interest-posts/interest/humor/type/hot" "$WEB/v1/tag-posts/tag/cat/type/hot" "$WEB/v1/search-posts?query=cat"; do
    code=$(web_get "$u")
    echo "[$code] ${u#"$WEB"}"
    summary
done
echo; echo "done"
