// Chapters 10-12 + references (English)
const { P, NItem, H1, H2, FIG, FIGGRID, TBL, t } = require("./helpers_en");
const { Paragraph, TextRun, AlignmentType, HeadingLevel } = require("docx");

const ch10 = [
  H1("Chapter 10: Tests, Results and Evaluation", { pageBreakBefore: true }),

  H2("10.1 Test Method"),
  P("The system was evaluated at three levels: unit testing of components (bringing up each driver and module in isolation and inspecting logs), integration testing (message exchange between the board, the MQTT broker and Home Assistant using MQTT client tools), and scenario-driven end-to-end testing (running complete application cycles as a real user). Development followed a feature-driven flow on separate Git branches, with each feature merged only after on-board testing; every capability was therefore measured under real conditions before entering the main line."),

  H2("10.2 Test Scenarios"),
  P("The main test scenarios and their results are given in Table 10-1."),
  ...TBL("Table 10-1: Test scenarios and results",
    ["Scenario", "Condition / input", "Expectation", "Result"],
    [
      ["Face unlock", "Enrolled face, different light and pose", "Lock opens and an access event is raised", "Pass"],
      ["Rejecting an unknown face", "Unenrolled person", "No unlock; retry possible", "Pass"],
      ["PIN unlock", "Correct / wrong PIN", "Unlock / error with a 1-second delay", "Pass"],
      ["Multi-sample enrollment", "Valid token, 3 samples", "Grouping under one name and improved matching", "Pass"],
      ["Token expiry", "QR after 3 / 10 minutes", "Request rejected and a new token issued", "Pass"],
      ["Attendance entry/exit", "Enrolled face + token", "Recorded on the server, the sheet and a Bale notification", "Pass"],
      ["Attendance de-duplication", "Two records < 60 s apart", "The second event suppressed", "Pass"],
      ["Durable queue", "Server outage and recovery", "Records retained and sent after reconnection", "Pass"],
      ["Wi-Fi reconnection", "Radio toggled", "Most recent network selected and joined", "Pass"],
      ["MQTT reconnection", "Broker reset", "5 consecutive attempts and subscriptions restored", "Pass"],
      ["Behavior during an Internet outage", "Internet fully cut", "All local features keep working", "Pass"],
      ["Behavioral-agent promotion", "20-decision window", "Threshold crossed, shadow \u2192 auto transition", "Pass"],
      ["Long-term stability", "Continuous operation", "No unintended resets (TWDT supervision)", "Pass"],
      ["Touch recovery", "I2C lockup of the GT911", "Hardware reset and continued operation", "Pass"],
    ],
    [24, 26, 32, 18], { cellSize: 22 }),

  H2("10.3 Behavioral-Agent Results"),
  P("The final model reached 94.2% accuracy for the light and 93.6% for the fan on the validation data of the final six days; the theoretical (Bayes) cap of the problem, given the intentional 4\u20135% label noise, is about 95%, so the model has effectively reached the problem's boundary. Three reference behavioral scenarios were also re-tested on the deployed firmware: night + presence + darkness (p \u2248 0.99 \u2192 light on), noon with the light on (p \u2264 0.05 \u2192 light off), and noon with the light off (p \u2248 0.07 \u2192 no action). The automatic promotion from the shadow phase to auto \u2014 after at least 15 correct decisions in the 20-decision window \u2014 was observed in the practical test."),

  H2("10.4 Engineering Challenges Resolved"),
  P("Along the way, several real problems were root-caused and fixed, each carrying its own engineering lesson:"),
  NItem("1. Out of memory during the TLS handshake after adding web graphics: root-cause analysis showed the LVGL render buffers occupying internal RAM; moving the buffers to PSRAM together with network keep-alive tuning (idle 30 / interval 5 / count 3), enabling lru_purge and 16 lwIP sockets resolved the problem permanently."),
  NItem("2. An I2C bus lockup of the GT911 touch controller: a mutex leak on the error path led to deadlock; fixing the lock-release path and adding a touch watchdog with a hardware reset sequence (RST and INT) made the system self-healing against this failure."),
  NItem("3. Watchdog panic loops in the face task: a long blocking wait for new work starved the sensing task; periodic TWDT feeding (at most every 5 seconds) in the wait loop and a 20-second timeout for receiving the image body guaranteed stability."),
  NItem("4. A passive behavioral model despite high accuracy: using the own-device-state feature caused inertia and inaction in auto mode; redesigning the feature vector to be context-only solved the problem at its root (chapter 6)."),

  H2("10.5 Overall Evaluation"),
  P("The overall evaluation shows that the system met its design goals: the complete face-unlock loop runs inside the local network and on the microcontroller; the local MQTT and Home Assistant infrastructure works without interruption through Internet outages; the four user channels present a consistent picture of the state; the attendance system with its durable queue and Jalali calendar has operated in a real environment; and the firmware has remained stable in long-running operation without unintended resets. The reliability mechanisms put in place (watchdogs, network-aware reconnection and memory monitoring) provided valuable practical experience in engineering scalable embedded systems."),
];

