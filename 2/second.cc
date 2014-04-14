/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

 //Lab assignment 5 Problem - 2

#include <iostream>
#include <fstream>
#include <string>
#include <cassert>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace std;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DynamicRoutingProtocol");
ofstream fp;
FlowMonitorHelper flowmonhelper;
Ptr<FlowMonitor> mon;

/**
 * Function to calculate packet lost and loss ratio
 * This function is involved at regular intervals during the simulation
 * Output of this function is redirected to a file, which is then parsed to plot the
 * loss vs. time graph
 */
void lossCalculator () 		
{
	double ratio=0;
	
	mon->CheckForLostPackets ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmonhelper.GetClassifier ());
	map<FlowId, FlowMonitor::FlowStats> stats = mon->GetFlowStats ();
	
	map<FlowId, FlowMonitor::FlowStats>::const_iterator i;
	uint32_t transmittedPcktsTemp = 0; 
	uint32_t lostPcktsTemp = 0; 
	
	for ( i = stats.begin (); i != stats.end (); ++i)
	{		
		//first 2 FlowIds are for the ping ECHO flows, which we will not consider
		if (i->first > 2)
		{
			Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
			if(t.destinationAddress == "10.0.2.2")
			{
				transmittedPcktsTemp += i->second.txPackets;
				lostPcktsTemp += i->second.lostPackets;
			}
		}
	}
	if (lostPcktsTemp == 0 && transmittedPcktsTemp == 0)	
		ratio=0;
	else
		ratio = (double)lostPcktsTemp/(double)transmittedPcktsTemp ;
	fp << Simulator::Now ().GetSeconds () <<" "<< ratio << endl;
}

