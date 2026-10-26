# 🔐 SmartLock – Khóa cửa thông minh nhập PIN trên Web (ESP32)

Đề tài IoT: **Khóa cửa thông minh kết hợp nhập mật khẩu (PIN Code) trên Web**, trọng tâm là *bảo mật & xử lý chuỗi*.

ESP32 tự phát WiFi và chạy một Web Server gồm **màn hình trạng thái** và **bàn phím số**. Người dùng kết nối điện thoại vào WiFi đó, nhập đúng PIN thì ESP32 kích relay để mở khóa và LED xanh sáng. Nhập sai thì LED đỏ nháy. Sai nhiều lần thì hệ thống báo động.

**Lắp trên breadboard, không cần màn hình OLED.** Phần bắt buộc chỉ gồm ESP32, breadboard, Relay 2 kênh, LED xanh, LED đỏ và dây cắm, không phải cắt hay xoắn dây nào. Còi buzzer, LED vàng, 2 nút và PIR là phần tùy chọn.

> 📘 **Hướng dẫn đầy đủ** gồm hình vẽ cách nối LED và nút, sơ đồ chân ESP32, cắm dây từng bước có kiểm tra, giải thích code, bảo mật, kiểm thử và xử lý lỗi: tải file [`SmartLock_HuongDan.html`](SmartLock_HuongDan.html) về, mở bằng trình duyệt, nhấn **Ctrl+P** để lưu thành PDF.

---

## Mục lục

