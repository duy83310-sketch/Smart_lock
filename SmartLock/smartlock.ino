/*
 * KHÓA CỬA THÔNG MINH - NHẬP PIN TRÊN WEB (ESP32)
 *   - Relay kênh 1 : chốt khóa cửa (relay đóng = mở khóa)
 *   - Relay kênh 2 : đầu ra báo động (bật trong lúc báo động)
 *   - LED xanh     : đã mở khóa / nháy khi nhập đúng PIN
 *   - LED đỏ       : đang khóa / nháy khi sai PIN / nháy nhanh khi báo động
 *   - LED vàng     : có người trước cửa hoặc đang tạm khóa
 *   - CÒI (buzzer) : kêu ngắt quãng khi báo động
 *   - 2 nút nhấn   : EXIT mở từ trong nhà, LOCK khóa ngay + tắt báo động
 *   - PIR HC-SR501 : phát hiện người trước cửa; nếu cửa đang KHÓA mà có người
 *                    đứng quá PIR_ALARM_DELAY_MS thì BÁO ĐỘNG (còi + đèn + relay 2)
 *   - Màn hình: hiển thị trên WEB, trang /screen dùng cho điện thoại / tablet gắn ở cửa
 *
 * "GND phụ": mỗi LED / nút nhấn / PIR được nối vào một GPIO đặt mức LOW
 * để làm chân GND riêng (chỉ dùng cho tải dòng nhỏ < 20mA).
 * Còi nối thẳng vào chân GND thật của ESP32 (tải lớn hơn).
 *
 * Không cần cài thêm thư viện. Board: ESP32 Dev Module
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// ===== WiFi do ESP32 tự phát =====
const char* AP_SSID = "SmartLock-ESP32";
const char* AP_PASS = "12345678";  // tối thiểu 8 ký tự

// ===== Chân kết nối (nhãn in trên board trong ngoặc) =====
const int PIN_RELAY_LOCK  = 26;  // (D26) Relay IN1
const int PIN_RELAY_ALARM = 27;  // (D27) Relay IN2
const int PIN_LED_RED     = 23;  // (D23) LED đỏ (+) qua 220Ω
const int PIN_LED_GREEN   = 18;  // (D18) LED xanh (+) qua 220Ω
const int PIN_LED_YELLOW  = 17;  // (TX2) LED vàng (+) qua 220Ω
const int PIN_BUZZER      = 25;  // (D25) Còi (+)
const int PIN_BTN_EXIT    = 32;  // (D32) Nút EXIT
const int PIN_BTN_LOCK    = 13;  // (D13) Nút LOCK
const int PIN_PIR         = 35;  // (D35) OUT cảm biến PIR

// GND phụ: các chân này luôn ở mức LOW
const int GND_PINS[] = {
  22,  // (D22) LED đỏ (−)
  19,  // (D19) LED xanh (−)
  16,  // (RX2) LED vàng (−)
  33,  // (D33) Nút EXIT
  14,  // (D14) Nút LOCK
  4,   // (D4)  PIR GND
};

const bool RELAY_ACTIVE_LOW = true;   // đa số module relay kích mức LOW
const bool USE_PIR          = true;   // đổi thành false nếu không lắp PIR

// ===== Thông số =====
const char* DEFAULT_PIN = "1234";
const int MAX_FAILS                  = 3;
const unsigned long LOCKOUT_MS       = 30000;
const unsigned long AUTO_LOCK_MS     = 5000;
const unsigned long ALARM_MS         = 15000;
const unsigned long SCREEN_MSG_MS    = 4000;
// Cửa đang khóa mà có người đứng trước cửa liên tục quá thời gian này -> báo động
// (đặt 0 để báo động ngay khi PIR phát hiện chuyển động)
const unsigned long PIR_ALARM_DELAY_MS = 8000;
const int BUZZER_FREQ                = 2000;  // Hz

WebServer server(80);
Preferences prefs;

// ===== Trạng thái =====
String currentPin;
bool isLocked = true;
unsigned long unlockedAt = 0;
int failCount = 0;
bool lockedOut = false;
unsigned long lockoutStart = 0;
bool alarmOn = false;
unsigned long alarmStart = 0;
bool motion = false;
unsigned long motionSince = 0;
bool buzzerOn = false;

String screenMsg;
unsigned long screenMsgAt = 0;

struct Button {
  int pin;
  bool last;
  unsigned long changedAt;
};
Button btnExit = {PIN_BTN_EXIT, HIGH, 0};
Button btnLock = {PIN_BTN_LOCK, HIGH, 0};

// ===== Nhật ký sự kiện (8 dòng gần nhất) =====
const int LOG_SIZE = 8;
String logs[LOG_SIZE];
int logCount = 0;

// ================= GIAO DIỆN WEB =================
// Một trang dùng cho 2 chế độ: "/" = màn hình + bàn phím, "/screen" = chỉ màn hình
const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Smart Lock</title>
<style>
  body{font-family:sans-serif;background:#1e1e2e;color:#fff;text-align:center;margin:0;padding:16px}
  .screen{background:#000;border:6px solid #2a2a3a;border-radius:14px;max-width:340px;margin:0 auto 14px;
    padding:12px 14px;font-family:monospace;color:#7fd3ff;text-align:left}
  .row{display:flex;justify-content:space-between;font-size:13px}
  .line{border-top:1px solid #1f4a66;margin:6px 0}
  #big{font-size:34px;font-weight:bold;text-align:center;margin:10px 0;letter-spacing:2px}
  #smsg{min-height:20px;text-align:center;font-size:15px;color:#fff}
  .c-red{color:#ff5c5c}.c-green{color:#4cd964}.c-yellow{color:#ffcc00}
  .blink{animation:bl .4s steps(1) infinite}
  @keyframes bl{50%{opacity:0}}
  #dots{font-size:32px;letter-spacing:8px;height:44px;margin:6px 0}
  .pad{display:grid;grid-template-columns:repeat(3,76px);gap:12px;justify-content:center}
  .pad button{height:68px;font-size:26px;border:none;border-radius:50%;background:#33334d;color:#fff}
  .pad button:active{background:#55557a}
  .pad .ok{background:#2e8b57}.pad .clr{background:#b03a2e}
  #msg{min-height:24px;margin:12px 0}
  .ok-msg{color:#4cd964}.err-msg{color:#ff5c5c}
  details{margin-top:18px}
  input{padding:10px;margin:6px;border-radius:8px;border:none;width:160px;font-size:16px}
  .btn{padding:10px 20px;border:none;border-radius:8px;background:#4a6cf7;color:#fff;font-size:16px;margin:6px}
  #log{list-style:none;padding:0;max-width:340px;margin:8px auto;text-align:left;font-size:13px;color:#c9c9d9}
  #log li{padding:4px 0;border-bottom:1px solid #33334d}
  body.screen-only{padding:0;display:flex;align-items:center;justify-content:center;min-height:100vh}
  body.screen-only .screen{max-width:none;width:92vw;margin:0}
  body.screen-only #big{font-size:15vw}
  body.screen-only #smsg{font-size:5vw;min-height:7vw}
  body.screen-only .row{font-size:4vw}
  body.screen-only .ctrl{display:none}
</style>
</head>
<body>
  <div class="screen">
    <div class="row"><span>SMART LOCK</span><span id="sMotion">--</span></div>
    <div class="line"></div>
    <div id="big">...</div>
    <div id="smsg"></div>
    <div class="line"></div>
    <div class="row"><span id="sAlarm">--</span></div>
  </div>

  <div class="ctrl">
    <div id="dots">----</div>
    <div class="pad">
      <button onclick="press(1)">1</button><button onclick="press(2)">2</button><button onclick="press(3)">3</button>
      <button onclick="press(4)">4</button><button onclick="press(5)">5</button><button onclick="press(6)">6</button>
      <button onclick="press(7)">7</button><button onclick="press(8)">8</button><button onclick="press(9)">9</button>
      <button class="clr" onclick="clr()">C</button><button onclick="press(0)">0</button><button class="ok" onclick="send()">OK</button>
    </div>
    <div id="msg"></div>
    <button class="btn" onclick="lockNow()">Khóa ngay</button>

    <details>
      <summary>Đổi mã PIN</summary>
      <input id="oldPin" type="password" inputmode="numeric" placeholder="PIN cũ"><br>
      <input id="newPin" type="password" inputmode="numeric" placeholder="PIN mới (4-8 số)"><br>
      <button class="btn" onclick="changePin()">Lưu</button>
    </details>

    <details open>
      <summary>Nhật ký</summary>
      <ul id="log"></ul>
    </details>
  </div>

<script>
if(location.pathname === '/screen') document.body.classList.add('screen-only');
let pin = '';
function $(id){ return document.getElementById(id); }
function show(){ $('dots').textContent = pin.length ? '●'.repeat(pin.length) : '----'; }
function press(k){ if(pin.length < 8){ pin += k; show(); } }
function clr(){ pin = ''; show(); }
function msg(t, ok){ $('msg').textContent = t; $('msg').className = ok ? 'ok-msg' : 'err-msg'; }

async function post(url, data){
  const r = await fetch(url, {method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:new URLSearchParams(data)});
  return r.json();
}
async function send(){
  if(pin.length < 4){ msg('PIN phải có 4-8 chữ số', false); return; }
  const j = await post('/unlock', {pin});
  msg(j.msg, j.ok); clr(); refresh();
}
async function lockNow(){ const j = await post('/lock', {}); msg(j.msg, j.ok); refresh(); }
async function changePin(){
  const o = $('oldPin'), n = $('newPin');
  const j = await post('/changepin', {old:o.value, new:n.value});
  msg(j.msg, j.ok); o.value = ''; n.value = '';
}
function big(text, cls){ $('big').textContent = text; $('big').className = cls; }
async function refresh(){
  try{
    const j = await (await fetch('/status')).json();
    if(j.alarm) big('! ALARM !', 'c-red blink');
    else if(j.lockout > 0) big('WAIT ' + j.lockout + 's', 'c-yellow');
    else if(j.locked) big('LOCKED', 'c-red');
    else big('OPEN', 'c-green');
    $('smsg').textContent = j.msg || (j.motion && j.locked ? 'Mời nhập mã PIN...' : '');
    $('sMotion').textContent = !j.hasPir ? '🚶 --' : (j.motion ? '🚶 Có người' : '🚶 Không có ai');
    $('sAlarm').textContent = j.alarm ? '🚨 BÁO ĐỘNG' : '🔕 Bình thường';
    $('log').innerHTML = j.log.map(l => '<li>' + l + '</li>').join('');
  }catch(e){
    big('MẤT KẾT NỐI', 'c-yellow');
  }
}
show(); refresh(); setInterval(refresh, 1000);
</script>
</body>
</html>
)rawliteral";

// ================= TIỆN ÍCH =================
String uptimeStr() {
  unsigned long s = millis() / 1000;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
  return buf;
}

void addLog(const String& msg) {
  for (int i = LOG_SIZE - 1; i > 0; i--) logs[i] = logs[i - 1];
  logs[0] = uptimeStr() + " " + msg;
  if (logCount < LOG_SIZE) logCount++;
  Serial.println(logs[0]);
}

// Thông báo ngắn hiện trên màn hình web trong vài giây
void showMsg(const String& m) {
  screenMsg = m;
  screenMsgAt = millis();
}

// ================= XỬ LÝ CHUỖI / BẢO MẬT =================
bool isValidPin(const String& p) {
  if (p.length() < 4 || p.length() > 8) return false;
  for (size_t i = 0; i < p.length(); i++) {
    if (!isDigit(p[i])) return false;
  }
  return true;
}

// So sánh thời gian cố định: không thoát sớm khi gặp ký tự sai
bool safeEquals(const String& a, const String& b) {
  if (a.length() != b.length()) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < a.length(); i++) diff |= a[i] ^ b[i];
  return diff == 0;
}

unsigned long lockoutRemainingSec() {
  if (!lockedOut) return 0;
  unsigned long elapsed = millis() - lockoutStart;
  if (elapsed >= LOCKOUT_MS) {
    lockedOut = false;
    return 0;
  }
  return (LOCKOUT_MS - elapsed) / 1000 + 1;
}

// ================= ĐIỀU KHIỂN PHẦN CỨNG =================
void setRelay(int pin, bool on) {
  digitalWrite(pin, (on ^ RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

// Bật/tắt còi (dùng tone để chạy được cả còi chủ động, còi thụ động và Wokwi)
void setBuzzer(bool on) {
  if (on == buzzerOn) return;
  buzzerOn = on;
  if (on) tone(PIN_BUZZER, BUZZER_FREQ);
  else    noTone(PIN_BUZZER);
}

// PIN đúng: LED xanh nháy 3 lần
void flashSuccess() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_LED_GREEN, LOW);  delay(80);
    digitalWrite(PIN_LED_GREEN, HIGH); delay(80);
  }
}

// PIN sai: LED đỏ nháy 3 lần
void flashFail() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_LED_RED, LOW);  delay(120);
    digitalWrite(PIN_LED_RED, HIGH); delay(120);
  }
}

void startAlarm(const String& reason) {
  if (!alarmOn) addLog("BÁO ĐỘNG: " + reason);
  alarmOn = true;
  alarmStart = millis();
  setRelay(PIN_RELAY_ALARM, true);
}

void stopAlarm() {
  if (!alarmOn) return;
  alarmOn = false;
  setRelay(PIN_RELAY_ALARM, false);
  setBuzzer(false);
  addLog("Tắt báo động");
}

void lockDoor(const String& source) {
  isLocked = true;
  setRelay(PIN_RELAY_LOCK, false);
  addLog("Khóa cửa (" + source + ")");
  showMsg("Đã khóa cửa");
}

void unlockDoor(const String& source) {
  isLocked = false;
  unlockedAt = millis();
  failCount = 0;
  stopAlarm();
  setRelay(PIN_RELAY_LOCK, true);
  addLog("Mở khóa (" + source + ")");
  showMsg("Mời vào!");
}

// LED đỏ : báo động = nháy nhanh, tạm khóa = nháy chậm, đang khóa = sáng, đã mở = tắt
// LED xanh: sáng khi đã mở khóa
// LED vàng: sáng khi có người trước cửa hoặc đang tạm khóa
// Còi    : kêu ngắt quãng (bíp - bíp) khi báo động
void updateLeds() {
  unsigned long t = millis();
  bool inLockout = lockoutRemainingSec() > 0;
  bool red;
  if (alarmOn) red = (t / 150) % 2;
  else if (inLockout) red = (t / 500) % 2;
  else red = isLocked;
  digitalWrite(PIN_LED_RED, red);
  digitalWrite(PIN_LED_GREEN, !isLocked);
  digitalWrite(PIN_LED_YELLOW, alarmOn ? (t / 150) % 2 == 0 : (motion || inLockout));
  setBuzzer(alarmOn && (t / 250) % 2 == 0);
}

// ================= CẢM BIẾN & NÚT NHẤN =================
void readSensors() {
  if (!USE_PIR) return;

  bool m = digitalRead(PIN_PIR) == HIGH;
  if (m && !motion) {
    motionSince = millis();
    addLog("Phát hiện người trước cửa");
  }
  motion = m;

  // Cửa đang khóa mà có người đứng lâu trước cửa -> báo động
  if (motion && isLocked && !alarmOn && lockoutRemainingSec() == 0 &&
      millis() - motionSince >= PIR_ALARM_DELAY_MS) {
    showMsg("Phát hiện người lạ!");
    startAlarm("PIR phát hiện người trước cửa");
    motionSince = millis();  // nếu người vẫn đứng đó, sau khi hết báo động sẽ đếm lại
  }
}

bool pressed(Button& b) {
  bool now = digitalRead(b.pin);
  if (now != b.last && millis() - b.changedAt > 50) {
    b.changedAt = millis();
    b.last = now;
    return now == LOW;
  }
  return false;
}

void handleButtons() {
  if (pressed(btnExit)) {
    unlockDoor("nút EXIT");
    updateLeds();
    flashSuccess();
  }
  if (pressed(btnLock)) {
    stopAlarm();
    lockDoor("nút LOCK");
  }
}

// ================= HTTP HANDLERS =================
void sendJson(bool ok, const String& msg) {
  server.send(200, "application/json", "{\"ok\":" + String(ok ? "true" : "false") + ",\"msg\":\"" + msg + "\"}");
}

String jsonBool(bool v) {
  return v ? "true" : "false";
}

void handleRoot() {
  server.send_P(200, "text/html", PAGE_HTML);
}

void handleStatus() {
  bool msgActive = screenMsg.length() && millis() - screenMsgAt < SCREEN_MSG_MS;
  String json = "{\"locked\":" + jsonBool(isLocked) +
                ",\"lockout\":" + String(lockoutRemainingSec()) +
                ",\"hasPir\":" + jsonBool(USE_PIR) +
                ",\"motion\":" + jsonBool(motion) +
                ",\"alarm\":" + jsonBool(alarmOn) +
                ",\"msg\":\"" + (msgActive ? screenMsg : "") + "\"" +
                ",\"log\":[";
  for (int i = 0; i < logCount; i++) {
    if (i) json += ",";
    json += "\"" + logs[i] + "\"";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleUnlock() {
  unsigned long remain = lockoutRemainingSec();
  if (remain > 0) {
    sendJson(false, "Sai quá nhiều lần! Thử lại sau " + String(remain) + "s");
    return;
  }

  String pin = server.arg("pin");
  pin.trim();
  if (!isValidPin(pin)) {
    sendJson(false, "PIN không hợp lệ (chỉ 4-8 chữ số)");
    return;
  }

  if (safeEquals(pin, currentPin)) {
    unlockDoor("PIN trên web");
    sendJson(true, "Mở khóa thành công!");
    updateLeds();
    flashSuccess();
    return;
  }

  failCount++;
  addLog("Sai PIN lần " + String(failCount));
  if (failCount >= MAX_FAILS) {
    failCount = 0;
    lockedOut = true;
    lockoutStart = millis();
    showMsg("Sai PIN quá 3 lần!");
    startAlarm("sai PIN " + String(MAX_FAILS) + " lần");
    sendJson(false, "Sai " + String(MAX_FAILS) + " lần! Tạm khóa " + String(LOCKOUT_MS / 1000) + "s");
  } else {
    showMsg("Sai PIN! " + String(failCount) + "/" + String(MAX_FAILS));
    sendJson(false, "Sai PIN! Còn " + String(MAX_FAILS - failCount) + " lần thử");
  }
  flashFail();
}

// Khóa từ web KHÔNG tắt được báo động: chỉ PIN đúng hoặc nút trong nhà mới tắt được
void handleLock() {
  lockDoor("web");
  sendJson(true, "Đã khóa cửa");
}

void handleChangePin() {
  String oldPin = server.arg("old");
  String newPin = server.arg("new");
  oldPin.trim();
  newPin.trim();

  if (lockoutRemainingSec() > 0) {
    sendJson(false, "Đang bị tạm khóa");
    return;
  }
  if (!safeEquals(oldPin, currentPin)) {
    addLog("Đổi PIN thất bại (sai PIN cũ)");
    sendJson(false, "PIN cũ không đúng");
    return;
  }
  if (!isValidPin(newPin)) {
    sendJson(false, "PIN mới phải gồm 4-8 chữ số");
    return;
  }

  currentPin = newPin;
  prefs.putString("pin", currentPin);
  addLog("Đã đổi PIN");
  showMsg("Đã đổi PIN");
  sendJson(true, "Đổi PIN thành công!");
}

// ================= SETUP / LOOP =================
void setup() {
  Serial.begin(115200);

  for (int pin : GND_PINS) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  pinMode(PIN_RELAY_LOCK, OUTPUT);
  pinMode(PIN_RELAY_ALARM, OUTPUT);
  setRelay(PIN_RELAY_LOCK, false);
  setRelay(PIN_RELAY_ALARM, false);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  pinMode(PIN_BTN_EXIT, INPUT_PULLUP);
  pinMode(PIN_BTN_LOCK, INPUT_PULLUP);
  pinMode(PIN_PIR, INPUT);

  prefs.begin("smartlock", false);
  currentPin = prefs.getString("pin", DEFAULT_PIN);

  lockDoor("khởi động");

  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("WiFi: ");
  Serial.println(AP_SSID);
  Serial.print("Ban phim:   http://");
  Serial.println(WiFi.softAPIP());
  Serial.print("Man hinh:   http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("/screen");

  server.on("/", HTTP_GET, handleRoot);
  server.on("/screen", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/unlock", HTTP_POST, handleUnlock);
  server.on("/lock", HTTP_POST, handleLock);
  server.on("/changepin", HTTP_POST, handleChangePin);
  server.begin();
}

void loop() {
  server.handleClient();
  handleButtons();
  readSensors();

  // Tự khóa lại khi hết thời gian mở khóa
  if (!isLocked && millis() - unlockedAt >= AUTO_LOCK_MS) {
    lockDoor("tự động");
  }

  if (alarmOn && millis() - alarmStart >= ALARM_MS) stopAlarm();

  updateLeds();
}
