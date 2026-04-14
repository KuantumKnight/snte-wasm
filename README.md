# SNTE — Smart Notification Throttling Engine

> A **C + WebAssembly** project demonstrating core DSA concepts through an intelligent notification filtering engine. Deployed live on GitHub Pages.

![C](https://img.shields.io/badge/C-00599C?style=flat&logo=c&logoColor=white)
![WebAssembly](https://img.shields.io/badge/WebAssembly-654FF0?style=flat&logo=webassembly&logoColor=white)
![GitHub Pages](https://img.shields.io/badge/Deployed-GitHub%20Pages-222?style=flat&logo=github)

## What It Does

The SNTE takes a stream of notifications and makes **real-time decisions**: **Show**, **Delay**, or **Suppress** — using adaptive algorithms that learn from user behavior.

- **Ring Buffer** — O(1) circular queue for incoming notification stream
- **Max-Heap** — O(log n) priority queue for highest-priority-first dispatching
- **Hash Table** — O(1) chained map tracking per-app click/ignore behavioral scores
- **Greedy Algorithm** — O(1) fast dispatching based on effective priority
- **Branch & Bound** — Optimal burst pruning (0/1 Knapsack variant)

## Project Structure

```
├── src/
│   ├── snte.h          # Data structure & API declarations
│   ├── snte.c          # Core C implementation (~350 lines)
│   └── bindings.c      # Emscripten WASM exports
├── docs/
│   ├── index.html      # Dashboard UI
│   ├── style.css       # Dark theme styling
│   └── app.js          # JS ↔ WASM bridge
├── .github/workflows/
│   └── deploy.yml      # CI: Build WASM + Deploy to Pages
├── Makefile            # Local build commands
└── README.md
```

**~600+ lines of C** · ~400 lines of HTML/CSS/JS

## How It Works

```
[Notification Input]
        │
        ▼
 ┌──────────────┐
 │  Ring Buffer  │  ← O(1) enqueue (circular, fixed capacity)
 └──────┬───────┘
        │
        ▼
 ┌──────────────┐     ┌──────────────┐
 │    Greedy     │◄────│  Hash Table  │  ← O(1) user score lookup
 │   Dispatch    │     │  (djb2 hash) │
 └──────┬───────┘     └──────────────┘
        │
   ┌────┼────┐
   ▼    ▼    ▼
 SHOW DELAY SUPPRESS
        │
        ▼
 ┌──────────────┐
 │   Max-Heap   │  ← O(log n) priority ordering
 └──────────────┘
        │
  (burst detected?)
        │
        ▼
 ┌──────────────┐
 │ Branch&Bound │  ← Optimal subset selection
 └──────────────┘
```

## Algorithms Detail

### Greedy Dispatch (O(1))
```
effective_priority = raw_priority + user_score
if effective_priority ≥ 7  → SHOW
if effective_priority ≥ 4  → DELAY
else                       → SUPPRESS
Emergency: raw_priority ≥ 9 → always SHOW
```

### Branch & Bound (Burst Pruning)
- **Problem**: Select K notifications from N to maximize utility
- **Approach**: Binary decision tree with upper-bound pruning
- **Bound**: Fractional relaxation (tight since all weights = 1)
- **Implementation**: Iterative DFS with explicit stack

### User Score (Hash Table)
```
score = 5.0 × (clicks − ignores) / total_interactions
Range: [−5.0, +5.0]
```

## Deploy

### Automatic (GitHub Actions)

1. Push to `main` branch
2. GitHub Actions installs Emscripten, compiles C → WASM, deploys to Pages
3. Enable **GitHub Pages** in repo settings → Source: **GitHub Actions**

### Local Build (optional)

```bash
# Install Emscripten: https://emscripten.org/docs/getting_started/downloads.html
make build     # Compile C → WASM
make serve     # Build + serve at localhost:8080
```

## Interactive Features

| Feature | What It Does |
|---------|-------------|
| **Send Notification** | Process a single notification through the engine |
| **Simulate Burst** | Fire 12 diverse notifications rapidly |
| **Run B&B** | Execute Branch & Bound on the current heap |
| **Click / Ignore** | Train the behavioral model (updates hash table scores) |
| **Data Structure Tabs** | Inspect Ring Buffer, Max-Heap, and Hash Table state live |

## Tech Stack

- **Engine**: Pure C (C99), ~600 lines
- **Compilation**: Emscripten → WebAssembly
- **Frontend**: Vanilla HTML/CSS/JS (no frameworks)
- **Deployment**: GitHub Actions → GitHub Pages
- **CI**: Automated WASM build pipeline

## Author

**Sarvesh M** — VIT University

---

*Built as a DSA project demonstrating practical application of fundamental data structures and algorithms.*
