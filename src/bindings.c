/*
 * SNTE - Emscripten WebAssembly Bindings
 * =======================================
 * Bridges the C engine to the JavaScript frontend.
 * All EMSCRIPTEN_KEEPALIVE functions are callable from JS
 * via Module.ccall() / Module.cwrap().
 *
 * Author: Sarvesh M
 */

#include "snte.h"
#include <emscripten/emscripten.h>
#include <string.h>
#include <stdio.h>

/* Singleton engine instance */
static Engine engine;
static int    initialized = 0;

/* Shared JSON output buffer (8 KB) */
static char json_buf[8192];

/* ─── Lifecycle ─── */

EMSCRIPTEN_KEEPALIVE
void init_engine(void) {
    engine_init(&engine);
    initialized = 1;
}

EMSCRIPTEN_KEEPALIVE
void reset_engine(void) {
    if (initialized) engine_free(&engine);
    engine_init(&engine);
    initialized = 1;
}

/* ─── Core Actions ─── */

EMSCRIPTEN_KEEPALIVE
int process_notification(const char *app, int priority, int category) {
    if (!initialized) init_engine();
    return (int)engine_process(&engine, app, priority, category);
}

EMSCRIPTEN_KEEPALIVE
void record_click(const char *app) {
    if (initialized) engine_click(&engine, app);
}

EMSCRIPTEN_KEEPALIVE
void record_ignore(const char *app) {
    if (initialized) engine_ignore(&engine, app);
}

/* ─── Statistics Queries ─── */

EMSCRIPTEN_KEEPALIVE
int get_total_shown(void)     { return initialized ? engine.total_shown     : 0; }

EMSCRIPTEN_KEEPALIVE
int get_total_delayed(void)   { return initialized ? engine.total_delayed   : 0; }

EMSCRIPTEN_KEEPALIVE
int get_total_suppressed(void){ return initialized ? engine.total_suppressed: 0; }

EMSCRIPTEN_KEEPALIVE
int get_ring_count(void)      { return initialized ? ring_count(&engine.ring) : 0; }

EMSCRIPTEN_KEEPALIVE
int get_heap_size(void)       { return initialized ? heap_size(&engine.heap)  : 0; }

EMSCRIPTEN_KEEPALIVE
int get_hash_entry_count(void){ return initialized ? engine.scores.entry_count: 0; }

EMSCRIPTEN_KEEPALIVE
double get_app_score(const char *app) {
    return initialized ? ht_get_score(&engine.scores, app) : 0.0;
}

/* ─── Data Structure State (JSON serialization) ─── */

EMSCRIPTEN_KEEPALIVE
const char* get_ring_state(void) {
    if (!initialized) return "[]";

    int pos = 0;
    pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, "[");

    RingBuffer *rb = &engine.ring;
    for (int i = 0; i < rb->count && pos < (int)sizeof(json_buf) - 128; i++) {
        int idx = (rb->head + i) % RING_BUFFER_CAPACITY;
        Notification *n = &rb->items[idx];
        if (i > 0) pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, ",");
        pos += snprintf(json_buf + pos, sizeof(json_buf) - pos,
            "{\"id\":%d,\"app\":\"%s\",\"pri\":%d,\"cat\":%d,"
            "\"eff\":%.1f,\"dec\":%d}",
            n->id, n->app_name, n->raw_priority, n->category,
            n->effective_priority, n->decision);
    }

    snprintf(json_buf + pos, sizeof(json_buf) - pos, "]");
    return json_buf;
}

EMSCRIPTEN_KEEPALIVE
const char* get_heap_state(void) {
    if (!initialized) return "[]";

    int pos = 0;
    pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, "[");

    MaxHeap *h = &engine.heap;
    for (int i = 0; i < h->size && pos < (int)sizeof(json_buf) - 128; i++) {
        Notification *n = &h->items[i];
        if (i > 0) pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, ",");
        pos += snprintf(json_buf + pos, sizeof(json_buf) - pos,
            "{\"id\":%d,\"app\":\"%s\",\"pri\":%d,"
            "\"eff\":%.1f,\"dec\":%d,\"idx\":%d}",
            n->id, n->app_name, n->raw_priority,
            n->effective_priority, n->decision, i);
    }

    snprintf(json_buf + pos, sizeof(json_buf) - pos, "]");
    return json_buf;
}

EMSCRIPTEN_KEEPALIVE
const char* get_hash_state(void) {
    if (!initialized) return "[]";

    int pos = 0;
    pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, "[");

    bool first = true;
    HashTable *ht = &engine.scores;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashNode *node = ht->buckets[i];
        while (node && pos < (int)sizeof(json_buf) - 128) {
            if (!first) pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, ",");
            pos += snprintf(json_buf + pos, sizeof(json_buf) - pos,
                "{\"app\":\"%s\",\"clicks\":%d,\"ignores\":%d,"
                "\"score\":%.2f,\"bucket\":%d}",
                node->app_name, node->click_count,
                node->ignore_count, node->user_score, i);
            first = false;
            node = node->next;
        }
    }

    snprintf(json_buf + pos, sizeof(json_buf) - pos, "]");
    return json_buf;
}

/* ─── Branch & Bound Burst Processing ─── */

EMSCRIPTEN_KEEPALIVE
const char* run_burst_bnb(int budget) {
    if (!initialized || engine.heap.size == 0) return "{\"shown\":0,\"decisions\":[]}";

    int n = engine.heap.size;
    if (n > MAX_HEAP_CAPACITY) n = MAX_HEAP_CAPACITY;
    if (budget <= 0) budget = SHOW_BUDGET;

    /* Copy current heap items into a batch array */
    Notification batch[MAX_HEAP_CAPACITY];
    Decision     decisions[MAX_HEAP_CAPACITY];

    for (int i = 0; i < n; i++)
        batch[i] = engine.heap.items[i];

    /* Run Branch & Bound optimization */
    int shown = branch_and_bound(batch, n, budget, decisions);

    /* Serialize result as JSON */
    int pos = 0;
    pos += snprintf(json_buf + pos, sizeof(json_buf) - pos,
                    "{\"shown\":%d,\"total\":%d,\"decisions\":[", shown, n);

    for (int i = 0; i < n && pos < (int)sizeof(json_buf) - 128; i++) {
        if (i > 0)
            pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, ",");
        pos += snprintf(json_buf + pos, sizeof(json_buf) - pos,
            "{\"id\":%d,\"app\":\"%s\",\"eff\":%.1f,"
            "\"greedy\":%d,\"bnb\":%d}",
            batch[i].id, batch[i].app_name,
            batch[i].effective_priority,
            batch[i].decision, decisions[i]);
    }

    snprintf(json_buf + pos, sizeof(json_buf) - pos, "]}");
    return json_buf;
}

/* ─── Entry Point ─── */

int main(void) {
    /* No-op: JS calls init_engine() explicitly */
    return 0;
}
