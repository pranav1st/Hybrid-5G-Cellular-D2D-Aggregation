/*
  With Relay
  Inside Building  (NLoS)
*/

#include "ns3/core-module.h"
#include "ns3/config-store.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/nr-module.h"
#include "ns3/lte-module.h"
#include "ns3/stats-module.h"
#include "ns3/config-store-module.h"
#include "ns3/antenna-module.h"
#include <iomanip>

#include "ns3/netanim-module.h"
#include <ns3/buildings-helper.h>
#include <ns3/buildings-module.h>
#include <bits/stdc++.h>
#include <string.h>
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include <ns3/constant-position-mobility-model.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NrProseDiscoveryL3RelaySelection");

void SendStuff (Ptr <Socket> sock, Ipv4Address dstAddr, uint16_t dstPort){
  NS_LOG_INFO ("SendStuff() called ...");
  Ptr<Packet> p = Create <Packet>(reinterpret_cast<uint8_t const *> ("I am long 20 bytes!"), 20 );
  p->AddPaddingAtEnd(100);
  sock->SendTo(p, 0, InetSocketAddress(dstAddr, dstPort));
  return;
}


void
TraceSinkPC5SignallingPacketTrace (Ptr<OutputStreamWrapper> stream,
                                   uint32_t srcL2Id, uint32_t dstL2Id, bool isTx, Ptr<Packet> p)
{
  NrSlPc5SignallingMessageType pc5smt;
  p->PeekHeader (pc5smt);
  *stream->GetStream () << Simulator::Now ().GetSeconds ();
  if (isTx)
    {
      *stream->GetStream () << "\t" << "TX";
    }
  else
    {
      *stream->GetStream () << "\t" << "RX";
    }
  *stream->GetStream () << "\t" << srcL2Id << "\t" << dstL2Id << "\t" << pc5smt.GetMessageName ();
  *stream->GetStream () << std::endl;
}

std::map<std::string, uint32_t> g_relayNasPacketCounter;

void
TraceSinkRelayNasRxPacketTrace (Ptr<OutputStreamWrapper> stream,
                                Ipv4Address nodeIp, Ipv4Address srcIp, Ipv4Address dstIp,
                                std::string srcLink, std::string dstLink, Ptr<Packet> p)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds ()
                        << "\t" << nodeIp
                        << "\t" << srcIp
                        << "\t" << dstIp
                        << "\t" << srcLink
                        << "\t" << dstLink
                        << std::endl;
  std::ostringstream  oss;
  oss << nodeIp << "      " << srcIp << "->" << dstIp << "      " << srcLink << "->" << dstLink;
  std::string mapKey = oss.str ();
  auto it = g_relayNasPacketCounter.find (mapKey);
  if (it == g_relayNasPacketCounter.end ())
    {
      g_relayNasPacketCounter.insert (std::pair < std::string, uint32_t> (mapKey, 1));
    }
  else
    {
      it->second += 1;
    }
}

