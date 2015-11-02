#include <iostream>
#include <sstream>
#include <boost/asio.hpp>
#include "scheduler.h"
#include "network.h"

#include "blinkyMeldBlockCode.h"
#include "meldInterpretEvents.h"
#include "meldInterpretMessages.h"
#include "meldInterpretVM.h"

#include "trace.h"

#include "global.h"

using namespace std;
using namespace BlinkyBlocks;
using namespace MeldInterpret;

BlinkyMeldBlockCode::BlinkyMeldBlockCode(BlinkyBlocksBlock *host): BlinkyBlocksBlockCode(host) {
  OUTPUT << "BlinkyMeldBlockCode constructor" << endl;
  vm = new MeldInterpretVM((BlinkyBlocksBlock*)hostBlock);
  hasWork = true; // mode fastest
  polling = false; // mode fastest
  currentLocalDate = 0; // mode fastest
  bb = (BlinkyBlocksBlock*) hostBlock;
  if((vm != NULL)) {
    for (int i = 0; i < NUM_PORTS; i++) {
      vm->neighbors[i] = vm->get_neighbor_ID(i);
      vm->enqueue_face(vm->neighbors[i], i, 1);
    }
    BaseSimulator::getScheduler()->schedule(new ComputePredicateEvent(BaseSimulator::getScheduler()->now(), bb));

  }
}

BlinkyMeldBlockCode::~BlinkyMeldBlockCode() {
  delete vm;
  OUTPUT << "BlinkyMeldBlockCode destructor" << endl;
}

void BlinkyMeldBlockCode::init() {
  stringstream info;

  if((vm != NULL)) {
    vm->enqueue_position((meld_int)bb->position.pt[0],(meld_int)bb->position.pt[1],(meld_int)bb->position.pt[2]);
    vm->enqueue_id((meld_int)global_id++);
    BaseSimulator::getScheduler()->schedule(new ComputePredicateEvent(BaseSimulator::getScheduler()->now(), bb));

    if((MeldInterpret::getScheduler()->getMode() == SCHEDULER_MODE_FASTEST) && !vm->deterministicSet) {
      vm->deterministicSet = true;
      info << "deterministic mode set";
      BlinkyBlocks::getScheduler()->trace(info.str(),hostBlock->blockId);
      OUTPUT << "deterministic mode enable on the VM " << hostBlock->blockId << endl;
    }
  }
}

void BlinkyMeldBlockCode::startup() {
  stringstream info;

  currentLocalDate = BaseSimulator::getScheduler()->now();
  info << "  Starting BlinkyMeldBlockCode in block " << hostBlock->blockId;
  BlinkyBlocks::getScheduler()->trace(info.str(),hostBlock->blockId);
  init();

}

void BlinkyMeldBlockCode::handleDeterministicMode(/*VMCommand &command*/){
  currentLocalDate = max(BaseSimulator::getScheduler()->now(), currentLocalDate);
  }

