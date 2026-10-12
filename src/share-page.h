// The web page of a shared terminal (share.C): it connects back with a
// WebSocket, shows the screen the terminal sends and, for a control link,
// sends the keys typed. The link's secret is after the #, so the browser
// never sends it in a request; the page takes it out of the address bar.

static const char share_page[] = R"PAGE(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="referrer" content="no-referrer">
<title>wrxvt</title>
<style>
:root { color-scheme: dark; }
html, body { margin: 0; height: 100%; background: #151515; color: #ddd;
  font: 14px system-ui, sans-serif; }
#bar { display: flex; gap: 12px; align-items: center; padding: 6px 12px;
  background: #222; border-bottom: 1px solid #333; white-space: nowrap; }
#title { flex: 1; overflow: hidden; text-overflow: ellipsis; font-weight: 600; }
#state { font-size: 12px; padding: 2px 10px; border-radius: 10px; background: #444; }
#state.view { background: #2b4a6b; }
#state.control { background: #6b2b2b; }
#wrap { padding: 12px; overflow: auto; }
#term { position: relative; display: inline-block; outline: none; white-space: pre;
  font-family: "DejaVu Sans Mono", "Liberation Mono", Menlo, Consolas, monospace;
  font-size: 15px; line-height: 1.2; cursor: text; }
#term:focus { box-shadow: 0 0 0 2px #6b2b2b; }
.r { height: 1.2em; }
.w { display: inline-block; width: 2ch; overflow: hidden; vertical-align: top; }
#cursor { position: absolute; width: 1ch; height: 1.2em; pointer-events: none;
  background: rgba(255, 255, 255, .5); }
#note { padding: 3em 1em; text-align: center; color: #aaa; }
</style>
</head>
<body>
<div id="bar"><span id="title">wrxvt</span><span id="state">connecting</span></div>
<div id="wrap"><div id="term" tabindex="0"><div id="cursor" hidden></div></div></div>
<div id="note" hidden></div>
<script>
"use strict";
const secret = location.hash.slice(1);
history.replaceState(null, "", location.pathname);   // the secret stays out of sight

const term = document.getElementById("term");
const cursor = document.getElementById("cursor");
const state = document.getElementById("state");
const titleEl = document.getElementById("title");
const note = document.getElementById("note");
let ws = null, role = null, cols = 0, rows = 0, rowEls = [], fatal = false, tries = 0;

function show(text, cls) { state.textContent = text; state.className = cls || ""; }
function stop(message) {
  fatal = true; note.textContent = message; note.hidden = false;
  show("not connected");
}

const esc = s => s.replace(/[&<>"]/g, c => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;"})[c]);

// a run: [text, fg, bg, flags]; flags: 1 bold, 2 italic, 4 underline,
// 8 crossed out, 16 faint, 32 overline, 128 wide (two cells)
function runHtml(run) {
  const [text, fg, bg, f] = run;
  let style = "color:" + fg + ";background:" + bg;
  if (f & 1) style += ";font-weight:bold";
  if (f & 2) style += ";font-style:italic";
  const lines = [];
  if (f & 4) lines.push("underline");
  if (f & 8) lines.push("line-through");
  if (f & 32) lines.push("overline");
  if (lines.length) style += ";text-decoration-line:" + lines.join(" ");
  if (f & 16) style += ";opacity:.6";
  return '<span ' + (f & 128 ? 'class="w" ' : '') + 'style="' + style + '">' + esc(text) + "</span>";
}

function fit() {
  if (!cols) return;
  const probe = document.createElement("span");
  probe.textContent = "0".repeat(100);
  probe.style.fontSize = "100px";
  term.appendChild(probe);
  const ratio = probe.getBoundingClientRect().width / 100 / 100;
  probe.remove();
  const size = Math.floor((window.innerWidth - 30) / (cols * ratio));
  term.style.fontSize = Math.max(6, Math.min(18, size)) + "px";
}
window.addEventListener("resize", fit);

function resize(c, r) {
  cols = c; rows = r;
  rowEls.forEach(e => e.remove());
  rowEls = [];
  for (let i = 0; i < rows; i++) {
    const e = document.createElement("div");
    e.className = "r";
    term.insertBefore(e, cursor);
    rowEls.push(e);
  }
  fit();
}

function onMessage(m) {
  if (m.type === "hello") {
    role = m.role; tries = 0;
    term.style.color = m.fg; term.style.background = m.bg;
    document.body.style.background = m.bg;
    show(role === "control" ? "you can type" : "watching", role);
    if (role === "control") term.focus();
    resize(m.cols, m.rows);
  } else if (m.type === "screen") {
    if (m.cols !== cols || m.rows !== rows) resize(m.cols, m.rows);
    for (const [i, runs] of m.lines)
      if (rowEls[i]) rowEls[i].innerHTML = runs.map(runHtml).join("");
    const [r, c, visible] = m.cursor;
    cursor.hidden = !visible;
    cursor.style.top = (r * 1.2) + "em";
    cursor.style.left = c + "ch";
    if (m.title !== undefined) {
      document.title = (m.title || "wrxvt") + " (shared)";
      titleEl.textContent = m.title || "wrxvt";
    }
  } else if (m.type === "error" || m.type === "bye") {
    stop(m.message);
  }
}

function connect() {
  if (!secret) return stop("This link has no key: copy the whole link, with the part after the #.");
  ws = new WebSocket((location.protocol === "https:" ? "wss://" : "ws://") + location.host + "/ws");
  ws.onopen = () => ws.send(JSON.stringify({type: "auth", token: secret}));
  ws.onmessage = e => onMessage(JSON.parse(e.data));
  ws.onclose = () => {
    role = null;
    if (fatal) return;
    show("disconnected, retrying");
    if (++tries <= 20) setTimeout(connect, Math.min(10000, 500 * tries));
    else stop("The terminal can't be reached.");
  };
}

function send(m) { if (ws && ws.readyState === 1 && role === "control") ws.send(JSON.stringify(m)); }

const named = new Set(["Enter", "Tab", "Backspace", "Escape", "ArrowUp", "ArrowDown",
  "ArrowLeft", "ArrowRight", "Home", "End", "PageUp", "PageDown", "Insert", "Delete"]);

term.addEventListener("keydown", e => {
  if (role !== "control" || e.isComposing || e.metaKey) return;
  const k = e.key;
  // Ctrl+Shift+C / Ctrl+Shift+V: the browser's copy and paste
  if (e.ctrlKey && e.shiftKey && (k === "C" || k === "V")) return;
  if (named.has(k) || /^F([1-9]|1[0-2])$/.test(k)) {
    send({type: "key", key: k, shift: e.shiftKey, ctrl: e.ctrlKey, alt: e.altKey});
  } else if ([...k].length === 1) {
    let s = k;
    if (e.ctrlKey && !e.altKey) {
      const code = k.toUpperCase().charCodeAt(0);
      if (code >= 64 && code <= 95) s = String.fromCharCode(code - 64);
      else if (k === " " || k === "2") s = "\0";
      else if (k === "?" || k === "8") s = "\x7f";
      else return;
    } else if (e.altKey && !e.ctrlKey) {
      s = "\x1b" + k;       // Alt (Ctrl+Alt is AltGr on many keyboards: plain text)
    }
    send({type: "text", data: s});
  } else {
    return;
  }
  e.preventDefault();
});

term.addEventListener("paste", e => {
  if (role !== "control") return;
  send({type: "paste", data: e.clipboardData.getData("text")});
  e.preventDefault();
});

connect();
</script>
</body>
</html>
)PAGE";
