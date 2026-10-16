/* Luna GC — live pause monitor */
"use strict";

const PHASE_COLORS = { mark: "#4f8ef7", update_refs: "#46c0c0",
                       sweep: "#7ac142", minor_collect: "#e58b1e" };
const RIVALS = [["Java (G1)", "#e5484d"], ["Go", "#30a46c"], ["Python", "#ffc53d"]];
const TARGET_MS = 0.064;

let refs = {}, benches = [];
let events = [], finished = {}, current = null, running = false, done = false;
let scale = "lin", showTarget = true;

const pauseCv = document.getElementById("pauseChart");
const cmpCv = document.getElementById("compareChart");
const pauseCtx = pauseCv.getContext("2d");
const cmpCtx = cmpCv.getContext("2d");

const $ = id => document.getElementById(id);

function fit(cv, ctx) {
  cv.width = Math.max(2, cv.clientWidth * devicePixelRatio);
  cv.height = Math.max(2, cv.clientHeight * devicePixelRatio);
  ctx.setTransform(devicePixelRatio, 0, 0, devicePixelRatio, 0, 0);
}

// ── event stream ────────────────────────────────────────────
const es = new EventSource("/events");
es.onmessage = e => handle(JSON.parse(e.data));
es.onerror = () => setPill("connection lost", false);

function handle(d) {
  switch (d.type) {
    case "reset":
      finished = {}; events = []; current = null; running = true; done = false;
      benches = d.benches || benches;
      clearTable(); renderBenchGrid(); setPill("live", true);
      updateBanner("starting run…"); drawAll(); break;
    case "bench_start":
      current = d.bench; events = []; running = true;
      setPill("live", true);
      updateBanner(`${d.bench} — benchmark ${d.idx + 1}/${d.total}`);
      renderBenchGrid(); updateCards({}); drawAll(); break;
    case "pause":
      if (d.bench !== current) return;
      events.push(d); updateCards({}); drawPause(); break;
    case "bench_done":
      finished[d.bench] = d.res;
      addTableRow(d.bench, d.res); renderBenchGrid();
      updateCards(d.res); drawAll(); break;
    case "all_done":
      running = false; done = true;
      updateBanner("all benchmarks completed");
      setPill("complete", false);
      drawAll(); break;
  }
}

function setPill(text, live) {
  const p = $("pill");
  p.innerHTML = `<span class="pulse"></span> ${text}`;
  p.classList.toggle("live", live);
}

// ── hydrate from snapshot ─────────────────────────────────────
async function hydrate() {
  const d = await (await fetch("/data")).json();
  refs = d.refs || {}; benches = d.benches || [];
  finished = d.finished || {}; current = d.current;
  events = d.events || []; running = d.running; done = d.done;
  if (running || done) {
    if (running) setPill("live", true);
    else setPill("complete", false);
  }
  for (const b of Object.keys(finished)) addTableRow(b, finished[b], true);
  renderBenchGrid();
  if (current) updateBanner(`${current} — in progress`);
  else if (done) updateBanner("all benchmarks completed");
  else updateBanner("ready — run will start on refresh");
  updateCards(); drawAll();
}

function updateBanner(text) {
  $("curBench").textContent = current || "—";
  $("curSub").textContent = text;
  const i = benches.indexOf(current);
  $("curNext").textContent = !done && i >= 0 && i + 1 < benches.length
      ? `next: ${benches[i + 1]}` : "";
}

// ── stat cards ────────────────────────────────────────────────
function updateCards(res) {
  res = res || {};
  const tot = events.reduce((a, e) => a + e.ms, 0);
  const mx = events.length ? Math.max(...events.map(e => e.ms)) : 0;
  const best = Math.max(mx, Object.keys(finished).reduce((m, k) =>
      Math.max(m, finished[k].max || 0), 0));

  $("sMax").innerHTML = best > 0 ? `${best.toFixed(3)}<span class="unit">ms</span>` : "—";
  $("sEvents").textContent = events.length || (res.events ?? "—");
  $("sTotal").innerHTML = `${(tot || res.total || 0).toFixed(1)}<span class="unit">ms</span>`;
  $("sUser").textContent = res.user != null ? res.user.toFixed(2) : "—";
  $("sRss").textContent = res.rss != null ? `${res.rss}` : "—";
  $("sUserSub").textContent = res.user != null && current
      ? `${current}` : "last finished benchmark";
  $("sRssSub").textContent = res.user != null && current
      ? `${current}` : "last finished benchmark";
  $("chartBench").textContent = current || "current run";
  $("chartCaption").textContent = events.length ? `${events.length} steps` : "";
}

