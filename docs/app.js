/*
 * SNTE — Dashboard Application
 * Bridges the Emscripten WASM module to the browser UI.
 */

/* ── App Icons Map ── */
const APP_ICONS = {
  Slack:     '💬', Email:    '✉️', Instagram: '📸',
  WhatsApp:  '📱', System:   '⚠️', Jira:      '🎫',
  GitHub:    '🐙', Promo:    '🏷️', News:      '📰',
  Calendar:  '📅'
};

const CATEGORIES = ['Social', 'Work', 'System', 'Promo'];
const DECISION_LABELS = ['SHOW', 'DELAY', 'SUPPRESS'];
const DECISION_CSS    = ['show', 'delay', 'suppress'];

/* ── Wrapped C Functions ── */
let SNTE = {};
let ready = false;

/* ── Called when Emscripten runtime is ready ── */
window.onWasmReady = function () {
  const wrap = Module.cwrap;

  SNTE.init         = wrap('init_engine',         null,     []);
  SNTE.reset        = wrap('reset_engine',        null,     []);
  SNTE.process      = wrap('process_notification','number', ['string','number','number']);
  SNTE.click        = wrap('record_click',        null,     ['string']);
  SNTE.ignore       = wrap('record_ignore',       null,     ['string']);
  SNTE.getShown     = wrap('get_total_shown',     'number', []);
  SNTE.getDelayed   = wrap('get_total_delayed',   'number', []);
  SNTE.getSuppressed= wrap('get_total_suppressed','number', []);
  SNTE.getRingCount = wrap('get_ring_count',      'number', []);
  SNTE.getHeapSize  = wrap('get_heap_size',       'number', []);
  SNTE.getHashCount = wrap('get_hash_entry_count','number', []);
  SNTE.getScore     = wrap('get_app_score',       'number', ['string']);
  SNTE.getRingState = wrap('get_ring_state',      'string', []);
  SNTE.getHeapState = wrap('get_heap_state',      'string', []);
  SNTE.getHashState = wrap('get_hash_state',      'string', []);
  SNTE.bnb          = wrap('run_burst_bnb',       'string', ['number']);

  SNTE.init();
  ready = true;

  document.getElementById('loading').style.display = 'none';
  document.getElementById('app').style.display = '';
  setupUI();
};

/* ── Feed History (in‑memory, newest first) ── */
let feedItems = [];

/* ── UI Setup ── */
function setupUI() {
  /* Priority slider display */
  const slider = document.getElementById('input-priority');
  const priDisplay = document.getElementById('pri-display');
  slider.addEventListener('input', () => {
    priDisplay.textContent = slider.value;
  });

  /* Send notification */
  document.getElementById('notif-form').addEventListener('submit', (e) => {
    e.preventDefault();
    const app = document.getElementById('input-app').value;
    const pri = parseInt(slider.value, 10);
    const cat = parseInt(document.getElementById('input-category').value, 10);
    sendNotification(app, pri, cat);
  });

  /* Simulate burst */
  document.getElementById('btn-burst').addEventListener('click', simulateBurst);

  /* Branch & Bound */
  document.getElementById('btn-bnb').addEventListener('click', runBnB);

  /* Reset */
  document.getElementById('btn-reset').addEventListener('click', () => {
    if (!ready) return;
    SNTE.reset();
    feedItems = [];
    renderFeed();
    updateStats();
    updateStructures();
    document.getElementById('bnb-result').style.display = 'none';
  });

  /* Tabs */
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-content').forEach(c => c.style.display = 'none');
      btn.classList.add('active');
      document.getElementById(btn.dataset.tab).style.display = '';
    });
  });

  updateStats();
}

/* ── Core: Send a notification through the C engine ── */
function sendNotification(app, priority, category) {
  if (!ready) return;

  const decision = SNTE.process(app, priority, category);

  feedItems.unshift({
    app,
    icon: APP_ICONS[app] || '📌',
    priority,
    category,
    decision,
    score: SNTE.getScore(app)
  });

  /* Cap feed at 100 items */
  if (feedItems.length > 100) feedItems.length = 100;

  renderFeed();
  updateStats();
  updateStructures();
}

/* ── Simulate a burst of 12 diverse notifications ── */
function simulateBurst() {
  if (!ready) return;

  const burst = [
    ['Slack',     3, 0], ['Email',    5, 1], ['Instagram', 2, 0],
    ['Promo',     1, 3], ['Jira',     6, 1], ['WhatsApp',  4, 0],
    ['News',      2, 3], ['GitHub',   7, 1], ['Promo',     1, 3],
    ['System',    9, 2], ['Calendar', 5, 2], ['Slack',     3, 0],
  ];

  let idx = 0;
  const interval = setInterval(() => {
    if (idx >= burst.length) { clearInterval(interval); return; }
    const [app, pri, cat] = burst[idx];
    sendNotification(app, pri, cat);
    idx++;
  }, 120);
}

/* ── Run Branch & Bound on current heap ── */
function runBnB() {
  if (!ready) return;

  const heapSize = SNTE.getHeapSize();
  if (heapSize === 0) return;

  const raw = SNTE.bnb(3); /* budget = 3 */
  let result;
  try { result = JSON.parse(raw); } catch { return; }

  const container = document.getElementById('bnb-content');
  const panel = document.getElementById('bnb-result');

  let html = `<p class="bnb-summary">Budget: <strong>3</strong> · Items: <strong>${result.total}</strong> · Selected: <strong>${result.shown}</strong></p>`;

  if (result.decisions) {
    result.decisions.forEach(d => {
      const greedyLabel = DECISION_LABELS[d.greedy] || '?';
      const bnbLabel    = DECISION_LABELS[d.bnb]    || '?';
      const changed = d.greedy !== d.bnb;
      html += `<div class="bnb-item">
        <span>${APP_ICONS[d.app] || '📌'} ${d.app}</span>
        <span>eff: ${d.eff.toFixed(1)}</span>
        <span style="color:${changed ? 'var(--accent)' : 'var(--text-muted)'}">
          ${greedyLabel} → ${bnbLabel}${changed ? ' ✦' : ''}
        </span>
      </div>`;
    });
  }

  container.innerHTML = html;
  panel.style.display = '';
}