- [Tính năng](#-tính-năng)
- [Cấu trúc thư mục](#-cấu-trúc-thư-mục)
- [Linh kiện](#-linh-kiện)
- [Nối dây trên breadboard](#-nối-dây-trên-breadboard)
- [Cắm dây từng bước](#-cắm-dây-từng-bước)
- [Cài đặt & nạp code](#-cài-đặt--nạp-code)
- [Sử dụng](#-sử-dụng)
- [Cơ chế quét của PIR](#-cơ-chế-quét-của-pir)
- [Cấu hình](#-cấu-hình)
- [Web API](#-web-api)
- [Bảo mật & xử lý chuỗi](#-bảo-mật--xử-lý-chuỗi)
- [Kiểm thử nhanh](#-kiểm-thử-nhanh)
- [Xử lý lỗi thường gặp](#-xử-lý-lỗi-thường-gặp)
- [Hạn chế & hướng phát triển](#-hạn-chế--hướng-phát-triển)

---

## ✨ Tính năng

| Tính năng | Mô tả |
|---|---|
| Bàn phím số trên Web | Nhập PIN trên điện thoại. Màn hình chỉ hiện dấu `●`, PIN được gửi bằng POST |
| Màn hình trên Web (thay OLED) | Màn hình ảo hiện LOCKED / OPEN / WAIT / ALARM, thông báo, trạng thái người và báo động. Trang `/screen` hiển thị toàn màn hình để gắn điện thoại hoặc tablet cũ ở cửa |
| Mở / khóa bằng relay | Relay kênh 1 là chốt khóa. Tự khóa lại sau 5 giây |
| Còi + LED báo trạng thái | LED xanh nháy khi PIN đúng, LED đỏ nháy khi sai. Khi báo động: còi kêu "bíp - bíp", LED đỏ + LED vàng nháy nhanh |
| Chống dò mật khẩu | Sai 3 lần thì tạm khóa 30 giây và báo động 15 giây (còi kêu, relay kênh 2 bật) |
| Đổi PIN | Phải nhập đúng PIN cũ. PIN lưu trong flash nên mất điện không mất |
| Nhật ký | 8 sự kiện gần nhất hiển thị trên web và in ra Serial |
| *(Tùy chọn)* Nút EXIT / LOCK | EXIT mở cửa từ trong nhà. LOCK khóa ngay và tắt báo động |
| *(Tùy chọn)* Còi buzzer | Kêu "bíp - bíp" khi báo động (chân D25) |
| *(Tùy chọn)* Cảm biến người | PIR phát hiện người: LED vàng sáng, hiện "Mời nhập mã PIN...". Cửa đang khóa mà người đứng quá `PIR_ALARM_DELAY_MS` (mặc định 8 giây) thì báo động |

## 📁 Cấu trúc thư mục

```
.
├── SmartLock/
│   └── smartlock.ino            # Mã nguồn Arduino cho ESP32 (không cần thư viện ngoài)
├── SmartLock_HuongDan.html      # Tài liệu hướng dẫn đầy đủ (xuất được PDF)
└── README.md
```

## 🧰 Linh kiện

| Linh kiện | Mức | SL | Vai trò |
|---|---|---|---|
| ESP32 DevKit V1 + cáp Micro USB | bắt buộc | 1 | Bộ xử lý, phát WiFi, chạy Web Server |
| Relay 2 kênh 5V | bắt buộc | 1 | CH1: chốt khóa · CH2: đầu ra báo động |
| LED đỏ + điện trở 220Ω | bắt buộc | 1 + 1 | Đang khóa, sai PIN, báo động |
| LED xanh + điện trở 220Ω | bắt buộc | 1 + 1 | Đã mở, PIN đúng |
| Breadboard (830 lỗ, hoặc 2 board 400 lỗ) | bắt buộc | 1 | Cắm ESP32, LED, điện trở, nút |
| Dây cắm M-M | bắt buộc | 10–20 | Nối giữa các hàng trên breadboard |
| Dây Dupont M-F | bắt buộc | 4–7 | Nối breadboard tới module có chân đực (Relay, PIR) |
| LED vàng + điện trở 220Ω | tùy chọn | 1 + 1 | Có người, đang tạm khóa |
| Nút nhấn | tùy chọn | 2 | EXIT, LOCK |
| Còi buzzer (loại nhỏ, ≤ 20mA) | tùy chọn | 1 | Kêu "bíp - bíp" khi báo động |
| PIR HC-SR501 | tùy chọn | 1 | Phát hiện người, báo động khi đứng lâu trước cửa đang khóa |

Không dùng: OLED, DHT11, quang trở, chiết áp.

## 🔌 Nối dây trên breadboard

### Cách bố trí

1. **Cắm ESP32**: cắm ESP32 vắt ngang rãnh giữa breadboard, mỗi hàng chân nằm một bên. ESP32 DevKit V1 khá rộng, nếu một bên chỉ còn 1 lỗ trống thì ghép 2 breadboard cạnh nhau, mỗi hàng chân ESP32 cắm lên một board.
2. **LED + điện trở**: cắm điện trở 220Ω từ hàng của chân GPIO sang một hàng trống, cắm **chân dài (+)** của LED vào hàng đó, **chân ngắn (−)** sang hàng của chân GND phụ (hoặc nối dây M-M về chân đó). Không cần xoắn hay quấn băng dính.
3. **GND phụ**: ESP32 chỉ có 2 chân GND, nên code đặt thêm một số GPIO ở mức `LOW` để làm GND riêng cho từng LED, nút nhấn và PIR. Cách này chỉ dùng cho tải dưới 20mA: LED khoảng 6mA, nút khoảng 0.07mA, PIR dưới 0.1mA. **Không** dùng cho relay hay còi (còi nối vào GND thật).
4. **Thanh nguồn**: kéo chân **VIN** ra thanh `+` và chân **GND** ra thanh `−` của breadboard. Relay và PIR cùng lấy nguồn 5V từ thanh `+`, relay và còi lấy GND từ thanh `−`.
5. **Nút nhấn**: cắm nút vắt ngang rãnh giữa (hoặc vào một vùng trống), dùng 2 chân **chéo nhau** nối tới GPIO và GND phụ.

### Sơ đồ chân (ESP32 DevKit V1 30 chân, mặt chip quay lên, USB quay xuống)

```
                                 ┌──────────────┐
                                 │ ESP-WROOM-32 │
                            EN   ┤              ├ D23  ◄ LED đỏ (+) qua 220Ω
                            VP   ┤              ├ D22  ◄ LED đỏ (−) [GND phụ]
                            VN   ┤              ├ TX0 
                            D34  ┤              ├ RX0 
       PIR OUT (tùy chọn) ► D35  ┤              ├ D21 
          Nút EXIT chân 1 ► D32  ┤              ├ D19  ◄ LED xanh (−) [GND phụ]
Nút EXIT chân 2 [GND phụ] ► D33  ┤              ├ D18  ◄ LED xanh (+) qua 220Ω
           Còi buzzer (+) ► D25  ┤              ├ D5  
                Relay IN1 ► D26  ┤              ├ D17  ◄ LED vàng (+) qua 220Ω (tùy chọn)
                Relay IN2 ► D27  ┤              ├ D16  ◄ LED vàng (−) [GND phụ]
Nút LOCK chân 2 [GND phụ] ► D14  ┤              ├ D4   ◄ PIR GND [GND phụ]
                            D12  ┤              ├ D2  
          Nút LOCK chân 1 ► D13  ┤              ├ D15 
                Relay GND ► GND  ┤              ├ GND  ◄ Còi (−)
    Relay VCC (+ PIR VCC) ► VIN  ┤              ├ 3V3  
                                 └────[USB]─────┘
```

> Board 38 chân có thứ tự chân khác. Luôn cắm theo **nhãn in trên board** (`D17` = GPIO17, `D16` = GPIO16).

### Bảng nối dây

| Linh kiện | Chân linh kiện | Chân ESP32 | Dây |
|---|---|---|---|
| **Relay** | VCC · GND · IN1 · IN2 | VIN *(thanh +)* · GND *(thanh −)* · D26 · D27 | 4 × M-F |
| **LED đỏ** | (+) qua 220Ω · (−) | D23 · D22 *(GND phụ)* | Cắm trên breadboard / M-M |
| **LED xanh** | (+) qua 220Ω · (−) | D18 · D19 *(GND phụ)* | Cắm trên breadboard / M-M |
| LED vàng | (+) qua 220Ω · (−) | D17 · D16 *(GND phụ)* | Cắm trên breadboard / M-M |
| Nút EXIT | 2 chân chéo nhau | D32 · D33 *(GND phụ)* | Cắm trên breadboard / M-M |
| Nút LOCK | 2 chân chéo nhau | D13 · D14 *(GND phụ)* | Cắm trên breadboard / M-M |
| Còi buzzer | (+) · (−) | D25 · GND *(thanh −)* | Cắm trên breadboard / M-M |
| PIR | VCC · GND · OUT | VIN *(thanh +)* · D4 *(GND phụ)* · D35 | 3 × M-F |

Quy tắc: linh kiện có chân trần (LED, điện trở, nút, còi) cắm thẳng lên breadboard, nối bằng dây M-M. Module có chân đực (Relay, PIR) nối bằng dây M-F.

## 🪛 Cắm dây từng bước

**Nạp code trước khi cắm dây.** Mỗi bước làm theo thứ tự: rút USB → cắm dây → cắm USB → kiểm tra.

| Bước | Việc làm | Kiểm tra |
|---|---|---|
| 0 | Cắm ESP32 lên breadboard, mặt chip lên, USB quay về phía mình. Nạp code, mở web | Web hiện `LOCKED` |
| 1 | **LED đỏ**: D23 → điện trở 220Ω → chân dài, chân ngắn → D22 | LED đỏ sáng. Nhập sai PIN thì nháy 3 lần |
| 2 | **LED xanh**: D18 → điện trở 220Ω → chân dài, chân ngắn → D19 | Nhập `1234` thì LED xanh nháy rồi sáng, sau 5 giây tắt |
| 3 | **Relay**: VCC → thanh + (VIN), GND → thanh − (GND), IN1 → D26, IN2 → D27 | Mở khóa thì relay 1 kêu "tách". Sai 3 lần thì relay 2 đóng 15 giây. **Đủ đề bài** 🎉 |
| 4 | *(tùy chọn)* **LED vàng**: D17 (+) / D16 (−) | Sáng khi có người hoặc trong lúc tạm khóa |
| 5 | *(tùy chọn)* **Nút**: EXIT → D32 / D33, LOCK → D13 / D14 (chân chéo) | EXIT mở khóa. LOCK khóa và tắt báo động |
| 6 | *(tùy chọn)* **Còi buzzer**: (+) → D25, (−) → thanh − (GND) | Nhập sai 3 lần thì còi kêu "bíp - bíp" trong lúc báo động |
| 7 | *(tùy chọn)* **PIR**: VCC → thanh + (VIN), GND → D4, OUT → D35 | Chờ 60 giây, vẫy tay thì LED vàng sáng, web hiện "Có người". Đang khóa mà đứng lâu (quá `PIR_ALARM_DELAY_MS`) thì báo động |
| 8 | Rà lại toàn bộ breadboard | Không có chân linh kiện nào cắm nhầm hàng hoặc chạm sang hàng bên cạnh |

> ⚠️ Các lỗ trong cùng một hàng 5 lỗ được nối thông với nhau, đừng cắm 2 chân của cùng một linh kiện vào cùng hàng. Còi nối vào GND thật. Không cắm vào TX0/RX0/D12.

## 🛠 Cài đặt & nạp code

1. Cài **Arduino IDE 2.x**.
2. Vào *File → Preferences → Additional boards manager URLs* và thêm đường dẫn:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. Vào *Boards Manager*, cài **esp32** của **Espressif Systems**. **Không cần cài thư viện nào.**
4. Mở `SmartLock/smartlock.ino`, chọn board **ESP32 Dev Module** và chọn đúng cổng.
5. Bấm **Upload**. Nếu báo lỗi `Failed to connect`, hãy giữ nút **BOOT** cho đến khi thấy chữ `Writing...`.
6. Mở Serial Monitor ở **115200 baud** rồi bấm **EN**.

Driver: Windows cần cài CP210x hoặc CH340. Linux chạy lệnh `sudo usermod -aG dialout $USER` rồi đăng nhập lại.

Code đã được biên dịch thử với esp32 core **3.3.12**, không có cảnh báo, chiếm khoảng 71% flash.

## 📱 Sử dụng

1. Trên điện thoại, kết nối WiFi **`SmartLock-ESP32`**, mật khẩu **`12345678`**. Nên tắt 4G.
2. Mở **http://192.168.4.1** để thấy màn hình ảo và bàn phím. Nhập PIN mặc định **`1234`**.
3. *(Tùy chọn)* Trên một điện thoại hoặc tablet thứ 2 gắn ở cửa, mở **http://192.168.4.1/screen** để làm màn hình hiển thị (thay OLED).

| Trạng thái | LED đỏ | LED xanh | LED vàng | Còi | Relay 1 | Relay 2 | Màn hình web |
|---|---|---|---|---|---|---|
| Đang khóa | Sáng | Tắt | Tắt | Tắt | Ngắt | Ngắt | LOCKED |
| PIN đúng | Tắt | Nháy → sáng | — | Tắt | Đóng | Ngắt | OPEN · Mời vào! |
| PIN sai | Nháy 3 lần | Tắt | — | Tắt | Ngắt | Ngắt | Sai PIN! 1/3 |
| Tạm khóa | Nháy chậm | Tắt | Sáng | Tắt | Ngắt | — | WAIT 25s |
| Báo động | **Nháy nhanh** | Tắt | **Nháy nhanh** | **Bíp - bíp** | Ngắt | **Đóng** | ! ALARM ! |

**Tắt báo động:** nhập đúng PIN, bấm nút EXIT (mở cửa) hoặc nút LOCK, hoặc chờ báo động tự tắt sau 15 giây. Nút "Khóa ngay" trên web **không** tắt được báo động.

## 👁 Cơ chế quét của PIR

**Phần cứng (HC-SR501).** Cảm biến đo sự thay đổi tia hồng ngoại do thân nhiệt người di chuyển. Khi thấy chuyển động, chân OUT lên `HIGH` (3.3V) và giữ trong khoảng thời gian chỉnh bằng biến trở **Tx**, xong thì về `LOW`. Biến trở **Sx** chỉnh độ nhạy (tầm xa). Nếu người vẫn còn cử động trong lúc OUT đang `HIGH` thì cảm biến giữ `HIGH` tiếp (chế độ lặp lại, jumper ở vị trí H). Vừa cấp nguồn, PIR cần khoảng 30–60 giây để ổn định, trong lúc đó OUT có thể bật tắt lung tung.

**Phần mềm.** `loop()` chạy liên tục và mỗi vòng gọi `readSensors()` một lần, tức là ESP32 đọc chân D35 hàng trăm lần mỗi giây (polling, không dùng ngắt):

```
đọc D35 ──► HIGH và lần trước chưa có người? ──► có: motionSince = bây giờ, ghi log "Phát hiện người trước cửa"
        └─► motion = trạng thái vừa đọc (HIGH = có người, LOW = không có ai)

nếu  motion  &&  cửa đang khóa  &&  chưa báo động  &&  không trong thời gian tạm khóa
     &&  bây giờ − motionSince ≥ PIR_ALARM_DELAY_MS (8 giây)
  ──► BÁO ĐỘNG 15 giây, motionSince = bây giờ
```

1. **Phát hiện người**: lúc OUT chuyển từ `LOW` sang `HIGH`, code ghi lại mốc thời gian `motionSince`. LED vàng sáng, web hiện "🚶 Có người" và "Mời nhập mã PIN...".
2. **Đếm thời gian đứng**: miễn là OUT còn `HIGH` thì `motion` vẫn là `true` và thời gian tính từ `motionSince` cứ tăng lên. Chỉ cần OUT về `LOW` (người đi khỏi, hoặc hết thời gian Tx mà không còn cử động) là `motion = false`. Lần `HIGH` sau sẽ đếm lại từ 0.
3. **Báo động**: cửa đang khóa mà OUT giữ `HIGH` liên tục đủ `PIR_ALARM_DELAY_MS` thì bật báo động: còi kêu, LED đỏ và LED vàng nháy nhanh, relay 2 đóng. Mốc `motionSince` được đặt lại tại thời điểm này.
4. **Khi nào không báo động**: cửa đang mở, đang báo động, hoặc đang tạm khóa vì sai PIN. Lúc này PIR vẫn được đọc và vẫn hiện "Có người", chỉ không kích báo động.

Hệ quả cần biết:
- Báo động kéo dài 15 giây, lâu hơn 8 giây chờ. Vì vậy nếu sau khi báo động tắt người vẫn đứng đó (OUT vẫn `HIGH`) thì báo động bật lại ngay. Muốn dừng hẳn thì nhập đúng PIN hoặc bấm EXIT để mở cửa.
- Thời gian OUT giữ `HIGH` (Tx) cũng được tính là "đang đứng". Vặn Tx quá lớn thì người chỉ đi ngang qua cũng có thể gây báo động. Nên vặn Tx hết ngược chiều kim đồng hồ (khoảng 3 giây).
- Code không bỏ qua giai đoạn PIR khởi động, nên sau khi cấp nguồn hãy chờ khoảng 60 giây rồi mới thử.

## ⚙️ Cấu hình

Các hằng số ở đầu file `SmartLock.ino`:

| Hằng số | Mặc định | Ý nghĩa |
|---|---|---|
| `AP_SSID` / `AP_PASS` | `SmartLock-ESP32` / `12345678` | Tên và mật khẩu WiFi do ESP32 phát |
| `DEFAULT_PIN` | `"1234"` | PIN dùng lần đầu |
| `USE_PIR` | `true` | Đổi thành `false` nếu không lắp PIR |
| `RELAY_ACTIVE_LOW` | `true` | Đổi thành `false` nếu relay hoạt động ngược |
| `GND_PINS[]` | `22, 19, 16, 33, 14, 4` | Các chân làm GND phụ |
| `MAX_FAILS` | `3` | Số lần được nhập sai trước khi tạm khóa |
| `LOCKOUT_MS` | `30000` | Thời gian tạm khóa (ms) |
| `AUTO_LOCK_MS` | `5000` | Thời gian chờ trước khi tự khóa lại (ms) |
| `ALARM_MS` | `15000` | Thời gian báo động (ms) |
| `PIR_ALARM_DELAY_MS` | `8000` | Người đứng liên tục trước cửa đang khóa quá thời gian này thì báo động (ms). Đặt `0` để báo động ngay |
| `BUZZER_FREQ` | `2000` | Tần số còi (Hz) |
| `SCREEN_MSG_MS` | `4000` | Thời gian thông báo ngắn hiện trên màn hình web (ms) |

## 🌐 Web API

| Đường dẫn | Phương thức | Tham số | Chức năng |
|---|---|---|---|
| `/` | GET | — | Màn hình ảo + bàn phím |
| `/screen` | GET | — | Chỉ màn hình, toàn trang (thay OLED) |
| `/status` | GET | — | Trạng thái dạng JSON |
| `/unlock` | POST | `pin` | Kiểm tra PIN và mở khóa |
| `/lock` | POST | — | Khóa ngay (không tắt báo động) |
| `/changepin` | POST | `old`, `new` | Đổi PIN và lưu vào flash |

Ví dụ phản hồi của `/status`:

```json
{"locked":true,"lockout":0,"hasPir":true,"motion":false,
 "alarm":false,"msg":"Sai PIN! 1/3","log":["00:01:20 Sai PIN lần 1","00:00:00 Khóa cửa (khởi động)"]}
```

## 🛡 Bảo mật & xử lý chuỗi

| Biện pháp | Cách làm | Chống lại |
|---|---|---|
| Kiểm tra đầu vào | `isValidPin()`: chỉ nhận 4–8 ký tự, tất cả là chữ số | Dữ liệu rác, chuỗi quá dài |
| Chuẩn hóa chuỗi | `trim()` trước khi so sánh | Khoảng trắng thừa |
| So sánh thời gian cố định | `safeEquals()` duyệt hết chuỗi bằng XOR, không dừng sớm ở ký tự sai | Timing attack |
| Giới hạn số lần thử | Sai 3 lần thì tạm khóa 30 giây và báo động | Brute-force |
| Gửi PIN bằng POST | PIN nằm trong body, không nằm trên URL | Lộ PIN qua lịch sử trình duyệt |
| Kiểm tra ở server | Chỉ ESP32 biết PIN đúng, `/status` không trả về PIN | Đọc mã nguồn trang web để lấy PIN |
| Đổi PIN cần PIN cũ | `/changepin` kiểm tra PIN cũ, lần thất bại được ghi nhật ký | Người lạ tự ý đổi PIN |
| Báo động người lạ | Cửa đang khóa mà PIR phát hiện người đứng lâu thì báo động | Rình trước cửa, đột nhập |
| Phân quyền tắt báo động | Chỉ PIN đúng hoặc nút LOCK trong nhà tắt được | Kẻ gian vào được WiFi rồi tắt báo động |
| Tự khóa lại | Khóa sau 5 giây | Quên khóa cửa |

Ước tính: dò hết PIN 4 số mất khoảng **27,8 giờ**, PIN 6 số mất khoảng **115 ngày**.

## ✅ Kiểm thử nhanh

| # | Thao tác | Kết quả mong đợi |
|---|---|---|
| 1 | Nhập `1234` | Relay 1 kêu "tách", LED xanh nháy rồi sáng, web hiện OPEN |
| 2 | Chờ 5 giây | Tự khóa, LED đỏ sáng |
| 3 | Nhập sai 3 lần | Tạm khóa 30 giây, còi kêu "bíp - bíp", LED đỏ + LED vàng nháy nhanh, relay 2 đóng, web hiện ! ALARM ! |
| 4 | Đang khóa, đứng/vẫy tay trước PIR khoảng 8 giây | Báo động PIR: còi kêu, LED nháy nhanh, relay 2 đóng, Serial in "BÁO ĐỘNG: PIR phát hiện người trước cửa" |
| 5 | Đang báo động, nhập đúng PIN hoặc bấm EXIT/LOCK | Báo động tắt, còi im |
| 6 | Đang báo động, bấm "Khóa ngay" trên web | Báo động vẫn tiếp tục |
| 7 | Mở `/screen` trên máy thứ 2, mở khóa trên máy 1 | Máy 2 đổi sang OPEN trong vòng 1 giây |
| 8 | Đổi PIN thành `5678`, rút điện rồi cắm lại | `5678` mở được, `1234` bị từ chối |
| 9 | Nhập `12` hoặc đổi PIN mới thành `abcd` | Báo PIN không hợp lệ |

Bản đầy đủ 15 kịch bản kiểm thử nằm trong file hướng dẫn HTML.

## 🧯 Xử lý lỗi thường gặp

| Hiện tượng | Cách sửa |
|---|---|
| Máy tính không nhận cổng COM / ttyUSB | Dùng cáp USB có truyền dữ liệu, cài driver CP210x/CH340 |
| LED không sáng | Kiểm tra chiều LED (chân dài nối phía điện trở), kiểm tra LED và điện trở có nằm đúng hàng breadboard, nạp lại code mới |
| LED chập chờn khi chạm vào dây | Chân cắm lỏng: cắm sâu lại, đổi sang lỗ khác hoặc thay dây cắm |
| Relay hoạt động ngược | `RELAY_ACTIVE_LOW = false` |
| Relay không nhả | Rút jumper JD-VCC, nối JD-VCC với 5V và VCC với 3V3 |
| Báo động liên tục (PIR) | Chưa lắp PIR nhưng `USE_PIR = true` làm chân D35 trôi nổi đọc ngẫu nhiên; hoặc PIR nhạy quá. Chưa lắp thì đặt `USE_PIR = false`, đã lắp thì chờ 60 giây và vặn Tx hết ngược chiều kim đồng hồ |
| Web báo "Có người" liên tục | Chờ 60 giây cho PIR ổn định, vặn Tx hết ngược chiều kim đồng hồ, kiểm tra cờ `USE_PIR` |
| Còi không kêu khi báo động | Còi hỏng, cắm ngược chiều, hoặc chưa nạp code có còi. Kiểm tra (+) → D25, (−) → GND thật |
| Bấm nút không có tác dụng | Dùng 2 chân **chéo nhau** của nút |
| Web hiện "MẤT KẾT NỐI" | Tắt 4G, kết nối lại WiFi `SmartLock-ESP32`, kiểm tra nguồn ESP32 |

## 🚧 Hạn chế & hướng phát triển

**Hạn chế**
- Dùng HTTP nên dữ liệu không được mã hóa.
- PIN được lưu dạng chuỗi gốc trong flash.
- Relay chỉ mô phỏng chốt khóa.
- Breadboard dễ lỏng chân khi di chuyển, kém bền hơn mạch hàn.

**Hướng phát triển**
- Hàn lên board đục lỗ hoặc làm PCB để mạch chắc chắn hơn.
- Gắn còi hoặc đèn chớp 5V vào relay kênh 2 để có âm thanh to hơn, không cần sửa code.
- Băm PIN bằng SHA-256 có salt (`mbedtls`). Mã hóa đường truyền bằng HTTPS hoặc dùng MQTT qua TLS.
- Chế độ STA để ESP32 vào WiFi nhà. Gửi thông báo Telegram khi có báo động. Mã OTP cho khách.
- Nối relay CH1 với khóa chốt điện 12V (dùng nguồn riêng, mắc thêm diode 1N4007).