const ch11real = [
  H1("Chapter 11: Running on Real Hardware", { pageBreakBefore: true }),

  H2("11.1 The Deployment Environment and Real Equipment"),
  P("The final validation of any embedded system is its deployment and operation in a real environment. This chapter presents photographic documentation of the system as deployed: from the ESP32-S3 board and its touch display on the workbench to the Home Assistant companion app on a phone, the Bale bot, the Google Sheets archive and the running Docker containers. Full recordings of the live tests are available in the project repository."),
  P("Figure 11-1 shows the final state of the workbench: the ESP32-S3-DevKitC-1 board (top) is connected to the 320\u00D7480 display through FPC adapters, the input/output circuitry (the lock relay, status LEDs and the button) is wired on a breadboard, and next to it the Home Assistant companion app on a phone mirrors the live state of the system."),
  ...FIG("../presentation/Images/Board Image.jpg", "Figure 11-1: The deployed system on the workbench \u2014 board, display and companion app", 380),

  H2("11.2 The Real Interface on the Display"),
  P("The nine LVGL interface pages (the main dashboard, door unlock, Wi-Fi scanning and joining, PIN entry, MQTT configuration, settings, the faces list and the two QR pages for attendance and web access) were captured directly from the board's real display and are shown in Figure 11-2. These captures demonstrate that the entire setup cycle \u2014 from joining the network and configuring the broker to managing faces and passwords \u2014 can be completed without a computer, using only the touch display."),
  ...FIGGRID("Figure 11-2: The nine LVGL interface pages \u2014 screenshots from the board's real display", [
    "../presentation/Images/LCD UI/Main Screen.png",
    "../presentation/Images/LCD UI/Unlock Screen.png",
    "../presentation/Images/LCD UI/Wifi List.png",
    "../presentation/Images/LCD UI/Wifi Enter Password.png",
    "../presentation/Images/LCD UI/Mqtt Setting.png",
    "../presentation/Images/LCD UI/Setting Screen.png",
    "../presentation/Images/LCD UI/Faces List.png",
    "../presentation/Images/LCD UI/Attend QR Code.png",
    "../presentation/Images/LCD UI/Web QR Code.png",
  ]),

  H2("11.3 The User Channels in Practice"),
  P("Figure 11-3 shows the system's web dashboard in its mobile view: the light and fan switches, door lock with PIN and face unlock, live environment state and the learning agent's mode (SHADOW/AUTO). The dashboard is served by the local server and is reached by scanning the QR code shown on the board."),
  ...FIG("../presentation/Images/Web Dashboard.png", "Figure 11-3: The system's web dashboard in its mobile view", 320),
  P("Figure 11-4 shows the Home Assistant companion app's dashboard on a real phone: environment cards, device controls, access state and the four real automations defined in automations.yaml \u2014 all states synchronized bidirectionally over MQTT."),
  ...FIG("../presentation/Images/Smart Home \u2013 Home Assistant_Dashboard.png", "Figure 11-4: The Home Assistant companion-app dashboard on a phone", 320),
  P("Figure 11-5 shows the Bale bot in operation: the Persian command menu, light and fan control through the reply keyboard, door opening with a 2-minute one-time password, and instant notifications of events (the door opening, residents entering and leaving)."),
  ...FIG("../presentation/Images/Bale_1.jpg", "Figure 11-5: The Bale bot \u2014 device control and event notifications", 320),

  H2("11.4 Service Deployment and Archiving"),
  P("Figure 11-6 shows Docker Desktop on the home server; four lightweight containers (the attendance service, Mosquitto, Home Assistant and the Bale bot) with a combined memory footprint of a few hundred megabytes form the platform's entire infrastructure."),
  ...FIG("../presentation/Images/Docker.png", "Figure 11-6: The system's running containers in Docker Desktop", 600),
  P("Figure 11-7 shows the attendance archive in Google Sheets; each record is stored with a Jalali date, the exact time, the person's name and the entry/exit type, while a Bale notification is dispatched at the same moment. This archive is the optional delivery tier: during an Internet outage the records persist in the durable on-board queue and are sent in order once connectivity returns."),
  ...FIG("../presentation/Images/Google Sheet.png", "Figure 11-7: The attendance archive in Google Sheets with Jalali dates", 470),
];

