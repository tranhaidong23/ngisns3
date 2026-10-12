
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

#include <iostream>
#include <iomanip>
#include <cstdint>

using namespace ns3;

int main(int argc, char* argv[])
{
    
    // 1. CẤU HÌNH THÔNG SỐ MÔ PHỎNG
    uint32_t numUavs = 4;             // Số lượng UAV
    double simulationTime = 20.0;     // Thời gian mô phỏng (giây)
    double spacing = 10.0;            // Khoảng cách giữa các UAV (mét)

    // Cho phép thay đổi thông số khi chạy chương trình
    CommandLine cmd;
    cmd.AddValue("simulationTime",
                 "Thoi gian mo phong (giay)",
                 simulationTime);
    cmd.AddValue("spacing",
                 "Khoang cach giua cac UAV (met)",
                 spacing);
    cmd.Parse(argc, argv);





    // 2. KHỞI TẠO CÁC NODE TRONG MẠNG
    // Tạo các node đại diện cho UAV
    NodeContainer uavs;
    uavs.Create(numUavs);

    // Tạo một node đại diện cho trạm điều khiển mặt đất
    NodeContainer gcs;
    gcs.Create(1);

    // Gộp tất cả node để cấu hình mạng
    NodeContainer allNodes;
    allNodes.Add(uavs);
    allNodes.Add(gcs);

   


    // 3. THIẾT LẬP MẠNG WI-FI AD HOC 

    // Sử dụng Wi-Fi 802.11b với tốc độ 1 Mbps
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    wifi.SetRemoteStationManager(
        "ns3::ConstantRateWifiManager",
        "DataMode", StringValue("DsssRate1Mbps"),
        "ControlMode", StringValue("DsssRate1Mbps")
    );

    // Thiết lập kênh truyền không dây
    YansWifiChannelHelper channel =
        YansWifiChannelHelper::Default();

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // Chế độ Ad Hoc: các node giao tiếp không cần Access Point
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    // Gắn thiết bị Wi-Fi cho tất cả node
    NetDeviceContainer devices =
        wifi.Install(phy, mac, allNodes);

    



    // 4. THIẾT LẬP VỊ TRÍ CÁC NODE

    MobilityHelper mobility;

    Ptr<ListPositionAllocator> positions =
        CreateObject<ListPositionAllocator>();

    // Các UAV nằm trên trục X, ở độ cao 10 mét
    for (uint32_t i = 0; i < numUavs; i++)
    {
        positions->Add(
            Vector((i + 1) * spacing, 0.0, 10.0)
        );
    }

    // Trạm GCS đặt tại gốc tọa độ
    positions->Add(Vector(0.0, 0.0, 0.0));

    mobility.SetPositionAllocator(positions);

    // Hiện tại các node đứng yên
    // Sau này có thể cập nhật vị trí từ Gazebo
    mobility.SetMobilityModel(
        "ns3::ConstantPositionMobilityModel"
    );

    mobility.Install(allNodes);

    



    // 5. CẤU HÌNH ĐỊA CHỈ IP

    // Cài đặt giao thức TCP/IP cho các node
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Các node sử dụng cùng mạng 10.20.0.0/24
    Ipv4AddressHelper address;
    address.SetBase(
        "10.20.0.0",
        "255.255.255.0"
    );

    Ipv4InterfaceContainer interfaces =
        address.Assign(devices);

    // Lấy địa chỉ IP của trạm GCS
    Ipv4Address gcsAddress =
        interfaces.GetAddress(numUavs);

   





    // 6. GCS NHẬN DỮ LIỆU TỪ CÁC UAV
    
    // Cổng UDP 3030 dùng để truyền dữ liệu telemetry
    const uint16_t telemetryPort = 3030;

    UdpServerHelper telemetryServer(telemetryPort);

    ApplicationContainer telemetryReceiver =
        telemetryServer.Install(gcs.Get(0));

    telemetryReceiver.Start(Seconds(0.5));
    telemetryReceiver.Stop(Seconds(simulationTime));

    




    // 7. CÁC UAV GỬI DỮ LIỆU VỀ GCS

    for (uint32_t i = 0; i < numUavs; i++)
    {
        // Mỗi UAV gửi dữ liệu đến địa chỉ IP của GCS
        UdpClientHelper client(
            gcsAddress,
            telemetryPort
        );

        // Mỗi UAV gửi tối đa 100 gói
        client.SetAttribute(
            "MaxPackets",
            UintegerValue(100)
        );

        // Khoảng thời gian giữa hai gói là 100 ms
        client.SetAttribute(
            "Interval",
            TimeValue(MilliSeconds(100))
        );

        // Kích thước mỗi gói là 208 byte
        client.SetAttribute(
            "PacketSize",
            UintegerValue(208)
        );

        ApplicationContainer sender =
            client.Install(uavs.Get(i));

        // Các UAV bắt đầu gửi lệch nhau 0,1 giây
        sender.Start(Seconds(1.0 + i * 0.1));
        sender.Stop(Seconds(simulationTime));
    }

    


    // 8. MÔ PHỎNG KÊNH ĐIỀU KHIỂN UAV

    // Cổng UDP 14550 dành cho dữ liệu điều khiển giả lập
    // Chưa sử dụng giao thức MAVLink thực tế
    const uint16_t mavlinkPort = 14550;

    // UAV0 nhận gói điều khiển từ GCS
    UdpServerHelper uavMavlinkServer(mavlinkPort);

    ApplicationContainer uavMavlinkReceiver =
        uavMavlinkServer.Install(uavs.Get(0));

    uavMavlinkReceiver.Start(Seconds(0.5));
    uavMavlinkReceiver.Stop(Seconds(simulationTime));

    // GCS gửi gói điều khiển đến UAV0
    UdpClientHelper gcsControl(
        interfaces.GetAddress(0),
        mavlinkPort
    );

    gcsControl.SetAttribute(
        "MaxPackets",
        UintegerValue(30)
    );

    // Gửi một gói sau mỗi 500 ms
    gcsControl.SetAttribute(
        "Interval",
        TimeValue(MilliSeconds(500))
    );

    gcsControl.SetAttribute(
        "PacketSize",
        UintegerValue(64)
    );

    ApplicationContainer gcsSender =
        gcsControl.Install(gcs.Get(0));

    gcsSender.Start(Seconds(2.0));
    gcsSender.Stop(Seconds(simulationTime));

    



    // 9. LƯU DỮ LIỆU GÓI TIN

    // Tạo file PCAP để phân tích bằng Wireshark
    phy.EnablePcapAll("uav-network");

    


    // 10. CHẠY MÔ PHỎNG
    

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    
    
    // 11. THỐNG KÊ KẾT QUẢ

    // Lấy số gói telemetry mà GCS nhận được
    Ptr<UdpServer> telemetry =
        DynamicCast<UdpServer>(
            telemetryReceiver.Get(0)
        );

    // Lấy số gói điều khiển mà UAV0 nhận được
    Ptr<UdpServer> control =
        DynamicCast<UdpServer>(
            uavMavlinkReceiver.Get(0)
        );

    uint64_t receivedTelemetry =
        telemetry->GetReceived();

    uint64_t receivedControl =
        control->GetReceived();

    // Số gói dự kiến nếu mỗi UAV gửi đủ 100 gói
    uint64_t expectedTelemetry =
        static_cast<uint64_t>(numUavs) * 100;

    // Tính tỷ lệ nhận gói thành công (PDR)
    double pdr = 0.0;

    if (expectedTelemetry > 0)
    {
        pdr = 100.0 * receivedTelemetry
              / expectedTelemetry;
    }

    // Chỉ in các thông số quan trọng
    std::cout << "\n===== KET QUA MO PHONG =====\n";

    std::cout << "Telemetry RX: "
              << receivedTelemetry << std::endl;

    std::cout << "Telemetry PDR: "
              << std::fixed << std::setprecision(2)
              << pdr << "%\n";

    std::cout << "Control RX UAV0: "
              << receivedControl << std::endl;

    // Giải phóng tài nguyên mô phỏng
    Simulator::Destroy();

    return 0;
}
