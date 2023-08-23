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
#include "ns3/ndnSIM/utils/tracers/custom-cs-tracer.hpp"
#include "ns3/ndnSIM/utils/tracers/custom-fib-tracer.hpp"

#include <memory>
#include <iostream>
#include <vector>
#include <string>
#include <optional>
#include <functional>

#include <cstdlib>
#include <linux/limits.h>


/**
 * Topology:
 *
 *
 * Brief:
 *
 *
 * Simulation:
 *
 *
 * Problem:
 *
 * What is the source of problem or when the problem occurs ?
 *
 * 1. When there is more than 2 applications installed in node ( either consumer or producer )
 * is producing error. ( Not even getting installed )?
 *
 *  > The error is generated at line no. 205 -- 221 ( just do debug ) but at consumer
 * Set Randomzier function.
 *
 * 2. Sometimes just changing the topology diminshes the  error. WHy ?
 *
 * 3. Other sample codes provided also generated exceptions but those are not the same as
 * we get here ( check test_1 ?) ( Where did it got error ? It got error after running )
 *
 * Resolved all these above errors just be commenting the  max setting line in consumer-cbr.
 *
 * But it introduced new error ? (Oops ! Fortunalty that error is already occur
 *  at the sampe code provided in the  official website )
 *
*/
// #define DO_LOG

#ifdef DO_LOG
#define print(x) std::cout << x 
#else
#define print(x)
#endif 

void CallAtEnd() {
  print("CallAtEnd");
}

/**
 * \brief Displays info about all nodes [Name IsNdnInstalled CsCsize PolicyUsed]
 *
*/
void ShowAllNodeInfo(std::shared_ptr<std::ostream> os) {
#define PRINTER(x) for(const std::string &s: x) {*os << s <<"\t"; }

  ns3::Ptr<ns3::Node> m_ptr;
  ns3::Ptr<ns3::ndn::L3Protocol> m_l3p;

  std::initializer_list<std::string> header = { "Id","Name","Ndn?","CsSize","Policy" };
  PRINTER(header); *os << "\n\n";

  for (ns3::NodeList::Iterator i = ns3::NodeList::Begin(); i != ns3::NodeList::End(); ++i) {
    m_ptr = *i;

    if (m_ptr->GetObject<ns3::ndn::L3Protocol>() != 0) {
      m_l3p = m_ptr->GetObject<ns3::ndn::L3Protocol>();
      const ns3::ndn::nfd::Cs& m_cs = m_l3p->getForwarder()->getCs();
      header = { std::to_string(m_ptr->GetId()) , ns3::Names::FindName(m_ptr), "Y",
      std::to_string(m_cs.getLimit()),std::string(m_cs.getPolicy()->getName()) };
    }
    else {
      header = { std::to_string(m_ptr->GetId()),ns3::Names::FindName(m_ptr),"N","0","None" };
    }

    PRINTER(header); *os << "\n";
  }
}

