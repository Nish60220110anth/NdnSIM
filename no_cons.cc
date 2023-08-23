#include "ns3/node-container.h"
#include "ns3/node-list.h"
#include "ns3/command-line.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/ptr.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/names.h"

#include "ns3/ndnSIM/helper/ndn-app-helper.hpp"
#include "ns3/ndnSIM/helper/ndn-global-routing-helper.hpp"
#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/ndnSIM/utils/tracers/ndn-l3-rate-tracer.hpp"
#include "ns3/ndnSIM/utils/tracers/ndn-cs-tracer.hpp"
#include "ns3/ndnSIM/utils/tracers/ndn-app-delay-tracer.hpp"

#include <memory>
#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <functional>

/**
 * \brief Simple topology with no consumers. This topology contains only producers.

 * \subsection Why
 *
 * To test the flow of packets in no consumer but with only producers state
 *
 * Topology:
 *           +
 *         /\ __________  /\
 *       / \/             \/ \
 *      /                     \
 *     /                       \
 *    /                         \
 *   /\+                         /\+
 *   \/                          \/
 *     \                       /
 *      \                     /
 *       \                   /
 *        \ /\ __________  /\
 *          \/             \/
 *
 * + => producer
 *
 * Simulation:
 *
 * time:     1 2 ---------- 10
 * producer:   2 ---------- 10
 *
 *
 * complete util functions
 * complete default + with output
 * dynamic update of fib => basic idea
 * equation intersection algo
 * tharak sir explain
 *
*/

class Parameters {
public:
  ~Parameters() = default;
  Parameters(std::string msg);
  Parameters();

  std::string sample;
};

Parameters::Parameters() {

}

Parameters::Parameters(std::string _msg) {
  this->sample = _msg;
}

namespace IO {
  void resetIO() {
    std::cin.tie(&std::cout);
  }

  void setIO() {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);
  }
}

namespace helper {
  std::optional<uint32_t> GetStopTime() {
    return std::optional<uint32_t>(60);
  }

  std::vector<int> GetConsumerIndexes() {
    return { 2,3,4 };
  }

  std::vector<int> GetProducerIndexes() {
    return { 0,1,5 };
  }

  std::vector<std::pair<int, int>> GetConsumerAppTime() {
    return { {0,50},{0,50},{0,50} };
  }

  std::vector<std::string> GetConsumerAppPrefixes() {
    return { "prefix-1","prefix-2","prefix-3" };
  }

  std::vector<ns3::ndn::AppHelper> GetConsumerAppHelpers() {
    std::vector<ns3::ndn::AppHelper> res;

    for (const std::string& str : GetConsumerAppPrefixes()) {
      ns3::ndn::AppHelper app("ns3::ndn::ConsumerCbr");
      app.SetAttribute("Prefix", ns3::StringValue(str));
      app.SetAttribute("Frequency", ns3::DoubleValue(10));
      app.SetAttribute("Randomize", ns3::StringValue("uniform"));

      res.push_back(app);
    }

    return res;
  }

  std::vector<std::pair<int, int>> GetProducerAppTime() {
    return { {1,50},{11,50},{21,50} };
  }

  std::vector<std::string> GetProducerAppPrefixes() {
    return { "prefix-1","prefix-2","prefix-3" };
  }

  std::vector<ns3::ndn::AppHelper> GetProducerAppHelpers() {
    std::vector<ns3::ndn::AppHelper> res;

    for (const std::string& str : GetProducerAppPrefixes()) {
      ns3::ndn::AppHelper app("ns3::ndn::Producer");
      app.SetAttribute("Prefix", ns3::StringValue(str));
      app.SetAttribute("PayloadSize", ns3::UintegerValue(512));
      app.SetAttribute("Postfix", ns3::StringValue("/"));
      app.SetAttribute("Freshness", ns3::TimeValue(ns3::Seconds(0)));

      res.push_back(app);
    }

    return res;
  }
}

namespace run {
  void SetConfig() {

  }

