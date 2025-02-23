package modules.QRSA;
@namespace(quisp::modules);

import modules.QRSA.ConnectionManager.*;
import modules.QRSA.HardwareMonitor.*;
import modules.QRSA.RealTimeController.*;
import modules.QRSA.RoutingDaemon.*;
import modules.QRSA.RuleEngine.*;

module quantumRoutingSoft
{
    parameters:
        int address;
        int number_of_qnics;
        int number_of_qnics_r;
        int number_of_qnics_rp;
        bool simultaneous_es_enabled;
    gates:
        //inout rdPort;//size will be determined by the number of connections defined at the end of this file
        inout cmPort;
        inout rePort;
        inout hmPort;
        inout qnics[] @loose;
        //inout QNICPort[1];//Maybe more than one if not ENDNODE

    submodules:
        rd: RoutingDaemon {
            parameters:
                address = address;//share the same address within all inner modules
                @display("p=115,63");
        }
        hm: HardwareMonitor {
            parameters:
                address = address;//share the same address within all inner modules
                @display("p=115,231");
                number_of_qnics = number_of_qnics;
                number_of_qnics_r = number_of_qnics_r;
                number_of_qnics_rp = number_of_qnics_rp;

        }
        cm: ConnectionManager {
            parameters:
                address = address;//share the same address within all inner modules
                number_of_qnics = number_of_qnics;
                number_of_qnics_r = number_of_qnics_r;
                number_of_qnics_rp = number_of_qnics_rp;
                total_number_of_qnics = number_of_qnics +  number_of_qnics_r + number_of_qnics_rp;
                simultaneous_es_enabled = simultaneous_es_enabled;
                @display("p=227,148");
        }
        rt: RealTimeController {
            parameters:
                address = address;//share the same address within all inner modules
                @display("p=227,231");
        }
        re: RuleEngine {
            parameters:
                address = address;//share the same address within all inner modules
                @display("p=227,63");
                number_of_qnics = number_of_qnics;
                number_of_qnics_r = number_of_qnics_r;
                number_of_qnics_rp = number_of_qnics_rp;
                total_number_of_qnics = number_of_qnics +  number_of_qnics_r + number_of_qnics_rp;


        }
    connections:
        cm.RouterPort <--> cmPort;
        re.RouterPort <--> rePort;
        re.HardwareMonitorPort <--> hm.RuleEnginePort;
        hm.RouterPort <--> hmPort;

        //for i=0..sizeof(qnics)-1{
        //    rd.dummyQNICLink++ <--> qnics[i];
        //}

}
