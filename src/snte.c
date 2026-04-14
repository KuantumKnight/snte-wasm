/*
 * SNTE - Smart Notification Throttling Engine
 * ============================================
 * Core implementation of all data structures and algorithms.
 *
 * Author: Sarvesh M
 */

#include "snte.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════
 *  RING BUFFER (Circular Queue)
 *
 *  Structure:  Fixed-size array with head/tail pointers
 *  Invariant:  count == (tail - head + capacity) % capacity
 *  Overflow:   Returns false; caller is responsible for eviction.
 *
 *  Time Complexity:
 *    enqueue   O(1)
 *    dequeue   O(1)
 *    is_full   O(1)
 *    is_empty  O(1)
 *
 *  Space: O(RING_BUFFER_CAPACITY) = O(1) since capacity is fixed
 * ═══════════════════════════════════════════════════════════════ */

void ring_init(RingBuffer *rb) {
    rb->head  = 0;
    rb->tail  = 0;
    rb->count = 0;
}

bool ring_is_full(RingBuffer *rb) {
    return rb->count == RING_BUFFER_CAPACITY;
}

bool ring_is_empty(RingBuffer *rb) {
    return rb->count == 0;
}

int ring_count(RingBuffer *rb) {
    return rb->count;
}

bool ring_enqueue(RingBuffer *rb, Notification n) {
    if (ring_is_full(rb)) return false;

    rb->items[rb->tail] = n;
    rb->tail = (rb->tail + 1) % RING_BUFFER_CAPACITY;
    rb->count++;
    return true;
}

bool ring_dequeue(RingBuffer *rb, Notification *out) {
    if (ring_is_empty(rb)) return false;

    *out = rb->items[rb->head];
    rb->head = (rb->head + 1) % RING_BUFFER_CAPACITY;
    rb->count--;
    return true;
}


/* ═══════════════════════════════════════════════════════════════
 *  MAX-HEAP (Binary Heap / Priority Queue)
 *
 *  Structure:  Array-based complete binary tree
 *  Property:   Parent >= both children (max-heap)
 *  Indexing:   Parent = (i-1)/2, Left = 2i+1, Right = 2i+2
 *
 *  Time Complexity:
 *    insert        O(log n)  — sift up
 *    extract_max   O(log n)  — sift down
 *    peek          O(1)
 *
 *  Space: O(MAX_HEAP_CAPACITY)
 * ═══════════════════════════════════════════════════════════════ */

void heap_init(MaxHeap *h) {
    h->size = 0;
}

/* Swap two notifications in the heap array */
static void heap_swap(Notification *a, Notification *b) {
    Notification tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * Sift Up (Bubble Up)
 * After inserting at the end, restore heap property by
 * comparing with parent and swapping upward.
 */
static void heap_sift_up(MaxHeap *h, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (h->items[idx].effective_priority >
            h->items[parent].effective_priority) {
            heap_swap(&h->items[idx], &h->items[parent]);
            idx = parent;
        } else {
            break;
        }
    }
}

/*
 * Sift Down (Heapify Down)
 * After removing the root, move the replacement element
 * down to the correct position.
 */
static void heap_sift_down(MaxHeap *h, int idx) {
    while (1) {
        int largest = idx;
        int left    = 2 * idx + 1;
        int right   = 2 * idx + 2;

        if (left < h->size &&
            h->items[left].effective_priority >
            h->items[largest].effective_priority) {
            largest = left;
        }
        if (right < h->size &&
            h->items[right].effective_priority >
            h->items[largest].effective_priority) {
            largest = right;
        }

        if (largest != idx) {
            heap_swap(&h->items[idx], &h->items[largest]);
            idx = largest;
        } else {
            break;
        }
    }
}

bool heap_insert(MaxHeap *h, Notification n) {
    if (h->size >= MAX_HEAP_CAPACITY) return false;

    h->items[h->size] = n;
    heap_sift_up(h, h->size);
    h->size++;
    return true;
}

bool heap_extract_max(MaxHeap *h, Notification *out) {
    if (h->size == 0) return false;

    *out = h->items[0];
    h->size--;

    if (h->size > 0) {
        h->items[0] = h->items[h->size];
        heap_sift_down(h, 0);
    }
    return true;
}

bool heap_peek(MaxHeap *h, Notification *out) {
    if (h->size == 0) return false;
    *out = h->items[0];
    return true;
}

