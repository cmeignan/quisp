/*
 * ConnectionManager.h
 *
 *  Created on: Sep 14, 2020
 *      Author: zigen
 */

#ifndef MODULES_CONNECTIONMANAGER_H_
#define MODULES_CONNECTIONMANAGER_H_

#include <string>
#include "IConnectionManager.h"

using namespace omnetpp;
using namespace quisp::messages;
using namespace quisp::rules;

namespace quisp {
namespace modules {


  typedef struct {
    int children;
    QNIC_pair_info QNIC_pair;
    int end_node;
  } PathLink; //CM

/** \class ConnectionManager ConnectionManager.cc
 *
 *  \brief ConnectionManager
 *
 * The ConnectionManager is one of the five key modules in the
 * software for a quantum repeater/router (qrsa).  It is responsible for
 * managing the connections: initiating ConnectionSetupRequests,
 * behaving as responder for a ConnectionSetupRequest (which involves
 * actually creating the RuleSets), and handling the requests and
 * responses as the move along the path at setup time.
 *
 * It communicates with the RuleEngine, which is responsible for
 * actually executing the Rules as it is notified of events, but
 * the ConnectionManager has _nothing_ to do with the actual
 * processing of the quantum states as they evolve.
 *
 * You will see member functions for the roles as initiator, responder,
 * and intermediate node.  The main tasks are to respond to ConnectionSetupRequest,
 * ConnectionSetupResponse, RejectConnectionSetupRequest, and ConnectionTeardown messages.
 *
 * It is also responsible for the end-to-end reservation of resources,
 * as dictated by the multiplexing (muxing) discipline in use.
 */
class ConnectionManager : public IConnectionManager {
 public:
  ConnectionManager();
  utils::ComponentProvider provider;

 protected:
  int my_address;
  int num_of_qnics;
  std::map<int, std::queue<ConnectionSetupRequest *>> connection_setup_buffer;  // key is qnic address
  std::map<int, int> connection_retry_count;  // key is qnic address

  std::map<int, std::vector<PathLink>> tree_path_test; // CM: Taking both into a struct after
  std::map<int, std::vector<int>> tree_path;
  std::map<int, int> correction_number_for_node;

  std::map<int, bool> qnic_res_table;
  std::vector<cMessage *> request_send_timing;  // self message, notification for sending out request
  bool simultaneous_es_enabled;
  bool es_with_purify;
  int num_remote_purification;
  int number_of_state_initiated = 0; // To label created states
  IRoutingDaemon *routing_daemon;
  IHardwareMonitor *hardware_monitor;

  void initialize() override;
  void handleMessage(cMessage *msg) override;
  void finish() override;

  void respondToRequest(ConnectionSetupRequest *pk);
  void tryRelayRequestToNextHop(ConnectionSetupRequest *pk);

  void queueApplicationRequest(ConnectionSetupRequest *pk);
  void initiateApplicationRequest(int qnic_address);
  void scheduleRequestRetry(int qnic_address);
  void popApplicationRequest(int qnic_address);
  void handleMultipartiteRequest(ConnectionSetupRequest *req, int actual_dst, int actual_src); //CM
  void relayRequestToNextHop(ConnectionSetupRequest *pk);
  void storeRuleSetForApplication(ConnectionSetupResponse *pk);
  void storeRuleSet(ConnectionSetupResponse *pk);

  void initiator_reject_req_handler(RejectConnectionSetupRequest *pk);
  void responder_reject_req_handler(RejectConnectionSetupRequest *pk);
  void intermediate_reject_req_handler(RejectConnectionSetupRequest *pk);

  void rejectRequest(ConnectionSetupRequest *req);

  SwappingConfig generateSwappingConfig(int swapper_address, std::vector<int> path, std::map<int, std::vector<int>> swapping_partners, std::vector<QNIC_pair_info> qnics,
                                        int num_resources);
  SwappingConfig generateSimultaneousSwappingConfig(int swapper_address, std::vector<int> path, std::vector<QNIC_pair_info> qnics, int num_resources);

  // Rule generators
  std::unique_ptr<Rule> purificationRule(int partner_address, int purification_type, int num_purification, QNIC_type qnic_type, int qnic_index, unsigned long ruleset_id,
                                         unsigned long rule_id);
  std::unique_ptr<Rule> swappingRule(SwappingConfig conf, unsigned long ruleset_id, unsigned long rule_id);
  std::unique_ptr<Rule> simultaneousSwappingRule(SwappingConfig conf, std::vector<int> path, unsigned long ruleset_id, unsigned long rule_id);
  std::unique_ptr<Rule> waitRule(int partner_address, int next_partner_address, unsigned long ruleset_id, unsigned long rule_id);
  std::unique_ptr<Rule> tomographyRule(int owner_address, int partner_address, int num_measure, QNIC_type qnic_type, int qnic_index, unsigned long ruleset_id,
                                       unsigned long rule_id);

  void reserveQnic(int qnic_address);
  void releaseQnic(int qnic_address);
  bool isQnicBusy(int qnic_address);
  QNIC_id getQnicInterface(int owner_address, int partner_address, std::vector<int> path, std::vector<QNIC_pair_info> qnics);

  unsigned long createUniqueId();
  static int computePathDivisionSize(int l);
  static int fillPathDivision(std::vector<int> path, int i, int l, int *link_left, int *link_right, int *swapper, int fill_start);

  PathLink getFather(int node);  // CM
  int getFatherAdress(int node);
  RuleSet *generateGeneralizedEntanglementSwappingRuleSet(int node, std::vector<PathLink> children_link, int size_tree);  // CM
};
}  // namespace modules
}  // namespace quisp
#endif /* MODULES_CONNECTIONMANAGER_H_ */
