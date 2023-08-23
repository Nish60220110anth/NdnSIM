#include "ns3/ndn-all.hpp"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-helper.h"

#include <vector>
#include <tuple>
#include <cassert>
#include <string>
#include <iomanip>
#include <bits/shared_ptr.h>

#include <ns3/ndnSIM/utils/tracers/custom-cs-tracer.hpp>
#include <ns3/ndnSIM/utils/tracers/custom-fib-tracer.hpp>

/**
 * how to read using command line in ns3
 *
 *
 * Why we always get size as zero ?
 *
 * 1. Is no data storing in the content store ? ( content store mistake )
 * any contradiction : No
 * Acutally it is storing. I checked it by writing a debug statement in the  OnIncomingData
 * funcion where it prints the  no of contents stored in the content store. I was getting
 * some expected value range, thus so content store doesn't have any problem.
 *
 * 2. Does producer not producing any data to get stored in the content store ? (
 * if it happens , then on no forwarder , the OnIncomingData method is called, so no data
 * is inserted )
 * any contradiction: yes! , if none is geting produced by the producer , then how some interests
 * are getting satisfied  ( shown by the data in l3tracer )
 *
 * 3. CsTracer is not updating it's own counter value ?
 * any contradiction: i couldn't understand the concept of cstracer source / sink / event ,
 * so i couldn't yet find the contradiction.
 *
 * what is the solution:
 *
 * need to find where content store is instantiated and aggegate the cs object into the forwarder
 * or node ( preferably forwarder becoz it is doesn't violate the context rules )
 *
 * after aggregating the cs in the forwarder , get the forwarder for the node using the L3Protocol
 * then get the cs using the getCs in the returned forwarder . Then connect both using the callback.
 *
 * what is need to learn:
 * 1. How cs is instantiated ( either by forwarder or in l3protocol ?)
 *
 * sol:
 * 1. how tracers are installed ? Is it something like analyzing the source and acting like sink ?
 * 2. how cs  tracer collects the info ?
 *
 * New info:
 *
 * new non-local face ids gets the value starts from 257
 *
 *
 * 1. to find in which simulation context we are running , use ns3::Simulator::GetContext()
 *
 * The context returns the  node id
 *
 * 2. Use the Names::FindName to find the name of the object
 * 3. use the Names::Add , Rename to update the name of the object
 *
 *
 *  doubts:
 * 1. Does using shared ptr doesn't require us to instantiate the class object ?
*/

namespace util {
  void SetTime(ns3::ApplicationContainer container, std::vector<std::pair<int, int>> interval) {
    int i = 0;
    for (ns3::ApplicationContainer::Iterator app = container.Begin(); app != container.End(); ++app) {
      ns3::Ptr<ns3::Application> ptrApp = *app;
      ptrApp->SetStartTime(ns3::Seconds(interval[i].first));
      ptrApp->SetStopTime(ns3::Seconds(interval[i].second));
    }
  }

  void SetConsumerStack(ns3::ndn::StackHelper& stackHelper) {
    stackHelper.setCsSize(10);
    stackHelper.SetDefaultRoutes(true);
  }

  void SetProducerStack(ns3::ndn::StackHelper& stackHelper) {
    stackHelper.setCsSize(1000);
    stackHelper.SetDefaultRoutes(true);
    stackHelper.setPolicy("nfd::cs::priority_fifo");
  }

  void SetIntermediateStack(ns3::ndn::StackHelper& stackHelper) {
    stackHelper.setCsSize(1000);
    stackHelper.SetDefaultRoutes(true);
    stackHelper.setPolicy("nfd::cs::lru");
  }


  void getNodeInfo(ns3::Ptr<ns3::Node> ptrNode, std::string path) {
    ns3::Ptr<ns3::ndn::L3Protocol> l3pt = ptrNode->GetObject<ns3::ndn::L3Protocol>();
    if (l3pt != 0) {
      std::cout << "NodeInfo [ " << ptrNode->GetId() << " ]\n";
      ns3::ndn::nfd::Cs& cs = l3pt->getForwarder()->getCs();
      std::cout << "Cs-Policy : " << cs.getPolicy()->getName() << "\n";
      std::cout << "Cs-Stored Packets : " << cs.size() << "\n";
      std::cout << "Cs-Size : " << cs.getLimit() << "\n";
      std::cout << "Face-Table-Count : " << l3pt->getFaceTable().size() << "\n";

      std::cout << "Face-Table Items\n";
      ns3::ndn::nfd::FaceTable& faceTable = l3pt->getFaceTable();
      int count = 1;

      // index face-id face-scope

      for (const ns3::ndn::Face& face : faceTable) {
        std::cout << std::setw(4) << std::right << count << ") ";
        std::cout << std::setw(3) << std::right << face.getId() << " ";
        std::cout << std::setw(6) << std::right << face.getState() << " ";
        std::cout << std::setw(10) << std::right << face.getScope() << "\n";
        count++;
      }
      std::cout << "\n";
    }
    else {
      std::cerr << "Ndn is not installed in the given node\n";
    }
  }
}

