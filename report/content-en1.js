// Chapters 1-3 (English)
const { P, B, NItem, H1, H2, FIG, TBL } = require("./helpers_en");

const ch1 = [
  H1("Chapter 1: Introduction", { pageBreakBefore: false }),

  H2("1.1 Problem Statement"),
  P("Internet-of-Things (IoT) smart-home systems have spread rapidly in recent years; nevertheless, most common commercial solutions rest on two pillars: numerous dedicated hardware devices inside the house, and cloud services for processing, authentication and data storage. Relying on the cloud raises three serious problems. First, the user's privacy is handed over entirely to an external entity: sensitive data such as residents' face images leaves the house and is kept on servers that are outside the user's control. Second, the operation of the system becomes dependent on the stability of the Internet connection; every outage means losing control of \u2014 and intelligence in \u2014 the home. Third, the network latency of the cloud round-trip is unacceptable for latency-critical decisions such as unlocking a door with face recognition."),
  P("On the other hand, the intelligent part of such systems \u2014 machine vision in particular \u2014 has traditionally required powerful processors and a dedicated camera, which drives up cost, installation complexity and power consumption. The central question of this project can therefore be formulated as follows: is it possible to build a complete, secure and intelligent smart home with minimal edge hardware, in such a way that the artificial-intelligence processing (face recognition) runs entirely on the microcontroller itself, the camera sensor and the rich user interface are borrowed from hardware the user already owns (a smartphone with its camera and browser), and no data ever leaves the home network?"),
  P("This project answers that question by designing and implementing a smart-home system based on the ESP32-S3 microcontroller, in which the user's face image is captured through the phone's browser (reached by scanning a QR code shown on the board's display), the neural-network inference runs locally on the microcontroller, and the entire supporting infrastructure (the MQTT message broker and the Home Assistant platform) is deployed locally on a home server using Docker. The result is a system that operates with nothing more than the home modem, shows no loss of functionality during international or even domestic Internet outages, and does not depend on any cloud service even when the Internet is available."),
  P("The overall block diagram of the system is shown in Figure 1-1."),
  ...FIG("figs/fig7_block_diagram.png", "Figure 1-1: Overall block diagram of the edge-processing smart-home system", 570),

  H2("1.2 Project Objectives"),
  P("The objectives of this project were defined as follows:"),
  NItem("1. Implementing an ESP32-S3-based IoT node with a color touch display, a secure on-board HTTPS web server and stable connectivity to the home network;"),
  NItem("2. Running the complete face-recognition pipeline (image decoding, face detection, feature-vector extraction and matching) on the microcontroller itself, without sending any image to an external service;"),
  NItem("3. Using the user's phone camera as the image input through a mobile web page and a QR-code mechanism, instead of adding a dedicated camera module;"),
  NItem("4. Deploying the local infrastructure \u2014 the MQTT broker (Mosquitto) and the Home Assistant platform \u2014 with Docker, and connecting the board to it through automatic service discovery;"),
  NItem("5. Designing a multi-channel user interface: the touch display, a mobile web panel entered via QR scan, the Home Assistant web dashboard and the Home Assistant mobile app, all driven by a single source of state;"),
  NItem("6. Developing a lightweight machine-learning agent that learns the user's behavioral pattern (light and fan control) by combining offline pre-training with on-chip online learning;"),
  NItem("7. Implementing a face-based attendance system with a local server, a durable message queue, a Google Sheets archive with Jalali (Persian) dates and Bale messenger notifications;"),
  NItem("8. Ensuring system reliability through runtime monitoring (watchdogs), network-aware reconnection logic and scenario-driven end-to-end tests."),

  H2("1.3 Innovations and Key Features"),
  P("Artificial intelligence on a constrained device: the most important axis of this project is moving intelligence to the edge. The entire face-recognition chain \u2014 from hardware JPEG decoding to feature extraction by a quantized neural network \u2014 runs on the microcontroller. This choice has two fundamental benefits: complete privacy (the image never leaves the board) and the near-total elimination of network latency in decision-making. At the same time, authentication reuses hardware the user already owns (the phone's camera and browser), keeping the edge hardware to a minimum."),
  P("Comprehensive user interaction: the system serves the user through four parallel channels \u2014 the on-board touch display (always available, even without a network), the mobile web panel entered via QR scan, the Home Assistant web dashboard and the Home Assistant mobile app. All channels are fed from a single source of state (the app_state module on the board) so that the state remains consistent everywhere."),
  P("Complete independence from the cloud: the MQTT broker and Home Assistant run as Docker containers on a home server. The system needs only the local network (the modem) and works fully during international and even domestic Internet outages; user data remains on the local network, which both guarantees privacy and minimizes network latency."),
  P("On-chip behavioral learning: the project's learning agent combines offline pre-training with online stochastic-gradient updates on the microcontroller itself, and operates in a shadow phase (predicting without acting) followed by an auto phase; the door lock is deliberately never under model control."),
  P("An attendance ecosystem: on top of the same face-recognition engine, a three-layer attendance system was implemented with a durable message queue on the board, a FastAPI server and an SQLite database, a Google Sheets archive with Jalali (Persian) dates, and Bale messenger notifications \u2014 independent of Home Assistant and without storing any cloud credentials on the board."),
  P("A comprehensive engineering exercise: this project was deliberately designed to cover the full spectrum of challenges of a real engineering effort \u2014 electronic design and wiring, hardware and memory constraints, real-time multi-core operating-system programming, networking and the MQTT and HTTPS/TLS protocols, security, interface and user-experience design, and software engineering (modular architecture, Git version control and feature-driven development on separate branches)."),

  H2("1.4 Scope and Assumptions"),
  P("The executable scope of the project is as follows: the edge hardware consists of an ESP32-S3-DevKitC-1 board with a 320\u00D7480 touch TFT display, a lock relay, a status light and a physical button, while the BME280 environmental sensor sits on the shared I2C bus. The system's image input is the user's phone camera; any fingerprint sensor or dedicated camera on the board is deferred to future work. The network infrastructure is the home LAN, and the services are deployed with Docker on a home computer/server; migration to a low-power always-on machine is on the development roadmap."),
  P("The main assumptions are: a home network with stable DHCP (with a reserved static address for the server in the practical deployment) is available; users reach the local network with smartphones carrying modern browsers (getUserMedia support); and the accuracy of the ESP-DL face engine with a 0.70 acceptance threshold is sufficient for a home environment. It is acknowledged that liveness detection is not implemented in the current version and is reported as a development path in the final chapter."),
];

