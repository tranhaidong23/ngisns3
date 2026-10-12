uav mophong
## Yêu cầu

- Docker Desktop hoặc Docker Engine
- Git (nếu muốn clone repository)

## Hướng dẫn chạy

### 1. Tải mã nguồn

Clone repository từ GitHub 

###  Build Docker Image

```bash
docker build -t uav-ns3:v1 .
```

### Chạy mô phỏng

```bash
docker run --rm uav-ns3:v1
```

## Kết quả mô phỏng mẫu

```text
===== KET QUA MO PHONG =====
Telemetry RX: 300
Telemetry PDR: 75.00%
Control RX UAV0: 30