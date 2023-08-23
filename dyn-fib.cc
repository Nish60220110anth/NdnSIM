#include "ns3/node-container.h"
#include "ns3/node-list.h"
#include "ns3/command-line.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/ptr.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/names.h"
#include "ns3/ndnSIM/model/ndn-common.hpp"
#include "ns3/topology-read-module.h"
#include "ns3/global-value.h"

#include "ns3/ndnSIM/helper/ndn-app-helper.hpp"
#include "ns3/ndnSIM/helper/ndn-global-routing-helper.hpp"
#include "ns3/ndnSIM/helper/ndn-stack-helper.hpp"
#include "ns3/ndnSIM/utils/topology/annotated-topology-reader.hpp"
#include "ns3/ndnSIM/utils/tracers/ndn-l3-rate-tracer.hpp"
#include "ns3/ndnSIM/utils/tracers/ndn-cs-tracer.hpp"
#include "ns3/ndnSIM/utils/tracers/ndn-app-delay-tracer.hpp"
#include "ns3/ndnSIM/utils/tracers/l2-rate-tracer.hpp"

#include <memory>
#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <functional>

#include <cstdlib>
#include <linux/limits.h>

/**
 * \brief Simple topology to check whether cache miss / hit
 * can occur properly.
 *
*/

void AddNamesToAllNodes() {
  int c = 0;
  for (ns3::NodeList::Iterator i = ns3::NodeList::Begin(); i != ns3::NodeList::End(); i++) {
    ns3::Names::Add("Node-" + std::to_string(c++), (*i)->GetObject<ns3::Node>());
  }
}

void PrintAllNodeInfo() {
  for (ns3::NodeList::Iterator i = ns3::NodeList::Begin(); i != ns3::NodeList::End(); i++) {
    ns3::Ptr<ns3::Node> ptr = *i;
    std::cout << ptr->GetId() << ") " << ns3::Names::FindName(ptr->GetObject<ns3::Node>()) << "\n";
  }
}

