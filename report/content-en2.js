// Chapters 4-6 (English)
const { P, NItem, H1, H2, FIG, TBL } = require("./helpers_en");

const ch4 = [
  H1("Chapter 4: Hardware and Firmware Design", { pageBreakBefore: true }),

  H2("4.1 Hardware Architecture"),
  P("The hardware core of the system is an ESP32-S3-DevKitC-1 development board (revision 1.1) with the N16R8 module, connected to a 320\u00D7480 touch TFT display driven by an ST7796 controller (an 8-bit Intel-8080 parallel interface), a GT911 capacitive touch controller, a BME280 environmental sensor, a lock relay, a status light and a physical button. The display and touch are driven through Espressif's official drivers, and the sensors and touch share one I2C bus (SDA = GPIO1, SCL = GPIO2), which requires external 4.7-k\u03A9 pull-up resistors per the module specifications. The complete pin mapping is given in Table 4-1."),
  ...TBL("Table 4-1: Hardware pin mapping",
    ["Pin", "Direction", "Device / function"],
    [
      ["GPIO1 / GPIO2", "I2C bus", "SDA / SCL of the shared GT911 and BME280 bus"],
      ["GPIO4 \u2013 GPIO18 (bound)", "8-bit parallel output + control", "ST7796 display data bus and control signals"],
      ["GPIO12", "Output", "Display backlight control (with a driver circuit)"],
      ["GPIO13", "Output", "Light (LED)"],
      ["GPIO14", "Input", "Physical button"],
      ["GPIO21", "Output", "Door-lock relay (active-high, 8-second auto-relock)"],
      ["GPIO41", "Input", "Interrupt pin (INT) of the GT911 touch controller"],
      ["GPIO42", "Output", "Reset pin (RST) of the GT911 touch controller"],
      ["GPIO35\u201337", "\u2014", "Internally reserved by the octal PSRAM (unusable)"],
    ],
    [20, 26, 54]),
  P("Two important constraints were respected in the pin selection: avoiding the octal-PSRAM reserved pins, and keeping several pins free (GPIO39, GPIO40, GPIO47 and GPIO48) for future expansion such as driving a real electric lock."),

  H2("4.2 Memory Partitioning"),
  P("The partition table of the 16 MB flash is given in Table 4-2. The large app partition (8 MB) was chosen to accommodate the ESP-DL models, the web server, the graphical interface and the many modules simultaneously, and the SPIFFS partition stores the face-feature database."),
  ...TBL("Table 4-2: Flash partitioning",
    ["Partition", "Size", "Purpose"],
    [
      ["nvs", "24 KB", "Storing settings: passwords, Wi-Fi, the MQTT broker, the learning-model weights, the attendance queue"],
      ["phy", "4 KB", "Radio calibration data"],
      ["factory", "8 MB", "Main application (firmware)"],
      ["storage (SPIFFS)", "1 MB", "Face-feature database (face_db)"],
    ],
    [18, 18, 64]),
  P("Beyond the flash, the 8 MB of octal PSRAM serves as the large working area (the image buffer, the render buffers and the mbedTLS auxiliary memory), while the internal memory is reserved for stacks and latency-sensitive buffers."),

  H2("4.3 Modular Firmware Structure"),
  P("The firmware is developed with ESP-IDF version 6.0.2 in C and is organized as independent modules: Wi-Fi management (wifi_manager), MQTT management (mqtt_manager), a secure web server (http_server), the face engine (face_recognition), the face database (face_db), enrollment tokens (enroll_token), attendance (attendance), the learning agent (ml_agent), the graphical interfaces (display/*), lock management (lock), the sensor (bme280/sensor_task), time synchronization (time_sync) and virtual devices (virtual_devices). The software layering is shown in Figure 4-1."),
  ...FIG("figs/fig6_stack.png", "Figure 4-1: Software layers of the system", 570),
  P("The key design principle is the single source of state: the app_state module is the only authority on the state of the light, fan, lock and sensors, and every change from any channel (touch, web, MQTT, button or model) passes through this layer. As a result, the display, the web server and MQTT always present a consistent picture of the system, and the learning agent's training path is also fed from a single point."),

  H2("4.4 FreeRTOS Tasks and Synchronization"),
  P("The main firmware tasks and their configuration are given in Table 4-3."),
  ...TBL("Table 4-3: FreeRTOS tasks in the firmware",
    ["Task", "Stack", "Priority", "Period / pattern", "Role"],
    [
      ["LVGL", "8 KB", "2", "2 ms tick", "Rendering the UI and processing touch"],
      ["face_worker", "16 KB", "3", "Event-driven (depth-1 queue)", "Serialized face detection and matching"],
      ["sensor_task", "3 KB", "2", "Every 5 s", "Reading the BME280 and publishing state"],
      ["ml_agent", "4 KB", "3", "Every 30 s", "Inference and behavioral learning"],
      ["attendance", "8 KB", "2", "Event-driven", "Sending attendance events from the NVS queue"],
      ["wifi_retry", "\u2014", "2", "Every 20 s", "Network-aware automatic reconnection"],
      ["touch_wdt", "\u2014", "\u2014", "Supervisory", "Hardware reset of the GT911 on lockup"],
      ["httpd (TLS)", "\u2014", "\u2014", "Event-driven", "Serving HTTPS requests"],
    ],
    [16, 12, 12, 26, 34]),
  P("Synchronization is performed with minimal shared memory: input images are handed to face_worker through a depth-1 queue so that model execution is serialized and memory freeing is deterministic; access to the shared I2C bus is protected with a mutex; and device-state changes are made only through app_state."),

  H2("4.5 Reliability and Error Handling"),
  P("The reliability of the system rests on four mechanisms. First, the task watchdog (TWDT) is active with a 15-second threshold and panic mode enabled, and the heavy face_worker task feeds it periodically (at most every 5 seconds) so that a long neural-network execution does not cause repeated resets. Second, the touch watchdog monitors the health of the GT911 and, on an interrupt gap longer than 6 seconds or consecutive I2C errors, recovers the touch controller with a hardware reset sequence (RST and INT). Third, network-aware reconnection logic: the Wi-Fi manager tries the list of known networks in most-recently-used order with an attempt quota, and the MQTT manager \u2014 with a four-state machine and five consecutive attempts (every 5 seconds) before each connection \u2014 probes the broker address with a temporary client. Fourth, memory monitoring: every 30 seconds the heap state and the largest free internal block are logged so that leaks or corruption surface early."),

  H2("4.6 Settings and Persistent Data Storage"),
  P("All user settings are kept in NVS: the lock PIN and the settings password (with a minimum-length check), the Wi-Fi network list ordered by most recent use, the MQTT broker address (enterable as an IP or a domain name for DDNS compatibility), the attendance configuration, active tokens, and the versioned weight block of the learning model (about 90 bytes), which is saved every 30 seconds when changed and safely rolled back to the pre-trained weights on a version mismatch. The face-feature database lives in the SPIFFS partition, and its metadata (the identifier-to-name mapping and the sample counters) is managed in NVS."),
];

