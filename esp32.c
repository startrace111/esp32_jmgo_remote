#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <WebServer.h>
#include <Preferences.h>

// config mode
#define BUTTON_PIN 9   // BOOT 按键
#define LED1 12
#define LED2 13

Preferences prefs;
WebServer server(80);
bool configFinished = false;

// 配置项
String ssid = "xiaomiiot";  // 默认WiFi名称
String password = "d123456lw";  // 默认WiFi密码
String mqtt_server = "192.168.31.204";  // 默认MQTT服务器
String mqtt_user = "mosquitto";  // 默认MQTT用户名
String mqtt_password = "d123456lw";  // 默认MQTT密码
String mqtt_topic = "homeassistant/switch/tv_switch/state";  // 默认MQTT主题
String mqtt_command_topic = "homeassistant/switch/tv_switch/set";  // 默认MQTT命令主题
String target_ip = "192.168.31.47";  // 默认TCP目标IP
int target_port = 9005;  // 默认目标端口

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
bool currentState = false; // 开关当前状态
BLEAdvertising* pAdvertising = nullptr;
BLEAdvertisementData advData;
bool bleInitialized = false;
bool bleBroadcasting = false;

// 结构体：按键命令（包含按下与释放）
typedef struct {
    const char* name;
    const uint8_t* press;
    const uint8_t* release;
    size_t press_len;
    size_t release_len;
} Command;

// 所有按键数据（每个都用 const uint8_t 定义）
const uint8_t power_press[]   = {0x09,0x12,0x07,0x0a,0x05,0x08,0xc3,0x05,0x10,0x01};
const uint8_t power_release[] = {0x09,0x12,0x07,0x0a,0x05,0x08,0xc3,0x05,0x10,0x00};

const uint8_t mongo_press[]   = {0x09,0x12,0x07,0x0a,0x05,0x08,0xc2,0x05,0x10,0x01};
const uint8_t mongo_release[] = {0x09,0x12,0x07,0x0a,0x05,0x08,0xc2,0x05,0x10,0x00};

const uint8_t return_press[]   = {0x08,0x12,0x06,0x0a,0x04,0x08,0x04,0x10,0x01};
const uint8_t return_release[] = {0x08,0x12,0x06,0x0a,0x04,0x08,0x04,0x10,0x00};

const uint8_t setting_press[]   = {0x09,0x12,0x07,0x0a,0x05,0x08,0xdd,0x04,0x10,0x01};
const uint8_t setting_release[] = {0x09,0x12,0x07,0x0a,0x05,0x08,0xdd,0x04,0x10,0x00};

const uint8_t ok_press[]   = {0x08,0x12,0x06,0x0a,0x04,0x08,0x17,0x10,0x01};
const uint8_t ok_release[] = {0x08,0x12,0x06,0x0a,0x04,0x08,0x17,0x10,0x00};

const uint8_t up_press[]   = {0x08,0x12,0x06,0x0a,0x04,0x08,0x13,0x10,0x01};
const uint8_t up_release[] = {0x08,0x12,0x06,0x0a,0x04,0x08,0x13,0x10,0x00};

const uint8_t down_press[]   = {0x08,0x12,0x06,0x0a,0x04,0x08,0x14,0x10,0x01};
const uint8_t down_release[] = {0x08,0x12,0x06,0x0a,0x04,0x08,0x14,0x10,0x00};

const uint8_t left_press[]   = {0x08,0x12,0x06,0x0a,0x04,0x08,0x15,0x10,0x01};
const uint8_t left_release[] = {0x08,0x12,0x06,0x0a,0x04,0x08,0x15,0x10,0x00};

const uint8_t right_press[]   = {0x08,0x12,0x06,0x0a,0x04,0x08,0x16,0x10,0x01};
const uint8_t right_release[] = {0x08,0x12,0x06,0x0a,0x04,0x08,0x16,0x10,0x00};

