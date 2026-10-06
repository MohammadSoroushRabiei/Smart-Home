// Smart-Home defense deck — English LTR, 13.33x7.5, Segoe UI (sibling of gen_deck.js)
const pptxgen = require("pptxgenjs");

const W = 13.33, H = 7.5, M = 0.5;
const BG_DARK = "16283F", BG = "FFFFFF", SURFACE = "F2F6FA", SURF2 = "EAF0F7";
const PRIMARY = "1E3A5F", PRIMARY_MID = "2F5D8C", ACCENT = "C9962E";
const TEXT = "1A2433", MUTED = "5B6B7E", HAIR = "D8E0EA", LIGHT_ON_DARK = "C7D4E3";
const TF = "Segoe UI", BF = "Segoe UI";
const FIGS = "D:/02-Projects/Smart-Home/report/figs";
const IMG = "D:/02-Projects/Smart-Home/presentation/Images";
const POSTERS = "D:/02-Projects/Smart-Home/presentation/posters";
const { execSync } = require("child_process");
const os = require("os"), fs = require("fs"), crypto = require("crypto"), path = require("path");
// silent copy of a test video (video stream copied bit-for-bit, audio dropped) —
// built in the OS temp dir so no duplicate media lands in the repo
function silentVideo(src, vf) {
  const out = path.join(os.tmpdir(), "deck-" + crypto.randomBytes(4).toString("hex") + ".mp4");
  try {
    const codec = vf ? `-vf "${vf}" -c:v libx264 -crf 20 -preset veryfast` : "-c:v copy";
    execSync(`ffmpeg -y -v error -i "${src}" ${codec} -an "${out}"`);
    return out;
  } catch { return src; }
}
const coverData = (name) => "image/jpeg;base64," + fs.readFileSync(path.join(POSTERS, name + ".jpg")).toString("base64");

const pres = new pptxgen();
pres.layout = "LAYOUT_WIDE";
pres.author = "Mohammad Soroush Rabiei";
pres.title = "Design and Implementation of an IoT-Based Smart Home System with Edge AI";
pres.theme = { headFontFace: TF, bodyFontFace: BF };

const TOTAL = 32;
const sh = () => ({ type: "outer", color: "1E3A5F", blur: 7, offset: 2, angle: 90, opacity: 0.14 });

function T(s, txt, o = {}) {
  s.addText(txt, { fontFace: BF, color: TEXT, margin: 0, ...o });
}
function card(s, x, y, w, h, fill = SURFACE, shadow = false, line = null) {
  s.addShape("roundRect", { x, y, w, h, fill: { color: fill }, rectRadius: 0.07,
    line: line ? { color: line, width: 0.75 } : { color: fill, width: 0 },
    ...(shadow ? { shadow: sh() } : {}) });
}
function imgCard(s, p, x, y, w, h) {
  s.addShape("rect", { x: x - 0.08, y: y - 0.08, w: w + 0.16, h: h + 0.16, fill: { color: "FFFFFF" }, line: { color: HAIR, width: 1 }, shadow: sh() });
  s.addImage({ path: p, x, y, w, h });
}
function header(s, kicker, title, titleSize = 29) {
  T(s, kicker, { x: M, y: 0.38, w: W - 2 * M, h: 0.34, fontSize: 13, color: ACCENT, bold: true });
  T(s, title, { x: M, y: 0.72, w: W - 2 * M, h: 0.78, fontSize: titleSize, fontFace: TF, color: PRIMARY });
}
function pageNum(s, n) {
  s.addText(`${n} / ${TOTAL}`, { x: 0.45, y: 7.08, w: 1.2, h: 0.32, fontSize: 12, fontFace: BF, color: MUTED, align: "left", margin: 0 });
}
// numbered row: gold numeral on the left, text to its right
function numRow(s, n, title, desc, x, y, w, opt = {}) {
  const numW = 0.62, gap = 0.18;
  T(s, String(n), { x, y: y - 0.02, w: numW, h: 0.55, fontSize: 21, fontFace: TF, color: ACCENT, align: "center" });
  const tx = x + numW + gap, tw = w - numW - gap;
  if (desc) {
    T(s, title, { x: tx, y, w: tw, h: 0.4, fontSize: opt.ts || 15.5, bold: true, color: PRIMARY });
    T(s, desc, { x: tx, y: y + (opt.dy || 0.38), w: tw, h: opt.dh || 0.62, fontSize: opt.ds || 13, color: MUTED, lineSpacingMultiple: 1.12 });
  } else {
    T(s, title, { x: tx, y, w: tw, h: opt.th || 0.7, fontSize: opt.ts || 15, lineSpacingMultiple: 1.15 });
  }
}

