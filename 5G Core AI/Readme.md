# 📱 Phát hiện bất thường trên dữ liệu mạng viễn thông bằng mô hình Informer

## 🔍 Mô tả
Dự án này sử dụng mô hình **Informer** – một kiến trúc Transformer hiệu quả, để **phát hiện bất thường (anomaly detection)** trong dữ liệu thời gian thực mạng viễn thông (Telecom time-series data).  
Quy trình bao gồm tiền xử lý dữ liệu, tạo các chuỗi thời gian đầu vào, huấn luyện mô hình, đánh giá sai số, và xác định các điểm bất thường dựa trên ngưỡng.

---

## ⚙️ Các bước thực hiện

### 1. 📥 Tải về mã nguồn mô hình
Dự án sử dụng mã nguồn chính thức từ GitHub:
```bash
git clone https://github.com/zhouhaoyi/Informer2020.git
```

---

### 2. 🧹 Tiền xử lý dữ liệu

- **Tạo đặc trưng thời gian**: sinh thêm các cột `month`, `day`, `weekday`, `hour`, `minute` từ cột `date`.
- **Làm sạch dữ liệu**:
  - Chuyển đổi `date` về định dạng chuẩn datetime.
  - Xử lý giá trị khuyết (`NaN`) bằng forward-fill và backward-fill.
- **Chuẩn hóa dữ liệu**:
  - Sử dụng `StandardScaler` để đưa dữ liệu về phân phối chuẩn.
- **Xác định kích thước dữ liệu** và hiển thị thông tin tổng quan.

---

### 3. 🧱 Tạo tập chuỗi đầu vào

- Cắt dữ liệu thành nhiều đoạn chuỗi thời gian:
  - `seq_len = 48` (đầu vào)
  - `label_len = 24` (context)
  - `pred_len = 12` (dự đoán)
- Tập dữ liệu trả về là các tensor `X`, `y` có kích thước:
  ```
  X: (num_samples, 48, num_features)
  y: (num_samples, 12, num_features)
  ```

---

### 4. 🧠 Huấn luyện mô hình Informer

- **Cấu hình mô hình**: `Informer` với các tham số cấu hình chuẩn.
- **Bộ tối ưu**: sử dụng `Adam`, hàm mất mát `MSELoss`.
- **Chia dữ liệu**: train, validation, test.
- **Theo dõi loss** qua từng epoch.
- **Tùy chọn**:
  - Lưu mô hình: `informer_telecom_anomaly_model.pth`
  - Lưu biểu đồ loss: `training_loss.png`

---

### 5. 🚨 Phát hiện bất thường

- **Dự đoán** chuỗi tiếp theo từ đầu vào.
- **Tính toán sai số** giữa dự đoán và thực tế: dùng MSE (reconstruction error).
- **Xác định ngưỡng** bất thường:
  - Ngưỡng được chọn là top 5% giá trị cao nhất của anomaly score.
- **Trả về kết quả**:
  - Dự đoán (`predictions`)
  - Sai số (`anomaly_scores`)
  - Các điểm bất thường (`anomalies`)
  - Ngưỡng (`threshold`)

---

## 📊 Visualization

- Biểu đồ training loss theo thời gian
- Biểu đồ anomaly score theo thời gian
- Phân bố bất thường theo giờ trong ngày
- Lưu kết quả ra file `.xlsx` gồm:
  - Kết quả chính
  - Thống kê
  - Top 20 điểm bất thường

---

## 📂 Lưu ý

- Chạy trên Google Colab để tương tác và upload dữ liệu dễ dàng.
- Dữ liệu yêu cầu phải có cột `date` và các chỉ số mạng (ví dụ: `BEARER_MME_UTIL`, `THROUGHPUT_4G`, ...).
- File đầu vào phải là `.csv`.

---

## 🧑‍💻 Tác giả

Phạm Chí Vỹ - Đại học Bách Khoa Hà Nội - Viettel Digital Talent 2025 (5G)