// ── benchmark progress grid ───────────────────────────────────
function renderBenchGrid() {
  const box = $("benchGrid");
  box.innerHTML = benches.map(b => {
    let cls = "", mid = "";
    if (finished[b]) { cls = "done"; mid = `max <span class="v">${finished[b].max.toFixed(3)}</span> ms`; }
    else if (current === b) cls = "now";
    return `<div class="bench-item ${cls}"><span>${b}</span>
            <span class="tag"><span class="st"></span>${mid}</span></div>`;
  }).join("");
  $("progressCount").textContent =
      `${Object.keys(finished).length}/${benches.length} done`;
}

// ── results table ─────────────────────────────────────────────
function clearTable() { $("summary").innerHTML = ""; }

function addTableRow(bench, res, hydrate = false) {
  const ref = refs[bench] || {};
  const cell = (v) => v != null
      ? (v > 0 ? `<b class="luna">${v.toFixed(3)}</b>`
               : `<span class="zero">0.000</span>`)
      : `<span class="zero">—</span>`;
  const rivalCell = v => v != null
      ? (v > 0 ? `<span class="rival-line"><span class="sw" style="background:${rivalColor(bench, v)}"></span>${v.toFixed(3)}</span>`
               : `<span class="zero">0.000</span>`)
      : `<span class="zero">—</span>`;
  const tr = document.createElement("tr");
  tr.dataset.bench = bench;
  tr.innerHTML = `
    <td class="left">${bench}</td>
    <td>${res.user != null ? res.user.toFixed(2) : "—"}</td>
    <td>${res.total != null ? res.total.toFixed(1) : "—"}</td>
    <td>${res.events ?? "—"}</td>
    <td>${cell(res.max)}</td>
    <td class="left">${cell(res.max)}</td>
    <td class="left">${rivalCell(ref["Java (G1)"])}</td>
    <td class="left">${rivalCell(ref.Go)}</td>
    <td class="left">${rivalCell(ref.Python)}</td>`;
  const old = $("summary").querySelector(`tr[data-bench="${bench}"]`);
  if (old) old.replaceWith(tr); else $("summary").appendChild(tr);
}
function rivalColor(bench, v) {
  const e = RIVALS.find(([l]) => refs[bench] && refs[bench][l] === v);
  return e ? e[1] : "#8b98a9";
}

// ── Y transforms ──────────────────────────────────────────────
function yLin(ms, H, pad, maxV) { return pad + (H - pad) * (1 - ms / maxV); }
function yLog(ms, H, pad) {
  const lo = -3.4, hi = 1.6; // ~0.4µs .. 40ms
  const L = Math.max(lo, Math.min(hi, Math.log10(Math.max(ms, 1e-4))));
  return pad + (H - pad) * (1 - (L - lo) / (hi - lo));
}
function yFor(ms, H, pad, maxV) {
  return scale === "lin" && maxV ? yLin(ms, H, pad, maxV) : yLog(ms, H, pad);
}
function rivalVals() {
  if (!current || !refs[current]) return [];
  return RIVALS.filter(([l]) => refs[current][l] != null)
      .map(([l, c]) => ({ lang: l, color: c, v: refs[current][l] }));
}

