// ═══════════════════════════════════════════════════
// SexLab NG HUD — Prisma UI JavaScript
// ═══════════════════════════════════════════════════

// ── 全局状态 ──────────────────────────────────────
let hudContainer = null;
let actorListEl = null;
let isDragging = false;
let dragOffsetX = 0;
let dragOffsetY = 0;

// ── 初始化 ────────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
  hudContainer = document.getElementById('hud-container');
  actorListEl = document.getElementById('actor-list');
  setupDrag();
});

// ── C++ InteropCall 入口 ──────────────────────────

/**
 * C++ 调用: prisma->InteropCall(view, "initHud", jsonStr)
 * 初始化 HUD — 创建 actor fragments
 */
function initHud(jsonStr) {
  const data = JSON.parse(jsonStr);

  // 恢复位置
  if (data.position && data.position.x >= 0) {
    hudContainer.style.left = data.position.x + 'px';
    hudContainer.style.top = data.position.y + 'px';
    hudContainer.style.right = 'auto';
  }

  // 清空并重建
  actorListEl.innerHTML = '';
  for (const actor of data.actors) {
    actorListEl.appendChild(createActorFragment(actor));
  }
}

/**
 * C++ 调用: prisma->InteropCall(view, "updateHud", jsonStr)
 * 每帧更新 enjoy + interactions
 */
function updateHud(jsonStr) {
  const data = JSON.parse(jsonStr);
  for (const actor of data.actors) {
    updateActorFragment(actor);
  }
}

// ── Actor Fragment 创建 ──────────────────────────

function createActorFragment(actor) {
  const frag = document.createElement('div');
  frag.className = 'actor-fragment';
  frag.id = `actor-${actor.index}`;

  // 名字
  const nameEl = document.createElement('div');
  nameEl.className = 'actor-name';
  nameEl.textContent = actor.name;
  frag.appendChild(nameEl);

  // Enjoy bar
  const barContainer = document.createElement('div');
  barContainer.className = 'enjoy-bar-container';

  const barNeg = document.createElement('div');
  barNeg.className = 'bar-negative';
  barNeg.id = `bar-neg-${actor.index}`;

  const barPos = document.createElement('div');
  barPos.className = 'bar-positive';
  barPos.id = `bar-pos-${actor.index}`;

  const degreeLabel = document.createElement('div');
  degreeLabel.className = 'degree-label';
  degreeLabel.id = `degree-${actor.index}`;
  degreeLabel.textContent = actor.degree;

  barContainer.appendChild(barNeg);
  barContainer.appendChild(barPos);
  barContainer.appendChild(degreeLabel);
  frag.appendChild(barContainer);

  // Interact list
  const interList = document.createElement('div');
  interList.className = 'interact-list';
  interList.id = `interact-list-${actor.index}`;
  frag.appendChild(interList);

  return frag;
}

// ── Actor Fragment 更新 ──────────────────────────

function updateActorFragment(actor) {
  const idx = actor.index;

  // ── Enjoy bar ──
  const value = actor.enjoy;    // -100 ~ +100
  const percent = Math.abs(value) / 100 * 50;  // 0 ~ 50%

  const barNeg = document.getElementById(`bar-neg-${idx}`);
  const barPos = document.getElementById(`bar-pos-${idx}`);
  if (barNeg && barPos) {
    if (value < 0) {
      barNeg.style.width = percent + '%';
      barPos.style.width = '0%';
    } else {
      barNeg.style.width = '0%';
      barPos.style.width = percent + '%';
    }
  }

  // ── Degree label ──
  const degreeEl = document.getElementById(`degree-${idx}`);
  if (degreeEl) {
    degreeEl.textContent = `${actor.degree}  (${value > 0 ? '+' : ''}${Math.round(value)})`;
    // 移除旧 degree 样式, 加新的
    degreeEl.className = `degree-label degree-${actor.degree}`;
  }

  // ── Interact rows ──
  const listEl = document.getElementById(`interact-list-${idx}`);
  if (!listEl) return;
  listEl.innerHTML = '';

  for (const inter of actor.interactions) {
    const row = document.createElement('div');
    row.className = 'interact-row';

    const typeSpan = document.createElement('span');
    typeSpan.className = 'interact-type';
    typeSpan.textContent = inter.type;

    const arrow = document.createElement('span');
    arrow.className = 'interact-arrow';
    arrow.textContent = '↔';

    const partner = document.createElement('span');
    partner.className = 'interact-partner';
    partner.textContent = inter.partner;

    // velocity 指示点
    const velDot = document.createElement('span');
    velDot.className = 'interact-velocity';
    if (inter.velocity < -0.01)
      velDot.classList.add('vel-approaching');
    else if (inter.velocity > 0.01)
      velDot.classList.add('vel-retreating');
    else
      velDot.classList.add('vel-static');

    row.appendChild(typeSpan);
    row.appendChild(arrow);
    row.appendChild(partner);
    row.appendChild(velDot);
    listEl.appendChild(row);
  }
}

// ── 拖拽逻辑 ─────────────────────────────────────

function setupDrag() {
  const header = document.getElementById('hud-header');
  if (!header) return;

  header.addEventListener('mousedown', (e) => {
    isDragging = true;
    dragOffsetX = e.clientX - hudContainer.offsetLeft;
    dragOffsetY = e.clientY - hudContainer.offsetTop;
    hudContainer.style.right = 'auto';  // 切换到 left 定位
    e.preventDefault();
  });

  document.addEventListener('mousemove', (e) => {
    if (!isDragging) return;
    const x = e.clientX - dragOffsetX;
    const y = e.clientY - dragOffsetY;
    hudContainer.style.left = x + 'px';
    hudContainer.style.top = y + 'px';
  });

  document.addEventListener('mouseup', () => {
    if (!isDragging) return;
    isDragging = false;
    // 回传位置给 C++
    const x = hudContainer.offsetLeft;
    const y = hudContainer.offsetTop;
    if (typeof skyrimPlatform !== 'undefined') {
      // Prisma JS Interop: 调用注册的 listener
      window.onHudPositionChanged(`${x},${y}`);
    }
  });
}