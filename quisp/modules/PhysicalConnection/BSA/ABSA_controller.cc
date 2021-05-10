#include "ABSA_Controller.h"

namespace quisp {
namespace modules {

Define_Module(ABSAController);

ABSAController::ABSAController() {}

void ABSAController::initialize(int stage) {
  EV<<"ABSA controller booted"<<"\n";

  current_trial_id = dblrand();
  handshake = false;
  auto_resend_ABSANotifier = true;
  // address of absa measurement node
  address = par("address");
  
  // ABSA must be connected two RGS source nodes
  if (getParentModule()->gateSize("quantum_absa_port") != 2) {
    error("No more or less than 2 neighbors are allowed for ABSA.", getParentModule()->gateSize("quantum_absa_port"));
    endSimulation();
  }
  // Checking Neighbor information
  checkNeighborAddress();
  checkNeighborBuffer();
  updateIDE_Parameter();

  // Should be parametrized?
  accepted_burst_interval = (double)1 / (double)photon_detection_per_sec;

  // Packet to start absa process
  ABSAstart *generatePacket = new ABSAstart;

  // emit the packets
  scheduleAt(simTime() + par("Initial_notification_timing_buffer"), generatePacket);
}

// sending notifiers to neighbors
void ABSAController::sendNotifiers() {
  // calculate time to travel
  // TODO: we would have to think about generation time of graph state
  double time = calculateTimeToTravel(max_neighbor_distance, speed_of_light_in_channel);  // When the packet reaches = simitme()+time
  
  // generate timing notifier for first node
  ABSMtimingNotifier *pk = generateNotifier(time, speed_of_light_in_channel, distance_to_neighbor, neighbor_address, accepted_burst_interval, photon_detection_per_sec, max_buffer);
  double first_nodes_timing = calculateEmissionStartTime(time, distance_to_neighbor, speed_of_light_in_channel);
  pk->setTiming_at(first_nodes_timing);  // Tell neighboring nodes to shoot photons so that the first one arrives at ABSA at the specified timing

  // generate timing notifier for second node
  ABSMtimingNotifier *pkt = generateNotifier(time, speed_of_light_in_channel, distance_to_neighbor_two, neighbor_address_two, accepted_burst_interval, photon_detection_per_sec, max_buffer);
  double second_nodes_timing = calculateEmissionStartTime(time, distance_to_neighbor_two, speed_of_light_in_channel);
  pkt->setTiming_at(second_nodes_timing);  // Tell neighboring nodes to shoot photons so that the first one arrives at ABSA at the specified timing
  // If you want some uncertainty in timing calculation, maybe second_nodes_timing+uniform(-n,n) helps

  try {
    send(pk, "toRouter_port");  // send to port out. connected to local routing module (routing.localIn).
    send(pkt, "toRouter_port");
  } catch (cTerminationException &e) {
    error("Error in ABSA_Controller.cc. It does not have port named toRouter_port.");
    endSimulation();
  } catch (std::exception &e) {
    error("Error in ABSA_Controller.cc. It does not have port named toRouter_port.");
    endSimulation();
  }
}

// handling message
void ABSAController::handleMessage(cMessage *msg) {

  if (dynamic_cast<ABSAstart *>(msg) != nullptr) {
    // when the absa process start, first  ABSA sends notifiers to neighbors
    sendNotifiers();
    delete msg;
    return;
  } else if (dynamic_cast<ABSAresult *>(msg) != nullptr) {
    auto_resend_ABSANotifier = false;  // Photon is arriving. No need to auto reschedule next round. Wait for the last photon fron either node.
    bubble("ABSAresult accumulated");
    BSAresult *pk = check_and_cast<BSAresult *>(msg);
    bool entangled = pk->getEntangled();
    int prev = getStoredABSAresultsSize();
    pushToABSAresults(entangled);
    int aft = getStoredABSAresultsSize();
    if (prev + 1 != aft) {
      error("Nahnah nah!");
    }
  } else if (dynamic_cast<ABSAfinish *>(msg) != nullptr) {  // Last photon from either node arrived.
    bubble("ABSAresult accumulated");
    BSAfinish *pk = check_and_cast<BSAfinish *>(msg);
    pushToABSAresults(pk->getEntangled());
    int stored = getStoredABSAresultsSize();
    char moge[sizeof(stored)];
    sprintf(moge, "%d", stored);
    bubble(moge);

    bubble("done");
    sendABSAresultsToNeighbors();  // memory leak
    clearABSAresults();
    auto_resend_ABSANotifier = true;
    current_trial_id = dblrand();
    // Schedule a checker with a time-out t, to see if both actually sent something.
    // Worst case is, when both have no free qubit, and no qubits get transmitted. In that case, this module needs to recognize that problem, and reschedule/resend the request
    // after a cetrain time.
  } else if (dynamic_cast<ABSAtimeoutChecker *>(msg) != nullptr) {
    BSAtimeoutChecker *pk = check_and_cast<BSAtimeoutChecker *>(msg);
    if (auto_resend_ABSANotifier == true && pk->getTrial_id() == current_trial_id) {
      // No photon came from both nodes. All of the resources must have been busy that time.
    }
  }
  delete msg;
}

// This method checks the address of the neighbors.
void ABSAController::checkNeighborAddress() {
  try {
    neighbor_address = getParentModule()->gate("quantum_absa_port$o", 0)->getNextGate()->getOwnerModule()->par("address");
    neighbor_address_two = getParentModule()->gate("quantum_absa_port$o", 1)->getNextGate()->getOwnerModule()->par("address");
    distance_to_neighbor = getParentModule()->gate("quantum_absa_port$o", 0)->getChannel()->par("distance");
    distance_to_neighbor_two = getParentModule()->gate("quantum_absa_port$o", 1)->getChannel()->par("distance");
    max_neighbor_distance = std::max(distance_to_neighbor, distance_to_neighbor_two);
  } catch (std::exception &e) {
    error("Error in ABSA_Controller.cc. Your stand-alone ABSA module may not connected to the neighboring node (quantum_port).");
    endSimulation();
  }
}

// Checks the buffer size of the connected qnics.
void ABSAController::checkNeighborBuffer() {
  try {
    neighbor_buffer = getParentModule()->gate("quantum_absa_port$o", 0)->getNextGate()->getNextGate()->getOwnerModule()->par("numBuffer");
    neighbor_buffer_two = getParentModule()->gate("quantum_absa_port$o", 1)->getNextGate()->getNextGate()->getOwnerModule()->par("numBuffer");
    max_buffer = std::min(neighbor_buffer, neighbor_buffer_two);  // Both nodes should transmit the same amount of photons.
  } catch (std::exception &e) {
    error("Error in ABSA_Controller.cc. ABSA couldnt find parameter numBuffer in the neighbor's qnic.");
    endSimulation();
  }
}

void ABSAController::updateIDE_Parameter() {
  try {
    photon_detection_per_sec = (int)par("photon_detection_per_sec");
    if (photon_detection_per_sec <= 0) {
      error("Photon detection per sec for ABSA must be more than 0.");
    }
    par("neighbor_address") = neighbor_address;
    par("neighbor_address_two") = neighbor_address_two;
    par("distance_to_neighbor") = distance_to_neighbor;
    par("distance_to_neighbor_two") = distance_to_neighbor_two;
    par("max_neighbor_distance") = max_neighbor_distance;
    par("max_buffer") = max_buffer;
    c = &par("Speed_of_light_in_fiber");
    speed_of_light_in_channel = c->doubleValue();  // per sec
  } catch (std::exception &e) {
    error(e.what());
  }
}

// Generates a ABSA timing notifier. This is also called only once for the same reason as sendNotifiers().
ABSMtimingNotifier *ABSAController::generateNotifier(double time, double speed_of_light_in_channel, double distance_to_neighbor, int destAddr, double accepted_burst_interval,
                                                   int photon_detection_per_sec, int max_buffer) {
  ABSMtimingNotifier *pk = new ABSMtimingNotifier();
  if (handshake == false)
    pk->setNumber_of_qubits(-1);  // if -1, neighbors will keep shooting photons anyway.
  else
    pk->setNumber_of_qubits(max_buffer);
  pk->setSrcAddr(getParentModule()->par("address"));  // packet source setting
  pk->setDestAddr(destAddr);
  pk->setKind(ABSAtimingNotifier_type);
  pk->setInterval(accepted_burst_interval);

  // If very high, all photons can nearly be sent here(to ABSA module) from neighboring nodes simultaneously
  pk->setAccepted_photons_per_sec(photon_detection_per_sec);
  double timing = calculateEmissionStartTime(time, distance_to_neighbor, speed_of_light_in_channel);

  // Tell neighboring nodes to shoot photons so that the first one arrives at ABSA at the specified timing
  pk->setTiming_at(timing);
  return pk;
}

// Generates a packet that includes the ABSA timing notifier and the ABSA entanglement attempt results.
CombinedABSAresults *ABSAController::generateNotifier_c(double time, double speed_of_light_in_channel, double distance_to_neighbor, int destAddr, double accepted_burst_interval,
                                                      int photon_detection_per_sec, int max_buffer) {
  CombinedABSAresults *pk = new CombinedABSAresults();
  if (handshake == false)
    pk->setNumber_of_qubits(-1);  // if -1, neighbors will keep shooting photons anyway.
  else
    pk->setNumber_of_qubits(max_buffer);
  pk->setSrcAddr(getParentModule()->par("address"));  // packet source setting
  pk->setDestAddr(destAddr);
  pk->setKind(ABSAtimingNotifier_type);
  pk->setInterval(accepted_burst_interval);

  // If very high, all photons can nearly be sent here(to ABSA module) from neighboring nodes simultaneously
  pk->setAccepted_photons_per_sec(photon_detection_per_sec);
  double timing = calculateEmissionStartTime(time, distance_to_neighbor, speed_of_light_in_channel);

  // Tell neighboring nodes to shoot photons so that the first one arrives at ABSA at the specified timing
  pk->setTiming_at(timing);
  return pk;
}

// Depending on the distance to the neighbor QNIC, this calculates when the neighbor needs to start the emission.
// The farther node emits it instantaneously, while the closer one needs to wait because 2 photons need to arrive at ABSA simultaneously.
double ABSAController::calculateEmissionStartTime(double time, double distance_to_node, double c) {
  // distance_to_node is the distance to ABSA to self
  double self_timeToTravel = calculateTimeToTravel(distance_to_node, c);

  // If shorter distance, then the node needs to wait a little for the other node with the longer distance
  if ((self_timeToTravel) != time) {
    // Waiting time
    return (time - self_timeToTravel) * 2;
  } else {
    // No need to wait
    return 0;
  }
}

double ABSAController::calculateTimeToTravel(double distance, double c) { return (distance / c); }

void ABSAController::BubbleText(const char *txt) {
  if (hasGUI()) {
    char text[32];
    sprintf(text, "%s", txt);
    bubble(text);
  }
}

cModule *ABSAController::getQNode() {
  // We know that Connection manager is not the QNode, so start from the parent.
  cModule *currentModule = getParentModule();
  try {
    cModuleType *QNodeType = cModuleType::get("networks.QNode");  // Assumes the node in a network has a type QNode
    while (currentModule->getModuleType() != QNodeType) {
      currentModule = currentModule->getParentModule();
    }
    return currentModule;
  } catch (std::exception &e) {
    error("No module with QNode type found. Have you changed the type name in ned file?");
    endSimulation();
  }
  return currentModule;
}

void ABSAController::pushToABSAresults(bool attempt_success) {
  int prev = getStoredABSAresultsSize();
  results[getStoredABSAresultsSize()] = attempt_success;
  int aft = getStoredABSAresultsSize();
  if (prev + 1 != aft) {
    error("Not working correctly");
  }
}
int ABSAController::getStoredABSAresultsSize() { return results.size(); }
void ABSAController::clearABSAresults() { results.clear(); }

// Instead of sendNotifiers, we invoke this during the simulation to return the next ABSA timing and the result.
// This should be simplified more.
void ABSAController::sendABSAresultsToNeighbors() {
  if (!passive) {
    CombinedABSAresults *pk, *pkt;

    double time = calculateTimeToTravel(max_neighbor_distance, speed_of_light_in_channel);  // When the packet reaches = simitme()+time
    pk = generateNotifier_c(time, speed_of_light_in_channel, distance_to_neighbor, neighbor_address, accepted_burst_interval, photon_detection_per_sec, max_buffer);
    double first_nodes_timing = calculateEmissionStartTime(time, distance_to_neighbor, speed_of_light_in_channel);

    // Tell neighboring nodes to shoot photons so that the first one arrives at ABSA at the specified timing
    pk->setTiming_at(first_nodes_timing);
    pk->setSrcAddr(address);
    pk->setDestAddr(neighbor_address);
    pk->setList_of_failedArraySize(getStoredABSAresultsSize());
    pk->setKind(5);

    pkt = generateNotifier_c(time, speed_of_light_in_channel, distance_to_neighbor_two, neighbor_address_two, accepted_burst_interval, photon_detection_per_sec, max_buffer);
    double second_nodes_timing = calculateEmissionStartTime(time, distance_to_neighbor_two, speed_of_light_in_channel);
    pkt->setTiming_at(second_nodes_timing);  // Tell neighboring nodes to shoot photons so that the first one arrives at ABSA at the specified timing
    EV << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!list of failed size = " << getStoredABSAresultsSize() << "\n";
    pkt->setSrcAddr(address);
    pkt->setDestAddr(neighbor_address_two);
    pkt->setList_of_failedArraySize(getStoredABSAresultsSize());
    pkt->setKind(5);

    for (auto it : results) {
      int index = it.first;
      bool entangled = it.second;
      // std::cout<<index<<" th, entangled = "<<entangled<<"\n";
      pk->setList_of_failed(index, entangled);
      pkt->setList_of_failed(index, entangled);
    }
    send(pk, "toRouter_port");
    send(pkt, "toRouter_port");

  } else {  // For SPDC type link
    CombinedBSAresults_epps *pk, *pkt;
    pk = new CombinedBSAresults_epps();
    pkt = new CombinedBSAresults_epps();

    pk->setSrcAddr(address);
    pk->setDestAddr(neighbor_address);
    pk->setList_of_failedArraySize(getStoredABSAresultsSize());
    pk->setKind(6);

    pkt->setSrcAddr(address);
    pkt->setDestAddr(neighbor_address_two);
    pkt->setList_of_failedArraySize(getStoredABSAresultsSize());
    pkt->setKind(6);

    EV << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!list of failed size = " << getStoredABSAresultsSize() << "\n";

    for (auto it : results) {
      int index = it.first;
      bool entangled = it.second;
      pk->setList_of_failed(index, entangled);
      pkt->setList_of_failed(index, entangled);
    }
    send(pk, "toRouter_port");
    send(pkt, "toRouter_port");
  }
}

// When the ABSA is passive, it does not know how many qubits to emit (because it depends on the neighbor's).
// Therefore, the EPPS sends a classical packet that includes such information.
// When CM receives it, it will also have to update the max_buffer of ABSAController, to know when the emission end and send the classical ABSAresults to the neighboring EPPS.
void ABSAController::setMax_buffer(int buffer) {
  Enter_Method("setMax_buffer()");
  if (!passive) {
    return;
  } else {
    max_buffer = buffer;
    par("max_buffer") = buffer;
  }
}


}  // namespace modules
}  // namespace quisp
