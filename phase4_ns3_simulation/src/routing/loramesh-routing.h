#ifndef LORAMESH_ROUTING_H
#define LORAMESH_ROUTING_H

#include <map>
#include <random>
#include <vector>

#include "../common/sim-types.h"

namespace lorasec {

class LoraMeshRouting
{
  public:
    LoraMeshRouting(const TopologySpec& topology,
                    SecurityMode securityMode,
                    uint32_t helloPeriodSec,
                    uint32_t metricVersionWindowSec);

    void NodeInit(uint32_t seed);
    std::vector<HelloMessage> HelloBroadcast(uint32_t currentSec);
    void RouteUpdate(uint32_t receiver, const HelloMessage& hello, uint32_t currentSec);
    bool DataForward(uint32_t from, uint32_t destination, uint32_t& nextHop, uint32_t& hopMetric) const;
    void Compromise(uint32_t nodeId);

    const TopologySpec& GetTopology() const;
    const std::map<uint32_t, RouteEntry>& GetRouteTable(uint32_t node) const;
    uint32_t GetRouteTableSize(uint32_t node) const;
    bool IsCompromised(uint32_t nodeId) const;
    uint32_t GetHelloSeq(uint32_t nodeId) const;
    uint32_t GetMetricVersion(uint32_t nodeId) const;

  private:
    struct NodeState
    {
        bool compromised{false};
        uint32_t helloSeq{0};
        uint32_t metricVersion{0};
        std::map<uint32_t, RouteEntry> routeTable;
    };

    bool VerifyHello(const HelloMessage& hello, uint32_t receiver, uint32_t currentSec) const;
    bool RequiresAuthentication() const;
    bool RequiresMetricVersion() const;
    bool IsNeighbor(uint32_t a, uint32_t b) const;

    TopologySpec m_topology;
    SecurityMode m_securityMode;
    uint32_t m_helloPeriodSec;
    uint32_t m_metricVersionWindowSec;
    std::vector<NodeState> m_nodes;
    std::mt19937 m_rng;
};

TopologySpec BuildLinearTopology();
TopologySpec BuildTreeTopology();
TopologySpec BuildGridTopology();
TopologySpec BuildLinear25Topology();
TopologySpec BuildGrid49Topology();

}

#endif