const ch2 = [
  H1("Chapter 2: Fundamentals and Technologies", { pageBreakBefore: true }),

  H2("2.1 The Internet of Things and Edge Processing"),
  P("The Internet of Things (IoT) refers to a collection of physical devices that gather data from \u2014 or act upon \u2014 their physical environment through sensors and actuators, and exchange it over a network. In the traditional architecture, the intelligent part of such systems is moved to the cloud; however, a newer trend known as Edge AI, and its TinyML branch, runs machine-learning inference directly on low-power microcontrollers. The benefits of this approach include keeping sensitive data inside the device, removing the dependence on a continuous Internet connection, reducing decision latency, and lowering energy consumption and cost. This project is a complete practical instance of that architecture: both the vision inference (face recognition) and the online learning of the user's behavioral pattern run on the microcontroller itself."),

  H2("2.2 The ESP32-S3 Microcontroller"),
  P("The ESP32-S3 is an Espressif chip with a dual-core Xtensa LX7 running at up to 240 MHz, integrated 802.11b/g/n Wi-Fi, and a set of vector instructions that accelerate machine-learning computation. This project uses the ESP32-S3-DevKitC-1 development board with an ESP32-S3-WROOM-1-N16R8 module, which provides 16 MB of flash and 8 MB of octal SPI PSRAM clocked at 80 MHz. The generous PSRAM is the decisive factor in running the LVGL graphical interface, a TLS web server and the neural-network models on a single chip: for example, the decoded image buffer (153.6 KB) and the LVGL draw buffers (about 76 KB) live in PSRAM so that the 512 KB of internal memory remains free for stacks and sensitive network buffers. One important pinout constraint of this module is that GPIO35 through GPIO37 are unavailable (reserved for the octal PSRAM), which was accounted for in the circuit design."),

  H2("2.3 FreeRTOS and Multi-Core Programming"),
  P("The official ESP-IDF framework runs on top of FreeRTOS, a real-time operating system with a priority scheduler that executes on both cores of the ESP32-S3. Units of execution (tasks) are created with their own priority, stack and communication queues, and synchronization mechanisms such as queues, semaphores and mutexes are essential for sharing data between them. The main challenge in this project was the coexistence of tasks with very different characters: a latency-sensitive graphical interface, heavy neural-network processing, a TLS web server and lightweight network tasks. The project's solution was to separate tasks with shallow queues (for example, a depth-1 queue for images entering the face engine) instead of sharing memory directly, and to register the heavy tasks with the task watchdog. This exercise practiced concurrency, deadlock and resource-starvation concepts in the form of a real problem."),

  H2("2.4 The LVGL Graphics Library"),
  P("LVGL is an open-source graphics library for embedded systems that provides modern widgets, touch support and partial rendering. Version 9.5 was used in this project, integrated with the official espressif/esp_lcd driver for the ST7796 display (an 8-bit parallel Intel 8080 interface clocked at 20 MHz) and the GT911 touch driver. Partial rendering is performed with two 320\u00D760 buffers (about 76 KB), which were moved to PSRAM; this relocation is later (chapter 10) traced as the root cause of an out-of-memory error during the TLS handshake."),

  H2("2.5 The ESP-DL Framework and Lightweight Machine Learning"),
  P("ESP-DL is Espressif's official collection of components for running shallow, quantized neural networks on ESP chips. Two ready-made components from this framework were used in this project: HumanFaceDetect for face detection and HumanFaceRecognizer for extracting a face embedding. The models are pre-quantized so that they fit in the microcontroller's memory and compute budget. Alongside machine vision, the behavioral-learning part uses the simple yet effective logistic-regression model: with weights of only a few dozen bytes it can be stored on the chip and updated online by stochastic gradient descent (SGD) \u2014 a deliberate choice demonstrating the principle that intelligence does not always require heavy models."),

  H2("2.6 The MQTT Protocol"),
  P("MQTT is a lightweight messaging protocol built on the publish/subscribe pattern that operates through a central broker; thanks to its low overhead it is the de-facto standard in the IoT world. The key capabilities used in this project are: QoS levels (this system uses QoS=1 for state messages), retained messages, which keep the last state for newly subscribing clients, and the Last Will and Testament (LWT), which publishes an offline message on the state topic when the board's connection drops abnormally. Separating components through hierarchical topics makes it possible to add new subscribers (the dashboard, the app, the attendance service) without modifying the board."),

  H2("2.7 Home Assistant and Docker"),
  P("Home Assistant is the most popular open-source home-automation platform, runs self-hosted, and through the MQTT Discovery mechanism automatically registers devices that publish configuration messages as entities. Docker is the containerization technology that runs each service together with its dependencies in an isolated package. The combination of the two is the backbone of the project's local infrastructure: Mosquitto and Home Assistant both run as containers on the home server, require no cloud service whatsoever, and can be moved easily to any other computer."),

  H2("2.8 Secure Web (HTTPS/TLS)"),
  P("Because the web pages and the face-authentication endpoints run directly on the board, the system's web server is deployed with the esp_https_server library and a self-signed certificate on TLS. On the first connection the user accepts the self-signed-certificate warning, after which all traffic (including the face image) is transferred encrypted. Modern browsers permit camera access (getUserMedia) only in a secure context over HTTPS; TLS is therefore both a security requirement and a technical prerequisite for the image input. The mbedTLS memory tuning (a 16 KB input buffer for the image upload) and the network keep-alive configuration are practical lessons from this part, discussed further in chapter 10."),
];

