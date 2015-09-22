/*
 * uniqueEventsId.h
 *
 *  Created on: 26 mars 2013
 *      Author: dom
 */

#ifndef UNIQUEEVENTSID_H_
#define UNIQUEEVENTSID_H_

#define EVENT_GENERIC								1
#define EVENT_CODE_START							2
#define EVENT_NI_START_TRANSMITTING					3
#define EVENT_NI_STOP_TRANSMITTING					4
#define EVENT_NI_RECEIVE							5
#define EVENT_NI_ENQUEUE_OUTGOING_MESSAGE 			6
#define EVENT_PROCESS_LOCAL_EVENT					7
#define EVENT_BLOCK_TIMER							8
#define EVENT_END_SIMULATION						9
#define BLOCKEVENT_GENERIC							10
#define EVENT_SAVE_SCREEN                           11

#define EVENT_VM_START_COMPUTATION					1001
#define EVENT_VM_END_COMPUTATION					1002
#define EVENT_VM_START_TRANSMISSION					1005
#define EVENT_VM_WAIT_FOR_COMMAND					1006
#define EVENT_VM_TIMEOUT_WAIT_FOR_COMMAND			1007

// VM
#define EVENT_SET_ID								1050
#define EVENT_STOP									1051
#define EVENT_HANDLE_DEBUG_COMMAND					1058
#define EVENT_DEBUG_PAUSE_SIMULATION				1059
#define EVENT_SET_DETERMINISTIC						1060
#define EVENT_RESUME_COMPUTATION					1061
#define EVENT_EXPECTED_COMPUTATION_PAUSE			1062
#define EVENT_EFFECTIVE_COMPUTATION_PAUSE			1063
#define EVENT_END_POLL								1064
#define EVENT_COMPUTE_PREDICATE                             1065
#define EVENT_ADD_TUPLE                                     1066
#define EVENT_REMOVE_TUPLE                                  1067

// General
#define EVENT_ADD_NEIGHBOR							1052
#define EVENT_REMOVE_NEIGHBOR						1053
#define EVENT_TAP									1054
#define EVENT_SET_COLOR								1055
#define EVENT_SET_POSITION							1068
#define EVENT_SEND_MESSAGE							EVENT_VM_START_TRANSMISSION
#define EVENT_RECEIVE_MESSAGE 						EVENT_NI_RECEIVE
#define EVENT_ACCEL									1056
#define EVENT_SHAKE									1057

// ROBOT BLOCKS
#define EVENT_MOTION_START							1101
#define EVENT_MOTION_STOP							1102
#define EVENT_MOTION_STEP							1103
#define EVENT_MOTION_END							1104


#endif /* UNIQUEEVENTSID_H_ */
