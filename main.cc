#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/ndnSIM-module.h"

#include <ns3/ndnSIM/utils/tracers/custom-cs-tracer.hpp>
#include <ns3/ndnSIM/utils/tracers/custom-fib-tracer.hpp>

namespace ns3 {

  int
    main(int argc, char* argv[])
  {
    // Setting default parameters for PointToPoint links and channels
    Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Mbps"));
    Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize", StringValue("10p"));

    // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Creating 3x3 topology
    PointToPointHelper p2p;
    PointToPointGridHelper grid(3, 3, p2p);
    grid.BoundingBox(100, 100, 200, 200);

    ns3::Names::Add("Node-0", ns3::NodeList::GetNode(0)->GetObject<ns3::Node>());
    ns3::Names::Add("Node-1", ns3::NodeList::GetNode(1)->GetObject<ns3::Node>());
    ns3::Names::Add("Node-2", ns3::NodeList::GetNode(2)->GetObject<ns3::Node>());

    // Install NDN stack on all nodes
    ndn::StackHelper ndnHelper;
    ndnHelper.SetDefaultRoutes(true);
    ndnHelper.setCsSize(1000);
    ndnHelper.InstallAll();

    // Set BestRoute strategy
    ndn::StrategyChoiceHelper::InstallAll("/", "/localhost/nfd/strategy/best-route");

    // Installing global routing interface on all nodes
    ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
    ndnGlobalRoutingHelper.InstallAll();

    // Getting containers for the consumer/producer
    Ptr<Node> producer = grid.GetNode(2, 2);
    NodeContainer consumerNodes;
    consumerNodes.Add(grid.GetNode(0, 0));
    consumerNodes.Add(grid.GetNode(0, 2));

    // Install NDN applications
    std::string prefix = "/prefix";

    ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
    consumerHelper.SetPrefix(prefix);
    consumerHelper.SetAttribute("Randomize", ns3::StringValue("uniform"));
    consumerHelper.SetAttribute("Frequency", StringValue("100")); // 100 interests a second
    consumerHelper.Install(consumerNodes);

    ndn::AppHelper producerHelper("ns3::ndn::Producer");
    producerHelper.SetPrefix(prefix);
    producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
    producerHelper.Install(producer);

    // Add /prefix origins to ndn::GlobalRouter
    ndnGlobalRoutingHelper.AddOrigins(prefix, producer);

    // Calculate and install FIBs
    ndn::GlobalRoutingHelper::CalculateRoutes();

    ns3::ndn::AppDelayTracer::InstallAll("./scratch/main-app-delay-trace.txt");
    ns3::ndn::L3RateTracer::InstallAll("./scratch/main-l3-packet-trace.txt");
    ns3::ndn::CsTracer::InstallAll("./scratch/main-cs-tracer.txt");
    ns3::ndn::custom::CsTracer::InstallAll("./scratch/main-custom-cs-tracer.txt");


    Simulator::Stop(Seconds(20.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
  }

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
