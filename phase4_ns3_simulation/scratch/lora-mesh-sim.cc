#include <cmath>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>

#include "ns3/command-line.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"

#include "../src/attack/attacker-node.h"
#include "../src/routing/loramesh-routing.h"

using namespace ns3;
using namespace lorasec;

namespace {

struct SimMetrics
{
    uint32_t generated{0};
    uint32_t delivered{0};
    uint32_t dropped{0};
    uint32_t droppedBySelective{0};
    uint32_t reroutedViaAttacker{0};
    uint64_t latencyMsTotal{0};
    std::vector<uint32_t> routeTableTotalBySample;
    std::vector<uint32_t> sampleTimesSec;
};

std::string
ToLower(std::string s)
{
    for (auto& ch : s)
    {
        if (ch >= 'A' && ch <= 'Z')
        {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return s;
}

TopologySpec
ParseTopology(const std::string& input)
{
    std::string t = ToLower(input);
    if (t == "linear")   { return BuildLinearTopology(); }
    if (t == "tree")     { return BuildTreeTopology(); }
    if (t == "linear25") { return BuildLinear25Topology(); }
    if (t == "grid49")   { return BuildGrid49Topology(); }
    return BuildGridTopology();
}

AttackType
ParseAttackType(const std::string& input)
{
    std::string a = ToLower(input);
    if (a == "spoofing")
    {
        return AttackType::SPOOFING;
    }
    if (a == "replay")
    {
        return AttackType::REPLAY;
    }
    if (a == "selective")
    {
        return AttackType::SELECTIVE;
    }
    return AttackType::NONE;
}

SecurityMode
ParseSecurityMode(const std::string& input)
{
    std::string mode = ToLower(input);
    if (mode == "patched")
    {
        return SecurityMode::PATCHED;
    }
    if (mode == "metricversion")
    {
        return SecurityMode::METRICVERSION;
    }
    return SecurityMode::BASELINE;
}

std::string
SecurityModeName(SecurityMode mode)
{
    switch (mode)
    {
    case SecurityMode::PATCHED:
        return "patched";
    case SecurityMode::METRICVERSION:
        return "metricversion";
    case SecurityMode::BASELINE:
    default:
        return "baseline";
    }
}

bool
UsesPatch(SecurityMode mode)
{
    return mode != SecurityMode::BASELINE;
}

bool
UsesMetricVersion(SecurityMode mode)
{
    return mode == SecurityMode::METRICVERSION;
}

uint32_t
DefaultVictim(const TopologySpec& topology)
{
    switch (topology.type)
    {
    case TopologyType::LINEAR:   return 5;
    case TopologyType::TREE:     return 7;
    case TopologyType::LINEAR25: return 24;
    case TopologyType::GRID49:   return 48;
    default:                     return 8;
    }
}

uint32_t
DefaultAttacker(const TopologySpec& topology)
{
    switch (topology.type)
    {
    case TopologyType::LINEAR:   return 2;
    case TopologyType::TREE:     return 2;
    case TopologyType::LINEAR25: return 8;   // ~1/3 into 25-node chain
    case TopologyType::GRID49:   return 24;  // centre of 7×7 grid
    default:                     return 4;
    }
}

std::vector<std::pair<uint32_t, uint32_t>>
TrafficPairs(const TopologySpec& topology)
{
    switch (topology.type)
    {
    case TopologyType::LINEAR:
        return {{0, 5}};
    case TopologyType::TREE:
        return {{0, 3}, {0, 6}, {0, 7}};
    case TopologyType::LINEAR25:
        return {{0, 24}};
    case TopologyType::GRID49:
        return {{0, 48}, {6, 42}, {42, 6}};
    default:
        return {{0, 8}, {2, 6}, {6, 2}};
    }
}

bool
StepForwardPacket(const LoraMeshRouting& routing,
                  AttackerNode& attacker,
                  const AttackConfig& config,
                  DataPacket packet,
                  SimMetrics& metrics,
                  uint32_t nowSec)
{
    std::set<uint32_t> visited;
    uint32_t current = packet.src;
    uint32_t hopCount = 0;
    bool rerouted = false;

    while (current != packet.dst)
    {
        if (visited.count(current) > 0)
        {
            metrics.dropped += 1;
            return false;
        }
        visited.insert(current);

        uint32_t nextHop = current;
        uint32_t hopMetric = 0;
        if (!routing.DataForward(current, packet.dst, nextHop, hopMetric))
        {
            metrics.dropped += 1;
            return false;
        }

        if (nextHop == config.attackerNode)
        {
            rerouted = true;
        }

        if (current == config.attackerNode && config.type == AttackType::SELECTIVE)
        {
            if (attacker.AttackC_SelectiveForwarding())
            {
                metrics.dropped += 1;
                metrics.droppedBySelective += 1;
                return false;
            }
        }

        hopCount += 1;
        if (hopCount > routing.GetTopology().nodeCount + 2)
        {
            metrics.dropped += 1;
            return false;
        }
        current = nextHop;
    }

    metrics.delivered += 1;
    metrics.latencyMsTotal += static_cast<uint64_t>(hopCount) * 50;
    if (rerouted)
    {
        metrics.reroutedViaAttacker += 1;
    }
    return true;
}

std::string
BuildJson(const std::string& topology,
          const std::string& attack,
          SecurityMode securityMode,
          uint32_t seed,
          uint32_t durationSec,
          const SimMetrics& metrics,
          uint32_t payloadBytes)
{
    double pdr = metrics.generated == 0 ? 0.0 : static_cast<double>(metrics.delivered) / metrics.generated;
    double pdrPercent = pdr * 100.0;
    double throughputPktPerSec = durationSec == 0 ? 0.0 : static_cast<double>(metrics.delivered) / durationSec;
    double throughputBps = durationSec == 0 ? 0.0 : (metrics.delivered * payloadBytes * 8.0) / durationSec;
    double avgLatencyMs = metrics.delivered == 0 ? 0.0 : static_cast<double>(metrics.latencyMsTotal) / metrics.delivered;
    double attackSuccess = metrics.generated == 0
                               ? 0.0
                               : (100.0 * static_cast<double>(metrics.reroutedViaAttacker) / metrics.generated);
    double selectiveSuccess = (metrics.generated == 0)
                                  ? 0.0
                                  : (100.0 * static_cast<double>(metrics.droppedBySelective) / metrics.generated);

    std::ostringstream os;
    os << std::fixed << std::setprecision(3);
    os << "{\n";
    os << "  \"topology\": \"" << topology << "\",\n";
    os << "  \"attack_type\": \"" << attack << "\",\n";
    os << "  \"state\": \"" << SecurityModeName(securityMode) << "\",\n";
    os << "  \"use_patch\": " << (UsesPatch(securityMode) ? "true" : "false") << ",\n";
    os << "  \"use_metric_version\": " << (UsesMetricVersion(securityMode) ? "true" : "false") << ",\n";
    os << "  \"seed\": " << seed << ",\n";
    os << "  \"duration_seconds\": " << durationSec << ",\n";
    os << "  \"packets_generated\": " << metrics.generated << ",\n";
    os << "  \"packets_delivered\": " << metrics.delivered << ",\n";
    os << "  \"packets_dropped\": " << metrics.dropped << ",\n";
    os << "  \"pdr\": " << pdr << ",\n";
    os << "  \"pdr_percent\": " << pdrPercent << ",\n";
    os << "  \"throughput_pkt_per_sec\": " << throughputPktPerSec << ",\n";
    os << "  \"throughput_bps\": " << throughputBps << ",\n";
    os << "  \"avg_latency_ms\": " << avgLatencyMs << ",\n";
    os << "  \"attack_success_redirect_percent\": " << attackSuccess << ",\n";
    os << "  \"attack_success_selective_drop_percent\": " << selectiveSuccess << ",\n";
    os << "  \"route_table_size_timeseries\": [\n";
    for (size_t i = 0; i < metrics.sampleTimesSec.size(); ++i)
    {
        os << "    {\"t\": " << metrics.sampleTimesSec[i] << ", \"total_entries\": "
           << metrics.routeTableTotalBySample[i] << "}";
        if (i + 1 < metrics.sampleTimesSec.size())
        {
            os << ",";
        }
        os << "\n";
    }
    os << "  ]\n";
    os << "}\n";
    return os.str();
}

} 

int
main(int argc, char* argv[])
{
    std::string topologyName = "linear";
    std::string attackName = "spoofing";
    std::string securityState = "baseline";
    std::string outputPath = "results/result.json";
    bool usePatch = false;
    bool useMetricVersion = false;
    uint32_t seed = 1;
    uint32_t durationSec = 300;
    uint32_t helloPeriodSec = 10;
    uint32_t samplePeriodSec = 10;
    uint32_t payloadBytes = 64;
    uint32_t dataStartSec = 0;  // 0 = auto-compute from topology diameter

    CommandLine cmd;
    cmd.AddValue("topology", "linear|tree|grid|linear25|grid49", topologyName);
    cmd.AddValue("attack", "spoofing|replay|selective|none", attackName);
    cmd.AddValue("state", "baseline|patched|metricversion", securityState);
    cmd.AddValue("usePatch", "0/1", usePatch);
    cmd.AddValue("useMetricVersion", "0/1", useMetricVersion);
    cmd.AddValue("seed", "deterministic seed", seed);
    cmd.AddValue("duration", "seconds", durationSec);
    cmd.AddValue("helloPeriod", "seconds", helloPeriodSec);
    cmd.AddValue("samplePeriod", "seconds", samplePeriodSec);
    cmd.AddValue("payloadBytes", "application payload bytes", payloadBytes);
    cmd.AddValue("dataStart", "seconds before data traffic begins (0=auto)", dataStartSec);
    cmd.AddValue("output", "json output path", outputPath);
    cmd.Parse(argc, argv);

    RngSeedManager::SetSeed(seed);

    SecurityMode securityMode = ParseSecurityMode(securityState);
    if (useMetricVersion)
    {
        securityMode = SecurityMode::METRICVERSION;
    }
    else if (usePatch)
    {
        securityMode = SecurityMode::PATCHED;
    }

    TopologySpec topology = ParseTopology(topologyName);
    NodeContainer nodes;
    nodes.Create(topology.nodeCount);

    AttackConfig config;
    config.type = ParseAttackType(attackName);
    config.attackerNode = DefaultAttacker(topology);
    config.victimNode = DefaultVictim(topology);
    config.frequencySec = helloPeriodSec;
    config.selectiveDropRate = 0.5;

    LoraMeshRouting routing(topology, securityMode, helloPeriodSec, 20);
    routing.NodeInit(seed + 11);
    routing.Compromise(config.attackerNode);

    AttackerNode attacker(config, seed + 101);
    SimMetrics metrics;
    uint32_t packetId = 0;
    auto trafficPairs = TrafficPairs(topology);

    // Auto-compute convergence wait from topology diameter × hello period
    if (dataStartSec == 0)
    {
        uint32_t diameter = topology.nodeCount;   // conservative upper bound
        if (topology.type == TopologyType::GRID || topology.type == TopologyType::GRID49)
        {
            uint32_t side = static_cast<uint32_t>(std::sqrt(static_cast<double>(topology.nodeCount)) + 0.5);
            diameter = 2 * (side - 1);
        }
        dataStartSec = diameter * helloPeriodSec + helloPeriodSec;
    }
    
    for (uint32_t sec = 1; sec <= durationSec; ++sec)
    {
        Simulator::Schedule(Seconds(sec), [sec, &routing]() { routing.HelloBroadcast(sec); });

        // Start data traffic only after route convergence
        if (sec >= dataStartSec)
        {
        Simulator::Schedule(Seconds(sec),
                            [sec, &routing, &attacker, &config, securityMode]() {
                                if (!attacker.ShouldAttackAt(sec))
                                {
                                    return;
                                }

                                for (uint32_t receiver = 0; receiver < routing.GetTopology().nodeCount; ++receiver)
                                {
                                    if (receiver == config.attackerNode)
                                    {
                                        continue;
                                    }

                                    if (config.type == AttackType::SPOOFING)
                                    {
                                        auto msg = attacker.AttackA_MetricSpoofing(sec,
                                                                                    routing.GetHelloSeq(config.victimNode),
                                                                                    securityMode);
                                        if (msg.has_value())
                                        {
                                            routing.RouteUpdate(receiver, msg.value(), sec);
                                        }
                                    }
                                    if (config.type == AttackType::REPLAY)
                                    {
                                        auto msg = attacker.AttackB_MetricReplay(sec,
                                                                                 routing.GetHelloSeq(config.victimNode),
                                                                                 routing.GetMetricVersion(config.victimNode),
                                                                                 securityMode);
                                        if (msg.has_value())
                                        {
                                            routing.RouteUpdate(receiver, msg.value(), sec);
                                        }
                                    }
                                }
                            });

        Simulator::Schedule(Seconds(sec),
                            [sec,
                             &metrics,
                             &trafficPairs,
                             &routing,
                             &attacker,
                             &config,
                             &packetId,
                             payloadBytes]() {
                                auto pair = trafficPairs[(sec - 1) % trafficPairs.size()];
                                DataPacket p{packetId++, pair.first, pair.second, sec, payloadBytes};
                                metrics.generated += 1;
                                StepForwardPacket(routing, attacker, config, p, metrics, sec);
                            });
        }

        if (samplePeriodSec > 0 && sec % samplePeriodSec == 0)
        {
            Simulator::Schedule(Seconds(sec), [sec, &routing, &metrics]() {
                uint32_t total = 0;
                for (uint32_t i = 0; i < routing.GetTopology().nodeCount; ++i)
                {
                    total += routing.GetRouteTableSize(i);
                }
                metrics.sampleTimesSec.push_back(sec);
                metrics.routeTableTotalBySample.push_back(total);
            });
        }
    }

    Simulator::Stop(Seconds(durationSec + 1));
    Simulator::Run();
    Simulator::Destroy();

    std::ofstream out(outputPath);
    out << BuildJson(topologyName, attackName, securityMode, seed, durationSec, metrics, payloadBytes);
    out.close();

    return 0;
}
