/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Network Topology
// 	  n2
//      \   p2p
//    	 \	1.5 Mbps  
//    	  \ 10 ms           p2p
//     	  n0 --------------------------------------- n1
//   	 / p2p              10 Mbps
//      /  1.5 Mbps         10 ms
//     /   10 ms(initially)
//   n3
// Queue Size is 10 for all the queues
// Delay for n2n0 and n0n1 link is 10 ms throughout
// Delay for n3n0 link is 10 ms initially but can be varied


// The goal of this experiment is to find the relationship between
// the propagation delay(RTT) and fairness in TCP NewReno

#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include <cstdio>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/service-flow.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-l4-protocol.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/point-to-point-remote-channel.h"
#include "ns3/csma-net-device.h"
#include "ns3/gnuplot.h"

using namespace std;
using namespace ns3;


NS_LOG_COMPONENT_DEFINE ("Lab4");

static const double totalTime = 5.0;
static double node2BytesRcv = 0.0;
static double node3BytesRcv = 0.0;

class Plotter
{
private:
  string m_graphicsFileName;
  string m_plotFileName;
  string m_plotTitle;
  string m_dataTitle;
  Gnuplot m_plot;
  Gnuplot2dDataset m_dataset;
  
public:
    Plotter(string fileNameWithNoExtension, string plotTitle, string dataTitle)
    {
      m_graphicsFileName        = fileNameWithNoExtension + ".png";
      m_plotFileName            = fileNameWithNoExtension + ".plt";
      m_plotTitle               = plotTitle;
      m_dataTitle               = dataTitle;
      
      // Instantiate the plot and set its title.
      Gnuplot plot (m_graphicsFileName);
      plot.SetTitle (m_plotTitle);

      // Make the graphics file, which the plot file will create when it
      // is used with Gnuplot, be a PNG file.
      plot.SetTerminal ("png");

      // Set the labels for each axis.
      plot.SetLegend ("Time Elapsed", "Current throughput");

      // Set the range for the x axis.
      plot.AppendExtra ("set xrange [0:+6]");
      
      m_plot = plot;
      
      // Instantiate the dataset, set its title, and make the points be
      // plotted along with connecting lines.
      Gnuplot2dDataset dataset;
      dataset.SetTitle (m_dataTitle);
      dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
      
      m_dataset = dataset;
    }
    
    void addDataset(double x, double y)
    {
      m_dataset.Add (x, y);
    }
    
    void plot()
    {
      // Add the dataset to the plot.
      m_plot.AddDataset (m_dataset);

      // Open the plot file.
      ofstream plotFile (m_plotFileName.c_str());

      // Write the plot file.
      m_plot.GenerateOutput (plotFile);
      // Close the plot file.
      plotFile.close ();   
    }
};

// plot1.plt -> gnuplot for current throughput of link1 vs time elapsed
// plot2.plt -> gnuplot for current throughput of link2 vs time elapsed
Plotter plot1("plot1", "Current throughput vs Time elapsed", "Link1");
Plotter plot2("plot2", "Current throughput vs Time elapsed", "Link2");

// ReceiveNode2Packet is triggered whenever a packet is received on n2
// Updates node2BytesRcv and adds dataset in plot1
void
ReceiveNode2Packet (string context, Ptr<const Packet> p, const Address& addr)
{
  NS_LOG_INFO (context <<
                 " Packet Received from Node 2 at " << Simulator::Now ().GetSeconds() << "from " << InetSocketAddress::ConvertFrom(addr).GetIpv4 ());
  node2BytesRcv += p->GetSize ();
  plot1.addDataset(Simulator::Now ().GetSeconds() , (node2BytesRcv * 8 / 1000000) / Simulator::Now ().GetSeconds()  );
}

// ReceiveNode3Packet is triggered whenever a packet is received on n3
// Updates node3BytesRcv and adds dataset in plot2
void
ReceiveNode3Packet (string context, Ptr<const Packet> p, const Address& addr)
{
  NS_LOG_INFO (context <<
                 " Packet Received from Node 3 at " << Simulator::Now ().GetSeconds() << "from " << InetSocketAddress::ConvertFrom(addr).GetIpv4 ());
  
  node3BytesRcv += p->GetSize ();
  plot2.addDataset(Simulator::Now ().GetSeconds() , (node3BytesRcv * 8 / 1000000) / Simulator::Now ().GetSeconds() );
}