const uint8_t option_press[]   = {0x08,0x12,0x06,0x0a,0x04,0x08,0x52,0x10,0x01};
const uint8_t option_release[] = {0x08,0x12,0x06,0x0a,0x04,0x08,0x52,0x10,0x00};

// volume
// min
const uint8_t volume_min_1[] = {0x31, 0x12, 0x2f, 0x22, 0x2d, 0x0a, 0x0a, 0x72, 0x65};
const uint8_t volume_min_2[] = {0x71, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x66, 0x6f, 0x12};
const uint8_t volume_min_3[] = {0x1f, 0x7b, 0x22, 0x72, 0x65, 0x71, 0x22, 0x3a, 0x22, 0x73};
const uint8_t volume_min_4[] = {0x65, 0x74, 0x56, 0x6f, 0x6c, 0x75, 0x6d, 0x65};
const uint8_t volume_min_5[] = {0x22, 0x2c, 0x22, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x22, 0x3a, 0x22};
const uint8_t volume_min_7[] = {0x22, 0x7d};

// mid
const uint8_t volume_mid_1[] = {0x32, 0x12, 0x30, 0x22, 0x2e, 0x0a, 0x0a, 0x72, 0x65, 0x71};
const uint8_t volume_mid_2[] = {0x65, 0x73, 0x74, 0x69, 0x6e, 0x66, 0x6f, 0x12, 0x20, 0x7b, 0x22};
const uint8_t volume_mid_3[] = {0x72, 0x65, 0x71, 0x22, 0x3a, 0x22, 0x73, 0x65, 0x74, 0x56};
const uint8_t volume_mid_4[] = {0x6f, 0x6c, 0x75, 0x6d, 0x65, 0x22, 0x2c, 0x22, 0x70, 0x61, 0x72};
const uint8_t volume_mid_5[] = {0x61, 0x6d, 0x22, 0x3a, 0x22};
const uint8_t volume_mid_7[] = {0x22, 0x7d};

// max
const uint8_t volume_max_full[] = {
  0x33, 0x12, 0x31, 0x22, 0x2f, 0x0a, 0x0a, 0x72, 0x65, 0x71, 0x65, 0x73, 0x74,
  0x69, 0x6e, 0x66, 0x6f, 0x12, 0x21, 0x7b, 0x22, 0x72, 0x65, 0x71, 0x22, 0x3a,
  0x22, 0x73, 0x65, 0x74, 0x56, 0x6f, 0x6c, 0x75, 0x6d, 0x65, 0x22, 0x2c, 0x22,
  0x70, 0x61, 0x72, 0x61, 0x6d, 0x22, 0x3a, 0x22, 0x31, 0x30, 0x30, 0x22, 0x7d
};

// 指令数组
Command commands[] = {
    {"power",   power_press,   power_release,   sizeof(power_press),   sizeof(power_release)},
    {"mongo",   mongo_press,   mongo_release,   sizeof(mongo_press),   sizeof(mongo_release)},
    {"return",  return_press,  return_release,  sizeof(return_press),  sizeof(return_release)},
    {"setting", setting_press, setting_release, sizeof(setting_press), sizeof(setting_release)},
    {"ok",      ok_press,      ok_release,      sizeof(ok_press),      sizeof(ok_release)},
    {"up",      up_press,      up_release,      sizeof(up_press),      sizeof(up_release)},
    {"down",    down_press,    down_release,    sizeof(down_press),    sizeof(down_release)},
    {"left",    left_press,    left_release,    sizeof(left_press),    sizeof(left_release)},
    {"right",   right_press,   right_release,   sizeof(right_press),   sizeof(right_release)},
    {"option",  option_press,  option_release,  sizeof(option_press),  sizeof(option_release)},
};

// 读取配置
void loadConfig() {
  prefs.begin("config", true);
  ssid = prefs.getString("ssid", "xiaomiiot");
  password = prefs.getString("password", "d123456lw");
  mqtt_server = prefs.getString("mqtt_server", "192.168.31.204");
  mqtt_user = prefs.getString("mqtt_user", "mosquitto");
  mqtt_password = prefs.getString("mqtt_password", "d123456lw");
  mqtt_topic = prefs.getString("mqtt_topic", "homeassistant/switch/tv_switch/state");
  mqtt_command_topic = prefs.getString("mqtt_command_topic", "homeassistant/switch/tv_switch/set");
  target_ip = prefs.getString("target_ip", "192.168.31.47");
  target_port = prefs.getInt("target_port", 9005);
  prefs.end();
}

