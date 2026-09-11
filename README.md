# Projek Plastika — Struktur Eksplisit

## Struktur folder
```
project/
├── platformio.ini
├── src/
│   └── main.cpp        <- HANYA logic embedded (sensor, relay, servo, API)
└── data/                <- di-upload ke LittleFS ESP32
    ├── index.html
    ├── style.css
    └── script.js
```

## Cara pakai (PlatformIO — VSCode extension)

1. Buka folder `project/` ini sebagai PlatformIO project di VSCode.
2. Upload firmware seperti biasa:
   - `PlatformIO: Upload` (ikon panah di toolbar bawah, atau `pio run --target upload`)
3. **Penting — langkah tambahan khusus untuk file di `data/`:**
   - Upload filesystem-nya secara terpisah:
     `PlatformIO: Upload Filesystem Image` (ada di menu PlatformIO sidebar → Platform → Upload Filesystem Image)
     atau lewat terminal: `pio run --target uploadfs`
   - Ini yang mem-flash `index.html`, `style.css`, `script.js` ke LittleFS di ESP32.
4. Setiap kali UI/UX developer mengubah file di `data/`, cukup jalankan ulang
   `uploadfs` — **tidak perlu** re-upload firmware (`main.cpp`) sama sekali.
5. Sebaliknya, kalau logic sensor/relay di `main.cpp` berubah, cukup upload
   firmware biasa — file di `data/` tidak akan tersentuh.

## Kalau masih pakai Arduino IDE

Arduino IDE juga bisa upload LittleFS lewat plugin
"ESP32 Sketch Data Upload", tapi PlatformIO jauh lebih direkomendasikan untuk
kasus kamu karena:
- Struktur folder eksplisit dari awal (tidak perlu naruh `data/` manual di sebelah `.ino`)
- `platformio.ini` mengunci versi library (reproducible build) — penting untuk kolaborasi di GitHub
- Command `uploadfs` terintegrasi baik tanpa plugin tambahan

## Kontrak API antara firmware dan frontend

UI/UX developer bebas mendesain ulang `index.html`/`style.css`/`script.js`
selama endpoint berikut tetap dipatuhi (ini "kontrak" antara dua sisi):

| Endpoint          | Method | Fungsi                                      |
|-------------------|--------|----------------------------------------------|
| `/`               | GET    | Serve `index.html`                           |
| `/style.css`      | GET    | Serve CSS statis                             |
| `/script.js`      | GET    | Serve JS statis                              |
| `/status`         | GET    | JSON: `progress`, `status`, `jarak`, `jumlah`, `pintu` |
| `/start?timer=N`  | GET    | Mulai mesin dengan durasi N detik            |
| `/reset-counter`  | GET    | Reset hitungan sampah ke 0                   |
| `/open-door`      | GET    | Buka pintu mesin                             |