void run() {

  ns3::NodeContainer nodes;
  nodes.Create(3);

  ns3::Names::Add("Node-0", nodes.Get(0));
  ns3::Names::Add("Node-1", nodes.Get(1));
  ns3::Names::Add("Node-2", nodes.Get(2));

  ns3::PointToPointHelper p2p;
  p2p.Install(nodes.Get(0), nodes.Get(1));
  p2p.Install(nodes.Get(1), nodes.Get(2));

  ns3::ndn::StackHelper consumerStack, producerStack, intermediateStack;
  util::SetConsumerStack(consumerStack);
  util::SetProducerStack(producerStack);
  util::SetIntermediateStack(intermediateStack);

  consumerStack.Install(nodes.Get(0));
  producerStack.Install(nodes.Get(2));
  intermediateStack.Install(nodes.Get(1));

  ns3::Ptr<ns3::Node> ptrProducer = nodes.Get(2);
  ns3::Ptr<ns3::ndn::L3Protocol> prodL3 = ptrProducer->GetObject<ns3::ndn::L3Protocol>();

  // nodes.Get(1) -> GetObject<ns3::ndn::L3Protocol>();
  // prodL3 -> getForwarder() -> getFib().
  std::string prefix = "prefix-1";

  ns3::ndn::GlobalRoutingHelper routingHelper;
  routingHelper.InstallAll();
  routingHelper.AddOrigin(prefix, nodes.Get(2));

  ns3::ndn::AppHelper consumerApp("ns3::ndn::ConsumerCbr");
  consumerApp.SetPrefix(prefix);

  ns3::ndn::StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/multicast");

  consumerApp.SetAttribute("Frequency", ns3::DoubleValue(10));
  consumerApp.SetAttribute("Randomize", ns3::StringValue("uniform"));
  consumerApp.SetPrefix("prefix-1");

  ns3::ApplicationContainer consAppCont = consumerApp.Install(nodes.Get(0));

  for (ns3::ApplicationContainer::Iterator i = consAppCont.Begin(); i != consAppCont.End(); ++i) {
    (*i)->SetStartTime(ns3::Seconds(1));
    (*i)->SetStopTime(ns3::Seconds(10));
  }

  ns3::ndn::AppHelper consAppHelpr("ns3::ndn::ConsumerCbr");
  consAppHelpr.SetPrefix("prefix-1");
  consAppHelpr.SetAttribute("Frequency", ns3::DoubleValue(10));
  consAppHelpr.SetAttribute("Randomize", ns3::StringValue("uniform"));

  consAppHelpr.Install(nodes.Get(0));

  // consAppCont = consumerApp.Install(nodes.Get(0));

  // ns3::ApplicationContainer::Iterator last = --consAppCont.End();
  // (*last)->SetStartTime(ns3::Seconds(11));
  // (*last)->SetStopTime(ns3::Seconds(20));

  ns3::ndn::AppHelper producerApp("ns3::ndn::Producer");
  producerApp.SetAttribute("Prefix", ns3::StringValue(prefix));
  producerApp.SetAttribute("PayloadSize", ns3::UintegerValue(512));

  ns3::ApplicationContainer prodAppCont = producerApp.Install(nodes.Get(2));
  util::SetTime(prodAppCont, { {0,21} });

  ns3::ndn::L3RateTracer::InstallAll("./scratch/l3-rate-tracer-main1.txt", ns3::Seconds(1));
  ns3::ndn::CsTracer::InstallAll("./scratch/cs-tracer-main1.txt", ns3::Seconds(2));
  ns3::ndn::custom::CsTracer::InstallAll("./scratch/custom-cs-tracer-main1.txt", ns3::Seconds(2));
  ns3::ndn::custom::FibTracer::InstallAll("./scratch/custom-fib-tracer-main1.txt", ns3::Seconds(2));

  // // util::getNodeInfo(nodes.Get(0), "");
  // // util::getNodeInfo(nodes.Get(1), "");

  // ns3::Ptr<ns3::ndn::L3Protocol> l3pt1 = nodes.Get(0)->GetObject<ns3::ndn::L3Protocol>();
  // ns3::Ptr<ns3::ndn::L3Protocol> l3pt2 = nodes.Get(1)->GetObject<ns3::ndn::L3Protocol>();

  // ns3::Simulator::Schedule(ns3::Seconds(2), std::bind([]() {
  //   std::cout << "Sample Test1\n";
  //   std::this_thread::sleep_for(std::chrono::duration<int>(5));
  //   }));

  // ns3::Simulator::Schedule(ns3::Seconds(2), std::bind([]() {
  //   std::cout << "Sample Test2\n";
  //   }));

  // std::shared_ptr<ns3::ndn::Face> face1 = l3pt1->getFaceById(257);
  // std::shared_ptr<ns3::ndn::Face> face2 = l3pt2->getFaceById(257);

  // std::cout << (face1.get() == face2.get() ? "Yes! Both are same" : "No! Both are not same") << "\n";

  // examples 

  // scenario - 1
  // take a face object from consumer and check whether the node returned by the 
  // helper function is actually working




  // end examples

  // routingHelper.CalculateRoutes();
  ns3::Simulator::Stop(ns3::Seconds(50));
  ns3::Simulator::Run();
  ns3::Simulator::Destroy();
}

int main(int argc, char* argv[]) {
  ns3::CommandLine cmd;

  // cmd.PrintHelp(std::cout);
  // cmd.Usage("Sample Help Usage for the Program\n");

  // bool iamboy = false;
  // int testostreonelevel = 100;

  // cmd.AddValue<bool>("iamboy", "are you a boy?", iamboy);
  // cmd.AddValue<int>("testlevel", "your testostreonelevel", testostreonelevel);

  cmd.Parse(argc, argv);

  run();
}