int 
main (int argc, char *argv[])
{
  // Setting the default tcpType to NewReno
  string tcpType = "NewReno";
  
  
  uint16_t port = 9000;
  
  // Parsing the command line arguments
  CommandLine cmd;
  cmd.AddValue ("Tcp", "Tcp type: 'NewReno', 'Tahoe', 'Reno', or 'Rfc793'", tcpType);
  cmd.Parse (argc, argv);
  
  if(tcpType != "NewReno" && tcpType != "Tahoe" && tcpType != "Reno" && tcpType != "Rfc793"){
    NS_LOG_UNCOND ("The Tcp type must be either 'NewReno', 'Tahoe', 'Reno', or 'Rfc793'.");
    return 1;
  }
  
  // disable fragmentation
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Set tcp type
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName ("ns3::Tcp" + tcpType)));
  // Set maximum queue size
  Config::SetDefault ("ns3::DropTailQueue::MaxPackets", UintegerValue(uint32_t(10)));
  //LogComponentEnable("Lab3", LOG_LEVEL_INFO);

  NS_LOG_INFO ("Creating Topology");

  // Create 4 nodes
  NodeContainer nodes;
  nodes.Create (4);
  
  // Create appropriate node containers
  NodeContainer n0n1 (nodes.Get (0), nodes.Get (1));
  NodeContainer n2n0 (nodes.Get (2), nodes.Get (0));
  NodeContainer n3n0 (nodes.Get (3), nodes.Get (0));

  // Assigning datarate of 10Mbps and delay of 10ms to d2d0 and d3d0 link
  PointToPointHelper link;
  link.SetDeviceAttribute("DataRate", DataRateValue(DataRate("1.5Mbps")));
  link.SetChannelAttribute("Delay", TimeValue(Time("10ms")));
  
  NetDeviceContainer d2d0 = link.Install(n2n0);
  NetDeviceContainer d3d0 = link.Install(n3n0);
  
  // Changing the datarate to 10Mbps for d0d1 link
  link.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
  NetDeviceContainer d0d1 = link.Install (n0n1);
  
  // Installing stack on all the nodes
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assigning addresses to all the interfaces
  Ipv4AddressHelper address;
  
  // n0 ---------------------------------------- n1
  // 10.1.1.1                              10.1.1.2   
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  // n2 ---------------------------------------- n0
  // 10.1.2.1                              10.1.2.2   
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i0 = address.Assign (d2d0);

  // n3 ---------------------------------------- n0
  // 10.1.3.1                              10.1.3.2   
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i0 = address.Assign (d3d0);
  
  ApplicationContainer apps;
  
  // Setting up source with OnOffHelper at n2 and n3
  OnOffHelper source("ns3::TcpSocketFactory", InetSocketAddress(i0i1.GetAddress (1), port));
  source.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=5]"));
  source.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  source.SetAttribute ("DataRate", DataRateValue (DataRate ("1.5Mbps")));
  source.SetAttribute ("PacketSize", UintegerValue (2000));
  
  // n2 tries to connect to n1 on port 9000
  apps.Add (source.Install (nodes.Get (2)));
  
  // n3 tries to connect to n2 on port 9001
  source.SetAttribute ("Remote", AddressValue(InetSocketAddress(i0i1.GetAddress (1), port + 1)));
  apps.Add (source.Install (nodes.Get (3)));
  
  // Setting up sink with packet sink helper
  // The sinks are at 10.1.1.2:9000 and 10.1.1.2:9001
  PacketSinkHelper sink ("ns3::TcpSocketFactory", InetSocketAddress(i0i1.GetAddress (1), port));
  apps.Add(sink.Install (nodes.Get (1)));
  
  sink.SetAttribute("Local", AddressValue(InetSocketAddress(i0i1.GetAddress(1), port + 1)));
  apps.Add (sink.Install (nodes.Get (1)));
  
  // Starting the applications to run for totalTime
  apps.Start(Seconds (0));
  apps.Stop(Seconds (totalTime));

  NS_LOG_INFO ("Enable static global routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  string context = "/NodeList/1/ApplicationList/0/$ns3::PacketSink/Rx";
  Config::Connect (context, MakeCallback(&ReceiveNode2Packet));
  
  context = "/NodeList/1/ApplicationList/1/$ns3::PacketSink/Rx";
  Config::Connect (context, MakeCallback(&ReceiveNode3Packet));
  
  AsciiTraceHelper ascii;
  link.EnableAsciiAll (ascii.CreateFileStream ("lab3-rtt.tr"));

  Simulator::Stop(Seconds(totalTime));
  Simulator::Run ();
  Simulator::Destroy ();
  
  cout << " Throughput from Node 2: " << (node2BytesRcv * 8 / 1000000) / totalTime << " Mbps" << endl;
  cout << " Throughput from Node 3: " << (node3BytesRcv * 8 / 1000000) / totalTime << " Mbps" << endl;
  
  // Plotting the datasets in 'plot1.plt' and 'plot2.plt'
  plot1.plot();
  plot2.plot();
  
  return 0;
}