int heap_size(MaxHeap *h) {
    return h->size;
}


/* ═══════════════════════════════════════════════════════════════
 *  HASH TABLE (Separate Chaining)
 *
 *  Structure:  Array of linked-list buckets
 *  Hash Fn:    djb2 (Dan Bernstein) — fast string hashing
 *  Collision:  Chaining with singly-linked lists
 *
 *  Time Complexity (average, assuming good distribution):
 *    lookup    O(1)
 *    insert    O(1)
 *    update    O(1)
 *
 *  Space: O(HASH_TABLE_SIZE + n) where n = number of entries
 *
 *  User Score Formula:
 *    score = 5.0 * (clicks - ignores) / total_interactions
 *    Range: [-5.0, +5.0]
 * ═══════════════════════════════════════════════════════════════ */

/*
 * djb2 Hash Function
 * hash(i) = hash(i-1) * 33 + str[i]
 * Starting seed: 5381
 * One of the best-known general-purpose string hash functions.
 */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % HASH_TABLE_SIZE;
}

void ht_init(HashTable *ht) {
    memset(ht->buckets, 0, sizeof(ht->buckets));
    ht->entry_count = 0;
}

/*
 * Find an existing node or create a new one.
 * Implements lazy initialization — entries are created
 * on first access to avoid wasted memory.
 */
static HashNode* ht_find_or_create(HashTable *ht, const char *app_name) {
    uint32_t idx = hash_string(app_name);
    HashNode *node = ht->buckets[idx];

    /* Walk the chain looking for a match */
    while (node) {
        if (strncmp(node->app_name, app_name, APP_NAME_MAX) == 0)
            return node;
        node = node->next;
    }

    /* Not found — allocate a new node at the head of the chain */
    node = (HashNode *)calloc(1, sizeof(HashNode));
    if (!node) return NULL;

    strncpy(node->app_name, app_name, APP_NAME_MAX - 1);
    node->app_name[APP_NAME_MAX - 1] = '\0';
    node->next = ht->buckets[idx];
    ht->buckets[idx] = node;
    ht->entry_count++;

    return node;
}

HashNode* ht_lookup(HashTable *ht, const char *app_name) {
    uint32_t idx = hash_string(app_name);
    HashNode *node = ht->buckets[idx];

    while (node) {
        if (strncmp(node->app_name, app_name, APP_NAME_MAX) == 0)
            return node;
        node = node->next;
    }
    return NULL;
}

/*
 * Recalculate user_score after an interaction event.
 * Formula: score = 5.0 * (clicks - ignores) / total
 */
static void ht_update_score(HashNode *node) {
    int total = node->click_count + node->ignore_count;
    if (total == 0) {
        node->user_score = 0.0;
    } else {
        node->user_score =
            5.0 * (node->click_count - node->ignore_count) / (double)total;
    }
}

void ht_record_click(HashTable *ht, const char *app_name) {
    HashNode *node = ht_find_or_create(ht, app_name);
    if (node) {
        node->click_count++;
        ht_update_score(node);
    }
}

void ht_record_ignore(HashTable *ht, const char *app_name) {
    HashNode *node = ht_find_or_create(ht, app_name);
    if (node) {
        node->ignore_count++;
        ht_update_score(node);
    }
}

double ht_get_score(HashTable *ht, const char *app_name) {
    HashNode *node = ht_lookup(ht, app_name);
    return node ? node->user_score : 0.0;
}

void ht_free(HashTable *ht) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        HashNode *node = ht->buckets[i];
        while (node) {
            HashNode *next = node->next;
            free(node);
            node = next;
        }
        ht->buckets[i] = NULL;
    }
    ht->entry_count = 0;
}


/* ═══════════════════════════════════════════════════════════════
 *  GREEDY DISPATCH ALGORITHM
 *
 *  Strategy: Make a locally-optimal O(1) decision for each
 *  notification based on its effective priority.
 *
 *  effective_priority = raw_priority + user_behavioral_score
 *
 *  Decision thresholds:
 *    >= 7.0   →  SHOW      (high-value, surface immediately)
 *    >= 4.0   →  DELAY     (moderate, batch for later)
 *    <  4.0   →  SUPPRESS  (low-value, discard)
 *
 *  Emergency Override:
 *    raw_priority >= 9  →  ALWAYS SHOW (regardless of score)
 *
 *  Time:  O(1) per notification
 *  Space: O(1)
 * ═══════════════════════════════════════════════════════════════ */