// 保存配置
void saveConfig() {
  prefs.begin("config", false);
  prefs.putString("ssid", ssid);
  prefs.putString("password", password);
  prefs.putString("mqtt_server", mqtt_server);
  prefs.putString("mqtt_user", mqtt_user);
  prefs.putString("mqtt_password", mqtt_password);
  prefs.putString("mqtt_topic", mqtt_topic);
  prefs.putString("mqtt_command_topic", mqtt_command_topic);
  prefs.putString("target_ip", target_ip);
  prefs.putInt("target_port", target_port);
  prefs.end();
}

// 提供网页表单
void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang='zh'>
  <head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1.0'>
    <title>ESP32 配置页面</title>
    <style>
      body { font-family: 'Arial'; padding: 2em; background: #f5f5f5; }
      h2 { color: #333; }
      input[type=text], input[type=password] {
        width: 100%; padding: 10px; margin: 8px 0; box-sizing: border-box;
      }
      button {
        background-color: #4CAF50; color: white; padding: 10px 20px;
        border: none; border-radius: 4px; cursor: pointer;
      }
      button:hover { background-color: #45a049; }
    </style>
  </head>
  <body>
    <h2>ESP32 配置页面</h2>
    <form action='/save' method='POST'>
      <label>WiFi SSID:</label>
      <input type='text' name='ssid' value='%SSID%'><br>
      <label>WiFi 密码:</label>
      <input type='text' name='password' value='%PASSWORD%'><br>

      <label>MQTT 服务器:</label>
      <input type='text' name='mqtt_server' value='%MQTT_SERVER%'><br>
      <label>MQTT 用户名:</label>
      <input type='text' name='mqtt_user' value='%MQTT_USER%'><br>
      <label>MQTT 密码:</label>
      <input type='text' name='mqtt_password' value='%MQTT_PASSWORD%'><br>

      <label>MQTT 主题:</label>
      <input type='text' name='mqtt_topic' value='%MQTT_TOPIC%'><br>
      <label>MQTT 命令主题:</label>
      <input type='text' name='mqtt_command_topic' value='%MQTT_COMMAND_TOPIC%'><br>

      <label>目标 IP:</label>
      <input type='text' name='target_ip' value='%TARGET_IP%'><br>
      <label>目标端口:</label>
      <input type='text' name='target_port' value='%TARGET_PORT%'><br>

      <button type='submit'>保存</button>
    </form>
  </body>
  </html>
  )rawliteral";

  // 替换模板中的占位符
  html.replace("%SSID%", ssid);
  html.replace("%PASSWORD%", password);
  html.replace("%MQTT_SERVER%", mqtt_server);
  html.replace("%MQTT_USER%", mqtt_user);
  html.replace("%MQTT_PASSWORD%", mqtt_password);
  html.replace("%MQTT_TOPIC%", mqtt_topic);
  html.replace("%MQTT_COMMAND_TOPIC%", mqtt_command_topic);
  html.replace("%TARGET_IP%", target_ip);
  html.replace("%TARGET_PORT%", String(target_port));
  server.send(200, "text/html; charset=UTF-8", html);
}

// 保存表单并重启
void handleSave() {
  if (server.method() == HTTP_POST) {
    ssid = server.arg("ssid");
    password = server.arg("password");
    mqtt_server = server.arg("mqtt_server");
    mqtt_user = server.arg("mqtt_user");
    mqtt_password = server.arg("mqtt_password");
    mqtt_topic = server.arg("mqtt_topic");
    mqtt_command_topic = server.arg("mqtt_command_topic");
    target_ip = server.arg("target_ip");
    target_port = server.arg("target_port").toInt();

    saveConfig();

    server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='UTF-8'></head><body><h2>保存成功！设备将重启…</h2></body></html>");
    delay(2000);
    ESP.restart();
  } else {
    server.send(405, "text/plain", "Method Not Allowed");
  }
}

// ========= LED 闪烁 ==========
void flashLedsOnce() {
  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, LOW);
  delay(100);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, HIGH);
  delay(100);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
}