// ── live pause chart ──────────────────────────────────────────
function drawPause() {
  fit(pauseCv, pauseCtx);
  const W = pauseCv.clientWidth, H = pauseCv.clientHeight, pad = 14, rp = 96;
  const ctx = pauseCtx;
  ctx.clearRect(0, 0, W, H);
  ctx.font = "11px ui-sans-serif, system-ui";
  ctx.lineWidth = 1;

  const rivals = rivalVals();
  const all = events.slice(-500);
  const maxV = scale === "lin"
      ? niceCeil(Math.max(TARGET_MS, ...all.map(e => e.ms), ...rivals.map(r => r.v)))
      : 0;

  // gridlines + labels
  ctx.textBaseline = "middle";
  const ticks = scale === "lin"
      ? linTicks(maxV)
      : [0.0005, 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 40];
  for (const v of ticks) {
    const y = yFor(v, H, pad, maxV);
    const axisW = W - rp;
    ctx.strokeStyle = "#18212f"; ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(axisW, y); ctx.stroke();
    ctx.fillStyle = "#6b7890";
    ctx.textAlign = "right";
    ctx.fillText(fmt(v), W - 8, y);
  }

  // rival max-pause lanes for the current benchmark
  for (const r of rivals) {
    const y = yFor(r.v, H, pad, maxV);
    ctx.strokeStyle = r.color; ctx.lineWidth = 1.6;
    ctx.setLineDash([7, 5]);
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W - rp, y); ctx.stroke();
    ctx.setLineDash([]); ctx.lineWidth = 1;
    ctx.fillStyle = r.color; ctx.textAlign = "left";
    ctx.fillText(`${r.lang} ${r.v.toFixed(3)}ms`, 10, y - 7);
  }

  // pause target
  if (showTarget && scale === "lin") {
    const y = yFor(TARGET_MS, H, pad, maxV);
    ctx.strokeStyle = "#3fb95088"; ctx.lineWidth = 1;
    ctx.setLineDash([3, 5]);
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W - rp, y); ctx.stroke();
    ctx.setLineDash([]);
    ctx.fillStyle = "#3fb950cc"; ctx.textAlign = "right";
    ctx.fillText("64µs target", W - rp - 8, y - 7);
  }

  // bars
  const n = all.length;
  if (n > 0) {
    const plot = W - rp;
    const bw = Math.max(1.5, plot / n - 1.5);
    for (let i = 0; i < n; i++) {
      const e = all[i];
      const x = i * (bw + 1.5);
      const y = yFor(e.ms, H, pad, maxV);
      ctx.fillStyle = PHASE_COLORS[e.phase] || "#7d8898";
      ctx.fillRect(x, y, bw, H - y);
    }
    ctx.fillStyle = "#5a6678"; ctx.textAlign = "right"; ctx.textBaseline = "alphabetic";
    const t = all[all.length - 1].t;
    ctx.fillText(`step ${all.length} at ${t.toFixed(2)}s into the run`, W - 8, H - 4);
  }
}

function fmt(v) {
  if (v >= 1) return `${v}ms`;
  if (v >= 0.001) return `${v.toFixed(v < 0.01 ? 4 : 3).replace(/0+$/, "").replace(/\.$/, "")}ms`;
  return `${(v * 1000).toFixed(0)}µs`;
}
function niceCeil(v) {
  if (v <= 0) return 0.1;
  return [0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 40].find(x => x >= v) || 40;
}
function linTicks(maxV) {
  const out = [];
  for (let v = 0; v <= maxV; v += stepFor(maxV)) out.push(round(v));
  if (!out.length) out.push(0, maxV);
  return out;
}
function stepFor(maxV) {
  if (maxV <= 0.2) return 0.05;
  if (maxV <= 0.5) return 0.1;
  if (maxV <= 1) return 0.2;
  if (maxV <= 2) return 0.5;
  if (maxV <= 5) return 1;
  return 2;
}
const round = (x) => Math.round(x * 1000) / 1000;