Decision greedy_dispatch(Engine *e, Notification *n) {
    /* Emergency override: critical alerts bypass all logic */
    if (n->raw_priority >= EMERGENCY_PRIORITY) {
        n->effective_priority = (double)n->raw_priority;
        return DECISION_SHOW;
    }

    /* Compute effective priority using behavioral data */
    double user_score = ht_get_score(&e->scores, n->app_name);
    n->effective_priority = (double)n->raw_priority + user_score;

    /* Apply greedy thresholds */
    if (n->effective_priority >= 7.0) return DECISION_SHOW;
    if (n->effective_priority >= 4.0) return DECISION_DELAY;
    return DECISION_SUPPRESS;
}


/* ═══════════════════════════════════════════════════════════════
 *  BRANCH & BOUND (Burst Pruning)
 *
 *  Problem:  Given N notifications during a burst, select at
 *            most K (budget) to SHOW, maximizing total utility.
 *
 *  This is a variant of the 0/1 Knapsack problem where each
 *  item has weight=1 and value=effective_priority.
 *
 *  Approach:
 *    1. Sort notifications by effective_priority (descending)
 *    2. Build a binary decision tree: include or exclude each
 *    3. Upper Bound = current_value + sum of remaining items
 *       (fractional relaxation — always tight here since w=1)
 *    4. Prune any branch where bound <= current best solution
 *
 *  Time:  O(2^n) worst case, but pruning makes it practical
 *         for small n (burst sizes typically < 20)
 *  Space: O(n) for the stack (iterative DFS)
 * ═══════════════════════════════════════════════════════════════ */

/* B&B search node for the decision tree */
typedef struct {
    double value;
    double bound;
    int    level;
    int    count;
    bool   included[MAX_HEAP_CAPACITY];
} BBNode;

/*
 * Compute the upper bound for a partial solution.
 * Uses the "fractional relaxation" — since all items have
 * weight 1, we can include whole items greedily.
 */
static double compute_bound(
    Notification *sorted, int n, int budget,
    int level, double current_value, int current_count
) {
    if (current_count >= budget) return current_value;

    double bound     = current_value;
    int    remaining = budget - current_count;

    for (int i = level; i < n && remaining > 0; i++) {
        bound += sorted[i].effective_priority;
        remaining--;
    }

    return bound;
}

int branch_and_bound(
    Notification *batch, int n, int budget, Decision *decisions
) {
    if (n == 0 || budget == 0) return 0;

    /* Trivial case: can show everything */
    if (n <= budget) {
        for (int i = 0; i < n; i++)
            decisions[i] = DECISION_SHOW;
        return n;
    }

    /* Create a sorted copy with index mapping back to original */
    Notification sorted[MAX_HEAP_CAPACITY];
    int index_map[MAX_HEAP_CAPACITY];

    for (int i = 0; i < n; i++) {
        sorted[i]    = batch[i];
        index_map[i] = i;
    }

    /* Insertion sort (n is small during bursts) - descending */
    for (int i = 1; i < n; i++) {
        Notification key = sorted[i];
        int key_idx      = index_map[i];
        int j            = i - 1;

        while (j >= 0 &&
               sorted[j].effective_priority < key.effective_priority) {
            sorted[j + 1]    = sorted[j];
            index_map[j + 1] = index_map[j];
            j--;
        }
        sorted[j + 1]    = key;
        index_map[j + 1] = key_idx;
    }

    /* Initialize all decisions as SUPPRESS */
    for (int i = 0; i < n; i++)
        decisions[i] = DECISION_SUPPRESS;

    /* ── Iterative DFS with explicit stack ── */
    #define BB_STACK_SIZE 256
    BBNode stack[BB_STACK_SIZE];
    int    top = -1;

    double best_value = 0.0;
    bool   best_included[MAX_HEAP_CAPACITY];
    memset(best_included, 0, sizeof(best_included));

    /* Push root node */
    BBNode root;
    root.value = 0.0;
    root.level = 0;
    root.count = 0;
    memset(root.included, 0, sizeof(root.included));
    root.bound = compute_bound(sorted, n, budget, 0, 0.0, 0);

    if (top < BB_STACK_SIZE - 1)
        stack[++top] = root;

    while (top >= 0) {
        BBNode current = stack[top--];

        /* Leaf node — check if this is the best complete solution */
        if (current.level >= n) {
            if (current.value > best_value) {
                best_value = current.value;
                memcpy(best_included, current.included,
                       sizeof(bool) * n);
            }
            continue;
        }

        /* Pruning: bound can't beat current best → skip subtree */
        if (current.bound <= best_value)
            continue;

        /* ── Branch 1: EXCLUDE current notification ── */
        BBNode exclude;
        exclude.value = current.value;
        exclude.level = current.level + 1;
        exclude.count = current.count;
        memcpy(exclude.included, current.included, sizeof(bool) * n);
        exclude.bound = compute_bound(
            sorted, n, budget,
            exclude.level, exclude.value, exclude.count
        );

        if (exclude.bound > best_value && top < BB_STACK_SIZE - 1)
            stack[++top] = exclude;

        /* ── Branch 2: INCLUDE current notification ── */
        if (current.count < budget) {
            BBNode include;
            include.value = current.value +
                            sorted[current.level].effective_priority;
            include.level = current.level + 1;
            include.count = current.count + 1;
            memcpy(include.included, current.included,
                   sizeof(bool) * n);
            include.included[current.level] = true;
            include.bound = compute_bound(
                sorted, n, budget,
                include.level, include.value, include.count
            );

            if (include.bound > best_value && top < BB_STACK_SIZE - 1)
                stack[++top] = include;

            /* Update best eagerly */
            if (include.value > best_value) {
                best_value = include.value;
                memcpy(best_included, include.included,
                       sizeof(bool) * n);
            }
        }
    }

    /* Map results back to original indices */
    int shown = 0;
    for (int i = 0; i < n; i++) {
        if (best_included[i]) {
            decisions[index_map[i]] = DECISION_SHOW;
            shown++;
        } else if (sorted[i].effective_priority >= 4.0) {
            decisions[index_map[i]] = DECISION_DELAY;
        }
        /* else: remains SUPPRESS */
    }

    return shown;
    #undef BB_STACK_SIZE
}


