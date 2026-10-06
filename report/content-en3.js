// Chapters 7-9 (English)
const { P, NItem, H1, H2, FIG, TBL } = require("./helpers_en");

const ch7 = [
  H1("Chapter 7: Multi-Channel User Interfaces", { pageBreakBefore: true }),

  H2("7.1 The Multi-Channel Architecture"),
  P("One of the design pillars of this project is comprehensive interaction coverage: the user must be able to reach the system in any situation \u2014 at the door, in another room, or away from home. Four parallel channels were therefore designed (Figure 7-1): the on-board touch display, which is always available even without a network; the mobile web panel, opened by scanning the QR code shown on the display, which uses the phone's camera as the authentication sensor; the Home Assistant web dashboard for management from a computer; and the Home Assistant mobile app, which brings the same entities onto the phone. All channels are fed from the single source of state so that the experience remains unified."),
  ...FIG("figs/fig3_ui_channels.png", "Figure 7-1: The four parallel user-interaction channels of the system", 570),

  H2("7.2 The Touch Display (LVGL)"),
  P("The on-board interface built with LVGL 9.5 consists of six pages: the main dashboard (Wi-Fi state and IP address with a QR code, MQTT state with a real switch, the attendance button, light and fan controls, the unlock button, the model-mode button that toggles between auto/shadow on a short tap and shows details on a long press, and three 96\u00D796 sensor cards for temperature/humidity/pressure); the lock's numeric keypad with visual feedback; the Wi-Fi setup page (scanning networks, highlighting recent networks, a password keyboard with a show/hide key); the settings page (changing the lock PIN and the settings password, the list of enrolled faces with deletion); the MQTT broker configuration page; and the full-screen attendance QR overlay with a countdown. The page design focuses on responsiveness and readability from a distance, and sensitive pages return to the dashboard when touch is idle."),

  H2("7.3 The Mobile Web Panel with QR Entry"),
  P("The secure web server on the board (esp_https_server) serves the mobile panel pages directly; the user scans the display's QR code to reach https://IP/\u2026 and, after accepting the self-signed certificate, the panel opens. The three key pages are: the face-recognition page (/recognize), which opens the camera with getUserMedia, crops the central frame to 320\u00D7240 and sends it to the board as a JPEG; the token-protected enrollment page (/enroll); and the token-protected attendance page (/attendance) with a 10-minute token. Light and fan control and PIN unlocking are also possible from the panel. The main web-server endpoints are listed in Table 7-1."),
  ...TBL("Table 7-1: Main web-server endpoints on the board",
    ["Path", "Role"],
    [
      ["/", "Web dashboard panel (light, fan and lock control; sensor readouts)"],
      ["/recognize", "Mobile face-recognition page (entered via the display's QR code)"],
      ["/enroll?token=\u2026", "Enrollment page protected by a 3-minute token"],
      ["/attendance?token=\u2026", "Attendance page protected by a 10-minute token"],
      ["/password", "Settings login page (creates a session)"],
      ["/api/face/recognize", "POST of a JPEG image for face matching"],
      ["/api/face/enroll?token=&name=", "POST enrolling a new face sample"],
      ["/api/attendance", "POST recording entry/exit with an image and token"],
      ["/api/light/set | /api/fan/set", "POST light and fan commands"],
      ["/api/lock/unlock", "POST unlocking with a PIN (with a deliberate 1-second delay on error)"],
      ["/api/settings/unlock", "POST entering settings and issuing the session cookie"],
      ["/api/settings", "POST management operations (the enrollment link, etc.)"],
    ],
    [34, 66]),
  P("The settings session is kept as a random 16-byte token in the board's memory and delivered in a shs cookie with HttpOnly, Secure and SameSite=Strict flags and a 10-minute expiry; only one active session is permitted at any moment. Binding the basic control endpoints (light/fan/sensors) to HA was deliberately kept loose so that the home remains controllable even without the server \u2014 a conscious decision for resilience."),

  H2("7.4 The Home Assistant Dashboard and Mobile App"),
  P("Thanks to MQTT auto-discovery, all 13 entities are immediately available in Home Assistant and appear in both the web dashboard and the mobile app without any extra configuration. The user can control the light and fan from anywhere in the house, view access events and presence state, monitor the behavioral agent's statistics and probabilities, and switch the auto/shadow mode. This channel provides a complete management panel without a single additional line of code on the board \u2014 a clear demonstration of the value of a standards-based (MQTT) architecture."),

  H2("7.5 User Experience and UI/UX Lessons"),
  P("Several user-experience patterns recurred in this project: short-path entry via QR instead of typing addresses and passwords; countdowns and automatic token renewal to prevent expired QR codes; immediate visual feedback for every action (from the MQTT state change to the attendance confirmation); graceful degradation on failure (when the network is gone, the display still manages the lock and lights); and aligning the mobile web pages' language with Persian. This part was in effect a practical exercise in the principle that, in embedded systems, a good user interface affects the adoption of the system as much as the correctness of its logic does."),
];