const ch12 = [
  H1("Chapter 12: Summary and Future Work", { pageBreakBefore: true }),

  H2("12.1 Summary"),
  P("In this project a complete IoT-based smart-home system was designed and implemented whose distinguishing feature is bringing artificial intelligence to a constrained edge device and reusing the user's existing hardware for authentication: face recognition runs entirely on the microcontroller, the camera and the rich interface are borrowed from the user's smartphone, and the infrastructure (MQTT and Home Assistant) is deployed fully locally with Docker. The completed components are: the ESP32-S3 node with its touch display and secure web server; the on-chip face pipeline with multi-sample enrollment; auto-discovery integration with 13 HA entities; the behavioral learning agent with offline pre-training and online learning (94.2% and 93.6% accuracy); the three-tier attendance system with a durable queue, a Jalali sheet and Bale notifications; and the reliability mechanisms (watchdogs and network-aware reconnection)."),
  P("From a learning perspective, the project was deliberately designed as a practical rehearsal of most specialized undergraduate courses: logic design and electronics (buses, relay, pull-up resistors), computer architecture and hardware constraints (memory, PSRAM, partitioning), operating systems and multi-core programming (FreeRTOS, concurrency, watchdogs), networking and protocols (MQTT, HTTPS/TLS, DNS and DHCP), databases and software engineering (modular architecture, a durable queue, Git and branch-based development), artificial intelligence (model quantization, TinyML, online learning), and interface and user-experience design. The final product, beyond the system itself, is a stable laboratory platform for developing and testing future ideas \u2014 from image recognition and voice commands to behavioral analysis of sensor data."),

  H2("12.2 Future Work"),
  P("The following development paths are proposed for the project:"),
  NItem("1. Federated learning on microcontrollers: applying federated learning to train behavioral models jointly across several boards in a home, without raw data leaving the devices \u2014 the natural continuation of this project's privacy-preserving intelligence-at-the-edge approach."),
  NItem("2. Voice intelligence: adding on-chip voice-command recognition with the ESP-SR framework as a fifth interaction channel."),
  NItem("3. Sensor behavioral analytics: extending the learning agent to real presence (PIR HC-SR501) and light-intensity (BH1750) sensors and a real two-channel relay fan."),
  NItem("4. Fingerprint as a second authentication factor: adding a fingerprint sensor for phone-free entry and two-factor authentication."),
  NItem("5. Hardening the MQTT broker (authentication and TLS) and upgrading web authentication to the WebAuthn standard on top of the existing tokens."),
  NItem("6. Liveness detection to harden face recognition against photographs and video replay."),
  NItem("7. Wireless firmware updates (OTA) for easier maintenance."),
  NItem("8. An LVGL simulator on the computer (WSL2 with SDL2) for a faster UI development cycle."),
  NItem("9. Improving the Home Assistant dashboard (charts, automations and face management from HA) and a model-retrain button."),
  NItem("10. Final deployment on permanent hardware: a real electric lock, an enclosure with clean wiring, I2C pull-up resistors, and moving the services to a low-power always-on computer."),
  NItem("11. Complementary interaction options: BLE/iBeacon and NFC for close-range entry, and a dedicated mobile app."),
  NItem("12. Designing a custom printed circuit board (PCB) for the product version of the system."),
];