const ch5 = [
  H1("Chapter 5: Communication Architecture and Local Deployment", { pageBreakBefore: true }),

  H2("5.1 A Cloud-Free, Local-First Approach"),
  P("The communication architecture of the project is built on the everything-local principle: the MQTT broker (Mosquitto) and the Home Assistant platform run as Docker containers on a home server, and the board connects to them over the home Wi-Fi (Figure 5-1). This deployment needs nothing more than the home modem; during international Internet outages \u2014 and even a complete domestic outage \u2014 every feature, from face unlock to lighting control, the dashboard and attendance, keeps working without interruption. When the Internet is available, the optional external services (the Google Sheets archive and Bale notifications) are fed through the local server, but they are never a precondition for operation and no critical data depends on the outside. A second benefit of this architecture is very low latency: the complete face-decision loop stays inside the local network."),
  ...FIG("figs/fig1_architecture.png", "Figure 5-1: Overall system architecture and local deployment", 570),

  H2("5.2 Topic Structure and Message Format"),
  P("All system messages are exchanged under the smarthome/ prefix with QoS=1 and retained mode for state topics. Table 5-1 lists the main topics."),
  ...TBL("Table 5-1: Main MQTT topics",
    ["Topic", "Communication path", "Message format"],
    [
      ["smarthome/status", "board \u2192 all (LWT)", "online / offline (will message)"],
      ["smarthome/light/state | set", "board \u2194 HA and web", "ON / OFF"],
      ["smarthome/fan/state | set", "board \u2194 HA and web", "ON / OFF"],
      ["smarthome/lock/state", "board \u2192 all", "LOCKED / UNLOCKED"],
      ["smarthome/sensor/state", "board \u2192 HA", "JSON: temperature, humidity, pressure"],
      ["smarthome/access/state", "board \u2192 HA", "JSON: access-event type"],
      ["smarthome/presence/state", "board \u2192 HA", "HOME / AWAY"],
      ["smarthome/ml/mode | mode/set", "board \u2194 HA", "AUTO / SHADOW"],
      ["smarthome/ml/stats", "board \u2192 HA", "JSON: probabilities, accuracies and counters"],
    ],
    [28, 24, 48]),

  H2("5.3 Integration with Home Assistant"),
  P("On connection, the board publishes the MQTT Discovery configuration messages for 13 entities, and Home Assistant registers them automatically (Table 5-2). HA connects to the broker through the Docker service name rather than an IP address, making the deployment independent of network layout. An important security decision was to report the door lock as a binary state sensor instead of a lock entity, so that before the broker is hardened there is no lock command surface at the HA level."),
  ...TBL("Table 5-2: Entities discovered in Home Assistant",
    ["Group", "Entities"],
    [
      ["Actuators", "Light, fan, and the model auto-mode switch"],
      ["Environmental sensors", "Temperature, humidity, pressure, light intensity (lux)"],
      ["State and security", "Lock state (binary_sensor), presence (binary_sensor), access event (event)"],
      ["Behavioral learning", "Agent mode, agent statistics (with JSON attributes), two prediction-probability sensors for light and fan"],
    ],
    [22, 78]),

  H2("5.4 Connection-State Management"),
  P("The connection lifecycle is tied to the network state: when Wi-Fi comes up, the MQTT connection starts, and when Wi-Fi drops, the MQTT connection is actively torn down so that pointless retries and pending queues do not accumulate. With every abnormal disconnection, the will message (LWT) publishes offline on smarthome/status, and retained messages ensure that the dashboard and the app see the last correct state immediately after connecting. The broker is probed with a temporary client before being saved into the settings, so that an address error surfaces at the moment of entry."),
];