/**
 * \brief Show info all applications installed in a node [Id StartTime StopTime Prefix LifeTime RetxTimer Randomize Frequency]
 *
*/
void ShowConsumerAppInfoOnNode(ns3::Ptr<ns3::Node> m_ptr) {
  int totalCount = m_ptr->GetNApplications();
  ns3::Ptr<ns3::Application> m_app;

  std::cout << "Id" << "\t" <<
    "StartTime" << "\t" <<
    "StopTime" << "\t" <<
    "Prefix" << "\t\t" <<
    "LifeTime" << "\t" <<
    "RetxTimer" << "\t" <<
    "Randomize" << "\t" <<
    "Frequency" << "\n";

#define PRINTER(i,st,et,prf,lft,rtx,rdm,frq) std::cout << i << "\t" \ 
  << st.Get().GetSeconds() << "\t\t" \
    << et.Get().GetSeconds() << "\t\t" \
    << prf.Get().toUri() << "\t" \
    << lft.Get().GetSeconds() << "\t\t" \
    << rtx.Get().GetSeconds() << "\t\t" \
    << rdm.Get() << "\t\t" \
    << frq.Get();

  for (int i = 0;i < totalCount;i++) {
    m_app = m_ptr->GetApplication(static_cast<uint32_t>(i));
    ns3::TimeValue startTime, stopTime;
    m_app->GetAttribute("StartTime", startTime);
    m_app->GetAttribute("StopTime", stopTime);

    ns3::ndn::NameValue prefix;
    m_app->GetAttribute("Prefix", prefix);

    ns3::TimeValue lifeTime;
    m_app->GetAttribute("LifeTime", lifeTime);

    ns3::TimeValue retxTime;
    m_app->GetAttribute("RetxTimer", retxTime);

    ns3::StringValue randomize;
    m_app->GetAttribute("Randomize", randomize);

    ns3::DoubleValue frequency;
    m_app->GetAttribute("Frequency", frequency);

    PRINTER(i, startTime, stopTime, prefix, lifeTime, retxTime, randomize, frequency);std::cout << "\n";
  }
}