/* ---------------- S1 — Cover ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG_DARK };
  s.addImage({ path: `${FIGS}/logo_1.png`, x: W / 2 - 0.62, y: 0.5, w: 1.24, h: 1.24 });
  T(s, "Isfahan University of Technology", { x: 1, y: 1.92, w: W - 2, h: 0.42, fontSize: 17, bold: true, color: ACCENT, align: "center" });
  T(s, "Faculty of Electrical and Computer Engineering", { x: 1, y: 2.32, w: W - 2, h: 0.36, fontSize: 14, color: LIGHT_ON_DARK, align: "center" });
  T(s, "Design and Implementation of an IoT-Based Smart Home", { x: 0.7, y: 2.95, w: W - 1.4, h: 0.85, fontSize: 35, fontFace: TF, color: "FFFFFF", align: "center", bold: true });
  T(s, "with edge-AI processing and a fully local infrastructure — no cloud required", { x: 1.7, y: 3.9, w: W - 3.4, h: 0.7, fontSize: 18, color: LIGHT_ON_DARK, align: "center", lineSpacingMultiple: 1.2 });
  s.addShape("line", { x: W / 2 - 1.1, y: 4.85, w: 2.2, h: 0, line: { color: ACCENT, width: 1 } });
  T(s, "Undergraduate Specialized Project — Computer Engineering", { x: 1, y: 5.05, w: W - 2, h: 0.36, fontSize: 14, color: LIGHT_ON_DARK, align: "center" });
  T(s, "Supervisor: Dr. Amir Khorsandi", { x: 1, y: 5.5, w: W - 2, h: 0.4, fontSize: 16, bold: true, color: "FFFFFF", align: "center" });
  T(s, "Student: Mohammad Soroush Rabiei", { x: 1, y: 5.95, w: W - 2, h: 0.4, fontSize: 16, bold: true, color: "FFFFFF", align: "center" });
  T(s, "October 2026", { x: 1, y: 6.55, w: W - 2, h: 0.35, fontSize: 13, color: "8FA3BC", align: "center" });
  s.addNotes("Welcome. Project title: an IoT-based smart home with edge AI and a fully local infrastructure. ~20 minutes; the core section is edge AI.");
}

/* ---------------- S2 — Agenda ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Roadmap", "Agenda");
  const items = [
    ["Problem & objectives", "Why a cloud-free smart home?"],
    ["Architecture, hardware & firmware", "ESP32-S3, FreeRTOS, touch display"],
    ["Local infrastructure", "MQTT & Home Assistant on Docker"],
    ["Edge AI", "Face recognition + a behavioral learning agent (core)"],
    ["Multi-channel interfaces", "Display, mobile web, dashboard & HA app"],
    ["Face-based attendance", "Three-tier architecture with a durable queue"],
    ["Security & privacy", "TLS, tokens, hard lock rules"],
    ["Tests, real hardware & takeaways", "14 E2E scenarios + live demo videos"],
  ];
  const colW = 6.05, rowH = 1.18, y0 = 1.85;
  items.forEach(([t, d], i) => {
    const col = i < 4 ? 0 : 1; // left column first
    const x = col ? W - M - colW : M;
    const y = y0 + (i % 4) * rowH;
    numRow(s, i + 1, t, d, x, y, colW, { dy: 0.37, dh: 0.4, ds: 12.5 });
    if (i % 4 < 3) s.addShape("line", { x, y: y + rowH - 0.18, w: colW, h: 0, line: { color: HAIR, width: 0.75 } });
  });
  pageNum(s, 2);
  s.addNotes("Talk path: from the problem to the wrap-up. Emphasis: section 4 (edge AI) is the deepest part.");
}

/* ---------------- S3 — Problem ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 1 — Introduction", "The problem: three failures of cloud-centric architectures");
  T(s, "Common commercial solutions rest on two pillars: multiple dedicated hardware devices and cloud services for processing, authentication and storage.", { x: M, y: 1.62, w: W - 2 * M, h: 0.45, fontSize: 14.5, color: MUTED, lineSpacingMultiple: 1.15 });
  const probs = [
    ["Privacy", "Sensitive data such as residents' face images leaves the house and sits on servers you do not control."],
    ["Internet dependence", "Every outage means losing control of — and intelligence in — the home."],
    ["Network latency", "A cloud round-trip is unacceptable for latency-critical decisions such as unlocking a door with your face."],
  ];
  const cw = 3.94, gap = 0.25, y = 2.35, ch = 2.15;
  probs.forEach(([t, d], i) => {
    const x = M + i * (cw + gap);
    card(s, x, y, cw, ch, SURFACE, true);
    T(s, t, { x: x + 0.25, y: y + 0.28, w: cw - 0.5, h: 0.45, fontSize: 18, bold: true, color: PRIMARY });
    T(s, d, { x: x + 0.25, y: y + 0.85, w: cw - 0.5, h: ch - 1.05, fontSize: 13.5, color: TEXT, lineSpacingMultiple: 1.25 });
  });
  T(s, "Moreover, machine vision traditionally requires a powerful processor and a dedicated camera — cost, installation complexity and power consumption all rise.", { x: M, y: 4.75, w: W - 2 * M, h: 0.45, fontSize: 14.5, color: MUTED, lineSpacingMultiple: 1.15 });
  card(s, M, 5.45, W - 2 * M, 1.35, SURF2);
  T(s, "The core question of this project", { x: M + 0.2, y: 5.62, w: W - 2 * M - 0.4, h: 0.35, fontSize: 14, bold: true, color: ACCENT, align: "center" });
  T(s, "Can a complete, secure and intelligent smart home be built with minimal edge hardware — all AI running on the microcontroller itself, the camera borrowed from the user's phone, and no data ever leaving the home network?", { x: M + 0.35, y: 5.98, w: W - 2 * M - 0.7, h: 0.75, fontSize: 15.5, bold: true, color: PRIMARY, align: "center", lineSpacingMultiple: 1.25 });
  pageNum(s, 3);
  s.addNotes("Three cloud problems: privacy, outages, latency. Plus the cost of dedicated vision hardware. Then the core question.");
}

/* ---------------- S4 — Solution + block diagram ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 1 — Introduction", "The answer: all intelligence on the edge, all data at home");
  imgCard(s, `${FIGS}/fig7_block_diagram.png`, W - M - 6.45, 1.95, 6.45, 4.5);
  const rows = [
    ["Full face recognition on the ESP32-S3", "From JPEG decoding to feature-vector matching — the image never leaves the board."],
    ["The user's phone = sensor & interface", "Camera and browser via a QR code on the display; no dedicated camera module."],
    ["Local infrastructure with Docker", "Mosquitto and Home Assistant on a home server; just a modem, no cloud services."],
    ["Four parallel interaction channels", "Touch display, mobile web, HA dashboard and HA app — with one source of truth."],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, M, 2.0 + i * 1.12, 5.35, { dy: 0.37, dh: 0.55, ds: 12.5 }));
  pageNum(s, 4);
  s.addNotes("Block diagram: the phone only lends its camera; all processing on the ESP32-S3; a Docker home server; the internet is optional, for archiving/notifications only.");
}

/* ---------------- S5 — Goals ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 1 — Introduction", "Project objectives");
  const goals = [
    ["An ESP32-S3 IoT node", "A color touch display, a secure on-board HTTPS web server and stable home-network connectivity"],
    ["Full face recognition on a microcontroller", "Decode, detect, embed and match — no image ever sent outside"],
    ["A camera borrowed from the user's phone", "Image input via a mobile web page and a QR mechanism instead of a dedicated camera module"],
    ["Local infrastructure with auto-discovery", "Mosquitto + Home Assistant on Docker with Discovery integration"],
    ["A multi-channel user interface", "Four parallel channels, all driven by one source of truth"],
    ["A lightweight behavioral-learning agent", "Light & fan control with offline pre-training + on-chip online learning"],
    ["Face-based attendance", "A local server, a durable message queue, a Jalali archive and Bale notifications"],
    ["Reliability & testing", "Watchdogs, network-aware reconnection and scenario-driven end-to-end tests"],
  ];
  const colW = 6.05, rowH = 1.22, y0 = 1.8;
  goals.forEach(([t, d], i) => {
    const col = i % 2 === 0 ? 0 : 1;
    const x = col ? W - M - colW : M;
    const y = y0 + Math.floor(i / 2) * rowH;
    numRow(s, i + 1, t, d, x, y, colW, { ts: 15, dy: 0.36, dh: 0.62, ds: 12.5 });
  });
  pageNum(s, 5);
  s.addNotes("The 8 official project goals — the list from chapter 1 of the report.");
}

/* ---------------- S6 — Innovations ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 1 — Introduction", "Innovations and distinguishing features");
  const rows = [
    ["AI on a constrained device", "The entire face pipeline — from hardware JPEG decoding to feature extraction — runs on a microcontroller: full privacy, near-zero latency."],
    ["Authentication with hardware the user already owns", "The phone's camera and browser act as the image sensor; edge hardware stays minimal."],
    ["Complete independence from the cloud", "Keeps working through international and domestic internet outages; data stays on the LAN."],
    ["On-chip behavioral learning", "Offline pre-training + online SGD updates; a shadow → auto lifecycle; the lock is never model-controlled."],
    ["An attendance ecosystem", "Three tiers with a durable on-board queue, a FastAPI/SQLite server, a Jalali sheet and Bale notifications — independent of HA."],
    ["A comprehensive engineering exercise", "Electronics, architecture, operating systems, networking, security, UI/UX and software engineering in one real project."],
  ];
  const rowH = 0.86, y0 = 1.78;
  rows.forEach(([t, d], i) => {
    const y = y0 + i * rowH;
    s.addShape("roundRect", { x: M, y: y + 0.12, w: 0.16, h: 0.16, rectRadius: 0.03, fill: { color: ACCENT }, line: { color: ACCENT, width: 0 } });
    T(s, t, { x: M + 0.35, y, w: 4.0, h: 0.8, fontSize: 15, bold: true, color: PRIMARY, valign: "top" });
    T(s, d, { x: M + 4.55, y, w: W - 2 * M - 4.55, h: 0.8, fontSize: 12.5, color: TEXT, valign: "top", lineSpacingMultiple: 1.15 });
    if (i < rows.length - 1) s.addShape("line", { x: M, y: y + rowH - 0.12, w: W - 2 * M, h: 0, line: { color: HAIR, width: 0.75 } });
  });
  pageNum(s, 6);
  s.addNotes("6 distinguishing features; the most important: intelligence on the edge and borrowing the camera from a phone.");
}

/* ---------------- S7 — Technologies ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 2 — Fundamentals", "The technologies at a glance");
  imgCard(s, `${FIGS}/fig6_stack.png`, W - M - 5.85, 1.95, 5.85, 4.17);
  const chips = ["ESP32-S3 (N16R8)", "ESP-IDF 6 + FreeRTOS", "LVGL 9.5", "ESP-DL", "MQTT 3.1.1", "Mosquitto", "Home Assistant", "Docker", "HTTPS / TLS", "FastAPI + SQLite"];
  const cw = 2.75, chh = 0.56, gx = 0.22, gy = 0.2, x0 = M, y0 = 2.05;
  chips.forEach((c, i) => {
    const col = i % 2, row = Math.floor(i / 2);
    const x = x0 + col * (cw + gx), y = y0 + row * (chh + gy);
    card(s, x, y, cw, chh, SURFACE);
    s.addText(c, { x, y, w: cw, h: chh, fontSize: 13, fontFace: BF, color: PRIMARY, align: "center", valign: "middle", margin: 0 });
  });
  T(s, "The infrastructure column: Docker containers on a home server; the edge column: dual-core FreeRTOS with LVGL, a TLS web server and ESP-DL models on a single chip.", { x: M, y: 5.6, w: 6.2, h: 1.0, fontSize: 13.5, color: TEXT, lineSpacingMultiple: 1.3 });
  pageNum(s, 7);
  s.addNotes("The software stack from hardware to user services. Right: the technology list.");
}

/* ---------------- S8 — Hardware & firmware ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 4 — Hardware & Firmware Design", "Edge hardware and firmware structure");
  const cw = 6.05, ch = 4.75, y0 = 1.8;
  card(s, M, y0, cw, ch, SURFACE, false);
  T(s, "Hardware", { x: M + 0.3, y: y0 + 0.22, w: cw - 0.6, h: 0.45, fontSize: 18, bold: true, color: PRIMARY });
  const hw = [
    "ESP32-S3-DevKitC-1 board with a WROOM-1-N16R8 module: 16 MB flash + 8 MB octal PSRAM (80 MHz)",
    "Dual-core Xtensa LX7 up to 240 MHz with ML-accelerating vector instructions",
    "ST7796 display at 320×480 (8-bit parallel) + a GT911 touch controller on I2C",
    "A BME280 environmental sensor: temperature, humidity and pressure on shared I2C",
    "A relay-driven door lock with a safe 8-second auto-relock + a status LED and physical button",
  ];
  hw.forEach((t, i) => T(s, t, { x: M + 0.3, y: y0 + 0.8 + i * 0.78, w: cw - 0.6, h: 0.74, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.15 }));
  card(s, W - M - cw, y0, cw, ch, SURF2, false);
  T(s, "Firmware (ESP-IDF + FreeRTOS)", { x: W - M - cw + 0.3, y: y0 + 0.22, w: cw - 0.6, h: 0.45, fontSize: 18, bold: true, color: PRIMARY });
  const fw = [
    "Dedicated flash partitioning: app, NVS and SPIFFS (the face database)",
    "Multi-core tasks with independent priorities and stacks: UI, face processing, networking, the learning agent",
    "Synchronization through shallow queues (e.g., a depth-1 face queue) instead of direct shared memory",
    "A task watchdog (TWDT) + a touch watchdog with a hardware reset sequence (RST/INT)",
    "Periodic heap monitoring (every 30 s) as an early memory-corruption alarm",
  ];
  fw.forEach((t, i) => T(s, t, { x: W - M - cw + 0.3, y: y0 + 0.8 + i * 0.78, w: cw - 0.6, h: 0.74, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.15 }));
  T(s, "Pin note: GPIO35–GPIO37 are unavailable due to the octal PSRAM.", { x: M, y: 6.68, w: W - 2 * M, h: 0.32, fontSize: 12, color: MUTED });
  pageNum(s, 8);
  s.addNotes("Left: hardware; right: firmware. Emphasize PSRAM as the enabler for running UI+TLS+CNN concurrently.");
}

/* ---------------- S9 — Local infrastructure ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 5 — Communication Architecture", "A fully local infrastructure: MQTT and Home Assistant");
  imgCard(s, `${FIGS}/fig1_architecture.png`, W - M - 6.6, 1.95, 6.6, 4.27);
  const rows = [
    ["MQTT: publish/subscribe with a central broker", "QoS=1 for state, retained messages and an LWT to detect a lost board."],
    ["Auto-discovery (MQTT Discovery)", "13 entities register in Home Assistant without any manual configuration."],
    ["Docker deployment on a home server", "Mosquitto + Home Assistant (+ the attendance server) — portable and cloud-free."],
    ["Runs on the home modem alone", "Works uninterrupted through international and domestic internet outages; minimal latency."],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, M, 2.0 + i * 1.12, 5.3, { dy: 0.37, dh: 0.55, ds: 12.5 }));
  pageNum(s, 9);
  s.addNotes("Hierarchical topics; adding a new subscriber without touching the board. The topic table is in the report (5-1).");
}

/* ---------------- S10 — Divider: Edge AI ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG_DARK };
  s.addText("EDGE AI", { x: 0.4, y: 1.5, w: W - 0.8, h: 3.4, fontSize: 130, fontFace: TF, color: "1D3350", align: "center", margin: 0, bold: true });
  T(s, "The core of this talk", { x: 1, y: 2.35, w: W - 2, h: 0.4, fontSize: 15, bold: true, color: ACCENT, align: "center" });
  T(s, "Artificial Intelligence on the Edge", { x: 1, y: 2.8, w: W - 2, h: 1.0, fontSize: 44, fontFace: TF, color: "FFFFFF", align: "center", bold: true });
  T(s, "Two intelligent engines, both on one constrained microcontroller", { x: 1, y: 3.9, w: W - 2, h: 0.45, fontSize: 16, color: LIGHT_ON_DARK, align: "center" });
  const boxes = [
    ["1 — Face recognition on a microcontroller", "A complete computer-vision pipeline with ESP-DL; authentication without a cloud"],
    ["2 — A behavioral learning agent on the chip", "Two logistic regressions; offline pre-training + online SGD"],
  ];
  const bw = 5.5, by = 4.75;
  boxes.forEach(([t, d], i) => {
    const x = i === 0 ? W / 2 - bw - 0.25 : W / 2 + 0.25;
    s.addShape("roundRect", { x, y: by, w: bw, h: 1.5, rectRadius: 0.08, fill: { color: "1D3350" }, line: { color: "2E4A6E", width: 1 } });
    T(s, t, { x: x + 0.3, y: by + 0.22, w: bw - 0.6, h: 0.45, fontSize: 16.5, bold: true, color: "FFFFFF", align: "center" });
    T(s, d, { x: x + 0.3, y: by + 0.75, w: bw - 0.6, h: 0.55, fontSize: 12.5, color: LIGHT_ON_DARK, align: "center", lineSpacingMultiple: 1.2 });
  });
  pageNum(s, 10);
  s.addNotes("The core section: two AI pillars. First face recognition, then the behavioral agent with a full model walkthrough.");
}

/* ---------------- S11 — Face pipeline ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Edge AI — 1) Face recognition", "The complete face-recognition pipeline on a microcontroller", 27);
  imgCard(s, `${FIGS}/fig2_face_pipeline.png`, (W - 8.9) / 2, 1.62, 8.9, 4.17);
  const notes = [
    "A depth-1 input queue — serialized processing, no memory interference",
    "Large buffers in PSRAM — internal RAM stays free for TLS",
    "Non-blocking hand-off via the face_worker task — the UI stays responsive",
  ];
  const nw = 4.1, ny = 6.1, gap = 0.25;
  notes.forEach((t, i) => {
    const x = M + i * (nw + gap);
    card(s, x, ny, nw, 0.85, SURF2);
    T(s, t, { x: x + 0.15, y: ny, w: nw - 0.3, h: 0.85, fontSize: 12, color: PRIMARY, valign: "middle", lineSpacingMultiple: 1.15 });
  });
  pageNum(s, 11);
  s.addNotes("A JPEG up to 300 KB from the phone browser → hardware decoding to an RGB565 320×240 buffer in PSRAM → HumanFaceDetect → embedding via HumanFaceRecognizer → matching against the face database in SPIFFS; threshold 0.70.");
}

/* ---------------- S12 — Enrollment & threshold ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Edge AI — 1) Face recognition", "Multi-sample enrollment and the decision threshold");
  const steps = [
    ["The admin issues a link", "From the settings page (protected by a password and a 10-minute session)"],
    ["A six-digit token valid for 3 minutes", "Link https://IP/enroll?token=… — the token is multi-use until it expires"],
    ["Multiple samples from the mobile web page", "POST /api/face/enroll — samples with varying pose and lighting"],
    ["Grouped under one name in the face database", "The face_db layer with a sample counter in the SPIFFS partition"],
  ];
  steps.forEach(([t, d], i) => numRow(s, i + 1, t, d, M, 1.95 + i * 1.18, 6.55, { dy: 0.38, dh: 0.6, ds: 12.5 }));
  const bx = W - M - 5.35, bw = 5.35;
  card(s, bx, 1.95, bw, 2.6, SURFACE, false);
  T(s, "Why 0.70?", { x: bx + 0.3, y: 2.18, w: bw - 0.6, h: 0.45, fontSize: 17, bold: true, color: PRIMARY });
  T(s, "The threshold was kept after practical experiments with multi-sample enrollment; enrolling several samples visibly improved matching under varying light and pose, and further calibration was deliberately postponed to later versions.", { x: bx + 0.3, y: 2.7, w: bw - 0.6, h: 1.7, fontSize: 13, color: TEXT, lineSpacingMultiple: 1.3 });
  card(s, bx, 4.75, bw, 1.85, SURF2, false);
  T(s, "Engineering decisions in the pipeline", { x: bx + 0.3, y: 4.95, w: bw - 0.6, h: 0.4, fontSize: 15, bold: true, color: PRIMARY });
  T(s, "A depth-1 queue for model stability, large buffers in PSRAM and a non-blocking web-server hand-off — all to keep the UI responsive on a single chip.", { x: bx + 0.3, y: 5.4, w: bw - 0.6, h: 1.1, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.25 });
  pageNum(s, 12);
  s.addNotes("The 4-step enrollment flow; note: the 0.70 threshold suffices with multi-sample enrollment (a deliberate decision).");
}

/* ---------------- S13 — ML model formulation ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Edge AI — 2) The behavioral learning agent", "Model formulation: two logistic regressions with context-only features", 26);
  const rowsT = [
    [{ text: "Description", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } },
     { text: "Count", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } },
     { text: "Feature group", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } }],
    ["The model's bias term", "1", "Bias"],
    ["Three sin/cos harmonics covering the daily pattern without a midnight discontinuity", "6", "Hourly harmonics"],
    ["Thursday and Friday", "2", "Holidays"],
    ["HOME after each successful unlock (min. 45 min)", "1", "Resident presence"],
    ["Normalized light intensity, temperature and humidity", "2", "Environmental context"],
  ];
  s.addTable(rowsT, {
    x: M, y: 1.95, w: 7.1, colW: [4.25, 0.85, 2.0], rowH: 0.52,
    fontFace: BF, fontSize: 12, color: TEXT, align: "left", valign: "middle",
    border: { pt: 0.5, color: HAIR }, fill: { color: "FFFFFF" }, margin: 0.06,
  });
  T(s, "Report table 6-1: the 12-dimensional context-only feature vector — no own-device state", { x: M, y: 5.35, w: 7.1, h: 0.55, fontSize: 12, color: MUTED, lineSpacingMultiple: 1.2 });
  const fx = W - M - 4.9, fw2 = 4.9;
  card(s, fx, 1.95, fw2, 2.5, SURFACE, false);
  T(s, "Decision model (one sigmoid neuron per device)", { x: fx + 0.25, y: 2.12, w: fw2 - 0.5, h: 0.42, fontSize: 14, bold: true, color: PRIMARY });
  s.addText([
    { text: "z = w\u1D40x ,   p = \u03C3(z) = 1 / (1 + e\u207B\u1DBB)", options: { breakLine: true } },
    { text: "", options: { breakLine: true } },
    { text: "p \u2265 0.75 \u2192 ON ,   p \u2264 0.25 \u2192 OFF", options: { breakLine: true } },
    { text: "0.25 < p < 0.75 \u2192 hold (no action)", options: {} },
  ], { x: fx + 0.25, y: 2.6, w: fw2 - 0.5, h: 1.7, fontSize: 14, fontFace: BF, color: TEXT, align: "left", margin: 0, lineSpacingMultiple: 1.25 });
  card(s, fx, 4.7, fw2, 2.1, "FBF4E4", false);
  T(s, "Engineering lesson: inert behavior", { x: fx + 0.25, y: 4.88, w: fw2 - 0.5, h: 0.42, fontSize: 14.5, bold: true, color: ACCENT });
  T(s, "In the first version, the own-device-state feature left the model passive despite 91% auto-mode accuracy; removing it and adding the third harmonic raised accuracy to 94% and made the behavior active.", { x: fx + 0.25, y: 5.32, w: fw2 - 0.5, h: 1.4, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.28 });
  pageNum(s, 13);
  s.addNotes("Two independent light/fan models. Features are purely contextual: circular time (harmonics), holidays, presence, light/temp/humidity. Key lesson: dropping the device-state feature removed the inertia.");
}

/* ---------------- S14 — Model figure ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Edge AI — 2) The behavioral learning agent", "Model architecture, formulas and decision rules", 27);
  imgCard(s, `${FIGS}/fig_ml_model.png`, (W - 8.55) / 2, 1.62, 8.55, 5.52);
  pageNum(s, 14);
  s.addNotes("This figure is the most complete view of the model: inputs, each device's sigmoid neuron, the decision rule with thresholds, the promotion gate, offline pre-training, online SGD and NVS persistence. The lock is never model-controlled.");
}

/* ---------------- S15 — Training ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Edge AI — 2) The behavioral learning agent", "Hybrid training: offline pre-training + online learning");
  const rowsT = [
    [{ text: "Value", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } },
     { text: "Parameter", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } }],
    ["30 days, 8,640 samples (5-minute steps)", "Pre-training dataset"],
    ["Full-batch gradient, 1,500 iterations, with L2", "Offline optimization"],
    ["The final 6 days of data", "Validation"],
    ["SGD at rate 0.08, weight clipping (±8)", "Online learning"],
    ["NVS (~90 bytes, every 30 s when changed)", "Weight storage"],
    ["Every 30 seconds", "Decision period"],
    ["p ≥ 0.75 on, p ≤ 0.25 off", "Action thresholds"],
    ["Light 94.2% — Fan 93.6% (Bayes cap ≈ 95%)", "Final accuracy"],
  ];
  s.addTable(rowsT, {
    x: M, y: 1.9, w: 6.3, colW: [4.2, 2.1], rowH: 0.55,
    fontFace: BF, fontSize: 12, color: TEXT, align: "left", valign: "middle",
    border: { pt: 0.5, color: HAIR }, margin: 0.06,
  });
  const lx = W - M - 5.6, lw = 5.6;
  card(s, lx, 1.9, lw, 2.3, SURFACE, false);
  T(s, "1) Offline pre-training (on a PC)", { x: lx + 0.28, y: 2.08, w: lw - 0.56, h: 0.42, fontSize: 15.5, bold: true, color: PRIMARY });
  T(s, "A 30-day synthetic dataset built from the user's policy with an intentional 4–5% label noise; full-batch optimization with L2 regularization. The theoretical (Bayes) cap of the problem is ≈ 95%.", { x: lx + 0.28, y: 2.55, w: lw - 0.56, h: 1.55, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.28 });
  card(s, lx, 4.4, lw, 2.35, SURF2, false);
  T(s, "2) Online learning (on the chip itself)", { x: lx + 0.28, y: 4.58, w: lw - 0.56, h: 0.42, fontSize: 15.5, bold: true, color: PRIMARY });
  T(s, "Every manual user action (from any channel) is a training sample and triggers one SGD step; the model's own actions are not trained on (no self-stimulation feedback). Weights persist in NVS with versioning and are restored after a reset.", { x: lx + 0.28, y: 5.05, w: lw - 0.56, h: 1.6, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.28 });
  T(s, "Report table 6-2: training parameters and agent performance", { x: M, y: 6.9, w: 5.6, h: 0.35, fontSize: 12, color: MUTED });
  pageNum(s, 15);
  s.addNotes("Two-stage training: offline numpy (full-batch GD + L2); then online SGD on the chip with η=0.08 and ±8 clipping. A subtle contract: the feature order is exactly identical between Python and C.");
}

/* ---------------- S16 — Learning GIF ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Edge AI — 2) The behavioral learning agent", "Online learning in action: from zero weights to the user's pattern", 27);
  imgCard(s, `${FIGS}/fig_ml_learning.gif`, M, 1.85, 7.6, 4.12);
  const rows = [
    ["Starts in the shadow phase with w = 0", "The model knows nothing; the p output is flat."],
    ["Every manual action = one SGD step", "Weights update at rate 0.08 with ±8 clipping"],
    ["p(light) bends toward the day/night pattern", "The hourly-harmonic and presence weights take shape."],
    ["Past the promotion gate → auto mode", "The 20-decision window reaches ≥ 85% accuracy."],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, 8.6, 2.0 + i * 1.12, 4.25, { dy: 0.37, dh: 0.55, ds: 11.5, ts: 13.5 }));
  T(s, "Note: the animation starts from zero weights so the learning process is visible; the real device boots from pre-trained weights.", { x: 8.6, y: 6.35, w: 4.25, h: 0.75, fontSize: 11.5, color: MUTED, lineSpacingMultiple: 1.2 });
  pageNum(s, 16);
  s.addNotes("The GIF: the left chart is p(light ON) across the day; the right chart shows the weights. Motion: every user action is an SGD step; the curve bends toward the real pattern; then promotion.");
}

/* ---------------- S17 — Shadow → Auto ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Edge AI — 2) The behavioral learning agent", "Model lifecycle: shadow phase → auto phase", 27);
  imgCard(s, `${FIGS}/fig5_ml_agent.png`, M, 1.85, 7.9, 4.23);
  const rows = [
    ["Shadow phase: predict, don't act", "A prediction every 30 s, published to MQTT and evaluated."],
    ["The promotion gate", "A 20-decision moving window; promotion at ≥ 15 correct decisions and ≥ 85% accuracy (independent per device)."],
    ["Auto phase: act only with confidence", "p ≥ 0.75 → on, p ≤ 0.25 → off, otherwise no action."],
    ["Manual action always wins", "Executed immediately and recorded as a training sample; the ML Autonomy switch in HA."],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, 8.85, 1.95 + i * 1.15, 4.0, { dy: 0.36, dh: 0.62, ds: 11.5, ts: 13.5 }));
  card(s, 8.85, 6.55, 4.0, 0.52, "FBF4E4");
  T(s, "The door lock is never under model control", { x: 9.0, y: 6.55, w: 3.7, h: 0.52, fontSize: 11.5, bold: true, color: ACCENT, valign: "middle" });
  pageNum(s, 17);
  s.addNotes("The shadow→auto lifecycle + returning to shadow via the HA switch. The hard security rule: the lock is outside model control.");
}

/* ---------------- S18 — ML results ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Edge AI — 2) The behavioral learning agent", "Results: the model has reached the theoretical limit", 27);
  const stats = [
    ["94.2%", "Light accuracy (validation)", PRIMARY],
    ["93.6%", "Fan accuracy (validation)", PRIMARY],
    ["≈ 95%", "The Bayes cap of the problem", ACCENT],
  ];
  const sw = 3.7, sy = 1.95, shh = 2.0;
  stats.forEach(([v, l, c], i) => {
    const x = M + i * (sw + 0.3);
    card(s, x, sy, sw, shh, SURFACE, true);
    T(s, v, { x, y: sy + 0.22, w: sw, h: 1.0, fontSize: 46, fontFace: TF, color: c, align: "center" });
    T(s, l, { x: x + 0.2, y: sy + 1.35, w: sw - 0.4, h: 0.45, fontSize: 14, color: MUTED, align: "center" });
  });
  T(s, "Reference behavioral scenarios — re-tested on the deployed firmware:", { x: M, y: 4.35, w: W - 2 * M, h: 0.45, fontSize: 15.5, bold: true, color: PRIMARY });
  const scen = [
    ["Night + presence + darkness", "p ≈ 0.99 → turn the light on"],
    ["Noon with the light on", "p ≤ 0.05 → turn the light off"],
    ["Noon with the light off", "p ≈ 0.07 → no action (dead zone)"],
  ];
  const rw = 4.0, ry = 4.95;
  scen.forEach(([t, d], i) => {
    const x = M + i * (rw + 0.25);
    card(s, x, ry, rw, 1.15, SURF2);
    T(s, t, { x: x + 0.22, y: ry + 0.14, w: rw - 0.44, h: 0.4, fontSize: 13.5, bold: true, color: PRIMARY });
    T(s, d, { x: x + 0.22, y: ry + 0.58, w: rw - 0.44, h: 0.45, fontSize: 13, color: TEXT });
  });
  T(s, "The labels intentionally carry 4–5% human noise; the automatic shadow → auto promotion was nevertheless observed in the practical test.", { x: M, y: 6.45, w: W - 2 * M, h: 0.4, fontSize: 12.5, color: MUTED });
  pageNum(s, 18);
  s.addNotes("Accuracy on the final 6 days of data. Three reference scenarios on the real firmware. The model has effectively reached the Bayes cap.");
}

/* ---------------- S19 — UI channels ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 7 — User Interfaces", "Four parallel channels with one source of truth");
  imgCard(s, `${FIGS}/fig3_ui_channels.png`, M, 1.95, 6.35, 3.62);
  const rows = [
    ["A secure web server on the board", "esp_https_server with a self-signed certificate; TLS is a technical prerequisite for camera access (getUserMedia)."],
    ["Short-path entry via QR", "The recognize, enroll and attendance pages with time-limited tokens and auto-renewal."],
    ["One source of truth (app_state)", "Display, web panel, HA dashboard and HA app always show a consistent picture."],
    ["Graceful degradation on failure", "With no network, the display still manages lock, light and fan."],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, 7.35, 1.95 + i * 1.15, 5.5, { dy: 0.36, dh: 0.6, ds: 12 }));
  T(s, "13 HA entities without a single extra line of code on the board; six LVGL pages on the display.", { x: 7.35, y: 6.55, w: 5.5, h: 0.4, fontSize: 12, color: MUTED });
  pageNum(s, 19);
  s.addNotes("Four channels: LCD, mobile web, HA dashboard, HA app. The endpoint table is in the report (7-1).");
}

/* ---------------- S20 — Attendance ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 8 — Attendance", "A three-tier architecture with a durable message queue");
  imgCard(s, `${FIGS}/fig4_attendance.png`, M, 1.95, 6.5, 3.77);
  const rows = [
    ["Board tier: QR generation and face matching", "A 10-minute QR with a countdown; 60-second anti-duplication."],
    ["A durable NVS queue (32 slots)", "If the network/server is unavailable, records wait and are sent in order later."],
    ["Local server: FastAPI + SQLite", "A table acting as an outbox; a 15-second sync."],
    ["Optional delivery: Jalali sheet + Bale", "A Jalali calendar with no dependency; the board holds no cloud credentials."],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, 7.45, 1.95 + i * 1.15, 5.4, { dy: 0.36, dh: 0.6, ds: 12 }));
  pageNum(s, 20);
  s.addNotes("Independent of HA and MQTT. Credential separation: board-server communication uses only a shared secret.");
}

/* ---------------- S21 — Security ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 9 — Security & Privacy", "The system's defense layers");
  const items = [
    ["Face-data privacy", "The image is processed and freed on the board itself; the only output is a decision and a feature vector — no raw image, no cloud service."],
    ["Web-layer security", "TLS with a self-signed certificate; an HttpOnly/Secure/SameSite=Strict session cookie with a 10-minute expiry; 3- and 10-minute tokens; a deliberate 1-second delay on wrong passwords."],
    ["Door-lock security", "Unlocking only through verified paths; no command path from HA; the relay has an independent failsafe: if the firmware dies, the lock re-engages automatically within 8 seconds."],
    ["A known boundary and an upgrade path", "Mosquitto currently runs without authentication (a deliberate choice for a home deployment); next: broker auth+TLS and WebAuthn — with no board changes."],
  ];
  const cw = 6.05, chh = 2.28, y0 = 1.85;
  items.forEach(([t, d], i) => {
    const col = i % 2 === 0 ? 0 : 1;
    const x = col ? W - M - cw : M;
    const y = y0 + Math.floor(i / 2) * (chh + 0.3);
    card(s, x, y, cw, chh, i === 0 ? SURF2 : SURFACE, true);
    T(s, t, { x: x + 0.3, y: y + 0.24, w: cw - 0.6, h: 0.45, fontSize: 16.5, bold: true, color: PRIMARY });
    T(s, d, { x: x + 0.3, y: y + 0.78, w: cw - 0.6, h: chh - 1.0, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.3 });
  });
  pageNum(s, 21);
  s.addNotes("Four security pillars. Emphasis: the face image never leaves the device; the lock is failsafe; the Mosquitto boundary is known and its upgrade path is planned.");
}

/* ---------------- S22 — Tests ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 10 — Tests & Results", "End-to-end testing: 14 scenarios, all 14 successful");
  const stats2 = [
    ["14/14", "Passing end-to-end scenarios"],
    ["3", "Test levels: unit, integration, E2E"],
    ["0", "Unintended resets in long-running operation"],
  ];
  const sw2 = 5.2, sy2 = 1.9;
  stats2.forEach(([v, l], i) => {
    const y = sy2 + i * 1.58;
    card(s, M, y, sw2, 1.38, SURFACE, true);
    T(s, v, { x: M + 0.3, y: y + 0.1, w: 1.9, h: 1.18, fontSize: 30, fontFace: TF, color: i === 0 ? ACCENT : PRIMARY, align: "center", valign: "middle" });
    T(s, l, { x: M + 2.4, y: y + 0.1, w: 2.55, h: 1.18, fontSize: 13, color: MUTED, align: "left", valign: "middle", lineSpacingMultiple: 1.2 });
  });
  const rowsT = [
    [{ text: "Result", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } },
     { text: "Scenario", options: { bold: true, color: "FFFFFF", fill: { color: PRIMARY }, align: "center" } }],
    ["Pass", "Door unlock with a face under different light and pose"],
    ["Pass", "Rejecting an unknown face and retrying"],
    ["Pass", "Attendance + 60-second anti-duplication"],
    ["Pass", "Durable queue: server outage and recovery"],
    ["Pass", "Wi-Fi and MQTT reconnection (5 attempts)"],
    ["Pass", "Full internet outage: all local features keep working"],
    ["Pass", "Automatic touch recovery from an I2C bus lockup"],
  ];
  s.addTable(rowsT, {
    x: 5.95, y: 1.9, w: 6.9, colW: [1.25, 5.65], rowH: 0.51,
    fontFace: BF, fontSize: 12, color: TEXT, align: "left", valign: "middle",
    border: { pt: 0.5, color: HAIR }, margin: 0.06,
  });
  T(s, "Method: feature-driven development on Git branches and on-board testing before merge; an excerpt of report table 10-1.", { x: M, y: 6.65, w: 5.2, h: 0.6, fontSize: 12, color: MUTED, lineSpacingMultiple: 1.25 });
  pageNum(s, 22);
  s.addNotes("Scenario-driven E2E testing by a real user. 7 of the 14 scenarios of report table 10-1.");
}

/* ---------------- S23 — Engineering challenges ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 10 — Tests & Results", "Engineering challenges, root-caused and fixed");
  const items = [
    ["Out of memory during the TLS handshake after adding web graphics", "Root cause: LVGL render buffers occupying internal RAM → buffers moved to PSRAM + a network keep-alive + lru_purge and 16 lwIP sockets"],
    ["An I2C bus lockup of the GT911 touch controller", "Root cause: a mutex leak on the error path → fixed the release path + a touch watchdog with a hardware reset sequence (RST/INT) — the system became self-healing"],
    ["Watchdog panic loops in the face task", "Root cause: long blocking waits for new work → periodic TWDT feeding (max every 5 s) in the wait loop + a 20-second timeout for the image body"],
    ["A passive behavioral model despite high accuracy", "Root cause: the own-device-state feature caused inertia and inaction → the feature vector was redesigned to be context-only (chapter 6)"],
  ];
  const rowH = 1.22, y0 = 1.85;
  items.forEach(([t, d], i) => {
    const y = y0 + i * rowH;
    T(s, String(i + 1), { x: M, y: y + 0.05, w: 0.6, h: 0.6, fontSize: 24, fontFace: TF, color: ACCENT, align: "center" });
    T(s, t, { x: M + 0.8, y, w: W - 2 * M - 0.95, h: 0.42, fontSize: 14.5, bold: true, color: PRIMARY });
    T(s, d, { x: M + 0.8, y: y + 0.44, w: W - 2 * M - 0.95, h: 0.66, fontSize: 12, color: MUTED, lineSpacingMultiple: 1.2 });
    if (i < items.length - 1) s.addShape("line", { x: M, y: y + rowH - 0.14, w: W - 2 * M, h: 0, line: { color: HAIR, width: 0.75 } });
  });
  pageNum(s, 23);
  s.addNotes("Each challenge: symptom → root cause → fix. Demonstrates real debugging skill in embedded systems.");
}

/* ---------------- S24 — Divider: real hardware ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG_DARK };
  s.addText("REAL", { x: 0.4, y: 0.75, w: W - 0.8, h: 2.9, fontSize: 115, fontFace: TF, color: "1D3350", align: "center", margin: 0, bold: true });
  imgCard(s, `${IMG}/Board Image.jpg`, W - M - 3.65, 2.0, 3.6, 4.8);
  T(s, "Chapter 11 — the final proof", { x: M, y: 2.1, w: 8.13, h: 0.4, fontSize: 15, bold: true, color: ACCENT });
  T(s, "Running on Real Hardware", { x: M, y: 2.55, w: 8.13, h: 0.9, fontSize: 40, fontFace: TF, color: "FFFFFF", bold: true });
  T(s, "Every component — exactly as designed — running on the actual board and services", { x: M, y: 3.6, w: 8.13, h: 0.5, fontSize: 15.5, color: LIGHT_ON_DARK });
  T(s, "1", { x: M, y: 4.45, w: 0.65, h: 0.55, fontSize: 21, fontFace: TF, color: ACCENT, align: "center" });
  T(s, "A real interface on the display", { x: M + 0.85, y: 4.45, w: 7.15, h: 0.4, fontSize: 16, bold: true, color: "FFFFFF" });
  T(s, "Nine LVGL pages — screenshots from the real LCD and a touch-test video", { x: M + 0.85, y: 4.85, w: 7.15, h: 0.4, fontSize: 13, color: LIGHT_ON_DARK });
  T(s, "2", { x: M, y: 5.55, w: 0.65, h: 0.55, fontSize: 21, fontFace: TF, color: ACCENT, align: "center" });
  T(s, "Live test recordings", { x: M + 0.85, y: 5.55, w: 7.15, h: 0.4, fontSize: 16, bold: true, color: "FFFFFF" });
  T(s, "Face-based attendance, the web dashboard, the Home Assistant app and the Bale bot", { x: M + 0.85, y: 5.95, w: 7.15, h: 0.4, fontSize: 13, color: LIGHT_ON_DARK });
  pageNum(s, 24);
  s.addNotes("A new chapter: from figures to real hardware. The bench photo: the ESP32-S3 board, a lit LCD and the HA app on a phone.");
}

/* ---------------- S25 — Real LCD UI gallery ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Real hardware — the touch display", "Nine LVGL pages, captured from the real LCD", 27);
  const lcd = [
    "Main Screen.png", "Unlock Screen.png", "Wifi List.png",
    "Mqtt Setting.png", "Setting Screen.png", "Attend QR Code.png",
  ];
  const cw = 1.72, chh = 2.56, gx = 0.2, gy = 0.2, x0 = W - M - cw - 2 * (cw + gx), y0 = 1.62;
  lcd.forEach((f, i) => {
    const col = i % 3, row = Math.floor(i / 3);
    const x = x0 + col * (cw + gx); // LTR: first item leftmost
    const y = y0 + row * (chh + gy);
    imgCard(s, `${IMG}/LCD UI/${f}`, x, y, cw, chh);
  });
  const rows = [
    ["Network and broker setup on the device itself", "Scan and join Wi-Fi with the on-screen keyboard; configure Mosquitto without a PC"],
    ["Face and password management from the display", "Enroll/delete faces, the settings-page password and the door PIN — all on the device"],
    ["The dual-mode attendance QR", "A 10-minute token with a countdown; entry and exit on the same page"],
    ["One source of truth for every channel", "The same app_state the web dashboard, HA and the Bale bot show"],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, M, 1.85 + i * 1.18, 6.28, { dy: 0.38, dh: 0.6, ds: 12.5 }));
  T(s, "6 of 9 pages — the other three (Wi-Fi password, web QR and the faces list) are in the repository README.", { x: M, y: 6.55, w: 6.28, h: 0.45, fontSize: 12, color: MUTED, lineSpacingMultiple: 1.2 });
  pageNum(s, 25);
  s.addNotes("Straight screenshots from the 320×480 LCD. Emphasis: full network and broker setup with no PC — only the display.");
}

/* ---------------- S26 — Video: LCD touch test ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Real hardware — video 1", "Touch-display test on the real board", 27);
  const vw = 2.98, vh = 5.3, vx = W - M - vw;
  s.addShape("roundRect", { x: vx - 0.08, y: 1.67, w: vw + 0.16, h: vh + 0.16, rectRadius: 0.05, fill: { color: "FFFFFF" }, line: { color: HAIR, width: 1 }, shadow: sh() });
  s.addMedia({ type: "video", path: silentVideo(`${IMG}/Lcd Test.mp4`, "scale=608:1080,setsar=1"), x: vx, y: 1.75, w: vw, h: vh, cover: coverData("poster-lcd") });
  T(s, "Real test recording — 39 s", { x: vx - 0.3, y: 7.1, w: vw + 0.6, h: 0.3, fontSize: 11.5, color: MUTED, align: "center" });
  const rows = [
    ["Touch and instant UI feedback", "Moving between pages, light/fan buttons and the brightness slider — while networking and the UI run concurrently"],
    ["Unlocking with a PIN and a face", "An on-screen numeric keypad and the face-recognition flow from the same page"],
    ["Live sensors", "BME280 temperature, humidity and pressure in real time on the bottom tiles"],
    ["Status LEDs on the board", "The LEDs on the top rail light up in sync with light commands"],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, M, 1.95 + i * 1.18, 8.9, { dy: 0.38, dh: 0.6, ds: 12.5 }));
  pageNum(s, 26);
  s.addNotes("An embedded video — click to play during the talk. Touching different pages, the door PIN, the sensors and the LED responses.");
}

/* ---------------- S27 — Video: attendance E2E ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Real hardware — video 2", "Face-based attendance, end to end", 27);
  const vw = 2.24, vh = 5.15, vx = W - M - vw - 0.09;
  s.addShape("roundRect", { x: vx - 0.09, y: 1.57, w: vw + 0.18, h: vh + 0.18, rectRadius: 0.12, fill: { color: "101B2B" }, line: { color: "2E4A6E", width: 1 }, shadow: sh() });
  s.addMedia({ type: "video", path: silentVideo(`${IMG}/Attendance test.mp4`), x: vx, y: 1.66, w: vw, h: vh, cover: coverData("poster-attendance") });
  T(s, "Real test recording — 30 s", { x: vx - 0.4, y: 6.98, w: vw + 0.8, h: 0.3, fontSize: 11.5, color: MUTED, align: "center" });
  const rows = [
    ["Scanning the QR from the LCD", "The display's 10-minute token is read with the phone's camera — no app install needed"],
    ["Face matching on the board itself", "A photo from the phone browser → hardware JPEG decoding → feature extraction → matching against the face database"],
    ["The record lands on the local server", "Entry/exit type with an exact timestamp in FastAPI/SQLite"],
    ["Archive and notification — immediately", "A Jalali sheet + a Bale message; if the network is down, the record waits in the NVS queue and is sent later"],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, M, 1.95 + i * 1.18, 9.5, { dy: 0.38, dh: 0.6, ds: 12.5 }));
  pageNum(s, 27);
  s.addNotes("The whole flow in one video: scanning the QR from the LCD, the camera and face matching, the entry record and the Bale banner at the end.");
}

/* ---------------- S28 — Video: web dashboard + HA app ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Real hardware — video 3", "The web dashboard and the Home Assistant app on a phone", 27);
  const vw = 2.24, vh = 5.15, vy = 1.66;
  const vids = [
    { f: "Smart Home – Home Assistant_Mobile.mp4", p: "poster-ha", cap: "Home Assistant app — 58 s", x: W - M - vw - 0.09 },
    { f: "Web Dashboard test.mp4", p: "poster-webdash", cap: "Web dashboard — 43 s", x: W - M - 2 * vw - 0.18 - 0.5 },
  ];
  vids.forEach(v => {
    s.addShape("roundRect", { x: v.x - 0.09, y: vy - 0.09, w: vw + 0.18, h: vh + 0.18, rectRadius: 0.12, fill: { color: "101B2B" }, line: { color: "2E4A6E", width: 1 }, shadow: sh() });
    s.addMedia({ type: "video", path: silentVideo(`${IMG}/${v.f}`), x: v.x, y: vy, w: vw, h: vh, cover: coverData(v.p) });
    T(s, v.cap, { x: v.x - 0.4, y: vy + vh + 0.16, w: vw + 0.8, h: 0.3, fontSize: 11.5, color: MUTED, align: "center" });
  });
  const rows = [
    ["The board's own web dashboard", "Light and fan switches, door lock with PIN and Face ID, live environment and ML-agent status"],
    ["The Home Assistant app with 13 entities", "MQTT auto-discovery; control, automation and notifications — all bidirectional"],
    ["One source of truth", "A change from any channel is reflected instantly in the others"],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, M, 1.95 + i * 1.35, 6.6, { dy: 0.38, dh: 0.62, ds: 12.5 }));
  pageNum(s, 28);
  s.addNotes("Two videos: the HA app scrolling through the dashboard and automations; the web dashboard unlocking the door and toggling devices.");
}

/* ---------------- S29 — Real channel screenshots ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Real hardware — stills", "The web dashboard, HA app and Bale bot in one frame", 27);
  const shots = [
    { f: `${IMG}/Web Dashboard.png`, w: 2.51, label: "Web dashboard" },
    { f: `${IMG}/Smart Home – Home Assistant_Dashboard.png`, w: 2.70, label: "Home Assistant app" },
    { f: `${IMG}/Bale_1.jpg`, w: 2.12, label: "Bale bot" },
  ];
  let sx = M;
  shots.forEach(sh2 => {
    const h2 = 4.7, x = sx;
    T(s, sh2.label, { x: x - 0.08, y: 1.58, w: sh2.w + 0.16, h: 0.3, fontSize: 11.5, color: MUTED, align: "center" });
    imgCard(s, sh2.f, x, 1.95, sh2.w, h2);
    sx = x + sh2.w + 0.25;
  });
  const rows = [
    ["Web dashboard (mobile view)", "Door lock with PIN and Face ID, environment, the ML agent in SHADOW"],
    ["The Home Assistant app", "Environment cards, device controls, access and four real automations"],
    ["The Bale bot", "A Persian command menu, door opening via a one-time password, event notifications"],
  ];
  rows.forEach(([t, d], i) => numRow(s, i + 1, t, d, 8.6, 1.95 + i * 1.5, 4.23, { dy: 0.4, dh: 0.72, ds: 12, ts: 14 }));
  pageNum(s, 29);
  s.addNotes("Three channels side by side: the board's web dashboard, the HA app and the Bale bot — all fed by one source of truth.");
}

/* ---------------- S30 — Deployment & archive ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Real hardware — infrastructure", "Docker deployment and the Jalali archive", 27);
  imgCard(s, `${IMG}/Docker.png`, M, 1.9, 7.3, 3.88);
  imgCard(s, `${IMG}/Google Sheet.png`, W - M - 3.5, 1.9, 3.5, 3.73);
  const cards3 = [
    "Four lightweight containers: Mosquitto, Home Assistant, attendance and bale-bot",
    "Automatic archiving with the Jalali calendar — entry/exit columns on the sheet",
    "A Bale notification the moment each attendance record lands",
  ];
  const cw3 = 3.94, cy = 6.0;
  cards3.forEach((t, i) => {
    const x = M + i * (cw3 + 0.25);
    card(s, x, cy, cw3, 0.75, SURF2);
    T(s, t, { x: x + 0.15, y: cy, w: cw3 - 0.3, h: 0.75, fontSize: 11.5, color: PRIMARY, valign: "middle", lineSpacingMultiple: 1.15 });
  });
  pageNum(s, 30);
  s.addNotes("Right: the Google Sheet with Jalali dates. Left: Docker Desktop with the four containers running.");
}

/* ---------------- S31 — Summary + future ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG };
  header(s, "Chapter 12 — Wrap-up", "Summary and future work");
  const cw = 6.05, y0 = 1.85, chh = 4.9;
  card(s, M, y0, cw, chh, SURFACE, false);
  T(s, "What was built", { x: M + 0.3, y: y0 + 0.22, w: cw - 0.6, h: 0.45, fontSize: 17, bold: true, color: PRIMARY });
  const done = [
    "Full face recognition on a microcontroller; camera and UI borrowed from a phone",
    "A 100% local infrastructure with Docker — full function through internet outages",
    "A behavioral agent at 94.2% and 93.6% with persistent online learning",
    "Three-tier attendance with a durable queue, a Jalali sheet and Bale notifications",
    "Four interaction channels aligned with one source of truth",
    "Proven reliability: watchdogs, network-aware reconnection, E2E tests",
  ];
  done.forEach((t, i) => T(s, t, { x: M + 0.3, y: y0 + 0.8 + i * 0.68, w: cw - 0.6, h: 0.64, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.15 }));
  card(s, W - M - cw, y0, cw, chh, SURF2, false);
  T(s, "Future work", { x: W - M - cw + 0.3, y: y0 + 0.22, w: cw - 0.6, h: 0.45, fontSize: 17, bold: true, color: PRIMARY });
  const next = [
    "Federated learning across microcontrollers — no raw data leaves the device",
    "On-chip voice intelligence with ESP-SR (a fifth channel)",
    "Liveness detection to harden face matching",
    "Wireless firmware updates (OTA)",
    "Hardening Mosquitto (auth + TLS) and migrating to WebAuthn",
    "Real presence/light sensors, two-factor fingerprinting and a custom PCB",
  ];
  next.forEach((t, i) => T(s, t, { x: W - M - cw + 0.3, y: y0 + 0.8 + i * 0.68, w: cw - 0.6, h: 0.64, fontSize: 12.5, color: TEXT, lineSpacingMultiple: 1.15 }));
  pageNum(s, 31);
  s.addNotes("Wrap-up: the project met every design goal. Future: federated, voice, liveness, OTA, hardening, PCB.");
}

/* ---------------- S32 — Closing ---------------- */
{
  const s = pres.addSlide(); s.background = { color: BG_DARK };
  s.addImage({ path: `${FIGS}/logo_1.png`, x: W / 2 - 0.45, y: 1.35, w: 0.9, h: 0.9 });
  T(s, "Thank you for your attention", { x: 1, y: 2.7, w: W - 2, h: 1.0, fontSize: 40, fontFace: TF, color: "FFFFFF", align: "center", bold: true });
  T(s, "Questions & Answers", { x: 1, y: 3.85, w: W - 2, h: 0.55, fontSize: 20, bold: true, color: ACCENT, align: "center" });
  s.addShape("line", { x: W / 2 - 1.1, y: 4.75, w: 2.2, h: 0, line: { color: "2E4A6E", width: 1 } });
  T(s, "Mohammad Soroush Rabiei — Design and Implementation of an IoT-Based Smart Home System with Edge AI", { x: 1.5, y: 5.0, w: W - 3, h: 0.45, fontSize: 13, color: LIGHT_ON_DARK, align: "center" });
  T(s, "Faculty of Electrical and Computer Engineering, Isfahan University of Technology — October 2026", { x: 1.5, y: 5.5, w: W - 3, h: 0.4, fontSize: 12, color: "8FA3BC", align: "center" });
  pageNum(s, 32);
  s.addNotes("End of the talk. Ready for questions. Slides 14 and 16 are good quick returns for ML questions.");
}

pres.writeFile({ fileName: "D:/02-Projects/Smart-Home/presentation/SmartHome-Defense-EN.pptx" })
  .then(() => console.log("WRITTEN OK"))
  .catch(e => { console.error("FAIL", e); process.exit(1); });