void flashLedsOnceSyn() {
  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, HIGH);
  delay(100);
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  delay(100);
  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, HIGH);
}
// ========= 配置模式 ==========
void startConfigPortal() {
  Serial.println("进入配置模式...");

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-Config");

  IPAddress IP = WiFi.softAPIP();
  Serial.print("配置页面地址：");
  Serial.println(IP);

  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();

  while (!configFinished) {
    flashLedsOnceSyn();
    server.handleClient();
  }
}

void setup_wifi() {
  Serial.println("进入主程序，连接 WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi 已连接");
    Serial.print("IP 地址: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n⚠️ WiFi 连接失败，重启设备");
    delay(2000);
    ESP.restart();
  }
}

void send_volume_packet(const uint8_t** blocks, const size_t* block_lengths, int block_count, const char* value_str) {
  uint8_t buffer[256];
  int pos = 0;

  for (int i = 0; i < block_count; i++) {
    if (blocks[i] == NULL && value_str != NULL) {
      size_t len = strlen(value_str);
      if (pos + len >= sizeof(buffer)) return;
      memcpy(buffer + pos, value_str, len);
      pos += len;
    } else if (blocks[i] != NULL) {
      size_t len = block_lengths[i];
      if (pos + len >= sizeof(buffer)) return;
      memcpy(buffer + pos, blocks[i], len);
      pos += len;
    }
  }

  send_tcp_binary(target_ip.c_str(), target_port, buffer, pos);
}

void handle_volume_command(String message) {
  if (message.equalsIgnoreCase("volume_low")) {
    const uint8_t* blocks[] = {volume_min_1, volume_min_2, volume_min_3, volume_min_4, volume_min_5, NULL, volume_min_7};
    size_t lengths[] = {9, 9, 10, 8, 11, 0, 2};
    send_volume_packet(blocks, lengths, 7, "0");
  }
  else if (message.equalsIgnoreCase("volume_mid")) {
    const uint8_t* blocks[] = {volume_mid_1, volume_mid_2, volume_mid_3, volume_mid_4, volume_mid_5, NULL, volume_mid_7};
    size_t lengths[] = {10, 11, 10, 11, 5, 0, 2};
    send_volume_packet(blocks, lengths, 7, "20");
  }
  else if (message.equalsIgnoreCase("volume_max")) {
    send_tcp_binary(target_ip.c_str(), target_port, volume_max_full, sizeof(volume_max_full));
  }
  else if (message.startsWith("volume:")) {
    String value = message.substring(7);
    int vol = value.toInt();
    if (vol < 0 || vol > 100) return;

    if (vol == 100) {
      send_tcp_binary(target_ip.c_str(), target_port, volume_max_full, sizeof(volume_max_full));
      return;
    }

    const uint8_t** blocks;
    const size_t* lengths;
    int count;

    if (vol < 10) {
      static const uint8_t* blocks_min[] = {volume_min_1, volume_min_2, volume_min_3, volume_min_4, volume_min_5, NULL, volume_min_7};
      static const size_t lens_min[] = {9, 9, 10, 8, 11, 0, 2};
      blocks = blocks_min; lengths = lens_min; count = 7;
    } else {
      static const uint8_t* blocks_mid[] = {volume_mid_1, volume_mid_2, volume_mid_3, volume_mid_4, volume_mid_5, NULL, volume_mid_7};
      static const size_t lens_mid[] = {10, 11, 10, 11, 5, 0, 2};
      blocks = blocks_mid; lengths = lens_mid; count = 7;
    }

    send_volume_packet(blocks, lengths, count, value.c_str());
  }
}