// ── worst-pause comparison chart ─────────────────────────────
function drawCompare() {
  fit(cmpCv, cmpCtx);
  const W = cmpCv.clientWidth, H = cmpCv.clientHeight;
  const ctx = cmpCtx;
  ctx.clearRect(0, 0, W, H);
  ctx.font = "11px ui-sans-serif, system-ui";

  const rows = Object.entries(finished)
      .map(([b, r]) => ({ b, max: r.max || 0 }))
      .sort((a, b2) => b2.max - a.max);
  if (!rows.length) {
    ctx.fillStyle = "#5a6678"; ctx.textAlign = "center";
    ctx.fillText("Benchmarks will appear here as they finish", W / 2, H / 2);
    return;
  }

  const plotL = 128, plotR = W - 12, plotT = 14, plotB = H - 26;
  const lo = -3.6, hi = 1.6; // logs: ~0.00025ms .. 40ms
  const X = v => {
    const L = Math.max(lo, Math.min(hi, Math.log10(Math.max(v, 1e-4))));
    return plotL + (plotR - plotL) * (L - lo) / (hi - lo);
  };
  const rowH = Math.min(56, (plotB - plotT) / rows.length);

  // x ticks
  ctx.textBaseline = "top"; ctx.textAlign = "center";
  for (const v of [0.001, 0.01, 0.1, 1, 10]) {
    const x = X(v);
    ctx.strokeStyle = "#18212f";
    ctx.beginPath(); ctx.moveTo(x, plotT); ctx.lineTo(x, plotB); ctx.stroke();
    ctx.fillStyle = "#5a6678"; ctx.fillText(v === 1 ? "1ms" : `${v}ms`, x, plotB + 8);
  }

  // rows: one line per benchmark, one lane per language
  rows.forEach((row, i) => {
    const cy = plotT + i * rowH + rowH / 2;
    ctx.textBaseline = "middle"; ctx.textAlign = "right";
    ctx.fillStyle = "#c6d0df";
    ctx.fillText(row.b, plotL - 10, cy);

    const lanes = [{ label: "Luna", v: row.max, color: "#4f8ef7" }];
    const ref = refs[row.b] || {};
    for (const [lang, color] of RIVALS) {
      if (ref[lang] != null) lanes.push({ label: lang, v: ref[lang], color });
    }
    const laneH = (rowH - 12) / lanes.length;
    lanes.forEach((ln, j) => {
      const ly = plotT + i * rowH + 6 + j * laneH + laneH / 2;
      if (ln.v > 0) {
        const x1 = X(ln.v);
        ctx.fillStyle = ln.color;
        ctx.fillRect(plotL, ly - laneH / 2, Math.max(2, x1 - plotL), laneH - 2);
        ctx.fillStyle = "#d9e2ee"; ctx.font = "10px ui-sans-serif, system-ui";
        ctx.textAlign = "left";
        ctx.fillText(`${ln.label} ${ln.v.toFixed(3)}`, Math.min(x1 + 6, plotR - 74), ly);
      } else {
        ctx.fillStyle = "#5a6678";
        ctx.fillRect(plotL, ly - 2, 6, 4);
        ctx.font = "10px ui-sans-serif, system-ui";
        ctx.textAlign = "left";
        ctx.fillText(`${ln.label} 0.000`, plotL + 10, ly);
      }
      ctx.font = "11px ui-sans-serif, system-ui";
    });
  });
}

// ── controls ──────────────────────────────────────────────────
$("segScale").addEventListener("click", e => {
  const b = e.target.closest("button"); if (!b) return;
  scale = b.dataset.scale;
  document.querySelectorAll("#segScale button").forEach(x =>
      x.classList.toggle("on", x === b));
  drawPause();
});
$("btnTarget").addEventListener("click", () => {
  showTarget = !showTarget;
  $("btnTarget").classList.toggle("on", showTarget);
  drawPause();
});
$("btnClear").addEventListener("click", () => { events = []; drawPause(); });
function restart() { fetch("/ctl?restart=1"); }

// ── resize / draw loop ────────────────────────────────────────
let rafId = 0;
function drawAll() {
  cancelAnimationFrame(rafId);
  rafId = requestAnimationFrame(() => { drawPause(); drawCompare(); });
}
let resizeTo = 0;
addEventListener("resize", () => {
  clearTimeout(resizeTo);
  resizeTo = setTimeout(drawAll, 120);
});

document.addEventListener("readystatechange", () => {
  if (document.readyState === "complete") hydrate();
});
drawAll();