int main(int argc, char* argv[]) {
  ns3::CommandLine cmd;
  // cmd.PrintHelp(std::cout);
  cmd.Parse(argc, argv);

  std::string topoFile = "scratch/dyn-fib-topology.txt";

  ns3::AnnotatedTopologyReader topoReader("", 40);
  topoReader.SetFileName(topoFile);
  topoReader.Read();

  ns3::NodeContainer nodes = topoReader.GetNodes();
  ns3::NodeContainer consCont, prodCont, intCont;

  std::set<int> consIndex, prodIndex;
  consIndex = { 1,2,3 }, prodIndex = { 18,19,21,22,23 };
  // 18: 3  19: 2  21: 1  22: 1  23: 2

  // Add consumer nodes to consumer container
  for (const int& i : consIndex) {
    consCont.Add(nodes.Get(i - 1));
  }

  // Add producer nodes to producer container
  for (const int& i : prodIndex) {
    prodCont.Add(nodes.Get(i - 1));
  }

  // Add intermediate nodes
  for (int i = 1;i <= 24;i++) {
    if (consIndex.find(i) == consIndex.end() && prodIndex.find(i) == prodIndex.end()) {
      intCont.Add(nodes.Get(i - 1));
    }
  }

  // Creating topology in hand just to make changes easily
  ns3::PointToPointHelper p2p;

  ns3::ndn::StackHelper cStkHlper, pStkHlper, iStkHlper;
  cStkHlper.SetDefaultRoutes(true);
  cStkHlper.setCsSize(1);
  cStkHlper.setPolicy("nfd::cs::lru");

  pStkHlper.SetDefaultRoutes(true);
  pStkHlper.setCsSize(1000);
  pStkHlper.setPolicy("nfd::cs::lru");

  iStkHlper.SetDefaultRoutes(true);
  iStkHlper.setCsSize(1000);
  iStkHlper.setPolicy("nfd::cs::lru");

  cStkHlper.Install(consCont);
  pStkHlper.Install(prodCont);
  iStkHlper.Install(intCont);

  ns3::ndn::GlobalRoutingHelper routingHelper;
  routingHelper.InstallAll();

  routingHelper.AddOrigin("prefix-3", nodes.Get(18 - 1));
  routingHelper.AddOrigin("prefix-2", nodes.Get(19 - 1));
  routingHelper.AddOrigin("prefix-1", nodes.Get(21 - 1));
  routingHelper.AddOrigin("prefix-1", nodes.Get(22 - 1));
  routingHelper.AddOrigin("prefix-2", nodes.Get(23 - 1));

  ns3::ndn::StrategyChoiceHelper::InstallAll("prefix-1", "/localhost/nfd/strategy/best-route");
  ns3::ndn::StrategyChoiceHelper::InstallAll("prefix-2", "/localhost/nfd/strategy/best-route");
  ns3::ndn::StrategyChoiceHelper::InstallAll("prefix-3", "/localhost/nfd/strategy/best-route");

  ns3::ndn::AppHelper consHelper("ns3::ndn::ConsumerCbr"), prodHelper("ns3::ndn::Producer");

  consHelper.SetAttribute("Randomize", ns3::StringValue("exponential"));
  consHelper.SetAttribute("Frequency", ns3::DoubleValue(10));
  consHelper.SetAttribute("StartSeq", ns3::IntegerValue(1));
  consHelper.SetAttribute("LifeTime", ns3::TimeValue(ns3::Seconds(1)));

  prodHelper.SetPrefix("prefix-1");
  prodHelper.SetAttribute("Freshness", ns3::TimeValue(ns3::Seconds(0)));
  prodHelper.SetAttribute("Postfix", ns3::ndn::NameValue("/"));
  prodHelper.SetAttribute("PayloadSize", ns3::UintegerValue(1024));

  ns3::ApplicationContainer prodAppCont = prodHelper.Install(prodCont.Get(2));
  prodAppCont.Start(ns3::Seconds(0)), prodAppCont.Stop(ns3::Seconds(50));

  prodAppCont = prodHelper.Install(prodCont.Get(3));
  prodAppCont.Start(ns3::Seconds(0)), prodAppCont.Stop(ns3::Seconds(50));

  prodHelper.SetPrefix("prefix-2");

  prodAppCont = prodHelper.Install(prodCont.Get(1));
  prodAppCont.Start(ns3::Seconds(0)), prodAppCont.Stop(ns3::Seconds(50));

  prodAppCont = prodHelper.Install(prodCont.Get(4));
  prodAppCont.Start(ns3::Seconds(0)), prodAppCont.Stop(ns3::Seconds(50));

  prodHelper.SetPrefix("prefix-3");

  prodAppCont = prodHelper.Install(prodCont.Get(0));
  prodAppCont.Start(ns3::Seconds(0)), prodAppCont.Stop(ns3::Seconds(50));

  // Only prefix - 1
  std::initializer_list<std::tuple<int, int, int>> consumerAppNodeTime = {
    std::make_tuple(1,1,15),
    std::make_tuple(2,16,30),
    std::make_tuple(3,31,45) };

  consHelper.SetPrefix("prefix-1");

  routingHelper.CalculateAllPossibleRoutes();

  ns3::ndn::L3RateTracer::InstallAll("./scratch/dyn-fib-l3ratetrace.txt");
  ns3::ndn::CsTracer::InstallAll("./scratch/dyn-fib-cstrace.txt",ns3::Seconds(2));
  ns3::ndn::AppDelayTracer::InstallAll("./scratch/dyn-fib-appdelaytrace.txt");
  
  ns3::GlobalValue::Bind("SimulatorImplementationType", ns3::StringValue("ns3::DistributedSimulatorImpl"));

  ns3::Simulator::Stop(ns3::Seconds(50));
  ns3::Simulator::Run();
  ns3::Simulator::Destroy();
}
