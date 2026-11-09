[简体中文](https://github.com/xland/ScreenCapture/) | [English](./ReadMe.en-US.md) | [Русский](./ReadMe.ru-RU.md) | Bahasa Indonesia

![banner](./banner.png)

**ScreenCapture** Alat penangkap layar Windows yang powerful dan ringan.

## Fitur

- Screenshot, anotasi, screenshot panjang (scroll), perekaman layar (GIF/MP4), pengenalan teks (OCR), pengenalan kode QR.
- Pipet warna dengan pintasan keyboard untuk menyalin warna RGB (`Ctrl+R`), HEX (`Ctrl+H`), dan CMYK (`Ctrl+K`).
- Menggambar elips, lingkaran sempurna (tahan `Shift`), persegi panjang, persegi (tahan `Shift`), panah, label bernomor, dll.
- Menggambar kurva, garis lurus (tahan `Shift`), mosaik, penghapus, teks.
- Mengubah atau menghapus elemen yang telah digambar kapan saja (arahkan kursor mouse pada elemen).
- Urungkan (`Ctrl+Z`), ulangi (`Ctrl+Y`), simpan ke file (`Ctrl+S`), simpan ke clipboard (`Ctrl+C` atau klik dua kali).
- Performa cepat dengan penggunaan memori minimal.
- Ukuran kecil, hanya satu file eksekusi, tanpa instalasi, tidak memerlukan pustaka tautan dinamis (kecuali pengenalan teks).
- Mendukung berbagai argumen baris perintah untuk langsung menjalankan fungsi tertentu.
- Mendukung mode sekali pakai (proses tidak akan terus berjalan di sistem).
- Dukungan multibahasa.

## Unduh

[Release](https://github.com/xland/ScreenCapture/releases/) (1MB)

## Sistem Operasi yang Didukung

- Windows 10 1607 atau yang lebih baru

## Kompilasi

- Branch main bergantung pada kerangka GUI [Ling](https://github.com/xland/Ling).
- Proyek ini dapat dikompilasi dengan Visual Studio 2026 (dengan C++ Desktop Dev Kit terpasang).
- [2.4.25 (berbasis D2D)](https://github.com/xland/ScreenCapture/tree/2.4.25) atau [2.3.3 (berbasis Qt)](https://github.com/xland/ScreenCapture/tree/2.3.3_qt) adalah branch stabil sebelumnya.

## Baris Perintah

```
// Hentikan proses segera setelah pengambilan gambar selesai.
> ScreenCapture.exe --auto-quit=true

// Lewati bilah alat setelah area dipilih dan langsung masuk ke fungsi yang ditentukan:
// long = tangkapan bergulir (screenshot panjang)
> ScreenCapture.exe --enter=long
// video = perekaman layar
> ScreenCapture.exe --enter=video
// ocr = pengenalan teks
> ScreenCapture.exe --enter=ocr
// qr = pengenalan kode QR
> ScreenCapture.exe --enter=qr

// Kedua argumen bisa digabung, misalnya: tanpa ikon tray, proses langsung keluar setelah screenshot panjang selesai.
> ScreenCapture.exe --enter=long --auto-quit=true
```

## Plugin Pengenalan Teks (OCR)

OCR offline bawaan ([PP-OCRv6](https://github.com/PaddlePaddle/PaddleOCR), model small dengan inferensi statis ncnn, mendukung bahasa Tionghoa, Inggris, dan Jepang). Tidak perlu lagi mengunduh plugin.

- OCR berjalan otomatis di latar belakang begitu area dipilih. Klik ikon OCR pada toolbar untuk masuk ke mode pemilihan teks: kursor berubah menjadi I-beam di atas teks yang dikenali, tahan tombol kiri mouse dan seret untuk memilih, lalu salin melalui `Ctrl+C` atau menu konteks. Tekan `ESC` atau klik ikon sekali lagi untuk keluar.
- Jendela pin juga mendukungnya: OCR dimulai otomatis saat pin dibuat; klik tombol OCR pada toolbarnya untuk masuk ke mode pemilihan teks.
- File model dibaca dari sub-direktori `ocr` di sebelah ScreenCapture.exe (det.param/det.bin/rec.param/rec.bin/dict.txt). Skrip konversi lihat `Tools/build_ocr_models.py`; model juga dapat disematkan ke dalam exe dengan makro `OCR_EMBED_MODELS` saat kompilasi.

## Sponsor

<table>
  <tr>
    <td align="center">
      <img alt="Sponsor Alipay" src="./alipay.jpg" width="160" height="160">
      <p>Sponsor Alipay</p>
    </td>
    <td align="center">
      <img alt="Sponsor WeChat" src="./wechat.png" width="160" height="160">
      <p>Sponsor WeChat</p>
    </td>
    <td align="center">
      <img alt="WeChat Penulis" src="./author.jpg" width="160" height="160">
      <p>WeChat Penulis</p>
    </td>
    <td align="center">
      <img alt="QR Code Akun Resmi" src="./gongzhonghao.jpg" width="160" height="160">
      <p>Blog WeChat: Desktop Software</p>
    </td>
  </tr>
</table>
