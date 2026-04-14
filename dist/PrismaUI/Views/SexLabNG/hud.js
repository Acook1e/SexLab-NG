// ═══════════════════════════════════════════════════
// SexLab NG HUD — Prisma UI JavaScript
// ═══════════════════════════════════════════════════

let hudContainer = null;
let actorListEl = null;
let sceneBarEl = null;
let sceneSelectEl = null;
let isDragging = false;
let dragOffsetX = 0;
let dragOffsetY = 0;
let isUpdatingScenePicker = false;
const interactionSlotsByActor = new Map();

const MIN_INTERACTION_SLOTS = 3;
const INTERACTION_PRIORITY = Object.freeze({
  Mouth: 0,
  Throat: 1,
  BreastLeft: 2,
  BreastRight: 3,
  HandLeft: 4,
  HandRight: 5,
  FingerLeft: 6,
  FingerRight: 7,
  Belly: 8,
  ThighLeft: 9,
  ThighRight: 10,
  ThighCleft: 11,
  ButtLeft: 12,
  ButtRight: 13,
  GlutealCleft: 14,
  FootLeft: 15,
  FootRight: 16,
  Vagina: 17,
  Anus: 18,
  Penis: 19,
});

// ── DOM Ready ─────────────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
  hudContainer = document.getElementById('hud-container');
  actorListEl = document.getElementById('actor-list');
  sceneBarEl = document.getElementById('scene-bar');
  sceneSelectEl = document.getElementById('scene-select');
  setupDrag();
  setupScenePicker();
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
  interactionSlotsByActor.clear();
  syncScenePicker(data.sceneSelection);

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
  syncScenePicker(data.sceneSelection);
  for (const actor of data.actors) {
    updateActorFragment(actor);
  }
}

function setupScenePicker() {
  if (!sceneSelectEl) return;

  sceneSelectEl.addEventListener('change', function () {
    if (isUpdatingScenePicker) return;
    if (window.onHudSceneSelected) {
      window.onHudSceneSelected(sceneSelectEl.value);
    }
  });
}

function syncScenePicker(sceneSelection) {
  if (!sceneBarEl || !sceneSelectEl) return;

  const options = sceneSelection?.options || [];
  if (options.length <= 1) {
    sceneBarEl.classList.add('is-hidden');
    sceneSelectEl.innerHTML = '';
    return;
  }

  sceneBarEl.classList.remove('is-hidden');

  const currentSignature = JSON.stringify(options.map(option => [option.ptr, option.name]));
  if (sceneSelectEl.dataset.signature !== currentSignature) {
    isUpdatingScenePicker = true;
    sceneSelectEl.innerHTML = '';

    for (const option of options) {
      const optionEl = document.createElement('option');
      optionEl.value = option.ptr;
      optionEl.textContent = option.name;
      sceneSelectEl.appendChild(optionEl);
    }

    sceneSelectEl.dataset.signature = currentSignature;
    isUpdatingScenePicker = false;
  }

  const selectedPtr = sceneSelection?.selectedPtr;
  if (selectedPtr) {
    isUpdatingScenePicker = true;
    sceneSelectEl.value = String(selectedPtr);
    isUpdatingScenePicker = false;
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

  const slots = buildInteractionSlots(index, interactions);
  if (slots.length === 0) return;

  for (const inter of slots) {
    if (!inter) {
      listEl.appendChild(createInteractionPlaceholder());
      continue;
    }

    listEl.appendChild(createInteractionRow(inter));
  }
}

function createInteractionRow(inter) {
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
  return row;
}

function createInteractionPlaceholder() {
  const row = document.createElement('div');
  row.className = 'interact-row is-empty';

  const content = document.createElement('div');
  content.className = 'interact-content';

  const placeholder = document.createElement('div');
  placeholder.className = 'interact-placeholder';
  placeholder.textContent = ' ';

  const velDot = document.createElement('span');
  velDot.className = 'interact-velocity';

  content.appendChild(placeholder);
  row.appendChild(content);
  row.appendChild(velDot);
  return row;
}

function buildInteractionSlots(index, interactions) {
  const normalized = [...(interactions || [])].sort(compareInteractions);
  const activeByKey = new Map();
  for (const inter of normalized) {
    activeByKey.set(getInteractionKey(inter), inter);
  }

  const slotKeys = interactionSlotsByActor.get(index) || [];
  for (const inter of normalized) {
    const key = getInteractionKey(inter);
    if (!slotKeys.includes(key)) {
      slotKeys.push(key);
    }
  }

  interactionSlotsByActor.set(index, slotKeys);

  const slots = slotKeys.map(key => activeByKey.get(key) || null);
  if (slotKeys.length === 0) {
    return slots;
  }

  while (slots.length < Math.max(MIN_INTERACTION_SLOTS, slotKeys.length)) {
    slots.push(null);
  }

  return slots;
}

function getInteractionKey(inter) {
  return inter.part || (inter.type + ':' + (inter.partner || ''));
}

function compareInteractions(lhs, rhs) {
  const lhsPriority = INTERACTION_PRIORITY[lhs.part] ?? 999;
  const rhsPriority = INTERACTION_PRIORITY[rhs.part] ?? 999;
  if (lhsPriority !== rhsPriority) {
    return lhsPriority - rhsPriority;
  }

  if (lhs.part !== rhs.part) {
    return lhs.part.localeCompare(rhs.part);
  }

  if (lhs.type !== rhs.type) {
    return lhs.type.localeCompare(rhs.type);
  }

  return (lhs.partner || '').localeCompare(rhs.partner || '');
}

// ═══════════════════════════════════════════════════
// 拖拽逻辑
// ═══════════════════════════════════════════════════

function setupDrag() {
  const handle = document.querySelector('#hud-header .drag-handle');
  if (!handle) return;

  handle.addEventListener('mousedown', function (e) {
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