int main(int argc, char* argv[]) {
  ns3::CommandLine cmd;
  uint8_t freq; // freqency of custom consumer
  cmd.AddValue<uint8_t>("freq", "frequency of the consumer", freq);
  cmd.PrintHelp(std::cout);
  cmd.Parse(argc, argv);

  std::string topoFileName = "./scratch/scene_1_topology.txt";
  ns3::AnnotatedTopologyReader topoReader("", 40);
  topoReader.SetFileName(topoFileName);
  topoReader.SetMobilityModel("ns3::ConstantPositionMobilityModel"); // constant non grid based mobility model
  topoReader.Read();

  ns3::NodeContainer nodes = topoReader.GetNodes(); // Get nodes in node container

  ns3::ndn::StackHelper stackHelper; // Helper to install ndn stack in ns3 nodes
  stackHelper.SetDefaultRoutes(true); // enable default faces in fib routes
  stackHelper.setCsSize(1); // prefer to use 1 instead of zero to make no use of content store
  stackHelper.setPolicy("nfd::cs::lru");

  // Install stack in consumer node 
  stackHelper.Install(nodes.Get(0));

  // Install stack in producer node
  stackHelper.Install(nodes.Get(8));

  // Install stack in all intermediate nodes
  stackHelper.setCsSize(1000);
  int totalCount = nodes.GetN();

  for (int i = 0;i < totalCount;i++) {
    if (i != 0 && i != 8) {
      stackHelper.Install(nodes.Get(i));
    }
  }

  // Worst idea, after calling the function, the variable is released 
  // So std::cout is released
  // So call empty function on free, thus std::cout is not affected
  ShowAllNodeInfo(std::shared_ptr<std::ostream>(&std::cout, std::bind([]() {})));

  // Add routing helper to help nodes to fill fib table entries
  ns3::ndn::GlobalRoutingHelper routingHelper;
  routingHelper.InstallAll(); // Install in all nodes as we are sure that all nodes have ndn stack installed


  // Set strategy choice on all nodes
  // access strategy is a dynamic update strategy
  ns3::ndn::StrategyChoiceHelper::InstallAll("prefix_1", "/localhost/nfd/strategy/access");
  ns3::ndn::StrategyChoiceHelper::InstallAll("prefix_2", "/localhost/nfd/strategy/access");

  ns3::ndn::AppHelper consAppHelper("ns3::ndn::ConsumerCbr"), prodAppHelper("ns3::ndn::Producer");

  prodAppHelper.SetAttribute("Freshness", ns3::TimeValue(ns3::Seconds(0)));
  prodAppHelper.SetAttribute("Postfix", ns3::ndn::NameValue("/"));
  prodAppHelper.SetAttribute("StartTime", ns3::TimeValue(ns3::Seconds(1)));
  prodAppHelper.SetAttribute("StopTime", ns3::TimeValue(ns3::Seconds(50)));
  prodAppHelper.SetAttribute("PayloadSize", ns3::UintegerValue(1024));


  // Add origins information to routing
  routingHelper.AddOrigin("prefix_2", nodes.Get(4));
  routingHelper.AddOrigin("prefix_2", nodes.Get(6));
  routingHelper.AddOrigin("prefix_1", nodes.Get(8));

  consAppHelper.SetAttribute("Randomize", ns3::StringValue("uniform"));
  consAppHelper.SetAttribute("Frequency", ns3::DoubleValue(freq));
  consAppHelper.SetAttribute("LifeTime", ns3::TimeValue(ns3::Seconds(1)));
  consAppHelper.SetAttribute("StartSeq", ns3::IntegerValue(1));

  consAppHelper.SetAttribute("StartTime", ns3::TimeValue(ns3::Seconds(1)));
  consAppHelper.SetAttribute("StopTime", ns3::TimeValue(ns3::Seconds(10)));
  consAppHelper.SetPrefix("prefix_1");
  consAppHelper.Install(nodes.Get(0));

  consAppHelper.SetAttribute("StartTime", ns3::TimeValue(ns3::Seconds(11)));
  consAppHelper.SetAttribute("StopTime", ns3::TimeValue(ns3::Seconds(20)));
  consAppHelper.SetPrefix("prefix_2");
  consAppHelper.Install(nodes.Get(1));

  consAppHelper.SetAttribute("StartTime", ns3::TimeValue(ns3::Seconds(21)));
  consAppHelper.SetAttribute("StopTime", ns3::TimeValue(ns3::Seconds(30)));
  consAppHelper.SetPrefix("prefix_1");
  consAppHelper.Install(nodes.Get(2));

  consAppHelper.SetAttribute("StartTime", ns3::TimeValue(ns3::Seconds(31)));
  consAppHelper.SetAttribute("StopTime", ns3::TimeValue(ns3::Seconds(40)));
  consAppHelper.SetPrefix("prefix_2");
  consAppHelper.Install(nodes.Get(3));

  std::cout << "\n";
  ShowConsumerAppInfoOnNode(nodes.Get(0));

  prodAppHelper.SetAttribute("Prefix", ns3::ndn::NameValue("prefix_1"));
  prodAppHelper.Install(nodes.Get(8)); // some weird bug

  prodAppHelper.SetAttribute("Prefix", ns3::ndn::NameValue("prefix_2"));
  prodAppHelper.Install(nodes.Get(4));

  prodAppHelper.SetAttribute("Prefix", ns3::ndn::NameValue("prefix_2"));
  prodAppHelper.Install(nodes.Get(6));
  
  ns3::ndn::CsTracer::InstallAll("./scratch/scene_1-cs-tracer.txt");
  ns3::ndn::L3RateTracer::InstallAll("./scratch/scene_1-l3rate-tracer.txt");
  ns3::ndn::AppDelayTracer::InstallAll("./scratch/scene_1-appdelay-tracer.txt");
  ns3::ndn::custom::CsTracer::InstallAll("./scratch/scene_1-custom-cs-tracer.txt");
  ns3::ndn::custom::FibTracer::InstallAll("./scratch/scene_1-custom-fib-tracer.txt");

  routingHelper.CalculateAllPossibleRoutes();

  // See more about this in documentation
  ns3::GlobalValue::Bind("SimulatorImplementationType", ns3::StringValue("ns3::DefaultSimulatorImpl"));
  // ns3::GlobalValue::Bind("SchedulerType", ns3::StringValue("ns3::DefaultSimulatorImpl"));

  ns3::Simulator::Schedule(ns3::Seconds(44), std::function<void()>([]()->void {
    CallAtEnd();
    }));

  ns3::Simulator::Stop(ns3::Seconds(55));
  ns3::Simulator::Run();
  ns3::Simulator::Destroy();
}