void execute_macro(const String& macro_raw) {
  String macro = macro_raw;
  int delay_ms = 500; // 默认延迟

  // 检查是否包含 :delay 语法
  int colonIndex = macro.lastIndexOf(':');
  if (colonIndex != -1 && colonIndex < macro.length() - 1) {
    String delay_part = macro.substring(colonIndex + 1);
    int parsed_delay = delay_part.toInt();
    if (parsed_delay > 0) {
      delay_ms = parsed_delay;
      macro = macro.substring(0, colonIndex); // 去掉延迟部分
    }
  }

  int start = 0;
  while (start < macro.length()) {
    int plusIndex = macro.indexOf('+', start);
    String segment = plusIndex == -1 ? macro.substring(start) : macro.substring(start, plusIndex);

    // 检查 * 语法
    int starIndex = segment.indexOf('*');
    String cmd = segment;
    int repeat = 1;

    if (starIndex != -1) {
      cmd = segment.substring(0, starIndex);
      repeat = segment.substring(starIndex + 1).toInt();
      if (repeat <= 0) repeat = 1;
    }

    for (int i = 0; i < repeat; i++) {
      projector_key(target_ip.c_str(), target_port, cmd.c_str(), 100);
      delay(delay_ms);  // 使用动态延迟
    }

    if (plusIndex == -1) break;
    start = plusIndex + 1;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(message);

  if (String(topic) == mqtt_command_topic) {
    if (message.equalsIgnoreCase("ON")) {
      currentState = true;
      Serial.println("Received ON, calling turnon()");
      turnon();
      return;
    }

    if (message.equalsIgnoreCase("OFF")) {
      currentState = false;
      Serial.println("Turning off...");
      turnon();  // 关闭广播
      delay(2000);
      projector_power_off(target_ip.c_str(), target_port);
      return;
    }

    // 音量控制（整合后的处理）
    if (message.startsWith("volume_") || message.startsWith("volume:")) {
      handle_volume_command(message);
      return;
    }

    // 单指令执行
    Command* cmd = get_command(message.c_str());
    if (cmd != nullptr) {
      Serial.println("Single command received: " + message);
      projector_key(target_ip.c_str(), target_port, message.c_str(), 100);
      return;
    }

    // 宏执行
    if (message.indexOf('+') != -1 || message.indexOf('*') != -1) {
      Serial.println("Executing macro: " + message);
      execute_macro(message);
      return;
    }

    Serial.println("Unknown command: " + message);
  }
}

void send_tcp_binary(const char* ip, int port, const uint8_t* data, size_t len) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct sockaddr_in server_addr {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
        send(sock, data, len, 0);
    }

    close(sock);
}

// 查找按键
Command* get_command(const char* name) {
    for (int i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        if (strcmp(name, commands[i].name) == 0) {
            return &commands[i];
        }
    }
    return NULL;
}

// 发送某个按键命令
void projector_key(const char* ip, int port, const char* key_name, int delay_ms) {
    Command* cmd = get_command(key_name);
    if (!cmd) return;
    Serial.print("Press Key:");
    Serial.println(key_name);
    send_tcp_binary(ip, port, cmd->press, cmd->press_len);
    usleep(delay_ms * 1000);
    send_tcp_binary(ip, port, cmd->release, cmd->release_len);
}

// 宏操作示例：多个按键顺序触发
void projector_macro(const char* ip, int port, const char** keys, int key_count, int delay_ms) {
    for (int i = 0; i < key_count; ++i) {
        projector_key(ip, port, keys[i], delay_ms);
        usleep(500 * 1000);  // 两键之间间隔
    }
}

// 关机：宏封装 power -> right -> ok
void projector_power_off(const char* ip, int port) {
    const char* power_macro[] = {"power", "right", "ok"};
    projector_macro(ip, port, power_macro, 3, 100);
}

