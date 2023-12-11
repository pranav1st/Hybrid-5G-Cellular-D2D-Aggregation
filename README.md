# Hybrid-5G-Cellular-D2D-Aggregation

## Brief description of method followed

The prototype is simulated on  NS3 or Network Simulator 3. It contains a gNodeB, a UE node which acts as a relay and another UE node acting as remote node. A “ConcreteWithoutWindows Residential” building, 40m wide, is placed 600m away from the gNodeB, The gNodeB and the Relay Node uses a Constant Position Mobility Model which is situated 450m from the gNodeB towards the Relay node, whereas the Remote Node moves according to Constant Velocity Mobility Model with a speed of 1 m/s by default along the Y-axis moving towards the building. The ProSe-enabled UEs and gNodeB implement the 3GPP UMa Pathloss Model. The transmit power is set as 46 dB for gNodeB and 23 dB for the UEs. The model will use one operation band, containing a single component carrier and 2 bandwidth parts. One bandwidth part is programmed to use for Uplink and Downlink relay communication with gNodeB and relay UE. Whereas, the other bandwidth part is used for sidelink communication between the Remote node and the Relay node. 
Each scenario starts with the device-discovery phase, in order to get information about the nodes in their vicinity. In our model, we will use Mode A discovery which does not require any assistance from the network, thereby, increasing its efficiency. The relay selection algorithm then chooses the best Relay node present (if present) based on the Max RSRP value. The client (transmitter) and server (receiver) applications are installed on both the UEs as well as the remote Host to enable communication of sixty packets with size of 1000 bytes each are sent every second.

## Simulation Setup:

   - Set simulation time to 20 seconds.
   - Use the 3GPP channel with NR ProSe Sidelink Network.
   - Frequency: 5890 MHz, Bandwidth: 40 MHz, 2 Bandwidth Parts.
   - 1 gNodeB, 1 Relay UE, and 1 Remote UE.
   - Use the UMa Pathloss Model for both Line of Sight (LoS) and Non-Line of Sight (NLoS) propagation.
   - Set transmission power for gNodeB to 46 dBm and for UE to 23 dBm.
   - Place a Concrete Residential Building, 40m wide, 600m away from gNodeB.
   - Set eNodeB height to 30m and UE height to 1.5m.

## Scenario Description

   **Scenario 1: (Relay-disabled LoS)**

   - Place Remote UE outdoors, 550m away from gNodeB.
   - Remote UE moves at 1 m/s away from gNB.
   - Measure Throughput, Packet Delivery Ratio, and End-to-End Delay.

   **Scenario 2: (Relay-disabled NLoS)**

   - Place Remote UE 50m inside a 300m long Residential Building.
   - Start 600m away from gNodeB, moving at 1 m/s away from gNB.
   - Measure Throughput, Packet Delivery Ratio, and End-to-End Delay.

   **Scenario 3: (Relay-enabled LoS)**

   - Place Remote UE outdoors, 600m away from gNodeB.
   - Place Relay UE at 450m from gNodeB.
   - Remote UE moves at 1 m/s away from gNB.
   - Repeat for different Remote UE locations (700m, 800m).
   - Measure Throughput, Packet Delivery Ratio, and End-to-End Delay.

   **Scenario 4: (Relay-enabled NLoS)**

   - Place Remote UE inside a Residential Building, 600m away from gNodeB.
   - Move at 1 m/s away from gNB.
   - Repeat for different Remote UE locations (700m, 800m).
   - Measure Throughput, Packet Delivery Ratio, and End-to-End Delay.

## Steps

1. **Discovery and Relay Selection:**

   - Start with Mode A discovery for device information.
   - Relay selection based on Max RSRP.

2. **Communication Applications:**

   - Install client and server applications on UEs and Remote Host.
   - Transmit 60 packets of 1000 bytes each every second.

3. **Performance Metrics:**

   - Evaluate Throughput, Packet Delivery Ratio, and End-to-End Delay for each scenario.

4. **Mobility Models:**

   - Use Constant Position Mobility Model for gNodeB and Relay UE.
   - Use Constant Velocity Mobility Model for Remote UE.

5. **Output and Analysis:**

   - Record simulation results for analysis.
   - Visualize the simulation using pcap files, animation, and mobility traces.

6. **Simulation Execution:**
   - Run the simulation.


## Installation

Get the code and move into psc-ns3 directory

```bash
git clone "https://github.com/usnistgov/psc-ns3.git" -b wns3-2023-nr-prose-preview
```
```bash
cd psc-ns3
```
Setup ns-3

```bash
./ns3 configure --enable-examples
```
```bash
./ns3
```

## Usage

NR ProSe Scenario - 1

```bash
mkdir output_scenario-1 
```

```bash
./ns3 run 'scenario-1' --cwd='output_scenario-1'
```

NR ProSe Scenario - 2

```bash
mkdir output_scenario-2 
```

```bash
./ns3 run 'scenario-2' --cwd='output_scenario-2'
```

NR ProSe Scenario - 3

```bash
mkdir output_scenario-3 
```

```bash
./ns3 run 'scenario-3' --cwd='output_scenario-3'
```

NR ProSe Scenario - 4

```bash
mkdir output_scenario-4 
```

```bash
./ns3 run 'scenario-4' --cwd='output_scenario-4'
```
