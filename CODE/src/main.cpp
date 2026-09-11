#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <LittleFS.h>

// Konfigurasi Wi-Fi
const char* ssid = "Esp32_surya";
const char* password = "surya123";

// Inisialisasi Hardware
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);
Servo servoPintu;

// Pemetaan Pin Hardware
const int PIN_RELAY_MESIN = 18; // Relay 1 (Mesin)
const int PIN_RELAY_PINTU = 19; // Relay 2 (Pintu)
const int PIN_SERVO       = 26; // Servo (GPIO 26)
const int PIN_TRIG        = 12; // Sensor Ultrasonik Trig (GPIO 14)
const int PIN_ECHO        = 14; // Sensor Ultrasonik Echo (GPIO 12)
const int PIN_IR          = 27; // Sensor IR (GPIO 27)

// Variabel Kontrol Sistem
bool prosesBerjalan = false;
bool pintuBisaDibuka = false;

unsigned long waktuMulai = 0;
unsigned long durasiMesinAktif = 20000; // Default 20 detik (ms)
const unsigned long JEDA_PERINGATAN = 5000; // 5 detik jeda awal

// --- VARIABEL IR SCANNER / COUNTER SAMPAH ---
int jumlahSampah = 0;
int statusIRTerakhir = HIGH;
unsigned long waktuDebounceTerakhir = 0;
const unsigned long JEDA_DEBOUNCE = 200; // 200ms untuk mencegah double count

// Logika Relay (Aktif LOW)
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// Fungsi Membaca Jarak dari Sensor Ultrasonik (cm)
float bacaJarak() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long durasi = pulseIn(PIN_ECHO, HIGH, 30000);
  if (durasi == 0) return 0.0;

  return (durasi * 0.034 / 2.0);
}

// Fungsi Scanner IR (Hitung Sampah Masuk)
void updateScannerIR() {
  int statusIR = digitalRead(PIN_IR);

  // Deteksi perubahan dari HIGH ke LOW (Objek melintas)
  if (statusIR == LOW && statusIRTerakhir == HIGH) {
    if ((millis() - waktuDebounceTerakhir) > JEDA_DEBOUNCE) {
      jumlahSampah++;
      waktuDebounceTerakhir = millis();
    }
  }
  statusIRTerakhir = statusIR;
}

// Fungsi Menampilkan Intro Awal pada LCD
void tampilkanIntroLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" PROJEK PLASTIKA");
  lcd.setCursor(0, 1);
  lcd.print("  SYSTEM READY  ");
}

// Fungsi Animasi Teks Berjalan di LCD
void animasiTeks(String teks, int baris, int kecepatan) {
  String pesan = "                " + teks + "                ";
  for (int i = 0; i < pesan.length() - 16; i++) {
    lcd.setCursor(0, baris);
    lcd.print(pesan.substring(i, i + 16));
    delay(kecepatan);
  }
}

// ------------------------------------------------------------------
// UI SEKARANG DILAYANI DARI LittleFS (folder data/), bukan string C++
// tanganiRoot() cuma bertugas STREAMING file index.html dari flash.
// CSS dan JS didaftarkan lewat server.serveStatic() di setup().
// ------------------------------------------------------------------
void tanganiRoot() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "Gagal membuka index.html di LittleFS");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}

