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
    if (t == "linear")
    {
        return BuildLinearTopology();
    }
    if (t == "tree")
    {
        return BuildTreeTopology();
    }
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

uint32_t
DefaultVictim(const TopologySpec& topology)
{
    if (topology.type == TopologyType::LINEAR)
    {
        return 5;
    }
    if (topology.type == TopologyType::TREE)
    {
        return 7;
    }
    return 8;
}

uint32_t
DefaultAttacker(const TopologySpec& topology)
{
    if (topology.type == TopologyType::LINEAR)
    {
        return 2;
    }
    if (topology.type == TopologyType::TREE)
    {
        return 2;
    }
    return 4;
}

std::vector<std::pair<uint32_t, uint32_t>>
TrafficPairs(const TopologySpec& topology)
{
    if (topology.type == TopologyType::LINEAR)
    {
        return {{0, 5}};
    }
    if (topology.type == TopologyType::TREE)
    {
        return {{0, 3}, {0, 6}, {0, 7}};
    }
    return {{0, 8}, {2, 6}, {6, 2}};
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
          bool usePatch,
          uint32_t seed,
          uint32_t durationSec,
          const SimMetrics& metrics,
          uint32_t payloadBytes)
{
    double pdr = metrics.generated == 0 ? 0.0 : static_cast<double>(metrics.delivered) / metrics.generated;
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
    os << "  \"use_patch\": " << (usePatch ? "true" : "false") << ",\n";
    os << "  \"seed\": " << seed << ",\n";
    os << "  \"duration_seconds\": " << durationSec << ",\n";
    os << "  \"packets_generated\": " << metrics.generated << ",\n";
    os << "  \"packets_delivered\": " << metrics.delivered << ",\n";
    os << "  \"packets_dropped\": " << metrics.dropped << ",\n";
    os << "  \"pdr\": " << pdr << ",\n";
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
    std::string outputPath = "results/result.json";
    bool usePatch = false;
    uint32_t seed = 1;
    uint32_t durationSec = 300;
    uint32_t helloPeriodSec = 10;
    uint32_t samplePeriodSec = 10;
    uint32_t payloadBytes = 64;

    CommandLine cmd;
    cmd.AddValue("topology", "linear|tree|grid", topologyName);
    cmd.AddValue("attack", "spoofing|replay|selective|none", attackName);
    cmd.AddValue("usePatch", "0/1", usePatch);
    cmd.AddValue("seed", "deterministic seed", seed);
    cmd.AddValue("duration", "seconds", durationSec);
    cmd.AddValue("helloPeriod", "seconds", helloPeriodSec);
    cmd.AddValue("samplePeriod", "seconds", samplePeriodSec);
    cmd.AddValue("payloadBytes", "application payload bytes", payloadBytes);
    cmd.AddValue("output", "json output path", outputPath);
    cmd.Parse(argc, argv);

    RngSeedManager::SetSeed(seed);

    TopologySpec topology = ParseTopology(topologyName);
    NodeContainer nodes;
    nodes.Create(topology.nodeCount);

    AttackConfig config;
    config.type = ParseAttackType(attackName);
    config.attackerNode = DefaultAttacker(topology);
    config.victimNode = DefaultVictim(topology);
    config.frequencySec = helloPeriodSec;
    config.selectiveDropRate = 0.5;

    LoraMeshRouting routing(topology, usePatch, helloPeriodSec, 20);
    routing.NodeInit(seed + 11);
    routing.Compromise(config.attackerNode);

    AttackerNode attacker(config, seed + 101);
    SimMetrics metrics;
    uint32_t packetId = 0;
    auto trafficPairs = TrafficPairs(topology);

    // Delay data traffic start to allow route convergence (linear: 60s, tree: 100s, grid: 80s)
    uint32_t dataStartSec = 120;
    
    for (uint32_t sec = 1; sec <= durationSec; ++sec)
    {
        Simulator::Schedule(Seconds(sec), [sec, &routing]() { routing.HelloBroadcast(sec); });

        // Start data traffic only after route convergence
        if (sec >= dataStartSec)
        {
        Simulator::Schedule(Seconds(sec),
                            [sec, &routing, &attacker, &config, usePatch]() {
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
                                                                                    usePatch);
                                        if (msg.has_value())
                                        {
                                            routing.RouteUpdate(receiver, msg.value(), sec);
                                        }
                                    }
                                    if (config.type == AttackType::REPLAY)
                                    {
                                        auto msg = attacker.AttackB_MetricReplay(sec,
                                                                                 routing.GetHelloSeq(config.victimNode),
                                                                                 sec / 20,
                                                                                 usePatch);
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
    out << BuildJson(topologyName, attackName, usePatch, seed, durationSec, metrics, payloadBytes);
    out.close();

    return 0;
}