/* ═══════════════════════════════════════════════════════════════
 *  ENGINE - High-level orchestrator
 *
 *  Workflow per notification:
 *    1. Create Notification struct, compute effective_priority
 *    2. Enqueue into Ring Buffer (evict oldest if full)
 *    3. Run Greedy Dispatch for O(1) decision
 *    4. Insert into Max-Heap for priority ordering
 *    5. Update statistics
 * ═══════════════════════════════════════════════════════════════ */

void engine_init(Engine *e) {
    ring_init(&e->ring);
    heap_init(&e->heap);
    ht_init(&e->scores);
    e->next_id         = 1;
    e->total_shown     = 0;
    e->total_delayed   = 0;
    e->total_suppressed = 0;
    e->burst_ts_count  = 0;
}

Decision engine_process(
    Engine *e, const char *app, int priority, int category
) {
    /* Clamp raw priority to valid range */
    if (priority < 1)  priority = 1;
    if (priority > 10) priority = 10;

    /* Build notification */
    Notification n;
    memset(&n, 0, sizeof(n));
    n.id = e->next_id++;
    strncpy(n.app_name, app, APP_NAME_MAX - 1);
    n.app_name[APP_NAME_MAX - 1] = '\0';
    n.raw_priority = priority;
    n.category     = category;
    n.effective_priority = priority + ht_get_score(&e->scores, app);

    /* Ring Buffer: evict oldest if full, then enqueue */
    if (ring_is_full(&e->ring)) {
        Notification discard;
        ring_dequeue(&e->ring, &discard);
    }
    ring_enqueue(&e->ring, n);

    /* Greedy decision */
    Decision d = greedy_dispatch(e, &n);
    n.decision = d;

    /* Max-Heap: insert for priority-ordered retrieval */
    heap_insert(&e->heap, n);

    /* Update aggregate statistics */
    switch (d) {
        case DECISION_SHOW:     e->total_shown++;      break;
        case DECISION_DELAY:    e->total_delayed++;     break;
        case DECISION_SUPPRESS: e->total_suppressed++;  break;
    }

    return d;
}

void engine_click(Engine *e, const char *app) {
    ht_record_click(&e->scores, app);
}

void engine_ignore(Engine *e, const char *app) {
    ht_record_ignore(&e->scores, app);
}

void engine_free(Engine *e) {
    ht_free(&e->scores);
    ring_init(&e->ring);
    heap_init(&e->heap);
}
