#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/seq-ts-header.h"

#include <iostream>
#include <iomanip>
#include <cstdint>
#include <map>
#include <limits>
#include <cmath>

using namespace ns3;

// Thống kê độ trễ của các gói Telemetry nhận thành công.
struct TelemetryStats
{
    uint64_t received = 0;
    double sumDelayMs = 0.0;
    double minDelayMs = std::numeric_limits<double>::infinity();
    double maxDelayMs = 0.0;
    double sumJitterMs = 0.0;
    uint64_t jitterSamples = 0;
    // Ghi nhớ độ trễ cuối cùng theo IP nguồn để không trộn gói của các UAV.
    std::map<Ipv4Address, double> previousDelayMs;
};

static TelemetryStats g_stats;

// PacketSink nhận gói UDP; UdpClient của ns-3 gắn SeqTsHeader khi gửi.
static void OnTelemetryReceived(Ptr<const Packet> packet, const Address& from)
{
    Ptr<Packet> copy = packet->Copy();
    SeqTsHeader header;
    if (copy->GetSize() < header.GetSerializedSize())
    {
        return;
    }
    copy->RemoveHeader(header);

    double delayMs = (Simulator::Now() - header.GetTs()).GetSeconds() * 1000.0;
    if (delayMs < 0.0)
    {
        return;
    }

    ++g_stats.received;
    g_stats.sumDelayMs += delayMs;
    g_stats.minDelayMs = std::min(g_stats.minDelayMs, delayMs);
    g_stats.maxDelayMs = std::max(g_stats.maxDelayMs, delayMs);

    if (InetSocketAddress::IsMatchingType(from))
    {
        Ipv4Address source = InetSocketAddress::ConvertFrom(from).GetIpv4();
        auto previous = g_stats.previousDelayMs.find(source);
        if (previous != g_stats.previousDelayMs.end())
        {
            g_stats.sumJitterMs += std::abs(delayMs - previous->second);
            ++g_stats.jitterSamples;
        }
        g_stats.previousDelayMs[source] = delayMs;
    }
}

int main(int argc, char* argv[])
{
    // 1. Cấu hình thông số mô phỏng
    uint32_t numUavs = 4;
    double simulationTime = 20.0;
    double spacing = 10.0;

    CommandLine cmd;
    cmd.AddValue("simulationTime", "Thoi gian mo phong (giay)", simulationTime);
    cmd.AddValue("spacing", "Khoang cach giua cac UAV (met)", spacing);
    cmd.Parse(argc, argv);

    // 2. Khởi tạo 4 UAV và một trạm điều khiển GCS
    NodeContainer uavs;
    uavs.Create(numUavs);
    NodeContainer gcs;
    gcs.Create(1);
    NodeContainer allNodes;
    allNodes.Add(uavs);
    allNodes.Add(gcs);

    // 3. Thiết lập mạng Wi-Fi Ad Hoc, 802.11b 1 Mbps
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("DsssRate1Mbps"),
                                 "ControlMode", StringValue("DsssRate1Mbps"));
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, allNodes);

    // 4. Thiết lập vị trí cố định (UAV ở độ cao 10 m)
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numUavs; ++i)
    {
        positions->Add(Vector((i + 1) * spacing, 0.0, 10.0));
    }
    positions->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // 5. Gán địa chỉ IP
    InternetStackHelper internet;
    internet.Install(allNodes);
    Ipv4AddressHelper address;
    address.SetBase("10.20.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);
    Ipv4Address gcsAddress = interfaces.GetAddress(numUavs);

    // 6. GCS nhận Telemetry qua UDP/3030
    // Dùng PacketSink để đọc SeqTsHeader và tính thời gian truyền của từng gói.
    const uint16_t telemetryPort = 3030;
    PacketSinkHelper telemetryServer("ns3::UdpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), telemetryPort));
    ApplicationContainer telemetryReceiver = telemetryServer.Install(gcs.Get(0));
    telemetryReceiver.Start(Seconds(0.5));
    telemetryReceiver.Stop(Seconds(simulationTime));
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(telemetryReceiver.Get(0));
    sink->TraceConnectWithoutContext("Rx", MakeCallback(&OnTelemetryReceived));

    // 7. Mỗi UAV gửi tối đa 100 gói Telemetry, mỗi 100 ms
    for (uint32_t i = 0; i < numUavs; ++i)
    {
        UdpClientHelper client(gcsAddress, telemetryPort);
        client.SetAttribute("MaxPackets", UintegerValue(100));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
        client.SetAttribute("PacketSize", UintegerValue(208));
        ApplicationContainer sender = client.Install(uavs.Get(i));
        sender.Start(Seconds(1.0 + i * 0.1));
        sender.Stop(Seconds(simulationTime));
    }

    // 8. GCS gửi gói điều khiển giả lập qua UDP/14550 đến UAV0
    // Chưa tích hợp giao thức MAVLink thực tế hoặc Bridge.
    const uint16_t mavlinkPort = 14550;
    UdpServerHelper uavMavlinkServer(mavlinkPort);
    ApplicationContainer uavMavlinkReceiver = uavMavlinkServer.Install(uavs.Get(0));
    uavMavlinkReceiver.Start(Seconds(0.5));
    uavMavlinkReceiver.Stop(Seconds(simulationTime));

    UdpClientHelper gcsControl(interfaces.GetAddress(0), mavlinkPort);
    gcsControl.SetAttribute("MaxPackets", UintegerValue(30));
    gcsControl.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
    gcsControl.SetAttribute("PacketSize", UintegerValue(64));
    ApplicationContainer gcsSender = gcsControl.Install(gcs.Get(0));
    gcsSender.Start(Seconds(2.0));
    gcsSender.Stop(Seconds(simulationTime));

    // 9. Lưu PCAP để phân tích bằng Wireshark
    phy.EnablePcapAll("uav-network");

    // 10. Chạy mô phỏng
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // 11. Thống kê kết quả
    Ptr<UdpServer> control = DynamicCast<UdpServer>(uavMavlinkReceiver.Get(0));
    uint64_t receivedTelemetry = g_stats.received;
    uint64_t receivedControl = control->GetReceived();
    uint64_t expectedTelemetry = static_cast<uint64_t>(numUavs) * 100;
    double pdr = expectedTelemetry > 0
                     ? 100.0 * receivedTelemetry / expectedTelemetry
                     : 0.0;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n===== KET QUA MO PHONG =====\n";
    std::cout << "Telemetry RX: " << receivedTelemetry << '\n';
    std::cout << "Telemetry PDR: " << pdr << "%\n";
    std::cout << "Control RX UAV0: " << receivedControl << '\n';

    std::cout << "\n===== THONG KE DO TRE TELEMETRY =====\n";
    if (receivedTelemetry > 0)
    {
        std::cout << "Average Delay: " << g_stats.sumDelayMs / receivedTelemetry << " ms\n";
        std::cout << "Min Delay: " << g_stats.minDelayMs << " ms\n";
        std::cout << "Max Delay: " << g_stats.maxDelayMs << " ms\n";
        if (g_stats.jitterSamples > 0)
        {
            std::cout << "Average Jitter: "
                      << g_stats.sumJitterMs / g_stats.jitterSamples << " ms\n";
        }
        else
        {
            std::cout << "Average Jitter: N/A (khong du goi)\n";
        }
    }
    else
    {
        std::cout << "Khong co goi Telemetry nao duoc nhan.\n";
    }

    Simulator::Destroy();
    return 0;
}