int main(int argc, char *argv[])
{
	
	
	string latency = "2ms";
	
	/**
	 * To enable logging for each and every component.
	 * OnOffApplication is used to send CBR packets over links.
	 * Packet Sink is installed at the receiver node to receieve the packets.
	 */
	LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
	LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

	fp.open ("lossVsTime.txt");
	if (!fp)
	{
		cout << "Cannot open the output file" << endl;
		exit (1);
	}
	/**
	 * The following configures the default behaviour of the global routing protocol
	 * Set to true if you want the protocol to respond to Interface Events.
	 */
	Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));

	CommandLine cmd;
	cmd.AddValue ("latency", "link Latency(in ms)", latency);
	cmd.Parse (argc, argv);

	NS_LOG_INFO ("Create nodes.");
	NodeContainer nodes;	//creating all the 5 nodes
	nodes.Create(5);

	/**
	 * gouping the nodes into containers according to links between them
	 */
	NodeContainer node0_1 = NodeContainer (nodes.Get (0), nodes.Get (1));
	NodeContainer node1_2 = NodeContainer (nodes.Get (1), nodes.Get (2));
	NodeContainer node0_3 = NodeContainer (nodes.Get (0), nodes.Get (3));
	NodeContainer node3_4 = NodeContainer (nodes.Get (3), nodes.Get (4));
	NodeContainer node4_2 = NodeContainer (nodes.Get (4), nodes.Get (2));

	//install internet stack on all the nodes
	InternetStackHelper st;
	st.Install (nodes);

	/**
	 * Creating channels and installing network devices for each interface
	 * IP Address assignment will follow
	 */
	NS_LOG_INFO ("Create channels.");
	PointToPointHelper point2point;
	point2point.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));
	point2point.SetChannelAttribute ("Delay", StringValue (latency));
	NetDeviceContainer device0_1 = point2point.Install (node0_1);
	NetDeviceContainer device1_2 = point2point.Install (node1_2);
	NetDeviceContainer device0_3 = point2point.Install (node0_3);
	NetDeviceContainer device4_2 = point2point.Install (node4_2);

	point2point.SetDeviceAttribute ("DataRate", StringValue ("500Kbps"));
	NetDeviceContainer device3_4 = point2point.Install (node3_4);

	/**
	 * Now that we have set up devices and interfaces, its time to assign IP address
	 * to each of the interfaces.
	 */
	NS_LOG_INFO ("Assign IP Addresses.");
	Ipv4AddressHelper ip;
	ip.SetBase ("10.0.1.0", "255.255.255.0");
	Ipv4InterfaceContainer interface0_1 = ip.Assign (device0_1);

	ip.SetBase ("10.0.2.0", "255.255.255.0");
	Ipv4InterfaceContainer interface1_2 = ip.Assign (device1_2);

	ip.SetBase ("10.0.3.0", "255.255.255.0");
	Ipv4InterfaceContainer interface0_3 = ip.Assign (device0_3);

	ip.SetBase ("10.0.4.0", "255.255.255.0");
	Ipv4InterfaceContainer interface3_4 = ip.Assign (device3_4);

	ip.SetBase ("10.0.5.0", "255.255.255.0");
	Ipv4InterfaceContainer interface4_2 = ip.Assign (device4_2);

	/**
	 * Turn on global routing protocol (like OSPF) so that routers/hosts can build up
	 * their routing tables and packets can be forwarded across the network.
	 */
	NS_LOG_INFO ("Enable static global routing.");
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

	NS_LOG_INFO ("Create Applications.");

	/**
	 * Following is the OnOffHelper which sets up the attributes and parameters for 
	 * generating a CBR (Constant Bit Rate) traffic.
	 * We need to set the destination address of the CBR traffic, the packet size, CBR rate, among
	 * others and install it on the orinating node.
	 */
	ApplicationContainer cbr;
	uint16_t cbrPort = 6666;
	//We will use UDP Packets. Since we need CBR traffic to node 2, we will get its IP address
	//from the second IP address in the Interface1_2.
	OnOffHelper onOffHelp ("ns3::UdpSocketFactory",  InetSocketAddress (interface1_2.GetAddress (1), cbrPort));
	onOffHelp.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
	onOffHelp.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
	onOffHelp.SetAttribute ("PacketSize", UintegerValue (200));

	// Flow from n0 to n2 (T=1sec to T=3.5sec) at 900Kbps
	onOffHelp.SetAttribute ("DataRate", StringValue ("900Kbps"));
	onOffHelp.SetAttribute ("StartTime", TimeValue (Seconds (1.0)));	//start time of flow
	onOffHelp.SetAttribute ("StopTime", TimeValue (Seconds (3.5)));		//end time of flow
	cbr.Add (onOffHelp.Install (nodes.Get (0)));					//installing on node0

	// Flow from n0 to n3 (T=1sec to T=3.5sec) at 300Kbps
	// We need to first reset the destination address attribute to the IP address of node 3
	onOffHelp.SetAttribute ("Remote",  AddressValue (Address (InetSocketAddress (interface0_3.GetAddress (1), cbrPort))) );
	onOffHelp.SetAttribute ("DataRate", StringValue ("300Kbps"));
	onOffHelp.SetAttribute ("StartTime", TimeValue (Seconds (1.5)));	//start time of flow
	onOffHelp.SetAttribute ("StopTime", TimeValue (Seconds (3.0)));		//start time of flow
	cbr.Add (onOffHelp.Install (nodes.Get (0)));						//installing on node0

	/*--------------------------------------------------
	**
	 * Creating a Sink to receive the CBR traffic packets.
	 * Note that this is optional
	 
	//for node 2
	PacketSinkHelper sink ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address::GetAny (), cbrPort)));
	ApplicationContainer apps = sink.Install (nodes.Get (2));
	apps.Start (Seconds (1.0));
	apps.Stop (Seconds (3.5));

	//for node 3
	PacketSinkHelper sink2 ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address::GetAny (), cbrPort)));
	ApplicationContainer apps2 = sink2.Install (nodes.Get (1));
	apps2.Start (Seconds (1.5));
	apps2.Stop (Seconds (3.0));
	shobhit if you add this, dont forget to add the id>2 in flow monitor
	-----------------------------------------------------*/


	/**
	 * There is a bug in the routing protocol(due to lack of perfect ARP) which I found on net
	 * The workaround is to use 2 explicit UDP packets as ping to set up ARP tables perfectly
	 */

	uint16_t  echoPort = 7777;

	// ping from node0 to node1
	UdpEchoClientHelper echoClientHelper (interface0_1.GetAddress (1), echoPort);
	echoClientHelper.SetAttribute ("MaxPackets", UintegerValue (1));
	echoClientHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
	echoClientHelper.SetAttribute ("PacketSize", UintegerValue (10));
	ApplicationContainer ping;
	   
	//using different start times to workaround Bug
	echoClientHelper.SetAttribute ("StartTime", TimeValue (Seconds (0.001)));
	ping.Add (echoClientHelper.Install (nodes.Get (0))); 

	// ping from node0 to node1
	echoClientHelper.SetAttribute ("RemoteAddress", AddressValue (interface1_2.GetAddress (1)) );
	echoClientHelper.SetAttribute ("RemotePort", UintegerValue (echoPort));
	echoClientHelper.SetAttribute ("StartTime", TimeValue (Seconds (0.006)));
	ping.Add (echoClientHelper.Install (nodes.Get (0)));


	/**
	 * To schedule the setDown to set the link down for link0-1
	 * we need to get the IP of the node1
	 */
	Ptr<Node> n1 = nodes.Get (1);
	Ptr<Ipv4> ipNode1 = n1->GetObject<Ipv4> ();

	/**
	 * Now node1 has many interfaces. The interface0 is for loopback
	 * Interface 1 if for the n0-n1 P2P link
	 * Interface 1 if for the n0-n3 P2P link
	 */
	uint32_t index = 1;		//selecting the interface1

	Simulator::Schedule (Seconds (2.0), &Ipv4::SetDown, ipNode1, index);	//link down at t=2
	Simulator::Schedule (Seconds (2.7), &Ipv4::SetUp, ipNode1, index);		//link up at t=2.7

	/**
	 * For tracing purposes
	 */
	AsciiTraceHelper ascii;
	Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("globalRouting.tr");
	point2point.EnableAsciiAll (stream);

	st.EnableAsciiIpv4All (stream);		//stack
	point2point.EnablePcapAll("DynamicRoutingProtocol");

	// Flow Monitor to monitor the entire traffic
	mon = flowmonhelper.InstallAll();		//Flow monitor installed over the entire network
	mon->Start (Seconds (0.5));		

	//call the LossCalculator function every 0.05 sec. Required for plotting loss vs. time graph
	for(double i=1; i<=4.0; i = i + 0.05)
		Simulator::Schedule (Seconds(i), &lossCalculator);
	

	NS_LOG_INFO ("Run Simulation.");
	Simulator::Stop (Seconds(8.0));
	Simulator::Run ();

	uint32_t transmittedPckts = 0; 
	uint32_t lostPckts = 0; 
	mon->CheckForLostPackets ();
	/**
	 * Following loop is to retrieve all the flow statistics corresponding to each flow
	 * From these statistics, we classify the flows and filter out the flows which are destined
	 * to node 2 by using the ip.destinationAddress == n2IpAddress.
	 */
	Ptr<Ipv4FlowClassifier> classify = DynamicCast<Ipv4FlowClassifier> (flowmonhelper.GetClassifier ());
	std::map<FlowId, FlowMonitor::FlowStats> statistics = mon->GetFlowStats ();
	map<FlowId, FlowMonitor::FlowStats>::const_iterator i;
	for (i = statistics.begin (); i != statistics.end (); ++i)
	{

		//first 2 FlowIds are for the ping ECHO flows, which we will not consider
		if (i->first > 2)
		{
			Ipv4FlowClassifier::FiveTuple t = classify->FindFlow (i->first);
			if(t.destinationAddress == "10.0.2.2")		//filtering packets destined to node 2
			{
				transmittedPckts += i->second.txPackets; 
				lostPckts += i->second.lostPackets; 
			}
		}
	}
	cout<<"\n\n----------------------------------\n\n";
	cout << "Total transmitted packets (destined to node2) = " << transmittedPckts << "\n"; 
	cout << "Total lost packets (destined to node2) =  " << lostPckts << "\n"; 
	cout << "Packets Lost Percentage (totalLost/totalTranmitted) [destined to node2]: " << ((lostPckts * 100) / transmittedPckts) << "%" << "\n"; 
	cout<<"\n\n----------------------------------\n\n";	
	mon->SerializeToXmlFile("assign-2.flowmon", true, true);

	  Simulator::Destroy ();
	  NS_LOG_INFO ("Done.");


	return 0;
}