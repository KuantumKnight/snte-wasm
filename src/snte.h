/*
 * SNTE - Smart Notification Throttling Engine
 * ============================================
 * A DSA-driven notification filtering engine implemented in C,
 * compiled to WebAssembly for browser-based demonstration.
 *
 * Data Structures:  Ring Buffer, Max-Heap, Chained Hash Table
 * Algorithms:       Greedy Dispatch, Branch & Bound Pruning
 *
 * Author: Sarvesh M
 */

#ifndef SNTE_H
#define SNTE_H

#include <stdint.h>
#include <stdbool.h>

/* ─── Configuration Constants ─── */
#define RING_BUFFER_CAPACITY  64
#define MAX_HEAP_CAPACITY     64
#define HASH_TABLE_SIZE       53      /* Prime for better distribution */
#define APP_NAME_MAX          32
#define BURST_THRESHOLD       8       /* Count to trigger burst mode   */
#define BURST_WINDOW_MS       3000    /* Window in milliseconds        */
#define SHOW_BUDGET           3       /* Max to show during a burst    */
#define EMERGENCY_PRIORITY    9       /* Raw priority >= always shown  */

/* ─── Decision Outcomes ─── */
typedef enum {
    DECISION_SHOW     = 0,
    DECISION_DELAY    = 1,
    DECISION_SUPPRESS = 2
} Decision;

/* ─── Category Types ─── */
typedef enum {
    CAT_SOCIAL  = 0,   /* Slack, WhatsApp, Instagram         */
    CAT_WORK    = 1,   /* Jira, GitHub, Email                */
    CAT_SYSTEM  = 2,   /* System Alerts, Calendar            */
    CAT_PROMO   = 3    /* Promotions, News                   */
} Category;

/* ─── Notification ─── */
typedef struct {
    int      id;
    char     app_name[APP_NAME_MAX];
    int      raw_priority;            /* 1-10, user-assigned           */
    int      category;
    double   effective_priority;      /* raw + user_score              */
    Decision decision;
    uint32_t timestamp;
} Notification;

/* ═══════════════════════════════════════════════
 *  Ring Buffer (Circular Queue)
 *  - O(1) enqueue, O(1) dequeue
 *  - Fixed-size, overwrites oldest on overflow
 * ═══════════════════════════════════════════════ */
typedef struct {
    Notification items[RING_BUFFER_CAPACITY];
    int head;
    int tail;
    int count;
} RingBuffer;

void ring_init(RingBuffer *rb);
bool ring_enqueue(RingBuffer *rb, Notification n);
bool ring_dequeue(RingBuffer *rb, Notification *out);
bool ring_is_full(RingBuffer *rb);
bool ring_is_empty(RingBuffer *rb);
int  ring_count(RingBuffer *rb);

/* ═══════════════════════════════════════════════
 *  Max-Heap (Priority Queue)
 *  - O(log n) insert, O(log n) extract-max
 *  - Array-based binary heap
 *  - Keyed on effective_priority
 * ═══════════════════════════════════════════════ */
typedef struct {
    Notification items[MAX_HEAP_CAPACITY];
    int size;
} MaxHeap;

void heap_init(MaxHeap *h);
bool heap_insert(MaxHeap *h, Notification n);
bool heap_extract_max(MaxHeap *h, Notification *out);
bool heap_peek(MaxHeap *h, Notification *out);
int  heap_size(MaxHeap *h);

/* ═══════════════════════════════════════════════
 *  Hash Table (Chained / Separate Chaining)
 *  - O(1) average lookup, insert, update
 *  - djb2 hash function
 *  - Tracks per-app behavioral scores
 * ═══════════════════════════════════════════════ */
typedef struct HashNode {
    char     app_name[APP_NAME_MAX];
    int      click_count;
    int      ignore_count;
    double   user_score;              /* Normalized: -5.0 to +5.0      */
    struct HashNode *next;            /* Chaining for collisions        */
} HashNode;

typedef struct {
    HashNode *buckets[HASH_TABLE_SIZE];
    int       entry_count;
} HashTable;

void      ht_init(HashTable *ht);
HashNode* ht_lookup(HashTable *ht, const char *app_name);
void      ht_record_click(HashTable *ht, const char *app_name);
void      ht_record_ignore(HashTable *ht, const char *app_name);
double    ht_get_score(HashTable *ht, const char *app_name);
void      ht_free(HashTable *ht);

/* ═══════════════════════════════════════════════
 *  Engine - Orchestrator
 *  Ties all data structures and algorithms together.
 * ═══════════════════════════════════════════════ */
typedef struct {
    RingBuffer  ring;
    MaxHeap     heap;
    HashTable   scores;
    int         next_id;
    int         total_shown;
    int         total_delayed;
    int         total_suppressed;
    uint32_t    burst_timestamps[BURST_THRESHOLD * 2];
    int         burst_ts_count;
} Engine;

void     engine_init(Engine *e);
Decision engine_process(Engine *e, const char *app, int priority, int category);
void     engine_click(Engine *e, const char *app);
void     engine_ignore(Engine *e, const char *app);
void     engine_free(Engine *e);

/* ═══════════════════════════════════════════════
 *  Algorithms
 * ═══════════════════════════════════════════════ */

/* Greedy Dispatch: O(1) per notification */
Decision greedy_dispatch(Engine *e, Notification *n);

/* Branch & Bound: Burst pruning optimization
 * Returns number of notifications marked SHOW.
 * Writes decisions into the `decisions` array. */
int branch_and_bound(Notification *batch, int n, int budget, Decision *decisions);

#endif /* SNTE_H */
