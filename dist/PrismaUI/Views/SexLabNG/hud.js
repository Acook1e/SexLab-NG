// ═══════════════════════════════════════════════════
// SexLab NG HUD — Prisma UI JavaScript
// ═══════════════════════════════════════════════════

let hudContainer = null;
let actorListEl = null;
let isDragging = false;
let dragOffsetX = 0;
let dragOffsetY = 0;

// ── DOM Ready ─────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
  hudContainer = document.getElementById('hud-container');
  actorListEl = document.getElementById('actor-list');
  setupDrag();
});

// ═══════════════════════════════════════════════════
// C++ InteropCall 入口
// ═══════════════════════════════════════════════════

/**
 * 初始化 HUD — 创建 actor fragments
 * C++ 调用: prisma->InteropCall(view, "initHud", jsonStr)
 */
function initHud(jsonStr) {
  const data = JSON.parse(jsonStr);

  // 恢复保存的位置
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
 * 每帧更新 enjoy + interactions
 * C++ 调用: prisma->InteropCall(view, "updateHud", jsonStr)
 */
function updateHud(jsonStr) {
  const data = JSON.parse(jsonStr);
  for (const actor of data.actors) {
    updateActorFragment(actor);
  }
}

// ═══════════════════════════════════════════════════
// Actor Fragment 创建
// ═══════════════════════════════════════════════════

function createActorFragment(actor) {
  const frag = document.createElement('div');
  frag.className = 'actor-fragment';
  frag.id = 'actor-' + actor.index;

  // 名字
  const nameEl = document.createElement('div');
  nameEl.className = 'actor-name';
  nameEl.textContent = actor.name;
  frag.appendChild(nameEl);

  // Enjoy bar 容器
  const barContainer = document.createElement('div');
  barContainer.className = 'enjoy-bar-container';

  const barNeg = document.createElement('div');
  barNeg.className = 'bar-negative';
  barNeg.id = 'bar-neg-' + actor.index;

  const barPos = document.createElement('div');
  barPos.className = 'bar-positive';
  barPos.id = 'bar-pos-' + actor.index;

  const degreeLabel = document.createElement('div');
  degreeLabel.className = 'degree-label';
  degreeLabel.id = 'degree-' + actor.index;
  degreeLabel.textContent = actor.degree;

  barContainer.appendChild(barNeg);
  barContainer.appendChild(barPos);
  barContainer.appendChild(degreeLabel);
  frag.appendChild(barContainer);

  // Interact 列表
  const interList = document.createElement('div');
  interList.className = 'interact-list';
  interList.id = 'interact-list-' + actor.index;
  frag.appendChild(interList);

  // 用初始数据刷新一次
  updateEnjoyBar(actor.index, actor.enjoy, actor.degree);
  updateInteractList(actor.index, actor.interactions);

  return frag;
}

// ═══════════════════════════════════════════════════
// Actor Fragment 更新
// ═══════════════════════════════════════════════════

function updateActorFragment(actor) {
  updateEnjoyBar(actor.index, actor.enjoy, actor.degree);
  updateInteractList(actor.index, actor.interactions);
}

/**
 * 更新双向进度条
 * value: -100 ~ +100
 * degree: "Neutral" / "Pain" / "Aroused" / ...
 */
function updateEnjoyBar(index, value, degree) {
  // percent: 0 ~ 50 (占条的一半宽度)
  const percent = Math.abs(value) / 100 * 50;

  const barNeg = document.getElementById('bar-neg-' + index);
  const barPos = document.getElementById('bar-pos-' + index);
  if (barNeg && barPos) {
    if (value < 0) {
      barNeg.style.width = percent + '%';
      barPos.style.width = '0%';
    } else {
      barNeg.style.width = '0%';
      barPos.style.width = percent + '%';
    }
  }

  const degreeEl = document.getElementById('degree-' + index);
  if (degreeEl) {
    const sign = value > 0 ? '+' : '';
    degreeEl.textContent = degree + '  (' + sign + Math.round(value) + ')';
    // 切换颜色 class
    degreeEl.className = 'degree-label degree-' + degree;
  }
}

/**
 * 更新交互列表
 * interactions: [{ part, type, partner, velocity }, ...]
 */
function updateInteractList(index, interactions) {
  const listEl = document.getElementById('interact-list-' + index);
  if (!listEl) return;

  listEl.innerHTML = '';

  if (!interactions || interactions.length === 0) return;

  for (const inter of interactions) {
    const row = document.createElement('div');
    row.className = 'interact-row';

    const content = document.createElement('div');
    content.className = 'interact-content';

    const meta = document.createElement('div');
    meta.className = 'interact-meta';

    const partSpan = document.createElement('span');
    partSpan.className = 'interact-self-part';
    partSpan.textContent = inter.part;

    const separator = document.createElement('span');
    separator.className = 'interact-separator';
    separator.textContent = '·';

    const typeSpan = document.createElement('span');
    typeSpan.className = 'interact-type';
    typeSpan.textContent = inter.type;

    meta.appendChild(partSpan);
    meta.appendChild(separator);
    meta.appendChild(typeSpan);

    const partnerLine = document.createElement('div');
    partnerLine.className = 'interact-partner-line';

    const arrow = document.createElement('span');
    arrow.className = 'interact-arrow';
    arrow.textContent = '↔';

    const partner = document.createElement('span');
    partner.className = 'interact-partner';
    partner.textContent = inter.partner;

    partnerLine.appendChild(arrow);
    partnerLine.appendChild(partner);

    content.appendChild(meta);
    content.appendChild(partnerLine);

    // velocity 指示点
    const velDot = document.createElement('span');
    velDot.className = 'interact-velocity';
    if (inter.velocity < -0.01)
      velDot.classList.add('vel-approaching');
    else if (inter.velocity > 0.01)
      velDot.classList.add('vel-retreating');
    else
      velDot.classList.add('vel-static');

    row.appendChild(content);
    row.appendChild(velDot);
    listEl.appendChild(row);
  }
}

// ═══════════════════════════════════════════════════
// 拖拽逻辑
// ═══════════════════════════════════════════════════

function setupDrag() {
  const header = document.getElementById('hud-header');
  if (!header) return;

  header.addEventListener('mousedown', function (e) {
    isDragging = true;
    dragOffsetX = e.clientX - hudContainer.offsetLeft;
    dragOffsetY = e.clientY - hudContainer.offsetTop;
    hudContainer.style.right = 'auto';
    e.preventDefault();
  });

  document.addEventListener('mousemove', function (e) {
    if (!isDragging) return;
    const x = e.clientX - dragOffsetX;
    const y = e.clientY - dragOffsetY;
    hudContainer.style.left = x + 'px';
    hudContainer.style.top = y + 'px';
  });

  document.addEventListener('mouseup', function () {
    if (!isDragging) return;
    isDragging = false;
    // 回传位置给 C++ 侧持久化
    const x = hudContainer.offsetLeft;
    const y = hudContainer.offsetTop;
    if (window.onHudPositionChanged) {
      window.onHudPositionChanged(x + ',' + y);
    }
  });
}