// Endpoint status AJAX
void tanganiStatus() {
  int progress = 0;
  String status = "STANDBY";

  float jarak = bacaJarak();

  if (jarak > 0 && jarak <= 20.0) {
    status = "⚠️ SAMPAH KOSONG!";
  } else if (prosesBerjalan) {
    unsigned long berlalu = millis() - waktuMulai;
    unsigned long totalWaktu = JEDA_PERINGATAN + durasiMesinAktif;

    if (berlalu < JEDA_PERINGATAN) {
      status = "⚠️ PERINGATAN: JAUHKAN TANGAN!";
      progress = (berlalu * 100) / totalWaktu;
    } else if (berlalu < totalWaktu) {
      status = "⚙️ MESIN SEDANG AKTIF";
      progress = (berlalu * 100) / totalWaktu;
    }
  } else if (pintuBisaDibuka) {
    status = "✅ SELESAI - BUKA PINTU";
    progress = 100;
  }

  String json = "{\"progress\":" + String(progress) +
                ", \"status\":\"" + status + "\"" +
                ", \"jarak\":" + String(jarak, 1) +
                ", \"jumlah\":" + String(jumlahSampah) +
                ", \"pintu\":" + String(pintuBisaDibuka ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

// Trigger Mulai Mesin
void tanganiStart() {
  if (!prosesBerjalan && !pintuBisaDibuka) {
    if (server.hasArg("timer")) {
      int timerInput = server.arg("timer").toInt();
      if (timerInput > 0) {
        durasiMesinAktif = timerInput * 1000;
      }
    }
    prosesBerjalan = true;
    pintuBisaDibuka = false;
    waktuMulai = millis();
    digitalWrite(PIN_RELAY_MESIN, RELAY_ON);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// Reset Hitungan Sampah
void tanganiResetCounter() {
  jumlahSampah = 0;
  server.sendHeader("Location", "/");
  server.send(303);
}

// Eksekusi Buka Pintu
void BukaPintuAction() {
  if (pintuBisaDibuka) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MEMBUKA PINTU...");

    digitalWrite(PIN_RELAY_PINTU, RELAY_ON);
    delay(200);

    servoPintu.write(180);
    lcd.setCursor(0, 1);
    lcd.print("PINTU TERBUKA   ");
    delay(5000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MENUTUP PINTU...");
    servoPintu.write(0);
    delay(1000);

    digitalWrite(PIN_RELAY_PINTU, RELAY_OFF);
    pintuBisaDibuka = false;

    // Menampilkan IP Address sejenak (3 detik)
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IP Address:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(3000);

    // Kembali ke Tampilan Intro Awal
    tampilkanIntroLCD();
  }
}

void tanganiBukaPintu() {
  BukaPintuAction();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  // Serial Monitor untuk debugging & feedback koneksi
  Serial.begin(115200);

  // Matikan Relay secepat mungkin saat booting
  pinMode(PIN_RELAY_MESIN, OUTPUT);
  pinMode(PIN_RELAY_PINTU, OUTPUT);
  digitalWrite(PIN_RELAY_MESIN, RELAY_OFF);
  digitalWrite(PIN_RELAY_PINTU, RELAY_OFF);

  // Setup Pin Sensor
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_IR, INPUT_PULLUP);

  // Servo Setup
  servoPintu.attach(PIN_SERVO);
  servoPintu.write(0);

  // Setup LCD
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("PROJEK PLASTIKA");
  animasiTeks("SELAMAT DATANG DI PROJEK PLASTIKA", 1, 120);

  // Mount LittleFS (true = format otomatis kalau belum pernah di-mount)
  if (!LittleFS.begin(true)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LittleFS GAGAL!");
    delay(3000);
  }

  // Koneksi WiFi
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  Serial.print("Menghubungkan ke WiFi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
    Serial.print(".");
  }

  // Feedback berhasil terhubung + IP Address ke Serial Monitor
  Serial.println();
  Serial.println("WiFi berhasil terhubung!");
  Serial.print("IP Address ESP32: ");
  Serial.println(WiFi.localIP());

  // Tampilkan IP Address saat pertama kali terhubung
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP Address:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
  delay(3000);

  // Masuk ke tampilan intro awal
  tampilkanIntroLCD();

  // Route Halaman & API
  server.on("/", tanganiRoot);
  server.on("/status", tanganiStatus);
  server.on("/start", tanganiStart);
  server.on("/reset-counter", tanganiResetCounter);
  server.on("/open-door", tanganiBukaPintu);

  // Static assets — file CSS/JS langsung diserve dari LittleFS
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/script.js", LittleFS, "/script.js");

  server.begin();
}

void loop() {
  server.handleClient();

  // Membaca scanner IR secara terus menerus
  updateScannerIR();

  float jarak = bacaJarak();
  unsigned long totalWaktu = JEDA_PERINGATAN + durasiMesinAktif;

  // 1. Kondisi Sampah Kosong (Jarak <= 20 cm)
  if (jarak > 0 && jarak <= 20.0 && !prosesBerjalan && !pintuBisaDibuka) {
    lcd.setCursor(0, 0);
    lcd.print(" STATUS SYSTEM ");
    lcd.setCursor(0, 1);
    lcd.print("SAMPAH KOSONG!  ");
  }
  // 2. Siklus Timer Berjalan
  else if (prosesBerjalan) {
    unsigned long waktuSekarang = millis() - waktuMulai;

    if (waktuSekarang < JEDA_PERINGATAN) {
      lcd.setCursor(0, 0);
      lcd.print("-- PERINGATAN --");
      lcd.setCursor(0, 1);
      lcd.print("JAUHKAN TANGAN! ");
    }
    else if (waktuSekarang < totalWaktu) {
      int sisaWaktu = (totalWaktu - waktuSekarang) / 1000;
      lcd.setCursor(0, 0);
      lcd.print("Sampah: " + String(jumlahSampah) + " Item  ");
      lcd.setCursor(0, 1);
      lcd.print("Timer: " + String(sisaWaktu) + " Detik   ");
    }
    else {
      digitalWrite(PIN_RELAY_MESIN, RELAY_OFF);
      prosesBerjalan = false;
      pintuBisaDibuka = true;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PROSES SELESAI");
      lcd.setCursor(0, 1);
      lcd.print("TOTAL: " + String(jumlahSampah) + " ITEM  ");
    }
  }
}