/* ── Render Notification Feed ── */
function renderFeed() {
  const feed = document.getElementById('feed');
  if (feedItems.length === 0) {
    feed.innerHTML = '<div class="feed-empty">No notifications yet. Send one or simulate a burst!</div>';
    return;
  }

  feed.innerHTML = feedItems.map((item, i) => {
    const cls  = DECISION_CSS[item.decision];
    const label = DECISION_LABELS[item.decision];
    return `<div class="feed-item decision-${cls}">
      <span class="feed-icon">${item.icon}</span>
      <div class="feed-info">
        <div class="feed-app">${item.app}</div>
        <div class="feed-meta">pri ${item.priority} · ${CATEGORIES[item.category]} · score ${item.score >= 0 ? '+' : ''}${item.score.toFixed(1)}</div>
      </div>
      <span class="feed-badge badge-${cls}">${label}</span>
      <div class="feed-actions">
        <button class="btn-sm" onclick="doClick('${item.app}',${i})" title="Click (engaged)">👆</button>
        <button class="btn-sm" onclick="doIgnore('${item.app}',${i})" title="Ignore (dismissed)">👋</button>
      </div>
    </div>`;
  }).join('');
}

/* ── User Interaction: Click / Ignore ── */
window.doClick = function (app, idx) {
  if (!ready) return;
  SNTE.click(app);
  feedItems[idx].score = SNTE.getScore(app);
  updateStats();
  updateStructures();
};

window.doIgnore = function (app, idx) {
  if (!ready) return;
  SNTE.ignore(app);
  feedItems[idx].score = SNTE.getScore(app);
  updateStats();
  updateStructures();
};

/* ── Update Statistics Bar ── */
function updateStats() {
  if (!ready) return;
  setText('stat-shown',      'stat-value', SNTE.getShown());
  setText('stat-delayed',    'stat-value', SNTE.getDelayed());
  setText('stat-suppressed', 'stat-value', SNTE.getSuppressed());
  setText('stat-ring',       'stat-value', SNTE.getRingCount());
  setText('stat-heap',       'stat-value', SNTE.getHeapSize());
  setText('stat-hash',       'stat-value', SNTE.getHashCount());
}

function setText(parentId, cls, value) {
  const el = document.getElementById(parentId);
  if (el) el.querySelector('.' + cls).textContent = value;
}

/* ── Update Data Structure Views ── */
function updateStructures() {
  if (!ready) return;
  updateRingView();
  updateHeapView();
  updateHashView();
}

function updateRingView() {
  const raw = SNTE.getRingState();
  let items;
  try { items = JSON.parse(raw); } catch { return; }

  const el = document.getElementById('ring-view');
  if (!items.length) { el.innerHTML = '<p class="ds-empty">Empty</p>'; return; }

  el.innerHTML = items.map((n, i) => {
    const cls = DECISION_CSS[n.dec];
    return `<div class="ring-item">
      <span class="ring-slot">[${i}]</span>
      <span class="item-app">${APP_ICONS[n.app] || ''} ${n.app}</span>
      <span class="item-pri">pri ${n.pri}</span>
      <span class="item-dec badge-${cls}" style="font-size:0.65rem">${DECISION_LABELS[n.dec]}</span>
    </div>`;
  }).join('');
}

function updateHeapView() {
  const raw = SNTE.getHeapState();
  let items;
  try { items = JSON.parse(raw); } catch { return; }

  const el = document.getElementById('heap-view');
  if (!items.length) { el.innerHTML = '<p class="ds-empty">Empty</p>'; return; }

  el.innerHTML = items.map(n => {
    const depth = Math.floor(Math.log2(n.idx + 1));
    const indent = '  '.repeat(depth);
    const cls = DECISION_CSS[n.dec];
    return `<div class="heap-item">
      <span class="heap-depth">${indent}[${n.idx}] d${depth}</span>
      <span class="item-app">${APP_ICONS[n.app] || ''} ${n.app}</span>
      <span class="item-pri">eff ${n.eff.toFixed(1)}</span>
      <span class="item-dec badge-${cls}" style="font-size:0.65rem">${DECISION_LABELS[n.dec]}</span>
    </div>`;
  }).join('');
}

function updateHashView() {
  const raw = SNTE.getHashState();
  let items;
  try { items = JSON.parse(raw); } catch { return; }

  const el = document.getElementById('hash-view');
  if (!items.length) { el.innerHTML = '<p class="ds-empty">No entries yet</p>'; return; }

  el.innerHTML = items.map(e => {
    const scoreCls = e.score > 0 ? 'score-positive' : e.score < 0 ? 'score-negative' : 'score-neutral';
    return `<div class="hash-item">
      <span class="hash-bucket">b[${e.bucket}]</span>
      <span class="item-app">${APP_ICONS[e.app] || ''} ${e.app}</span>
      <span class="hash-counts">${e.clicks}✓ ${e.ignores}✗</span>
      <span class="hash-score ${scoreCls}">${e.score >= 0 ? '+' : ''}${e.score.toFixed(2)}</span>
    </div>`;
  }).join('');
}