int
main (int argc, char *argv[])
{
  // Rem
  std::string remMode = "CoverageArea";
  std::string typeOfRem = "DlRem";
  std::string simTag = "";

  //Common configuration
  uint8_t numBands = 1;
  double centralFrequencyHz = 5.89e9; // band n47
  double bandwidth = 40e6; 
  double centralFrequencyCc0 = 5.89e9;
  double bandwidthCc0 = bandwidth;
  std::string pattern = "DL|DL|DL|F|UL|UL|UL|UL|UL|UL|"; 
  double bandwidthCc0Bpw0 = bandwidthCc0 / 2;
  double bandwidthCc0Bpw1 = bandwidthCc0 / 2;

  //In-network devices configuration
  uint16_t gNbNum = 1;
  double gNbHeight = 30;
  double ueHeight = 1.5;
  uint16_t numerologyCc0Bwp0 = 1; 
  double gNBtotalTxPower = 46; 
  bool cellScan = false;  
  double beamSearchAngleStep = 10.0; 

  //Sidelink configuration
  uint16_t numerologyCc0Bwp1 = 1; 

  // Topology parameters
  uint16_t ueNum = 2; 
  uint16_t relayNum = 1; 
  double ueTxPower = 23; 

  // Simulation timeline parameters
  Time simTime = Seconds (20); 

  // NR Discovery
  Time discInterval = Seconds (1); // interval between two discovery announcements
  double discStartMin = 2; 
  double discStartMax = 4; 
  std::string discModel = "ModelA" ; 
  std::string relaySelectAlgorithm ("MaxRsrpRelay"); 
  Time t5087 = Seconds (5);

  // Applications configuration
  uint32_t packetSizeDlUl = 1000; //bytes
  uint32_t lambdaDlUl = 60; // packets per second
  uint32_t trafficStart = 4; //traffis start time in seconds

  CommandLine cmd;
  cmd.AddValue ("ueNum",
                "Number of UEs in the simulation",
                ueNum);
  cmd.AddValue ("relayNum",
                "Number of relay UEs in the simulation",
                relayNum);  
  cmd.AddValue ("simTime",
                "Simulation time in seconds",
                simTime);
  cmd.AddValue ("discStartMin",
                "Lower bound of discovery start time in seconds",
                discStartMin);
  cmd.AddValue ("discStartMax",
                "Upper bound of discovery start time in seconds",
                discStartMax);  
  cmd.AddValue ("discInterval",
                "Interval between two Prose discovery announcements",
                discInterval);
  cmd.AddValue ("discModel",
                "Discovery model (ModelA for announcements and ModelB for requests/responses)",
                discModel);
  cmd.AddValue ("relaySelectAlgorithm",
                "The Relay UE (re)selection algorithm the Remote UEs will use (FirstAvailableRelay|RandomRelay|MaxRsrpRelay)", 
                relaySelectAlgorithm);
  cmd.AddValue ("t5087",
                "The duration of Timer T5087 (Prose Direct Link Release Request Retransmission)",
                t5087);
  cmd.AddValue ("trafficStart",
                "the start time of remote traffic in seconds",
                trafficStart);
  cmd.AddValue ("remMode",
                "What type of REM map to use: BeamShape, CoverageArea, UeCoverage."
                "BeamShape shows beams that are configured in a user's script. "
                "Coverage area is used to show worst-case SINR and best-case SNR maps "
                "considering that at each point of the map the best beam is used "
                "towards that point from the serving gNB and also of all the interfering"
                "gNBs in the case of worst-case SINR."
                "UeCoverage is similar to the previous, just that it is showing the "
                "uplink coverage.",
                remMode);
  cmd.AddValue ("typeOfRem",
                "The type of Rem to generate (DL or UL) in the case of BeamShape option. Choose among "
                "'DlRem', 'UlRem'.",
                typeOfRem);
  cmd.AddValue ("simTag",
                "Simulation string tag that will be concatenated to output file names",
                simTag);

  // Parse the command line
  cmd.Parse (argc, argv);
  
  NS_ABORT_IF (numBands < 1);

  // Number of relay and remote UEs
  uint16_t remoteNum = ueNum - relayNum;
  std::cout << "UEs configuration: " << std::endl;
  std::cout << " Number of Relay UEs = " << relayNum << std::endl << " Number of Remote UEs = " << remoteNum << std::endl;
  
  //Check if the frequency is in the allowed range.
  NS_ABORT_IF (centralFrequencyHz > 6e9);

  //Setup large enough buffer size to avoid overflow
  Config::SetDefault ("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue (999999999));

  //Set the TxPower for UEs
  Config::SetDefault ("ns3::NrUePhy::TxPower", DoubleValue (ueTxPower));

  // Discovery Interval
  Config::SetDefault ("ns3::NrSlUeProse::DiscoveryInterval",TimeValue (discInterval));
  // T5087 timer for retransmission of failed Prose Direct Link Release Request
  Config::SetDefault ("ns3::NrSlUeProseDirectLink::T5087", TimeValue (t5087));
  
  // Create gNBs and in-network UEs, configure positions
  NodeContainer gNbNodes;
  NodeContainer inNetUeNodes;
  MobilityHelper mobilityG;

  gNbNodes.Create (gNbNum);

  mobilityG.SetMobilityModel("ns3::ConstantPositionMobilityModel");   

  
  Ptr<ListPositionAllocator> gNbPositionAlloc = CreateObject<ListPositionAllocator> ();
  int32_t yValue = 0.0;
  gNbPositionAlloc->Add (Vector (0.0, yValue, gNbHeight));

  mobilityG.SetPositionAllocator (gNbPositionAlloc);
  mobilityG.Install(gNbNodes);

  
  std::cout << "GNB Pos: " << 0.0 << "," << 0.0 << "," << gNbHeight << std::endl;
  
  //Position of the building
  Ptr<GridBuildingAllocator> gridBuildingAllocator;
  gridBuildingAllocator = CreateObject<GridBuildingAllocator> ();
  gridBuildingAllocator->SetAttribute ("GridWidth", UintegerValue (1));
  gridBuildingAllocator->SetAttribute ("LengthX", DoubleValue (40));
  gridBuildingAllocator->SetAttribute ("LengthY", DoubleValue (300));
  gridBuildingAllocator->SetAttribute ("DeltaX", DoubleValue (10));
  gridBuildingAllocator->SetAttribute ("DeltaY", DoubleValue (10));
  gridBuildingAllocator->SetAttribute ("Height", DoubleValue (10));    // nFloors
  gridBuildingAllocator->SetBuildingAttribute ("Type", EnumValue (Building::Residential));
  gridBuildingAllocator->SetBuildingAttribute ("ExternalWallsType", EnumValue (Building::ConcreteWithoutWindows));
  gridBuildingAllocator->SetBuildingAttribute ("NRoomsY", UintegerValue (2));
  gridBuildingAllocator->SetBuildingAttribute ("NFloors", UintegerValue (3));
  gridBuildingAllocator->SetBuildingAttribute ("NFloors", UintegerValue (3));
  gridBuildingAllocator->SetAttribute ("MinX", DoubleValue (-20));
  gridBuildingAllocator->SetAttribute ("MinY", DoubleValue (600));
  gridBuildingAllocator->Create (1);

  BuildingsHelper::Install (gNbNodes);

  //Create UE nodes and define their mobility
  NodeContainer relayUeNodes;
  relayUeNodes.Create (relayNum);
  NodeContainer remoteUeNodes;
  remoteUeNodes.Create (remoteNum);

  uint64_t stream = 1;
  Ptr<UniformRandomVariable> m_uniformRandomVariablePositionX = CreateObject<UniformRandomVariable> ();
  m_uniformRandomVariablePositionX->SetStream (stream++);
  Ptr<UniformRandomVariable> m_uniformRandomVariablePositionY = CreateObject<UniformRandomVariable> ();
  m_uniformRandomVariablePositionY->SetStream (stream++);

  AsciiTraceHelper ascii;

  MobilityHelper mobilityRemotes;
  mobilityRemotes.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");   
  Ptr<ListPositionAllocator> positionAllocRemotes = CreateObject<ListPositionAllocator> ();
        
  for (uint16_t i = 0; i < remoteNum; i++)
    {
      double x = 0;
      double y = 650;
      std::cout << "Remote UE Pos: " << x << "," << y << "," << ueHeight << std::endl;
      positionAllocRemotes->Add (Vector (x, y, ueHeight));
    }
  mobilityRemotes.SetPositionAllocator (positionAllocRemotes);
  mobilityRemotes.Install (remoteUeNodes);
  //Added Extra
  BuildingsHelper::Install (remoteUeNodes);

  for (uint16_t i = 0; i < remoteNum; i++)
    {
      double speed = 1; // m/s
      double rm_vel_x = 0.0;
      double rm_vel_y = speed;
      remoteUeNodes.Get (i)->GetObject<ConstantVelocityMobilityModel> ()->SetVelocity (Vector (rm_vel_x, rm_vel_y, 0.0));
    }

  MobilityHelper mobilityRelays;
  mobilityRelays.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  Ptr<ListPositionAllocator> positionAllocRelays = CreateObject<ListPositionAllocator> ();  
  for (uint16_t i = 0; i < relayNum; i++)
    {
      double x = 0;
      double y = 450;
      std::cout << "Relay UE Pos: " << x << "," << y << "," << 10 << std::endl;
      positionAllocRelays->Add (Vector (x, y, ueHeight));
    }
  mobilityRelays.SetPositionAllocator (positionAllocRelays);
  mobilityRelays.Install (relayUeNodes);
  BuildingsHelper::Install (relayUeNodes);



  //Store UEs positions
  std::ofstream myfile;
  myfile.open ("NrSlDiscoveryTopology.txt");
  
  myfile << "  X Y Z\n";
  uint32_t gnb = 1;
  for (NodeContainer::Iterator j = gNbNodes.Begin (); j != gNbNodes.End (); ++j)
    {
      Ptr<Node> object = *j;
      Ptr<MobilityModel> position = object->GetObject<MobilityModel> ();
      NS_ASSERT (position != 0);
      Vector pos = position->GetPosition ();
      myfile << "gNB " << gnb << " " << pos.x << " " << pos.y << " " << pos.z << "\n";
      gnb++;
    }
  uint32_t ue = 1;
  for (NodeContainer::Iterator j = relayUeNodes.Begin (); j != relayUeNodes.End (); ++j)
    {
      Ptr<Node> object = *j;
      Ptr<MobilityModel> position = object->GetObject<MobilityModel> ();
      NS_ASSERT (position != 0);
      Vector pos = position->GetPosition ();
      myfile << "UE " << ue << " " << pos.x << " " << pos.y << " " << pos.z << "\n";
      ue++;
    }
  for (NodeContainer::Iterator j = remoteUeNodes.Begin (); j != remoteUeNodes.End (); ++j)
    {
      Ptr<Node> object = *j;
      Ptr<MobilityModel> position = object->GetObject<MobilityModel> ();
      NS_ASSERT (position != 0);
      Vector pos = position->GetPosition ();
      myfile << "UE " << ue << " " << pos.x << " " << pos.y << " " << pos.z << "\n";
      ue++;
    }

  myfile.close();

  Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper> ();
  Ptr<NrHelper> nrHelper = CreateObject<NrHelper> ();
  Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
  nrHelper->SetBeamformingHelper (idealBeamformingHelper);
  nrHelper->SetEpcHelper (epcHelper);

  /*************************Spectrum division ****************************/

  BandwidthPartInfoPtrVector allBwps;
  CcBwpCreator ccBwpCreator;

  OperationBandInfo band;

  std::unique_ptr<ComponentCarrierInfo> cc0 (new ComponentCarrierInfo ());
  std::unique_ptr<BandwidthPartInfo> bwp0 (new BandwidthPartInfo ());
  std::unique_ptr<BandwidthPartInfo> bwp1 (new BandwidthPartInfo ());


  band.m_centralFrequency  = centralFrequencyHz;
  band.m_channelBandwidth = bandwidth;
  band.m_lowerFrequency = band.m_centralFrequency - band.m_channelBandwidth / 2;
  band.m_higherFrequency = band.m_centralFrequency + band.m_channelBandwidth / 2;

  // Component Carrier 0
  cc0->m_ccId = 0;
  cc0->m_centralFrequency = centralFrequencyCc0;
  cc0->m_channelBandwidth = bandwidthCc0;
  cc0->m_lowerFrequency = cc0->m_centralFrequency - cc0->m_channelBandwidth / 2;
  cc0->m_higherFrequency = cc0->m_centralFrequency + cc0->m_channelBandwidth / 2;

  // BWP 0
  bwp0->m_bwpId = 0;
  bwp0->m_centralFrequency = cc0->m_lowerFrequency + cc0->m_channelBandwidth / 4;
  bwp0->m_channelBandwidth = bandwidthCc0Bpw0;
  bwp0->m_lowerFrequency = bwp0->m_centralFrequency - bwp0->m_channelBandwidth / 2;
  bwp0->m_higherFrequency = bwp0->m_centralFrequency + bwp0->m_channelBandwidth / 2;
  bwp0->m_scenario = BandwidthPartInfo::Scenario::UMa_Buildings;

  cc0->AddBwp (std::move (bwp0));

  // BWP 1
  bwp1->m_bwpId = 1;
  bwp1->m_centralFrequency = cc0->m_higherFrequency - cc0->m_channelBandwidth / 4;
  bwp1->m_channelBandwidth = bandwidthCc0Bpw1;
  bwp1->m_lowerFrequency = bwp1->m_centralFrequency - bwp1->m_channelBandwidth / 2;
  bwp1->m_higherFrequency = bwp1->m_centralFrequency + bwp1->m_channelBandwidth / 2;
  bwp1->m_scenario = BandwidthPartInfo::Scenario::RMa;

  cc0->AddBwp (std::move (bwp1));


  band.AddCc (std::move (cc0));

  /********************* END Spectrum division ****************************/


  epcHelper->SetAttribute ("S1uLinkDelay", TimeValue (MilliSeconds (0)));

  //Set gNB scheduler
  nrHelper->SetSchedulerTypeId (TypeId::LookupByName ("ns3::NrMacSchedulerTdmaRR"));

  //gNB Beamforming method
  if (cellScan)
    {
      idealBeamformingHelper->SetAttribute ("BeamformingMethod", TypeIdValue (CellScanBeamforming::GetTypeId ()));
      idealBeamformingHelper->SetBeamformingAlgorithmAttribute ("BeamSearchAngleStep", DoubleValue (beamSearchAngleStep));
    }
  else
    {
      idealBeamformingHelper->SetAttribute ("BeamformingMethod", TypeIdValue (DirectPathBeamforming::GetTypeId ()));
    }

  nrHelper->InitializeOperationBand (&band);
  allBwps = CcBwpCreator::GetAllBwps ({band});

  // Antennas for all the UEs
  nrHelper->SetUeAntennaAttribute ("NumRows", UintegerValue (1)); 
  nrHelper->SetUeAntennaAttribute ("NumColumns", UintegerValue (2)); 
  nrHelper->SetUeAntennaAttribute ("AntennaElement", PointerValue (CreateObject<IsotropicAntennaModel> ()));

  // Antennas for all the gNbs
  nrHelper->SetGnbAntennaAttribute ("NumRows", UintegerValue (4));
  nrHelper->SetGnbAntennaAttribute ("NumColumns", UintegerValue (8));
  nrHelper->SetGnbAntennaAttribute ("AntennaElement", PointerValue (CreateObject<IsotropicAntennaModel> ()));

  //gNB bandwidth part manager setup.
  nrHelper->SetGnbBwpManagerAlgorithmAttribute ("GBR_CONV_VOICE", UintegerValue (0)); 

  //Install only in the BWP that will be used for in-network
  uint8_t bwpIdInNet = 0;
  BandwidthPartInfoPtrVector inNetBwp;
  inNetBwp.insert (inNetBwp.end (), band.GetBwpAt (/*CC*/ 0,bwpIdInNet));
  NetDeviceContainer inNetUeNetDev = nrHelper->InstallUeDevice (inNetUeNodes, inNetBwp);
  NetDeviceContainer enbNetDev = nrHelper->InstallGnbDevice (gNbNodes, inNetBwp);

  //SL UE MAC configuration
  nrHelper->SetUeMacAttribute ("EnableSensing", BooleanValue (false));
  nrHelper->SetUeMacAttribute ("T1", UintegerValue (2));
  nrHelper->SetUeMacAttribute ("T2", UintegerValue (33));
  nrHelper->SetUeMacAttribute ("ActivePoolId", UintegerValue (0));
  nrHelper->SetUeMacAttribute ("ReservationPeriod", TimeValue (MilliSeconds (20)));
  nrHelper->SetUeMacAttribute ("NumSidelinkProcess", UintegerValue (255));
  nrHelper->SetUeMacAttribute ("EnableBlindReTx", BooleanValue (true));
  nrHelper->SetUeMacAttribute ("SlThresPsschRsrp", IntegerValue (-128));

  //SL BWP manager configuration
  uint8_t bwpIdSl = 1;
  nrHelper->SetBwpManagerTypeId (TypeId::LookupByName ("ns3::NrSlBwpManagerUe"));
  nrHelper->SetUeBwpManagerAlgorithmAttribute ("GBR_MC_PUSH_TO_TALK", UintegerValue (bwpIdSl));

  //Install both BWPs on SL UEs: relays and remotes
  NetDeviceContainer relayUeNetDev = nrHelper->InstallUeDevice (relayUeNodes, allBwps);
  NetDeviceContainer remoteUeNetDev = nrHelper->InstallUeDevice (remoteUeNodes, allBwps);
  
  std::set<uint8_t> slBwpIdContainer;
  slBwpIdContainer.insert (bwpIdInNet);
  slBwpIdContainer.insert (bwpIdSl);

  //Setup BWPs numerology, Tx Power and pattern
  nrHelper->GetGnbPhy (enbNetDev.Get (0), 0)->SetAttribute ("Numerology", UintegerValue (numerologyCc0Bwp0));
  nrHelper->GetGnbPhy (enbNetDev.Get (0), 0)->SetAttribute ("Pattern", StringValue (pattern));
  nrHelper->GetGnbPhy (enbNetDev.Get (0), 0)->SetAttribute ("TxPower", DoubleValue (gNBtotalTxPower));

  for (auto it = enbNetDev.Begin (); it != enbNetDev.End (); ++it)
    {
      DynamicCast<NrGnbNetDevice> (*it)->UpdateConfig ();
    }
    
  for (auto it = relayUeNetDev.Begin (); it != relayUeNetDev.End (); ++it)
    {
      DynamicCast<NrUeNetDevice> (*it)->UpdateConfig ();
    }
  for (auto it = remoteUeNetDev.Begin (); it != remoteUeNetDev.End (); ++it)
    {
      DynamicCast<NrUeNetDevice> (*it)->UpdateConfig ();
    }
  
  Ptr<NrSlHelper> nrSlHelper = CreateObject <NrSlHelper> ();
  // Put the pointers inside NrSlHelper
  nrSlHelper->SetEpcHelper (epcHelper);

  std::string errorModel = "ns3::NrEesmIrT1";
  nrSlHelper->SetSlErrorModel (errorModel);
  nrSlHelper->SetUeSlAmcAttribute ("AmcModel", EnumValue (NrAmc::ErrorModel));

  nrSlHelper->SetNrSlSchedulerTypeId (NrSlUeMacSchedulerSimple::GetTypeId ());
  nrSlHelper->SetUeSlSchedulerAttribute ("FixNrSlMcs", BooleanValue (true));
  nrSlHelper->SetUeSlSchedulerAttribute ("InitialNrSlMcs", UintegerValue (14));


  //Configure U2N relay UEs for SL
  std::set<uint8_t> slBwpIdContainerRelay;
  slBwpIdContainerRelay.insert (bwpIdSl);
  nrSlHelper->PrepareUeForSidelink (relayUeNetDev, slBwpIdContainerRelay);

  //Configure SL-only UEs for SL
  nrSlHelper->PrepareUeForSidelink (remoteUeNetDev, slBwpIdContainer);

  //SlResourcePoolNr IE
  LteRrcSap::SlResourcePoolNr slResourcePoolNr;
  Ptr<NrSlCommResourcePoolFactory> ptrFactory = Create<NrSlCommResourcePoolFactory> ();

  //Configure specific parameters of interest:
  std::vector <std::bitset<1> > slBitmap = {1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1};
  ptrFactory->SetSlTimeResources (slBitmap);
  ptrFactory->SetSlSensingWindow (100); // T0 in ms
  ptrFactory->SetSlSelectionWindow (5);
  ptrFactory->SetSlFreqResourcePscch (10); // PSCCH RBs
  ptrFactory->SetSlSubchannelSize (10);
  ptrFactory->SetSlMaxNumPerReserve (3);
  //Once parameters are configured, we can create the pool
  LteRrcSap::SlResourcePoolNr pool = ptrFactory->CreatePool ();
  slResourcePoolNr = pool;

  //Configure the SlResourcePoolConfigNr IE, which hold a pool and its id
  LteRrcSap::SlResourcePoolConfigNr slresoPoolConfigNr;
  slresoPoolConfigNr.haveSlResourcePoolConfigNr = true;
  //Pool id, ranges from 0 to 15
  uint16_t poolId = 0;
  LteRrcSap::SlResourcePoolIdNr slResourcePoolIdNr;
  slResourcePoolIdNr.id = poolId;
  slresoPoolConfigNr.slResourcePoolId = slResourcePoolIdNr;
  slresoPoolConfigNr.slResourcePool = slResourcePoolNr;

  //Configure the SlBwpPoolConfigCommonNr IE, which hold an array of pools
  LteRrcSap::SlBwpPoolConfigCommonNr slBwpPoolConfigCommonNr;
  //Array for pools, we insert the pool in the array as per its poolId
  slBwpPoolConfigCommonNr.slTxPoolSelectedNormal [slResourcePoolIdNr.id] = slresoPoolConfigNr;

  //Configure the BWP IE
  LteRrcSap::Bwp bwp;
  bwp.numerology = numerologyCc0Bwp1;
  bwp.symbolsPerSlots = 14;
  bwp.rbPerRbg = 1;
  bwp.bandwidth = bandwidthCc0Bpw1/1000/100;

  //Configure the SlBwpGeneric IE
  LteRrcSap::SlBwpGeneric slBwpGeneric;
  slBwpGeneric.bwp = bwp;
  slBwpGeneric.slLengthSymbols = LteRrcSap::GetSlLengthSymbolsEnum (14);
  slBwpGeneric.slStartSymbol = LteRrcSap::GetSlStartSymbolEnum (0);

  //Configure the SlBwpConfigCommonNr IE
  LteRrcSap::SlBwpConfigCommonNr slBwpConfigCommonNr;
  slBwpConfigCommonNr.haveSlBwpGeneric = true;
  slBwpConfigCommonNr.slBwpGeneric = slBwpGeneric;
  slBwpConfigCommonNr.haveSlBwpPoolConfigCommonNr = true;
  slBwpConfigCommonNr.slBwpPoolConfigCommonNr = slBwpPoolConfigCommonNr;

  LteRrcSap::SlFreqConfigCommonNr slFreConfigCommonNr;
  for (const auto &it:slBwpIdContainer)
    {
      // it is the BWP id
      slFreConfigCommonNr.slBwpList [it] = slBwpConfigCommonNr;
    }

  //Configure the TddUlDlConfigCommon IE
  LteRrcSap::TddUlDlConfigCommon tddUlDlConfigCommon;
  tddUlDlConfigCommon.tddPattern = pattern;

  //Configure the SlPreconfigGeneralNr IE
  LteRrcSap::SlPreconfigGeneralNr slPreconfigGeneralNr;
  slPreconfigGeneralNr.slTddConfig = tddUlDlConfigCommon;

  //Configure the SlUeSelectedConfig IE
  LteRrcSap::SlUeSelectedConfig slUeSelectedPreConfig;
  slUeSelectedPreConfig.slProbResourceKeep = 0;
  //Configure the SlPsschTxParameters IE
  LteRrcSap::SlPsschTxParameters psschParams;
  psschParams.slMaxTxTransNumPssch = 1;
  //Configure the SlPsschTxConfigList IE
  LteRrcSap::SlPsschTxConfigList pscchTxConfigList;
  pscchTxConfigList.slPsschTxParameters [0] = psschParams;
  slUeSelectedPreConfig.slPsschTxConfigList = pscchTxConfigList;

  LteRrcSap::SidelinkPreconfigNr slPreConfigNr;
  slPreConfigNr.slPreconfigGeneral = slPreconfigGeneralNr;
  slPreConfigNr.slUeSelectedPreConfig = slUeSelectedPreConfig;
  slPreConfigNr.slPreconfigFreqInfoList [0] = slFreConfigCommonNr;

  //Communicate the above pre-configuration to the NrSlHelper
  //For SL-only UEs
  nrSlHelper->InstallNrSlPreConfiguration (remoteUeNetDev, slPreConfigNr);

  //For U2N relay UEs
  LteRrcSap::SlFreqConfigCommonNr slFreConfigCommonNrRelay;
  slFreConfigCommonNrRelay.slBwpList [bwpIdSl] = slBwpConfigCommonNr;

  LteRrcSap::SidelinkPreconfigNr slPreConfigNrRelay;
  slPreConfigNrRelay.slPreconfigGeneral = slPreconfigGeneralNr;
  slPreConfigNrRelay.slUeSelectedPreConfig = slUeSelectedPreConfig;
  slPreConfigNrRelay.slPreconfigFreqInfoList [0] = slFreConfigCommonNrRelay;

  nrSlHelper->InstallNrSlPreConfiguration (relayUeNetDev, slPreConfigNrRelay);

  //For L3 U2N relay (re)selection criteria
  LteRrcSap::SlRemoteUeConfig slRemoteConfig;
  slRemoteConfig.slReselectionConfig.slRsrpThres = -110;
  slRemoteConfig.slReselectionConfig.slFilterCoefficientRsrp = 0.5;
  slRemoteConfig.slReselectionConfig.slHystMin = 10;

  LteRrcSap::SlDiscConfigCommon slDiscConfigCommon;
  slDiscConfigCommon.slRemoteUeConfigCommon = slRemoteConfig;

  nrSlHelper->InstallNrSlDiscoveryConfiguration (relayUeNetDev, remoteUeNetDev, slDiscConfigCommon);

  /****************************** End SL Configuration ***********************/

  stream += nrHelper->AssignStreams (enbNetDev, stream);
  stream += nrHelper->AssignStreams (relayUeNetDev, stream);
  stream += nrSlHelper->AssignStreams (relayUeNetDev, stream);
  stream += nrHelper->AssignStreams (remoteUeNetDev, stream);
  stream += nrSlHelper->AssignStreams (remoteUeNetDev, stream);

  Ptr<Node> pgw = epcHelper->GetPgwNode ();
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (2500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.000)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  std::cout << "IP configuration: " << std::endl;
  std::cout << " Remote Host: " << remoteHostAddr << std::endl;

  internet.Install (relayUeNodes);
  Ipv4InterfaceContainer ueIpIfaceRelay;
  ueIpIfaceRelay = epcHelper->AssignUeIpv4Address (NetDeviceContainer (relayUeNetDev));
  std::vector<Ipv4Address> relayIpv4AddressVector (relayNum);

  for (uint32_t u = 0; u < relayUeNodes.GetN (); ++u)
    {
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (relayUeNodes.Get (u)->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

      relayIpv4AddressVector [u] = relayUeNodes.Get (u)->GetObject<Ipv4L3Protocol> ()->GetAddress (1,0).GetLocal ();
      std::cout << " In-network U2N relay UE: " << relayIpv4AddressVector [u] << std::endl;
    }

  nrHelper->AttachToClosestEnb (relayUeNetDev, enbNetDev);

  internet.Install (remoteUeNodes);
  Ipv4InterfaceContainer ueIpIfaceRemote;
  ueIpIfaceRemote = epcHelper->AssignUeIpv4Address (NetDeviceContainer (remoteUeNetDev));
  std::vector<Ipv4Address> remoteIpv4AddressVector (remoteNum);

  for (uint32_t u = 0; u < remoteUeNodes.GetN (); ++u)
    {
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteUeNodes.Get (u)->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);

      remoteIpv4AddressVector [u] = remoteUeNodes.Get (u)->GetObject<Ipv4L3Protocol> ()->GetAddress (1,0).GetLocal ();
      std::cout << " Out-of-network remote UE: " << remoteIpv4AddressVector [u] << std::endl;
    }

  //Create ProSe helper
  Ptr<NrSlProseHelper> nrSlProseHelper = CreateObject <NrSlProseHelper> ();
  nrSlProseHelper->SetEpcHelper (epcHelper);
  
  // Install ProSe layer and corresponding SAPs in the UES
  nrSlProseHelper->PrepareUesForProse (relayUeNetDev);
  nrSlProseHelper->PrepareUesForProse (remoteUeNetDev);
   
  //Configure ProSe Unicast parameters.
  nrSlProseHelper->PrepareUesForUnicast (relayUeNetDev);
  nrSlProseHelper->PrepareUesForUnicast (remoteUeNetDev);

  Config::SetDefault ("ns3::NrSlUeProseDirectLink::T5080", TimeValue (Seconds (2.0)));

  NS_LOG_INFO ("Configuring discovery relay"); 
     
  //Relay Discovery Model
  NrSlUeProse::DiscoveryModel model;
  if (discModel=="ModelA")
    {
      model = NrSlUeProse::ModelA;
    }
  else if (discModel=="ModelB")
    {
      model = NrSlUeProse::ModelB;
    }
  else
    {
      NS_FATAL_ERROR ("Wrong discovery model! It should be either ModelA or ModelB");
    }

  //Relay Selection Algorithm
  Ptr<NrSlUeProseRelaySelectionAlgorithm> algorithm;
  if (relaySelectAlgorithm == "FirstAvailableRelay")
    {
      algorithm = CreateObject<NrSlUeProseRelaySelectionAlgorithmFirstAvailable> ();
    }
  else if (relaySelectAlgorithm == "RandomRelay")
    {
      algorithm = CreateObject<NrSlUeProseRelaySelectionAlgorithmRandom> ();
    }
  else if (relaySelectAlgorithm == "MaxRsrpRelay")
    {
      algorithm = CreateObject<NrSlUeProseRelaySelectionAlgorithmMaxRsrp> ();
    }
  else
    {
      NS_FATAL_ERROR ("Unrecognized relay selection algorithm!");
    }
  
  //Configure discovery for Relay UEs
  std::vector<uint32_t> relayCodes;
  std::vector<uint32_t> relayDestL2Ids;

  std::vector<Time> startTimeRemote;
  std::vector<Time> startTimeRelay;
  
  Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
  rand->SetStream (stream++);
  double discStart;
  std::cout << "Discovery configuration: " << std::endl;

  for (uint32_t i = 1; i <= relayNum; ++i)
    {
      relayCodes.push_back (i+100);
      relayDestL2Ids.push_back (i+500);

      discStart = rand->GetValue (discStartMin, discStartMax);
      startTimeRelay.push_back (Seconds (discStart));
      std::cout << " UE " << i << ": discovery start = " << discStart << " s and discovery interval = " << discInterval.GetSeconds () << " s" << std::endl;
    }
  for (uint32_t j = 1; j <= remoteNum; ++j)
    {    
      discStart = rand->GetValue (discStartMin, discStartMax);
      startTimeRemote.push_back (Seconds (discStart));
      std::cout << " UE " << j+relayNum << ": discovery start = " << discStart << " s and discovery interval = " << discInterval.GetSeconds () << " s" << std::endl;
    }

  //Configure the UL data radio bearer that the relay UE will use for U2N relaying traffic
  Ptr<EpcTft> tftRelay = Create<EpcTft> ();
  EpcTft::PacketFilter pfRelay;
  tftRelay->Add (pfRelay);
  enum EpsBearer::Qci qciRelay;
  qciRelay = EpsBearer::GBR_CONV_VOICE;
  EpsBearer bearerRelay (qciRelay);

  //Start discovery and relay selection
  nrSlProseHelper->StartRemoteRelayConnection (remoteUeNetDev, startTimeRemote,
                                               relayUeNetDev, startTimeRelay,
                                               relayCodes, relayDestL2Ids, 
                                               model, algorithm, 
                                               tftRelay, bearerRelay);

  /*********************** End ProSe configuration ***************************/


  /********* Applications configuration ******/
  ///*
  // install UDP applications
  uint16_t dlPort = 1234;
  uint16_t ulPort = dlPort + gNbNum + 1;
  ApplicationContainer clientApps, serverApps;

  std::cout << "Remote traffic configuration: " << std::endl;
    UdpEchoClientHelper ulClient (remoteHostAddr, ulPort);
  
  // REMOTE UEs TRAFFIC
  for (uint32_t u = 0; u < remoteUeNodes.GetN (); ++u)
    {
      //DL traffic
      PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
      serverApps.Add (dlPacketSinkHelper.Install (remoteUeNodes.Get (u)));

      UdpEchoClientHelper dlClient (ueIpIfaceRemote.GetAddress (u), dlPort);
      std::cout << ueIpIfaceRemote.GetAddress (u) << " , " << dlPort << std::endl;
      dlClient.SetAttribute ("PacketSize", UintegerValue (packetSizeDlUl));
      dlClient.SetAttribute ("Interval", TimeValue (Seconds (1.0 / lambdaDlUl)));
      dlClient.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
      clientApps.Add(dlClient.Install (remoteHost));



      std::cout << " DL: " << remoteHostAddr << " -> " << ueIpIfaceRemote.GetAddress (u) << ":" << dlPort <<
        " start time: " << trafficStart << " s, end time: " << simTime.GetSeconds () << " s" << std::endl;

      Ptr<EpcTft> tftDl = Create<EpcTft> ();
      EpcTft::PacketFilter pfDl;
      pfDl.localPortStart = dlPort;
      pfDl.localPortEnd = dlPort;
      ++dlPort;
      tftDl->Add (pfDl);

      enum EpsBearer::Qci qDl;
      qDl = EpsBearer::GBR_CONV_VOICE;

      EpsBearer bearerDl (qDl);
      nrHelper->ActivateDedicatedEpsBearer (remoteUeNetDev.Get (u), bearerDl, tftDl);

      // UL traffic
      PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
      serverApps.Add (ulPacketSinkHelper.Install (remoteHost));

      ulClient.SetAttribute ("PacketSize", UintegerValue (packetSizeDlUl));
      ulClient.SetAttribute ("Interval", TimeValue (Seconds (1.0 / lambdaDlUl)));
      ulClient.SetAttribute ("MaxPackets", UintegerValue (0xFFFFFFFF));
      clientApps.Add(ulClient.Install (remoteUeNodes.Get (u)));  
      



      std::cout << " UL: " << ueIpIfaceRemote.GetAddress (u)  << " -> " << remoteHostAddr << ":"  << ulPort  <<
        " start time: " << trafficStart << " s, end time: " << simTime.GetSeconds () << " s" << std::endl;

      Ptr<EpcTft> tftUl = Create<EpcTft> ();
      EpcTft::PacketFilter pfUl;
      pfUl.remoteAddress = remoteHostAddr; //IMPORTANT!!!
      pfUl.remotePortStart = ulPort;
      pfUl.remotePortEnd = ulPort;
      ++ulPort;
      tftUl->Add (pfUl);


      enum EpsBearer::Qci qUl;

      qUl = EpsBearer::GBR_CONV_VOICE;
      EpsBearer bearerUl (qUl);
      nrHelper->ActivateDedicatedEpsBearer (remoteUeNetDev.Get (u), bearerUl, tftUl);
      

    }

  
  std::cout << std::endl;
  // Assume dlPacketSinkHelper is the PacketSinkHelper for downlink traffic

  serverApps.Start (Seconds (trafficStart));
  clientApps.Start (Seconds (trafficStart));
  serverApps.Stop (simTime);
  clientApps.Stop (simTime);

  //*/
  /********* END traffic applications configuration ******/
  
  // AsciiTraceHelper ascii;
  // SINR Tracing
  Ptr<OutputStreamWrapper> SinrTraceStream = ascii.CreateFileStream ("NrSinrTrace.txt");
  *SinrTraceStream->GetStream () << "Time (s)\tnodeIp\tsrcIp\tdstIp\tsrcLink\tdstLink" << std::endl;
  for (uint32_t i = 0; i < remoteUeNetDev.GetN (); ++i)
    {
      double rsrp = remoteUeNetDev.Get (i)->GetObject<NrUeNetDevice> ()->GetPhy(0)->GetRsrp();
    } 


  // PC5-S messages tracing
  Ptr<OutputStreamWrapper> Pc5SignallingPacketTraceStream = ascii.CreateFileStream ("NrSlPc5SignallingPacketTrace.txt");
  *Pc5SignallingPacketTraceStream->GetStream () << "Time (s)\tTX/RX\tsrcL2Id\tdstL2Id\tmsgType" << std::endl;
  for (uint32_t i = 0; i < remoteUeNetDev.GetN (); ++i)
    {
      Ptr<NrSlUeProse> prose = remoteUeNetDev.Get (i)->GetObject<NrUeNetDevice> ()->GetSlUeService ()->GetObject <NrSlUeProse> ();
      prose->TraceConnectWithoutContext ("PC5SignallingPacketTrace",
                                         MakeBoundCallback (&TraceSinkPC5SignallingPacketTrace,
                                                            Pc5SignallingPacketTraceStream));
    }
  for (uint32_t i = 0; i < relayUeNetDev.GetN (); ++i)
    {
      Ptr<NrSlUeProse> prose = relayUeNetDev.Get (i)->GetObject<NrUeNetDevice> ()->GetSlUeService ()->GetObject <NrSlUeProse> ();
      prose->TraceConnectWithoutContext ("PC5SignallingPacketTrace",
                                         MakeBoundCallback (&TraceSinkPC5SignallingPacketTrace,
                                                            Pc5SignallingPacketTraceStream));
    }

  // Remote messages tracing
  Ptr<OutputStreamWrapper> RelayNasRxPacketTraceStream = ascii.CreateFileStream ("NrSlRelayNasRxPacketTrace.txt");
  *RelayNasRxPacketTraceStream->GetStream () << "Time (s)\tnodeIp\tsrcIp\tdstIp\tsrcLink\tdstLink" << std::endl;
  for (uint32_t i = 0; i < relayUeNetDev.GetN (); ++i)
    {
      Ptr<EpcUeNas> epcUeNas = relayUeNetDev.Get (i)->GetObject<NrUeNetDevice> ()->GetNas ();

      epcUeNas->TraceConnectWithoutContext ("NrSlRelayRxPacketTrace",
                                            MakeBoundCallback (&TraceSinkRelayNasRxPacketTrace,
                                                               RelayNasRxPacketTraceStream));
    }    

  //Enable discovery traces
  nrSlProseHelper->EnableDiscoveryTraces ();

  //Enable relay traces
  nrSlProseHelper->EnableRelayTraces ();

  // Flow Monitor
  Ptr<FlowMonitor> monitor;
  FlowMonitorHelper flowMonitorHelper;
  monitor = flowMonitorHelper.InstallAll();


  //Rem parameters
  uint16_t remBwpId = 0;
  double xMin = -20.0;
  double xMax = 20.0;
  uint16_t xRes = 50;
  double yMin = 0.0;
  double yMax = 800.0;
  uint16_t yRes = 200;
  double z = 1.5;

  //Radio Environment Map Generation for ccId 0

  Ptr<NrRadioEnvironmentMapHelper> remHelper = CreateObject<NrRadioEnvironmentMapHelper> ();
  remHelper->SetMinX (xMin);
  remHelper->SetMaxX (xMax);
  remHelper->SetResX (xRes);
  remHelper->SetMinY (yMin);
  remHelper->SetMaxY (yMax);
  remHelper->SetResY (yRes);
  remHelper->SetZ (z);
  remHelper->SetSimTag (simTag);

  enbNetDev.Get (0)->GetObject<NrGnbNetDevice> ()->GetPhy (remBwpId)->GetSpectrumPhy(0)->GetBeamManager ()->ChangeBeamformingVector (remoteUeNetDev.Get (0));


  // if (remMode == "BeamShape")
  //   {
  //     remHelper->SetRemMode (NrRadioEnvironmentMapHelper::BEAM_SHAPE);

  //     if (typeOfRem.compare ("DlRem") == 0)
  //       {
  //         remHelper->CreateRem (enbNetDev, remoteUeNetDev.Get (0), remBwpId);
  //       }
  //     else if (typeOfRem.compare ("UlRem") == 0)
  //       {
  //         remHelper->CreateRem (remoteUeNetDev, enbNetDev.Get (0), remBwpId);
  //       }
  //     else
  //       {
  //         NS_ABORT_MSG ("typeOfRem not supported. "
  //                       "Choose among 'DlRem', 'UlRem'.");
  //       }
  //   }
  // else if (remMode == "CoverageArea")
  //   {
  //     remHelper->SetRemMode (NrRadioEnvironmentMapHelper::COVERAGE_AREA);
  //     remHelper->CreateRem (enbNetDev, remoteUeNetDev.Get (0), remBwpId);
  //   }
  // else if (remMode == "UeCoverage")
  //   {
  //     remHelper->SetRemMode (NrRadioEnvironmentMapHelper::UE_COVERAGE);
  //     remHelper->CreateRem (remoteUeNetDev, enbNetDev.Get (0), remBwpId);
  //   }
  // else
  //   {
  //     NS_ABORT_MSG ("Not supported remMode.");
  //   }


  //Run the simulation
  Simulator::Stop (simTime);
  p2ph.EnablePcapAll ("pcap-scratch");
  AnimationInterface Anim ("scratch.xml");
  Simulator::Run ();
  
  //Write traces
  std::cout << "/*********** Simulation done! ***********/\n"  << std::endl;
  std::cout << "Number of packets relayed by the L3 UE-to-Network relays:"  << std::endl;
  std::cout << " relayIp      srcIp->dstIp      srcLink->dstLink\t\tnPackets"  << std::endl;
  for (auto it = g_relayNasPacketCounter.begin (); it != g_relayNasPacketCounter.end (); ++it)
    {
      std::cout << " " << it->first << "\t\t" << it->second << std::endl;
    }
  
  int j=0;
  float AvgThroughput = 0;
  Time Jitter;
  Time Delay;
  uint32_t SentPackets = 0;
  uint32_t ReceivedPackets = 0;
  uint32_t DroppedPackets = 0;
  uint32_t totalLostPackets=0;
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitorHelper.GetClassifier ());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();

  std::ofstream flowOut("flowmon-relayservice-flowstats");
  for (auto iter = stats.begin (); iter != stats.end (); ++iter) {
          SentPackets = SentPackets +(iter->second.txPackets);
          ReceivedPackets = ReceivedPackets + (iter->second.rxPackets);
          DroppedPackets += iter->second.txPackets - iter->second.rxPackets;
          AvgThroughput = AvgThroughput + iter->second.rxBytes;
          
          Delay = Delay + (iter->second.delaySum);
          Jitter=Jitter+iter->second.jitterSum;
          totalLostPackets+=iter->second.lostPackets;
          j = j + 1;
  }
  AvgThroughput = AvgThroughput * 8.0 / ((10-1.0) * 1024.0);
  Delay = Delay / ReceivedPackets;
  Jitter=Jitter/ReceivedPackets;

  
  flowOut << "--------Total Results of the simulation----------"<<std::endl << "\n";
  flowOut << "Total Received Packets = " << ReceivedPackets << "\n";
  flowOut << "Total sent packets  = " << SentPackets << "\n";
  flowOut << "Total Lost Packets = " << DroppedPackets << "\n";
  flowOut << "Packet Loss ratio = " << ((DroppedPackets*100.00)/SentPackets)<< " %" << "\n";
  flowOut << "Packet delivery ratio = " << ((ReceivedPackets*100.00)/SentPackets)<< " %" << "\n";
  flowOut << "Average Throughput = " << AvgThroughput<< " Kbps" << "\n";
  flowOut << "End to End Delay = " << Delay << "\n";
  flowOut << "Total Flow id " << j << "\n";

  
  std::cout << "Results\n\n";
  std::cout << "Total sent packets  = " << SentPackets << "\n";
  std::cout << "Total Received Packets = " << ReceivedPackets << "\n";
  std::cout << "Total Lost packets =" << DroppedPackets <<"\n";
  std::cout << "End to End Delay = " << Delay << "\n";
  std::cout << "Jitter = " << Jitter << "\n";
  std::cout << "Packet delivery ratio = " << ReceivedPackets << " / " << SentPackets << " = " << ((ReceivedPackets*100.00)/SentPackets)<< "%" << "\n";
  std::cout << "Packet Loss ratio = " << ((DroppedPackets*100.00)/SentPackets)<< " %" << "\n";
  std::cout << "Average Throughput = " << AvgThroughput<< "Kbps" << "\n\n";
  
  // std::string tr_name ("");
  MobilityHelper::EnableAsciiAll (ascii.CreateFileStream ("relayservice.mob"));
  flowMonitorHelper.SerializeToXmlFile("relayservice-flowmon.xml", true, true);
  flowOut.close();


  Simulator::Destroy ();
  return 0;
}