const ch6 = [
  H1("Chapter 6: Artificial Intelligence on the Edge", { pageBreakBefore: true }),

  H2("6.1 On-Device Face-Processing Architecture"),
  P("The intelligent heart of the project is the complete face-recognition pipeline that runs entirely on the ESP32-S3 (Figure 6-1). A JPEG image (up to 300 KB) is sent from the user's phone browser to the board over HTTPS; the esp_jpeg module decodes it with hardware acceleration into an RGB565 buffer of 320\u00D7240 (153.6 KB in PSRAM); the face-detection model (HumanFaceDetect from the ESP-DL framework) then yields the face coordinates, and the recognition model (HumanFaceRecognizer) extracts the image's feature vector. This vector is compared with the enrolled vectors in the face database (the SPIFFS partition), and if the similarity exceeds the 0.70 threshold the identity is confirmed and the lock command or the attendance event is issued."),
  ...FIG("figs/fig2_face_pipeline.png", "Figure 6-1: The face-recognition pipeline on the microcontroller", 570),
  P("Running this pipeline on a microcontroller dictated three important engineering decisions. First, a depth-1 input queue: concurrent requests are queued and model execution is serialized to prevent memory interference and instability. Second, placing the buffers in PSRAM: the decode buffer and the model's large variables live in PSRAM, leaving the internal memory free for stacks and mbedTLS. Third, non-blocking hand-off: the web server does not wait for processing to finish; the face_worker task applies the result independently, keeping the UI responsive."),

  H2("6.2 Face Enrollment and Sample Management"),
  P("The enrollment process was designed with the simultaneous goals of simplicity and security: the administrator creates an enrollment link from the settings page (protected by a password and a 10-minute session); the server generates a six-digit token valid for 3 minutes, shareable as an https://IP/enroll?token=\u2026 link. The user enters the person's name on the mobile web page and captures the face image (POST /api/face/enroll). The token is multi-use until it expires so that several samples of one person can be captured. A person's multiple samples are grouped under one name in the face_db layer (with a sample counter), and this multi-sample enrollment measurably improves matching under varying light and pose. The 0.70 acceptance threshold was kept after practical experiments with multi-sample enrollment, and further calibration was deliberately postponed to later versions."),

  H2("6.3 The User Behavioral-Learning Agent"),
  P("Alongside machine vision, the project includes a lightweight learning agent that learns the user's behavioral pattern for light and fan control. The model is two simple logistic regressions operating on a 12-dimensional context-only feature vector (Table 6-1): hourly characteristics as sinusoidal harmonics, the Thursday/Friday holiday state, resident presence, and the normalized light intensity, temperature and humidity. The own-current-state feature was deliberately removed: in the first version its presence left the model passive in auto mode despite 91% accuracy (inert behavior), and removing it \u2014 together with adding the third harmonic \u2014 raised accuracy from 88% to 94%."),
  ...TBL("Table 6-1: Input features of the behavioral learning agent",
    ["Feature group", "Count", "Description"],
    [
      ["Bias", "1", "The model's constant term"],
      ["Hourly harmonics", "6", "Three sin/cos harmonics covering the daily pattern"],
      ["Holidays", "2", "Thursday and Friday"],
      ["Resident presence", "1", "HOME after each successful unlock (at least 45 min)"],
      ["Environmental context", "2", "Normalized light intensity and temperature (humidity in the vector)"],
    ],
    [26, 12, 62]),
  P("Training is hybrid: offline pre-training on a 30-day synthetic dataset (8,640 samples in 5-minute steps, generated from the user's policy with an intentional 4\u20135% label noise), followed by online learning with stochastic gradient descent (learning rate 0.08 and weight clipping) on the chip itself. The parameters are given in Table 6-2."),
  ...TBL("Table 6-2: Training parameters and agent performance",
    ["Parameter", "Value"],
    [
      ["Pre-training dataset", "30 days, 8,640 samples (5-minute steps)"],
      ["Offline optimization", "Full-batch gradient, 1,500 iterations, with L2"],
      ["Validation", "The final 6 days of data"],
      ["Online learning", "SGD at rate 0.08, weight clipping (\u00B18)"],
      ["Weight storage", "NVS (a versioned ~90-byte block, every 30 s)"],
      ["Decision period", "Every 30 seconds"],
      ["Action thresholds", "p \u2265 0.75 on, p \u2264 0.25 off"],
      ["Final accuracy (validation)", "Light 94.2% \u2014 Fan 93.6% (Bayes cap \u2248 95%)"],
    ],
    [40, 60]),
  P("The agent operates in two phases (Figure 6-2): in the shadow phase the model predicts every 30 seconds, reports to MQTT and is evaluated, but takes no action; after at least 15 correct decisions in a 20-decision window it is automatically promoted to the auto phase. In the auto phase the model's own actions are not used for training, whereas every manual user action (from any channel) is executed immediately and recorded as a training sample. The definitive security rule is that the door lock is outside model control."),
  ...FIG("figs/fig5_ml_agent.png", "Figure 6-2: The shadow phase, promotion gate and online learning of the behavioral agent", 570),

  H2("6.4 Memory and Resource Considerations"),
  P("Running two vision models, a graphical interface and a TLS web server simultaneously on one chip turned resource management into a primary design problem: the ESP-DL models are pre-quantized to fit the limited flash and memory; the behavioral agent's weights total a few dozen bytes; the image buffer and the render buffers were moved to PSRAM; and periodic monitoring of the largest free internal block serves as an early memory-corruption alarm. The result is a system that delivers meaningful intelligence with tens of dollars of hardware and no companion processor."),
];

module.exports = { ch4, ch5, ch6 };