const ch8 = [
  H1("Chapter 8: The Face-Based Attendance System", { pageBreakBefore: true }),

  H2("8.1 The Three-Tier Architecture"),
  P("On top of the same face-recognition engine, a complete attendance system was implemented that is deliberately independent of Home Assistant and MQTT, organized in three tiers (Figure 8-1): the board tier (QR generation, face matching, the durable queue), the local server tier (FastAPI in Docker with an SQLite database) and the optional delivery tier (the Google Sheets archive and Bale notifications over the Internet). Credential separation is enforced: the board holds no Google keys or Bale tokens, and communication with the server uses only a shared secret."),
  ...FIG("figs/fig4_attendance.png", "Figure 8-1: The three-tier architecture of the attendance system", 570),

  H2("8.2 The Attendance Recording Flow on the Board"),
  P("The user taps the Attend button on the display's dashboard; the display shows the QR code of the https://IP/attendance?token=\u2026 address with a 10-minute validity and renews it automatically with a countdown. The user scans the QR code with their phone, selects Entry or Exit on the Persian page and captures their face with the phone's camera; the board matches the face with the same chapter-6 pipeline, extracts the person's name from the face database and creates an attendance_in or attendance_out event with the device's timestamp. To prevent duplicate records, the same person recording the same event type within a 60-second window is suppressed. Door-unlock events \u2014 by face and by PIN \u2014 are also sent to the server for reporting."),

  H2("8.3 The Durable Queue and the Local Server"),
  P("Because losing a record is unacceptable, events are stored in a 32-slot queue in NVS before being sent, and an independent sender task delivers them to the server reliably; if the network or the server is unavailable, the records remain and are cleared in order once connectivity returns (attendance sending requires device timekeeping, and is rejected with an explicit error without NTP synchronization). The server \u2014 a FastAPI service in a Docker container with the /api/event, /health and /stats endpoints \u2014 records every event in an SQLite table. Acting as an outbox, this table carries partial indexes: a background worker picks up unsynchronized records every 15 seconds and pushes them to the integrations; even during an Internet outage the data stays intact in SQLite and the synchronization completes later."),

  H2("8.4 Notifications and Archiving"),
  P("The server has two optional integrations: the Bale messenger (with a Telegram-compatible API), which sends notifications for entry, exit, face-based unlocking and PIN unlocking; and a Google Sheets archive through an Apps Script webhook, to which each record is appended automatically. Dates in the sheet and in the Bale messages use the Jalali (Persian) calendar (an independent, dependency-free conversion in the jdate module), while timestamps in SQLite are kept in the Gregorian calendar. The face-similarity value is deliberately kept only in SQLite and is not transferred to the sheet, so that the management archive stays clean. Implementing this layer in full was a practical example of designing multi-component systems with a durable queue and synchronization in the face of unavailability."),
];

const ch9 = [
  H1("Chapter 9: Security and Privacy", { pageBreakBefore: true }),

  H2("9.1 Privacy of Face Data"),
  P("The most sensitive data in this system is the residents' face imagery, and the project's architecture ensures that this data never leaves the device's domain or the local network: the image is sent directly to the board itself, decoded and processed on that same chip, and the only outputs of the processing are a decision and a feature vector in the face database \u2014 no raw image, and no dependence on a cloud face-recognition service. Consequently, even under full network eavesdropping, no usable image content is available to an attacker. This characteristic \u2014 intelligence on a constrained device instead of cloud hardware and intelligence \u2014 is the project's central focus."),

  H2("9.2 Web-Layer Security"),
  P("All pages and endpoints are served over TLS with the board's self-signed certificate; this choice, beyond encryption, is the technical prerequisite for browser camera access. Sensitive flows carry two-stage authentication: the settings session (admin password \u2192 a secure 10-minute single-session cookie) and time-limited tokens for enrollment (3 minutes) and attendance (10 minutes), which are transmitted through QR codes and have a limited scope. PIN validation embeds a deliberate 1-second delay on wrong answers to neutralize rapid guessing. Passwords are stored in NVS with a minimum-length check."),

  H2("9.3 Door-Lock Security"),
  P("The door lock is protected by three hard rules. First, unlocking is possible only through verified paths (a face above threshold, the keypad PIN, the web-panel PIN and the physical button), and there is no lock command path from Home Assistant at all; the lock state is reported only as a binary sensor. Second, the learning agent is structurally barred from controlling the lock. Third, the lock relay is designed with an independent failsafe: even if the firmware dies, the lock automatically re-engages within 8 seconds at most."),

  H2("9.4 Current Security Boundaries and the Hardening Path"),
  P("In the current version, the Mosquitto broker runs without authentication for the simplicity of a home deployment, and this security boundary was chosen knowingly as the first step; fully hardening the broker (user/password authentication and TLS) and migrating the web authentication mechanism to the WebAuthn standard on top of the existing tokens sit at the top of the project's security roadmap. This separation of the lock command path from the other capabilities was designed from day one so that those upgrades are possible without touching the board, by changing only the infrastructure."),
];

module.exports = { ch7, ch8, ch9 };