  void SetTopology(std::unique_ptr<Parameters> param) {
    ns3::NodeContainer cont;
    cont.Create(6);

    ns3::NodeContainer prodCont;
    prodCont.Add(cont.Get(0));
    prodCont.Add(cont.Get(1));
    prodCont.Add(cont.Get(5));

    ns3::NodeContainer inCont;
    inCont.Add(cont.Get(2));
    inCont.Add(cont.Get(3));
    inCont.Add(cont.Get(4));

    ns3::PointToPointHelper p2p;
    p2p.Install(cont.Get(0), cont.Get(1));
    p2p.Install(cont.Get(1), cont.Get(3));
    p2p.Install(cont.Get(3), cont.Get(5));
    p2p.Install(cont.Get(0), cont.Get(2));
    p2p.Install(cont.Get(2), cont.Get(4));
    p2p.Install(cont.Get(4), cont.Get(5));

    ns3::ndn::StackHelper inStkHlper, prdStkHlper;
    inStkHlper.setCsSize(1000);
    inStkHlper.SetDefaultRoutes(true);
    inStkHlper.SetLinkDelayAsFaceMetric();
    inStkHlper.setPolicy("nfd::cs::priority_fifo");

    prdStkHlper.setCsSize(1000);
    prdStkHlper.SetDefaultRoutes(true);
    prdStkHlper.SetLinkDelayAsFaceMetric();
    prdStkHlper.setPolicy("nfd::cs::lru");

    inStkHlper.Install(inCont);
    prdStkHlper.Install(prodCont);
  }

  void AddRoutingInfo() {
    using helper::GetProducerAppPrefixes;

    ns3::ndn::GlobalRoutingHelper routingHelper;
    routingHelper.InstallAll();

    std::vector<std::string> prefixes = GetProducerAppPrefixes();
    for (const std::string& str : prefixes) {
      routingHelper.AddOrigin(str, ns3::NodeList::GetNode(0));
      routingHelper.AddOrigin(str, ns3::NodeList::GetNode(1));
      routingHelper.AddOrigin(str, ns3::NodeList::GetNode(5));
    }

    routingHelper.CalculateRoutes();
  }

  void InstallApplication() {
    ns3::NodeContainer prod, cons;

    prod.Add(ns3::NodeList::GetNode(0));
    prod.Add(ns3::NodeList::GetNode(1));
    prod.Add(ns3::NodeList::GetNode(5));

    cons.Add(ns3::NodeList::GetNode(2));
    cons.Add(ns3::NodeList::GetNode(3));
    cons.Add(ns3::NodeList::GetNode(4));

    std::vector<std::pair<int, int>> consTime, prodTime;
    consTime = helper::GetConsumerAppTime();
    prodTime = helper::GetProducerAppTime();

    int i = 0;

    for (ns3::ndn::AppHelper consHlper : helper::GetConsumerAppHelpers()) {
      int start = consTime[i].first, end = consTime[i].second;

      for (ns3::Ptr<ns3::Node>& node : cons) {
        ns3::ApplicationContainer app = consHlper.Install(node);

        int last = app.GetN() - 1;
        app.Get(last)->SetStartTime(ns3::Seconds(start));
        app.Get(last)->SetStopTime(ns3::Seconds(end));
      }

      i++;
    }

    i = 0;

    for (ns3::ndn::AppHelper prodHelper : helper::GetProducerAppHelpers()) {
      int start = prodTime[i].first, end = prodTime[i].second;

      for (ns3::Ptr<ns3::Node>& node : prod) {
        ns3::ApplicationContainer app = prodHelper.Install(node);

        int last = app.GetN() - 1;
        app.Get(last)->SetStartTime(ns3::Seconds(start));
        app.Get(last)->SetStopTime(ns3::Seconds(end));
      }
    }
  }

  void InstallTracers() {
    ns3::ndn::AppDelayTracer::InstallAll("./scratch/no-cons-app-delay.txt");
    ns3::ndn::L3RateTracer::InstallAll("./scratch/no-cons-l3-tracer.txt");
    ns3::ndn::CsTracer::InstallAll("./scratch/no-cons-cs-tracer.txt");
  }

}

int main(int argc, char* argv[]) {
  std::unique_ptr<Parameters> param = std::make_unique<Parameters>();
  ns3::CommandLine cmd;
  cmd.Parse(argc, argv);

  // cmd.PrintHelp(std::cout);
  // cmd.AddValue("psample", param->sample);

  // std::function<uint32_t(std::optional<uint32_t>)> getOptValue = 
  // [](std::optional<uint32_t> t) -> uint32_t {
  //   return 
  //   };

  std::function<void()> periodicPrinter = [&]() -> void {
    std::cout << "10 Seconds completed\n";
    ns3::Simulator::Schedule(ns3::Seconds(10), periodicPrinter);
    };

  run::SetConfig();
  run::SetTopology(std::move(param));
  run::AddRoutingInfo();
  run::InstallApplication();
  run::InstallTracers();

  ns3::Simulator::Stop(ns3::Seconds(helper::GetStopTime().value_or(60)));
  ns3::Simulator::Schedule(ns3::Seconds(10), periodicPrinter);
}

/**
 * 1. one consumer / one producer
 * one prefix
 * measure fib entries getting updated or not , cache content
 * how are interest names are getting different
 *
 * 2. same as before one but with two diff  overlapping applications ,
 * one producer
 *
*/