void BlinkyMeldBlockCode::processLocalEvent(EventPtr pev) {
  stringstream info;
  assert(vm != NULL);
  info.str("");
  // OUTPUT << bb->blockId << " processLocalEvent: date: "<< BaseSimulator::getScheduler()->now() << " process event " << pev->getEventName() << "(" << pev->eventType << ")" << ", random number : " << pev->randomNumber << endl;

#ifdef TEST_DETER
  cout << bb->blockId << " processLocalEvent: date: "<< BaseSimulator::getScheduler()->now() << " process event " << pev->getEventName() << "(" << pev->eventType << ")" << ", random number : " << pev->randomNumber << endl;
#endif
  switch (pev->eventType) {
    case EVENT_COMPUTE_PREDICATE:
      {
        vm->processOneRule();
        if(vm->isWaiting()){
          int delay = (bb->getNextRandomNumber() % (1000 - 100 +1 )) + 100;
          BaseSimulator::getScheduler()->schedule(new ComputePredicateEvent(BaseSimulator::getScheduler()->now()+delay, bb));
        }
      }
      break;
    case EVENT_STOP:
      {
        //getDebugger()->sendTerminateMsg(bb->blockId);
        delete vm;
        info << "VM stopped";
      }
      break;
    case EVENT_ADD_NEIGHBOR:
      {
        unsigned int face = (boost::static_pointer_cast<AddNeighborEvent>(pev))->face;
        vm->neighbors[face] = (boost::static_pointer_cast<AddNeighborEvent>(pev))->target;
        vm->enqueue_face(vm->neighbors[face], face, 1);
        BaseSimulator::getScheduler()->schedule(new ComputePredicateEvent(BaseSimulator::getScheduler()->now(), bb));
        info << "Add neighbor "<< (boost::static_pointer_cast<AddNeighborEvent>(pev))->target << " at face " << BlinkyBlocks::NeighborDirection::getString(BlinkyBlocks::NeighborDirection::getOpposite((boost::static_pointer_cast<AddNeighborEvent>(pev))->face));
      }
      break;
    case EVENT_ADD_EDGE:
      {
        int target = (boost::static_pointer_cast<AddEdgeEvent>(pev))->target;
        vm->enqueue_edge(target);
        BaseSimulator::getScheduler()->schedule(new ComputePredicateEvent(BaseSimulator::getScheduler()->now(), bb));
      }
    case EVENT_REMOVE_NEIGHBOR:
      {
        unsigned int face = (boost::static_pointer_cast<AddNeighborEvent>(pev))->face;
        vm->neighbors[face] = 0;
        vm->enqueue_face(vm->neighbors[face], face, -1);
        BaseSimulator::getScheduler()->schedule(new ComputePredicateEvent(BaseSimulator::getScheduler()->now(), bb));
        // info << "Remove neighbor at face " << BlinkyBlocks::NeighborDirection::getString(BlinkyBlocks::NeighborDirection::getOpposite(face));
      }
      break;
    case EVENT_TAP:
      {
        vm->enqueue_tap();
        BaseSimulator::getScheduler()->schedule(new ComputePredicateEvent(BaseSimulator::getScheduler()->now(), bb));
        info << "tapped";
      }
      break;
    case EVENT_SET_COLOR:
      {
        bb->getTime();
        //Called by the VM, no need to enqueue things
        Color color = (boost::static_pointer_cast<SetColorEvent>(pev))->color;
        bb->setColor(color);
#ifdef TEST_DETER
        cout << bb->blockId << " SET_COLOR_EVENT" << endl;
#endif
        info << "set color "<< color;
      }
      break;
    case EVENT_SET_POSITION:
      {
        bb->getTime();
        int x = (boost::static_pointer_cast<SetPositionEvent>(pev))->x;
        int y = (boost::static_pointer_cast<SetPositionEvent>(pev))->y;
        int z = (boost::static_pointer_cast<SetPositionEvent>(pev))->z;
        bb->setPosition(x,y,z);
        vm->enqueue_position((meld_int)bb->position.pt[0],(meld_int)bb->position.pt[1],(meld_int)bb->position.pt[2]);
        BaseSimulator::getScheduler()->schedule(new ComputePredicateEvent(BaseSimulator::getScheduler()->now(), bb));
        info << "set position " << x << " " << y << " " << z;
      }
      break;
    case EVENT_MOVE_TO:
      {
        bb->getTime();
        int x = (boost::static_pointer_cast<MoveToEvent>(pev))->x;
        int y = (boost::static_pointer_cast<MoveToEvent>(pev))->y;
        int z = (boost::static_pointer_cast<MoveToEvent>(pev))->z;
        Vecteur position = bb->position;
        int _x = position.pt[0];
        int _y = position.pt[1];
        int _z = position.pt[2];
        if (z != _z) {
          puts("error with move to action");
        } else if (x != _x) {
          int tx = _x + (x - _x) / abs(x - _x);
          bb->setPosition(tx,_y,_z);
          vm->enqueue_position((meld_int)bb->position.pt[0],(meld_int)bb->position.pt[1],(meld_int)bb->position.pt[2]);
          BaseSimulator::getScheduler()->schedule(new ComputePredicateEvent(BaseSimulator::getScheduler()->now(), bb));
        } else if (y != _y) {
          int ty = _y + (y - _y) / abs(y - _y);
          bb->setPosition(_x,ty,_z);
          vm->enqueue_position((meld_int)bb->position.pt[0],(meld_int)bb->position.pt[1],(meld_int)bb->position.pt[2]);
          BaseSimulator::getScheduler()->schedule(new ComputePredicateEvent(BaseSimulator::getScheduler()->now(), bb));
        } else {
          puts("error with move to action");
        }
       }
       break;
      /*The interface being connected is tested in function tuple_send of the MeldInterpVM*/
    case EVENT_SEND_MESSAGE:
      {
        MessagePtr message = (boost::static_pointer_cast<VMSendMessageEvent>(pev))->message;
        P2PNetworkInterface *interface = (boost::static_pointer_cast<VMSendMessageEvent>(pev))->sourceInterface;
        BlinkyBlocks::getScheduler()->schedule(new NetworkInterfaceEnqueueOutgoingEvent(BaseSimulator::getScheduler()->now(), message, interface));
      }
      break;
    case EVENT_SEND_MESSAGE_TO_BLOCK:
      {
        MessagePtr message = (boost::static_pointer_cast<VMSendMessageEvent2>(pev))->message;
        BlinkyBlocks::BlinkyBlocksBlock* target = (boost::static_pointer_cast<VMSendMessageEvent2>(pev))->target;
        target->scheduleLocalEvent(EventPtr(new VMReceiveMessageEvent2(BaseSimulator::getScheduler()->now(), hostBlock, message) ));
      }
      break;
    case EVENT_RECEIVE_MESSAGE: /*EVENT_NI_RECEIVE: */
      {
        MessagePtr mes = (boost::static_pointer_cast<NetworkInterfaceReceiveEvent>(pev))->message;
        switch(mes->type){
          case ADD_TUPLE_MSG_ID:
            BlinkyBlocks::getScheduler()->schedule(new AddTupleEvent(BaseSimulator::getScheduler()->now(), hostBlock, boost::static_pointer_cast<AddTupleMessage>(mes)->tuple, bb->getDirection(mes->sourceInterface->connectedInterface)));
            break;
          case REMOVE_TUPLE_MSG_ID:
            BlinkyBlocks::getScheduler()->schedule(new RemoveTupleEvent(BaseSimulator::getScheduler()->now(), hostBlock, boost::static_pointer_cast<RemoveTupleMessage>(mes)->tuple, bb->getDirection(mes->sourceInterface->connectedInterface)));
            break;
        }
#ifdef TEST_DETER
        cout << "message received from " << command->sourceInterface->hostBlock->blockId << endl;
#endif
      }
      break;
    case EVENT_RECEIVE_MESSAGE_FROM_BLOCK: /*EVENT_NI_RECEIVE: */
      {
        MessagePtr mes = (boost::static_pointer_cast<NetworkInterfaceReceiveEvent>(pev))->message;
        switch(mes->type){
          case ADD_TUPLE_MSG_ID:
            BlinkyBlocks::getScheduler()->schedule(new AddTupleEvent(BaseSimulator::getScheduler()->now(), hostBlock, boost::static_pointer_cast<AddTupleMessage>(mes)->tuple, -1));
            break;
          case REMOVE_TUPLE_MSG_ID:
            BlinkyBlocks::getScheduler()->schedule(new RemoveTupleEvent(BaseSimulator::getScheduler()->now(), hostBlock, boost::static_pointer_cast<RemoveTupleMessage>(mes)->tuple, -1));
            break;
        }
      }
      break;
    case EVENT_ACCEL:
      {
        /*Not written yet, have to check how the vm handle accel (tuple, etc)*/
        info << "accel";
      }
      break;
    case EVENT_SHAKE:
      {
        /*Not written yet, same as accel*/

        info << "shake";
      }
      break;
    case EVENT_SET_DETERMINISTIC:
      {
        /*Not sure how to handle that with MeldInterp*/
        OUTPUT << "VM set in deterministic mode " << hostBlock->blockId << endl;
        info << "VM set in deterministic mode";
      }
      break;
    case EVENT_END_POLL:
      {
        polling = false;
        /*Not written yet
          Need to check what this is for*/
        info << "Polling time period ended";
      }
      break;
    case EVENT_ADD_TUPLE:
      this->vm->receive_tuple(1, boost::static_pointer_cast<AddTupleEvent>(pev)->tuple, boost::static_pointer_cast<AddTupleEvent>(pev)->face);
      BaseSimulator::getScheduler()->schedule(new ComputePredicateEvent(BaseSimulator::getScheduler()->now(), bb));
      //info << "Adding tuple";
      break;
    case EVENT_REMOVE_TUPLE:
      this->vm->receive_tuple(-1, boost::static_pointer_cast<RemoveTupleEvent>(pev)->tuple, boost::static_pointer_cast<RemoveTupleEvent>(pev)->face);
      BaseSimulator::getScheduler()->schedule(new ComputePredicateEvent(BaseSimulator::getScheduler()->now(), bb));
      //info << "Removing tuple";
      break;
    default:
      break;
  }
  if(info.str() != "") {
    BlinkyBlocks::getScheduler()->trace(info.str(),hostBlock->blockId);
  }
}

BlinkyBlocks::BlinkyBlocksBlockCode* BlinkyMeldBlockCode::buildNewBlockCode(BlinkyBlocksBlock *host) {
  return(new BlinkyMeldBlockCode(host));
}