const ch3 = [
  H1("Chapter 3: Requirements Analysis and System Specification", { pageBreakBefore: true }),

  H2("3.1 Functional Requirements"),
  P("The functional requirements of the system are given in Table 3-1."),
  ...TBL("Table 3-1: Functional requirements",
    ["Code", "Requirement", "Description"],
    [
      ["FR-01", "Sensor reading", "Periodic reading of temperature, humidity and pressure from the BME280 on the shared I2C bus"],
      ["FR-02", "Publishing state to MQTT", "Publishing light, fan, lock and sensor state with QoS=1 and retained messages"],
      ["FR-03", "Auto-discovery in Home Assistant", "Publishing Discovery configuration messages for 13 entities"],
      ["FR-04", "Multi-channel control", "Controlling light, fan and lock from the touch display, web panel, HA dashboard and HA app"],
      ["FR-05", "Face unlock", "Receiving an image from the mobile browser, face matching on the board with a 0.70 threshold, and relay activation"],
      ["FR-06", "PIN unlock", "A numeric keypad on the touch display with visual feedback"],
      ["FR-07", "Face enrollment", "Adding a new face through a six-digit, 3-minute token and a mobile web page; multi-sample grouping"],
      ["FR-08", "Attendance", "Entry/exit recording with a face via the QR code on the display; a durable queue and delivery to the local server"],
      ["FR-09", "Behavioral learning", "Predicting light and fan state with the on-chip model in the shadow and auto phases"],
      ["FR-10", "Settings management", "Changing passwords, Wi-Fi and MQTT broker settings from the display and the web pages"],
    ],
    [12, 26, 62]),

  H2("3.2 Non-Functional Requirements"),
  P("The non-functional requirements of the system are given in Table 3-2."),
  ...TBL("Table 3-2: Non-functional requirements",
    ["Code", "Requirement", "Description"],
    [
      ["NFR-01", "Privacy", "The face image is processed and freed only on the board; no data is sent to any cloud service"],
      ["NFR-02", "Internet independence", "Full operation on the local network alone; compatible with international and domestic Internet outages"],
      ["NFR-03", "Low latency", "The face-based lock decision loop stays inside the local network and never crosses the cloud"],
      ["NFR-04", "Reliability", "Automatic recovery from Wi-Fi/MQTT outages, recovery from touch I2C lockups, and 15-second watchdog supervision"],
      ["NFR-05", "Transport security", "A TLS web server with a self-signed certificate, an HttpOnly/SameSite=Strict session cookie and time-limited tokens"],
      ["NFR-06", "Extensibility", "A modular architecture with a single source of state; new sensors/actuators without redesign"],
      ["NFR-07", "Debuggability", "Periodic heap reporting every 30 seconds and structured logging for root-cause analysis"],
    ],
    [12, 26, 62]),

  H2("3.3 System Inputs and Outputs"),
  P("The system inputs are: display touch events (the GT911 controller on I2C), the face image received from the user's phone browser (a JPEG of up to 300 KB), numeric PINs entered on the on-screen keypad, the physical button on the board, and the BME280 environmental sensor data (temperature, humidity, pressure)."),
  P("The system outputs are: the graphical interface on the 320\u00D7480 display (dashboard, keypad, settings pages and QR codes), the door-lock relay command (active-high with a safe 8-second auto-relock), the light state, and the stream of MQTT messages to the local infrastructure, which feeds the Home Assistant dashboard, the mobile app and the attendance service."),

  H2("3.4 Operational Scenarios"),
  P("The main use-case scenarios of the system are as follows:"),
  NItem("1. Door unlock with a face: the user scans the QR code on the display with their phone, the phone's web page captures the image and sends it to the board; the face engine on the board makes the decision and, on a match, energizes the lock relay for a safe interval."),
  NItem("2. Door unlock with a PIN: when no phone is available, the numeric keypad on the touch display accepts the lock PIN."),
  NItem("3. New face enrollment: the administrator issues a six-digit token from the settings page, enters the person's name on the mobile web page and captures several face samples; the samples are grouped under one name."),
  NItem("4. Attendance: the user presses the Attend button on the display, scans the 10-minute QR code, selects entry or exit and captures their face; the event goes to the local server and is reflected in the sheet and in Bale."),
  NItem("5. Home control from all four channels: light and fan from the display, the mobile web panel, the HA dashboard or the HA app; every change is reflected live in the other channels."),
  NItem("6. Behavioral learning: in the shadow phase the model reports and evaluates its predictions; after passing the promotion gate it enters the auto phase and adjusts the light and fan according to the behavioral pattern; every manual user action is executed immediately and recorded as a training sample."),
];

module.exports = { ch1, ch2, ch3 };