const refs = [
  new Paragraph({
    alignment: AlignmentType.CENTER,
    heading: HeadingLevel.HEADING_1,
    pageBreakBefore: true,
    spacing: { before: 240, after: 300, line: Math.ceil(18 * 23), lineRule: "atLeast" },
    children: [t("References", { size: 36, bold: true })],
  }),
  ...[
    "[1] Espressif Systems, \u201CESP-IDF Programming Guide (v6.x).\u201D [Online]. Available: https://docs.espressif.com/projects/esp-idf/",
    "[2] Espressif Systems, \u201CESP-DL: Deep Learning Library for ESP Series.\u201D [Online]. Available: https://github.com/espressif/esp-dl",
    "[3] Espressif Systems, \u201Chuman_face_detect / human_face_recognition Components Registry.\u201D [Online]. Available: https://components.espressif.com/",
    "[4] FreeRTOS Team, \u201CThe FreeRTOS Reference Manual.\u201D [Online]. Available: https://www.freertos.org/",
    "[5] LVGL, \u201CLight and Versatile Graphics Library, v9.5 Documentation.\u201D [Online]. Available: https://docs.lvgl.io/",
    "[6] OASIS Standard, \u201CMQTT Version 3.1.1 Specification.\u201D [Online]. Available: https://docs.oasis-open.org/mqtt/mqtt/v3.1.1/",
    "[7] Eclipse Mosquitto, \u201CMosquitto Documentation.\u201D [Online]. Available: https://mosquitto.org/documentation/",
    "[8] Home Assistant, \u201CMQTT Discovery Documentation.\u201D [Online]. Available: https://www.home-assistant.io/integrations/mqtt/",
    "[9] Docker Inc., \u201CDocker Documentation.\u201D [Online]. Available: https://docs.docker.com/",
    "[10] E. Rescorla and T. Dierks, \u201CThe Transport Layer Security (TLS) Protocol Version 1.2,\u201D RFC 5246, 2008.",
    "[11] FastAPI, \u201CFastAPI Documentation.\u201D [Online]. Available: https://fastapi.tiangolo.com/",
    "[12] SQLite Consortium, \u201CSQLite Documentation.\u201D [Online]. Available: https://www.sqlite.org/docs.html",
    "[13] P. Warden and D. Situnayake, \u201CTinyML: Machine Learning with TensorFlow Lite on Arduino and Ultra-Low-Power Microcontrollers,\u201D O'Reilly Media, 2020.",
    "[14] B. McMahan et al., \u201CCommunication-Efficient Learning of Deep Networks from Decentralized Data (Federated Learning),\u201D in Proc. AISTATS, 2017.",
    "[15] W3C / FIDO Alliance, \u201CWeb Authentication (WebAuthn): An API for accessing Public Key Credentials.\u201D [Online]. Available: https://www.w3.org/TR/webauthn/",
    "[16] Bosch Sensortec, \u201CBME280 Combined Humidity and Pressure Sensor Datasheet.\u201D [Online]. Available: https://www.bosch-sensortec.com/",
    "[17] Goodix, \u201CGT911 Capacitive Touch Panel Controller Datasheet.\u201D [Online]. Available: https://www.goodix.com/",
    "[18] Sitronix Technology, \u201CST7796 320x480 TFT Controller Datasheet.\u201D [Online]. Available: https://www.sitronix.com.tw/",
  ].map(txt => new Paragraph({
    alignment: AlignmentType.LEFT,
    indent: { left: 420, hanging: 420 },
    spacing: { line: 312, after: 60 },
    children: [t(txt, { size: 21 })],
  })),
];

function ENTitleLine(text, size, bold = false, before = 0, after = 200) {
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before, after, line: Math.ceil((size / 2) * 23), lineRule: "atLeast" },
    children: [t(text, { size, bold })],
  });
}

const titleEN = [
  ENTitleLine("Department of Electrical and Computer Engineering", 28, false, 2200, 500),
  ENTitleLine("Design and Implementation of a Smart Home IoT System", 32, true, 800, 100),
  ENTitleLine("with On-Device Edge AI Face Authentication", 32, true, 0, 100),
  ENTitleLine("and a Fully Local, Cloud-Free Infrastructure", 32, true, 0, 700),
  ENTitleLine("Undergraduate Specialized Project", 26, false, 600, 500),
  ENTitleLine("Supervisor:", 24, true, 800, 60),
  ENTitleLine("Dr. Amir Khorsandi", 26, false, 0, 400),
  ENTitleLine("Student:", 24, true, 300, 60),
  ENTitleLine("Mohammad Soroush Rabiei", 26, false, 0, 900),
  ENTitleLine("September 2026", 26, false, 500, 0),
];

module.exports = { ch10, ch11real, ch12, refs, titleEN };
