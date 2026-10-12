// The web page of a shared terminal (share.C): it connects back with a
// WebSocket, shows the screen the terminal sends and, for a control link,
// sends what is typed: from a keyboard, a phone's on-screen keyboard or an
// input method (through a hidden text field), the keys of the bar under
// the screen (Esc, Tab, Ctrl, Alt, arrows...), pastes, and clicks and the
// wheel. The link's secret is after the #, so the browser never sends it
// in a request; the page takes it out of the address bar.

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
/* the field that receives what is typed: on the cursor (so input methods
   put their window there), invisible */
#input { position: absolute; width: 2ch; height: 1.2em; padding: 0; border: 0;
  margin: 0; opacity: 0; resize: none; overflow: hidden; font: inherit;
  color: transparent; background: transparent; caret-color: transparent;
  outline: none; z-index: -1; }
/* four groups of four keys: one line on a wide screen, a group per line
   on a phone, the keys sharing the width */
#keys { position: sticky; bottom: 0; display: flex; flex-wrap: wrap; gap: 6px 14px;
  padding: 8px 12px; background: #222; border-top: 1px solid #333; }
#keys .group { display: flex; gap: 6px; flex: 1 1 200px; max-width: 280px; }
#keys button { flex: 1; min-width: 40px; height: 38px; padding: 0 6px; border: 1px solid #444;
  border-radius: 6px; background: #333; color: #eee; font: 14px system-ui, sans-serif;
  touch-action: manipulation; -webkit-user-select: none; user-select: none; }