bool check_tcp_status() {
  WiFiClient client;
  client.setTimeout(1000);  // 设置 1 秒连接超时
  if (!client.connect(target_ip.c_str(), target_port)) {
    return false;
  }
  client.stop();
  return true;
}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32Client", mqtt_user.c_str(), mqtt_password.c_str())) {
      Serial.println("connected");
      mqttClient.subscribe(mqtt_command_topic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void turnon() {
  if (currentState && (!bleInitialized || pAdvertising == nullptr)) {
    // 如果当前要开启广播，但 BLE 已被 deinit，则重新初始化
    setup_bleadv();
  }

  if (!bleInitialized || pAdvertising == nullptr) return;

  if (currentState && !bleBroadcasting) {
    pAdvertising->start();
    bleBroadcasting = true;
    Serial.println("广播已开启");
  } else if (!currentState && bleBroadcasting) {
    pAdvertising->stop();
    bleBroadcasting = false;
    Serial.println("广播已关闭");

    BLEDevice::deinit();         // 清理 BLE 模块
    bleInitialized = false;
    pAdvertising = nullptr;
  }
}

void setup_bleadv() {
  BLEDevice::init("ESP32-Remote");
  pAdvertising = BLEDevice::getAdvertising();

  // 制造商自定义数据
  uint8_t mfg_data[] = {
    0x46, 0x00, // Company Code (0x0046)
    0x53, 0x59, 0x7C, 0xB2, 0xDA, 0xE5, 0xC0,
    0xFF, 0xFF, 0xFF, 0xFF,
    0x5D, 0x00, 0xFF,
    0x4B, 0x4D, 0x53, 0x57, 0x61, 0x6E, 0x64  // 最后这 3 字节：'W', 'a', 'n', 'd'
  };

  // 构造为 Arduino String 类型
  String rawData = "";
  for (size_t i = 0; i < sizeof(mfg_data); ++i) {
    rawData += (char)mfg_data[i];
  }

  advData.setManufacturerData(rawData);
  pAdvertising->setAdvertisementData(advData);
  bleInitialized = true;  // 标记已初始化
}

void flashLeds(int times, int interval = 300) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED1, HIGH);
    digitalWrite(LED2, HIGH);
    delay(interval);
    digitalWrite(LED1, LOW);
    digitalWrite(LED2, LOW);
    delay(interval);
  }
}

void setLeds(bool on) {
  digitalWrite(LED1, on ? HIGH : LOW);
  digitalWrite(LED2, on ? HIGH : LOW);
}



// ========= 主程序 ==========
void startMainProgram() {
  // Wi-Fi配置
  setup_wifi();
  // LED 常亮
  digitalWrite(LED1, HIGH);
  digitalWrite(LED2, HIGH);
  // 蓝牙配置
  setup_bleadv();
  // mqtt配置
  mqttClient.setServer(mqtt_server.c_str(), 1883);
  mqttClient.setCallback(callback);
}

// ========= 初始化 ==========
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);

  loadConfig();

  // LED 闪烁两秒
  unsigned long startTime = millis();
  bool enterConfig = false;
  while (millis() - startTime < 2000) {
    flashLedsOnce();
    if (digitalRead(BUTTON_PIN) == LOW) {
      enterConfig = true;
      break;
    }
  }

  if (enterConfig) {
    startConfigPortal();
  } else {
    digitalWrite(LED1, HIGH);
    digitalWrite(LED2, HIGH);
    startMainProgram();
  }
}

void loop() {
  if (!configFinished) {
    // 配置模式：处理网页请求
    server.handleClient();
    return;
  }
  // put your main code here, to run repeatedly:
  if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();
  static unsigned long last_check = 0;
  if (millis() - last_check > 5000) {  // 每5秒检查一次
    last_check = millis();
    bool status = check_tcp_status();
    mqttClient.publish(mqtt_topic.c_str(), status ? "ON" : "OFF", true);
    if(status&&currentState){//如果电视现在是开机状态，并且广播还在发送中，取消发送
      currentState=false;
    }
    turnon();

  }
}

//todo: 初始化信息配置，HA控制界面，多电视控制，esp模块化集成别的功能
