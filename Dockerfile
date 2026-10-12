
# ============================================
# 1. CHUẨN BỊ MÔI TRƯỜNG UBUNTU
# ============================================

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Cài đặt các công cụ cần thiết cho ns-3
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    python3 \
    git \
    wget \
    ca-certificates \
    bzip2 \
    libboost-dev \
    && rm -rf /var/lib/apt/lists/*

# ============================================
# 2. TẢI MÃ NGUỒN NS-3
# ============================================

WORKDIR /opt

# Tải và giải nén ns-3 phiên bản 3.45
RUN wget https://www.nsnam.org/releases/ns-allinone-3.45.tar.bz2 \
    && tar -xjf ns-allinone-3.45.tar.bz2 \
    && mv ns-allinone-3.45/ns-3.45 /opt/ns-3 \
    && rm -rf ns-allinone-3.45 ns-allinone-3.45.tar.bz2

# ============================================
# 3. ĐƯA CODE UAV VÀO NS-3
# ============================================

WORKDIR /opt/ns-3

# Sao chép chương trình UAV vào thư mục scratch
COPY scratch/uavns3.cc /opt/ns-3/scratch/uavns3.cc

# ============================================
# 4. BIÊN DỊCH CHƯƠNG TRÌNH
# ============================================

# Chỉ biên dịch các module cần cho mạng UAV
RUN ./ns3 configure \
    --disable-examples \
    --disable-tests \
    --enable-modules=core,network,internet,mobility,wifi,applications \
    && ./ns3 build -j 2

# ============================================
# 5. CHẠY MÔ PHỎNG
# ============================================

CMD ["./ns3", "run", "uavns3", "--no-build"]