#keys button:active, #keys button.on { background: #6b2b2b; border-color: #8b3b3b; }
@media (max-width: 480px) { #keys .group { flex-basis: 100%; max-width: none; } }
</style>
</head>
<body>
<div id="bar"><span id="title">wrxvt</span><span id="state">connecting</span></div>
<div id="wrap"><div id="term"><div id="cursor" hidden></div><textarea id="input"
  autocapitalize="off" autocomplete="off" autocorrect="off" spellcheck="false"
  enterkeyhint="send" aria-label="Type into the terminal" tabindex="-1"></textarea></div></div>
<div id="keys" hidden>
<div class="group"><button data-key="Escape">Esc</button><button data-key="Tab">Tab</button
><button data-mod="ctrl">Ctrl</button><button data-mod="alt">Alt</button></div>
<div class="group"><button data-key="ArrowLeft" aria-label="Left">&larr;</button
><button data-key="ArrowDown" aria-label="Down">&darr;</button
><button data-key="ArrowUp" aria-label="Up">&uarr;</button
><button data-key="ArrowRight" aria-label="Right">&rarr;</button></div>
<div class="group"><button data-key="Home">Home</button><button data-key="End">End</button
><button data-key="PageUp">PgUp</button><button data-key="PageDown">PgDn</button></div>
<div class="group"><button data-text="&#3;" aria-label="Control C">^C</button
><button data-text="&#4;" aria-label="Control D">^D</button
><button data-text="&#26;" aria-label="Control Z">^Z</button
><button data-show="keyboard" aria-label="Keyboard">&#9000;</button></div>
</div>
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
    keys.hidden = role !== "control";
    term.style.cursor = role === "control" ? "text" : "default";
    if (role === "control") focusInput();
    resize(m.cols, m.rows);
  } else if (m.type === "screen") {
    if (m.cols !== cols || m.rows !== rows) resize(m.cols, m.rows);
    for (const [i, runs] of m.lines)
      if (rowEls[i]) rowEls[i].innerHTML = runs.map(runHtml).join("");
    const [r, c, visible] = m.cursor;
    cursor.hidden = !visible;
    cursor.style.top = input.style.top = (r * 1.2) + "em";
    cursor.style.left = input.style.left = c + "ch";
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

/* Typing. Everything goes through #input, a hidden text field on the
   cursor: keys that make text arrive as "beforeinput" events (letters,
   dead keys, input methods, phone keyboards), the others (Enter, arrows,
   Ctrl+letter...) as "keydown". The bar's Ctrl and Alt hold for the next
   key, for keyboards that have none. */
const input = document.getElementById("input");
const keys = document.getElementById("keys");
const mods = {ctrl: false, alt: false};
const SENTINEL = " ";   // so a phone's backspace has something to delete

function resetInput() { input.value = SENTINEL; input.setSelectionRange(1, 1); }
function focusInput() { if (role === "control") { input.focus({preventScroll: true}); resetInput(); } }
function setMod(name, on) {
  mods[name] = on;
  keys.querySelector('[data-mod="' + name + '"]').classList.toggle("on", on);
}
function clearMods() { setMod("ctrl", false); setMod("alt", false); }

// one character with Ctrl and/or Alt, as terminals send it
function modChar(ch, ctrl, alt) {
  let s = ch;
  if (ctrl) {
    const code = ch.toUpperCase().charCodeAt(0);
    if (code >= 64 && code <= 95) s = String.fromCharCode(code - 64);
    else if (ch === " " || ch === "2") s = "\0";
    else if (ch === "?" || ch === "8") s = "\x7f";
    else return null;
  }
  return alt ? "\x1b" + s : s;
}

function sendText(text) {
  if (!text) return;
  if (mods.ctrl || mods.alt) {
    const chars = [...text];
    const first = modChar(chars[0], mods.ctrl, mods.alt);
    text = (first === null ? chars[0] : first) + chars.slice(1).join("");
    clearMods();
  }
  send({type: "text", data: text});
}

function sendKey(key, shift, ctrl, alt) {
  send({type: "key", key: key, shift: !!shift, ctrl: !!(ctrl || mods.ctrl), alt: !!(alt || mods.alt)});
  clearMods();
}

const named = new Set(["Enter", "Tab", "Backspace", "Escape", "ArrowUp", "ArrowDown",
  "ArrowLeft", "ArrowRight", "Home", "End", "PageUp", "PageDown", "Insert", "Delete"]);

// keys that don't make text; true if it was one
function onKey(e) {
  if (role !== "control" || e.isComposing || e.keyCode === 229 || e.metaKey) return false;
  const k = e.key;
  // Ctrl+Shift+C / Ctrl+Shift+V: the browser's copy and paste
  if (e.ctrlKey && e.shiftKey && (k === "C" || k === "V")) return false;
  if (named.has(k) || /^F([1-9]|1[0-2])$/.test(k)) {
    sendKey(k, e.shiftKey, e.ctrlKey, e.altKey);
  } else if ([...k].length === 1 && (e.ctrlKey !== e.altKey)) {
    // Ctrl+letter, Alt+letter (Ctrl+Alt is AltGr on many keyboards: text)
    const s = modChar(k, e.ctrlKey, e.altKey);
    if (s === null) return false;
    send({type: "text", data: s});
    clearMods();
  } else {
    return false;   // text: it comes as beforeinput
  }
  e.preventDefault();
  return true;
}
input.addEventListener("keydown", onKey);
// after selecting text on the page the field has lost the focus: typing
// brings it back (and the key still counts)
document.addEventListener("keydown", e => {
  if (role !== "control" || e.target === input || e.ctrlKey && e.shiftKey && (e.key === "C" || e.key === "V")) return;
  if (!onKey(e) && !e.ctrlKey && !e.altKey && !e.metaKey) focusInput();
});

input.addEventListener("beforeinput", e => {
  if (role !== "control") { e.preventDefault(); return; }
  switch (e.inputType) {
    case "insertText":
    case "insertReplacementText":
      sendText(e.data !== null ? e.data : (e.dataTransfer ? e.dataTransfer.getData("text/plain") : ""));
      break;
    case "insertLineBreak":
    case "insertParagraph":
      sendKey("Enter");
      break;
    case "deleteContentBackward":
      sendKey("Backspace");
      break;
    case "deleteContentForward":
      sendKey("Delete");
      break;
    case "insertFromPaste":
    case "insertFromDrop":
      break;              // the paste and drop handlers do it
    default:
      return;             // composing (an input method): see compositionend
  }
  e.preventDefault();
});

// what an input method composed
input.addEventListener("compositionend", e => { sendText(e.data); resetInput(); });
// browsers without beforeinput: whatever got into the field
input.addEventListener("input", e => {
  if (e.isComposing) return;
  const text = input.value.startsWith(SENTINEL) ? input.value.slice(SENTINEL.length) : input.value;
  if (input.value.length < SENTINEL.length) sendKey("Backspace");
  else if (text) sendText(text);
  resetInput();
});

input.addEventListener("paste", e => {
  e.preventDefault();
  if (role === "control") send({type: "paste", data: e.clipboardData.getData("text")});
});

// the bar: pointerdown, not click, so the field keeps the focus (and a
// phone its keyboard)
keys.addEventListener("pointerdown", e => {
  const b = e.target.closest("button");
  if (!b) return;
  e.preventDefault();
  if (b.dataset.mod) setMod(b.dataset.mod, !mods[b.dataset.mod]);
  else if (b.dataset.key) sendKey(b.dataset.key);
  else if (b.dataset.text) { send({type: "text", data: b.dataset.text}); clearMods(); }
  if (document.activeElement !== input) focusInput();
});
// phones open their keyboard only for a focus from a tap (a click)
keys.addEventListener("click", e => { if (e.target.closest("button")) focusInput(); });

/* The mouse: clicks and the wheel go to programs that want them (htop,
   mc, vim with mouse=a); the wheel also scrolls full-screen programs.
   A click elsewhere just puts the focus back for typing. */
function cellAt(e) {
  const r = term.getBoundingClientRect();
  const cw = r.width / Math.max(cols, 1), ch = r.height / Math.max(rows, 1);
  return {col: Math.floor((e.clientX - r.left) / cw), row: Math.floor((e.clientY - r.top) / ch)};
}
let down = null;
term.addEventListener("pointerdown", e => { down = {x: e.clientX, y: e.clientY}; });
term.addEventListener("pointerup", e => {
  if (role !== "control" || !down) return;
  const moved = Math.abs(e.clientX - down.x) + Math.abs(e.clientY - down.y) > 6;
  down = null;
  // a drag selects text to copy (focusing the field would lose it)
  if (moved || !window.getSelection().isCollapsed) return;
  focusInput();
  // a click goes to the program; a tap only brings up the keyboard
  if (e.pointerType === "touch") return;
  const c = cellAt(e);
  send({type: "mouse", button: e.button === 2 ? 3 : e.button === 1 ? 2 : 1, row: c.row, col: c.col});
});
term.addEventListener("contextmenu", e => { if (role === "control") e.preventDefault(); });
let wheelSum = 0;
term.addEventListener("wheel", e => {
  if (role !== "control") return;
  e.preventDefault();
  // one notch of a wheel (about 100 pixels, or 3 lines) is one turn;
  // a touchpad's small steps add up
  wheelSum += e.deltaMode === 1 ? e.deltaY * 33 : e.deltaMode === 2 ? e.deltaY * 400 : e.deltaY;
  const c = cellAt(e);
  while (Math.abs(wheelSum) >= 100) {
    send({type: "mouse", button: wheelSum < 0 ? 4 : 5, row: c.row, col: c.col});
    wheelSum -= Math.sign(wheelSum) * 100;
  }
}, {passive: false});

connect();
</script>
</body>
</html>
)PAGE";
