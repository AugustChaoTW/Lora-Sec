#include "loramesh-routing.h"

#include <algorithm>

namespace lorasec {

LoraMeshRouting::LoraMeshRouting(const TopologySpec& topology,
                                 SecurityMode securityMode,
                                 uint32_t helloPeriodSec,
                                 uint32_t metricVersionWindowSec)
    : m_topology(topology),
      m_securityMode(securityMode),
      m_helloPeriodSec(helloPeriodSec),
      m_metricVersionWindowSec(metricVersionWindowSec),
      m_nodes(topology.nodeCount)
{
}

void
LoraMeshRouting::NodeInit(uint32_t seed)
{
    m_rng.seed(seed);
    for (uint32_t node = 0; node < m_topology.nodeCount; ++node)
    {
        m_nodes[node].compromised = false;
        m_nodes[node].helloSeq = 1;
        m_nodes[node].metricVersion = 0;
        m_nodes[node].routeTable.clear();
        m_nodes[node].routeTable.emplace(node,
                                         RouteEntry{node,
                                                    0,
                                                    node,
                                                    m_nodes[node].helloSeq,
                                                    m_nodes[node].metricVersion});
    }
}

std::vector<HelloMessage>
LoraMeshRouting::HelloBroadcast(uint32_t currentSec)
{
    std::vector<HelloMessage> output;
    if (m_helloPeriodSec == 0 || currentSec % m_helloPeriodSec != 0)
    {
        return output;
    }

    for (uint32_t node = 0; node < m_topology.nodeCount; ++node)
    {
        m_nodes[node].helloSeq += 1;
        m_nodes[node].metricVersion = currentSec / std::max<uint32_t>(1, m_metricVersionWindowSec);

        auto selfIt = m_nodes[node].routeTable.find(node);
        if (selfIt != m_nodes[node].routeTable.end())
        {
            selfIt->second.metric = 0;
            selfIt->second.nextHop = node;
            selfIt->second.seq = m_nodes[node].helloSeq;
            selfIt->second.metricVersion = m_nodes[node].metricVersion;
        }

        for (uint32_t neigh : m_topology.adjacency[node])
        {
            const auto routes = m_nodes[node].routeTable;
            for (const auto& kv : routes)
            {
                const auto& entry = kv.second;
                HelloMessage hello{entry.destination,
                                   node,
                                   entry.metric,
                                   entry.seq,
                                   RequiresAuthentication(),
                                   entry.metricVersion};
                output.push_back(hello);
                RouteUpdate(neigh, hello, currentSec);
            }
        }
    }
    return output;
}

void
LoraMeshRouting::RouteUpdate(uint32_t receiver, const HelloMessage& hello, uint32_t currentSec)
{
    if (!IsNeighbor(receiver, hello.sender))
    {
        return;
    }

    if (!VerifyHello(hello, receiver, currentSec))
    {
        return;
    }

    uint32_t newMetric = hello.metric + 1;
    auto& table = m_nodes[receiver].routeTable;
    auto it = table.find(hello.claimedSrc);

    if (it == table.end())
    {
        table.emplace(hello.claimedSrc,
                      RouteEntry{hello.claimedSrc,
                                 newMetric,
                                 hello.sender,
                                 hello.seq,
                                 hello.metricVersion});
        return;
    }

    bool better = false;
    if (hello.seq > it->second.seq)
    {
        better = true;
    }
    else if (hello.seq == it->second.seq && newMetric < it->second.metric)
    {
        better = true;
    }

    if (better)
    {
        it->second.metric = newMetric;
        it->second.nextHop = hello.sender;
        it->second.seq = hello.seq;
        it->second.metricVersion = hello.metricVersion;
    }
}

bool
LoraMeshRouting::DataForward(uint32_t from, uint32_t destination, uint32_t& nextHop, uint32_t& hopMetric) const
{
    const auto& table = m_nodes[from].routeTable;
    auto it = table.find(destination);
    if (it == table.end())
    {
        return false;
    }
    nextHop = it->second.nextHop;
    hopMetric = it->second.metric;
    return true;
}

void
LoraMeshRouting::Compromise(uint32_t nodeId)
{
    if (nodeId < m_nodes.size())
    {
        m_nodes[nodeId].compromised = true;
    }
}

const TopologySpec&
LoraMeshRouting::GetTopology() const
{
    return m_topology;
}

const std::map<uint32_t, RouteEntry>&
LoraMeshRouting::GetRouteTable(uint32_t node) const
{
    return m_nodes[node].routeTable;
}

uint32_t
LoraMeshRouting::GetRouteTableSize(uint32_t node) const
{
    return static_cast<uint32_t>(m_nodes[node].routeTable.size());
}

bool
LoraMeshRouting::IsCompromised(uint32_t nodeId) const
{
    return m_nodes[nodeId].compromised;
}

uint32_t
LoraMeshRouting::GetHelloSeq(uint32_t nodeId) const
{
    return m_nodes[nodeId].helloSeq;
}

uint32_t
LoraMeshRouting::GetMetricVersion(uint32_t nodeId) const
{
    return m_nodes[nodeId].metricVersion;
}

bool
LoraMeshRouting::VerifyHello(const HelloMessage& hello, uint32_t receiver, uint32_t currentSec) const
{
    if (RequiresAuthentication() && !hello.authenticated)
    {
        return false;
    }

    if (!RequiresMetricVersion())
    {
        return true;
    }

    auto table = m_nodes[receiver].routeTable;
    auto it = table.find(hello.claimedSrc);
    if (it != table.end())
    {
        if (hello.seq < it->second.seq)
        {
            return false;
        }

        if (hello.seq == it->second.seq && hello.metricVersion < it->second.metricVersion)
        {
            return false;
        }
    }

    (void) currentSec;
    return true;
}

bool
LoraMeshRouting::RequiresAuthentication() const
{
    return m_securityMode != SecurityMode::BASELINE;
}

bool
LoraMeshRouting::RequiresMetricVersion() const
{
    return m_securityMode == SecurityMode::METRICVERSION;
}

bool
LoraMeshRouting::IsNeighbor(uint32_t a, uint32_t b) const
{
    const auto& neigh = m_topology.adjacency[a];
    return std::find(neigh.begin(), neigh.end(), b) != neigh.end();
}

TopologySpec
BuildLinearTopology()
{
    TopologySpec t;
    t.type = TopologyType::LINEAR;
    t.nodeCount = 6;
    t.adjacency.assign(t.nodeCount, {});
    for (uint32_t i = 0; i + 1 < t.nodeCount; ++i)
    {
        t.adjacency[i].push_back(i + 1);
        t.adjacency[i + 1].push_back(i);
    }
    return t;
}

TopologySpec
BuildTreeTopology()
{
    TopologySpec t;
    t.type = TopologyType::TREE;
    t.nodeCount = 8;
    t.adjacency.assign(t.nodeCount, {});
    std::vector<std::pair<uint32_t, uint32_t>> edges = {
        {0, 1}, {0, 2}, {1, 3}, {1, 4}, {2, 5}, {4, 6}, {5, 7}};
    for (const auto& edge : edges)
    {
        t.adjacency[edge.first].push_back(edge.second);
        t.adjacency[edge.second].push_back(edge.first);
    }
    return t;
}

TopologySpec
BuildGridTopology()
{
    TopologySpec t;
    t.type = TopologyType::GRID;
    t.nodeCount = 9;
    t.adjacency.assign(t.nodeCount, {});
    auto id = [](uint32_t r, uint32_t c) { return r * 3 + c; };

    for (uint32_t r = 0; r < 3; ++r)
    {
        for (uint32_t c = 0; c < 3; ++c)
        {
            uint32_t n = id(r, c);
            if (r > 0)
            {
                t.adjacency[n].push_back(id(r - 1, c));
            }
            if (r < 2)
            {
                t.adjacency[n].push_back(id(r + 1, c));
            }
            if (c > 0)
            {
                t.adjacency[n].push_back(id(r, c - 1));
            }
            if (c < 2)
            {
                t.adjacency[n].push_back(id(r, c + 1));
            }
        }
    }
    return t;
}

}
