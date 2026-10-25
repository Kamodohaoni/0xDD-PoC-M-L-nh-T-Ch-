# MÁY ẢO SYM-VM — phiên bản 1.0

Đây là **máy ảo của riêng bạn**: một trình chạy mã lệnh viết bằng C, chỉ hiểu đúng
**ngôn ngữ lệnh SYM** kiểu tiếng Việt mà bạn (hoặc AI) tự viết ra.

Tệp chương trình có đuôi `.sym`, bắt buộc có dòng chữ ký đầu tiên:

```
%SYM 1
```

> Chỉ `mayao.exe` đọc được tệp này. Mở bằng bất kỳ phần mềm khác → máy ảo báo lỗi
> *"đây KHÔNG PHAI tệp mã SYM"*.

---

## Chạy chương trình

```
mayao vidu.sym
```

## Biên dịch lại (khi sửa `mayao.c`)

Chạy file **`bien_dich.bat`** (cần Visual Studio Build Tools đã cài sẵn ở `D:\VSPRODUCT`).
Vì máy bạn có VS Build Tools nhưng `cl` chưa vào PATH, file bat sẽ tự nạp môi trường trước.

---

## MÃ HEX BÍ MẬT — tệp `.symx` 🕵️

Bạn viết chương trình bằng lệnh tiếng Việt thường (`.sym`), rồi **mã hóa** thành
tệp nhị phân bí mật mà **chỉ máy ảo SYM-VM giải mã được**:

```
mayao -bien vidu.sym vidu.symx     (mã hóa)
mayao vidu.symx                    (chạy tệp bí mật)
```

Cấu trúc tệp `.symx`:
- 4 byte chữ ký `SYMX` (53 59 4D 58)
- 1 byte phiên bản (1)
- 4 byte số kiểm tra (checksum) — **tệp bị sửa dù 1 byte là máy ảo từ chối chạy**
- Phần còn lại: mã lệnh đã **XOR hóa bằng khóa bí mật** — xem bằng hex editor
  cũng chỉ thấy các byte loạn, không đọc ra lệnh nào cả.

Máy ảo cũng tự nhận biết: dùng `vidu.sym` hay `vidu.symx` đều được, không cần biết
tham số dạng nào.

---

## Các lệnh SYM

| Lệnh | Ý nghĩa | Ví dụ |
|------|---------|-------|
| `DAT <biến> <giá trị>` | Tạo/gán biến (số hoặc chuỗi) | `DAT tuoi 18` |
| `IN <giá trị>` | In ra rồi xuống dòng | `IN nhiet` |
| `INN <giá trị>` | In, không xuống dòng | `INN "Tong: "` |
| `HOI <biến>` | Nhập số từ bàn phím | `HOI tuoi` |
| `CONG <biến> <so>` | biến = biến + so | `CONG diem 5` |
| `TRU <biến> <so>` | biến = biến − so | `TRU diem 1` |
| `NHAN <biến> <so>` | biến = biến × so | `NHAN tien 2` |
| `CHIA <biến> <so>` | biến = biến ÷ so (chia lấy phần nguyên) | `CHIA tien 3` |
| `NHAY @nhan` | Nhảy vô điều kiện | `NHAY @ket_thuc` |
| `NEU <a> <so_sánh> <b> NHAY @nhan` | Nhảy nếu đúng | `NEU tuoi LON_HON 17 NHAY @lon` |
| `@nhan` | Đặt nhãn (nằm riêng một dòng) | `@lon` |
| `DUNG` | Kết thúc chương trình | `DUNG` |

So sánh có sẵn: `BANG` `KHONG_BANG` `LON_HON` `NHO_HON` `LON_HON_BANG` `NHO_HON_BANG`

Khác:
- Chú thích bắt đầu bằng `#` — bỏ qua tới hết dòng.
- Chuỗi đặt trong dấu nháy `"..."`, cho phép `"Xin chao"` có dấu cách, hỗ trợ `\n` `\t` `\\` `\"`.
- Lệnh không phân biệt hoa/thường (`in` == `IN`).
- Chia cho 0, lệnh lạ, nhãn không tồn tại, dùng biến chưa tạo → báo lỗi **kèm số dòng**.
- Giới hạn 5.000.000 bước để tránh treo máy vì vòng lặp vô hạn.

> Lưu ý hiển thị: cửa sổ cmd mặc định không hiển thị dấu tiếng Việt trong chuỗi
> `"..."`. Muốn có dấu, chạy `chcp 65001` trước khi chạy máy ảo.

---

## Ví dụ `vidu.sym`

```
%SYM 1

IN "May ao SYM khoi dong!"
DAT hon 10
INN "Hon ban dau = "
IN hon

NHAY @tiep

IN "Dong nay bi nhay qua, khong bao gio chay."

@tiep
HOI tuoi
NEU tuoi LON_HON 17 NHAY @lon
IN "Ban la tre em."
NHAY @xong

@lon
IN "Ban la nguoi truong thanh."

@xong
CONG tuoi 1
IN "Mot nam nua tuoi ban tren hon la: "
IN tuoi

DUNG
```

---

## Cấu trúc dự án

| File | Vai trò |
|------|---------|
| `mayao.c` | Toàn bộ mã nguồn máy ảo (C, 1 file duy nhất) |
| `mayao.exe` | Máy ảo đã biên dịch — trình chạy mã lệnh của bạn |
| `vidu.sym` | Chương trình SYM mẫu |
| `bien_dich.bat` | Biên dịch lại máy ảo bằng cl (VS Build Tools) |
| `HUONG_DAN.md` | Tài liệu này |

## Ý tưởng tiếp theo (nếu bạn muốn)

- Thêm vòng lặp `LAP`, `HAM`/`GOI` (hàm con).
- Giao diện đồ hoạ đơn giản.
- Trình soạn giao diện viết lệnh SYM riêng của bạn.
