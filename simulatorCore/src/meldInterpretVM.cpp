#include "meldInterpretEvents.h"
#include "meldInterpretMessages.h"
#include "meldInterpretScheduler.h"
#include "events.h"
#include <iostream>
#include "meldInterpretVM.h"

#define myassert(e)	((e) ? (void)0 : __myassert(__FILE__, __LINE__,#e))


namespace MeldInterpret{

  const unsigned char* MeldInterpretVM::meld_prog;
  char** MeldInterpretVM::tuple_names;
  char* rule_names_load[] = {"_init -o node-axioms.", };
  char** MeldInterpretVM::rule_names = rule_names_load;

  map<int, MeldInterpretVM*> MeldInterpretVM::vmMap;
  bool MeldInterpretVM::configured = false;
  bool MeldInterpretVM::debugging = false;
  unsigned char * MeldInterpretVM::arguments = NULL;


  MeldInterpretVM::MeldInterpretVM(BlinkyBlocks::BlinkyBlocksBlock *b){
    /* Init will always be predicate 0 */
    TYPE_INIT = 0;
    /* Initialize unknown type IDs */
    TYPE_EDGE = -1;
    TYPE_TERMINATE = -1;
    TYPE_NEIGHBORCOUNT = -1;
    TYPE_NEIGHBOR = -1;
    TYPE_VACANT = -1;
    TYPE_TAP = -1;
    TYPE_SETCOLOR = -1;
    TYPE_SETCOLOR2 = -1;
    TYPE_POSITION = -1;
    TYPE_SETPOSITION = -1;
    TYPE_MOVETO = -1;
    TYPE_ID = -1;

    OUTPUT << "MeldInterpretVM constructor" << endl;
    vm_alloc();
    vm_init();
    host = b;
    blockId = (NodeID)b->blockId;
    hasWork = true;
    polling = false;
    deterministicSet = false;
    firstStart = true;
    currentLocalDate = 0;

    /* Enqueue init to derive the program's axioms */
    enqueue_init();

    // introduce initial set of axioms
    numNeighbors = getNeighborCount();
    enqueue_count(numNeighbors, 1);

    vmMap.insert(std::pair<int, MeldInterpretVM*>(blockId,this));
  }

  MeldInterpretVM::~MeldInterpretVM(){
    /*for (int i = 0; i < NUM_TYPES; i++) {
      if (TUPLES[i].head == NULL)
      continue;

      tuple_entry *tupleEntry = TUPLES[i].head;
      tuple_entry *tupleNextEntry;
      while (tupleEntry != NULL) {
      tupleNextEntry = tupleEntry->next;
      free(tupleEntry);
      tupleEntry = tupleNextEntry;
      }
      }*/
    free(delayedTuples);
    free(tuples);
    free(newStratTuples);
    free(newTuples);
    //free(receivedTuples);
    //free(arguments);
  }

  Time MeldInterpretVM::myGetTime(){
    // simulator time is in us while the unit in the vm is ms.
    return (Time)(host->getTime()/1000);
  }

  /* Print the content of the newTuples queue */
  void MeldInterpretVM::print_newTuples(void)
  {
    fprintf(stderr, "\x1b[34m--%d--\tContent of queue newTuples: \n", blockId);
    tuple_entry *tupleEntry;
    for (tupleEntry = newTuples->head; tupleEntry != NULL; tupleEntry = tupleEntry->next) {
      tuple_print(tupleEntry->tuple, stderr);
      fprintf(stderr, " -- isNew = %d\n", tupleEntry->records.count);
    }
    fprintf(stderr, "\x1b[0m");
  }

  /* Prints the content of the newStartTuples queue */
  void MeldInterpretVM::print_newStratTuples(void)
  {
    fprintf(stderr, "\x1b[34m--%d--\tContent of queue newStratTuples: \n",
        blockId);
    if (newStratTuples) {
      tuple_pentry *tupleEntry;
      for (tupleEntry = newStratTuples->queue; tupleEntry != NULL; tupleEntry = tupleEntry->next) {
        tuple_print(tupleEntry->tuple, stderr);
        fprintf(stderr, " -- isNew = %d\n", tupleEntry->records.count);
      }
    }
    fprintf(stderr, "\x1b[0m");
  }


  /* Gets ID of neighbor on face 'face' */
  inline NodeID MeldInterpretVM::get_neighbor_ID(int face){
    if (face == UP)
      return up();
    else if (face == DOWN)
      return down();
    else if (face == WEST)
      return west();
    else if (face == EAST)
      return east();
    else if (face == NORTH)
      return north();
    else if (face == SOUTH)
      return south();
    else {
      myassert(0);
      return -1;
    }
  }

  /* Enqueue a tuple for execution */
  void MeldInterpretVM::enqueueNewTuple(tuple_t tuple, record_type isNew){
    myassert (TUPLE_TYPE(tuple) < NUM_TYPES);

    if (TYPE_IS_STRATIFIED(TUPLE_TYPE(tuple))) {
      p_enqueue(newStratTuples, TYPE_STRATIFICATION_ROUND(TUPLE_TYPE(tuple)), tuple, 0, isNew);
    }
    else {
      queue_enqueue(newTuples, tuple, isNew);
    }
    tuple_print(tuple, stdout);
    puts("");
  }

  /* Enqueue a neighbor or vacant tuple */
  void MeldInterpretVM::enqueue_face(NodeID neighbor, meld_int face, int isNew){
    tuple_t tuple = NULL;

    if (neighbor <= 0) {
      if(TYPE_VACANT == -1) /* no such predicate in the program */
        return;
      tuple = tuple_alloc(TYPE_VACANT);
      SET_TUPLE_FIELD(tuple, 0, &face);
    }
    else {
      if(TYPE_NEIGHBOR == -1) /* no such predicate in the program */
        return;

      tuple = tuple_alloc(TYPE_NEIGHBOR);
      SET_TUPLE_FIELD(tuple, 0, &neighbor);
      SET_TUPLE_FIELD(tuple, 1, &face);
    }

    enqueueNewTuple(tuple, (record_type) isNew);
  }

  /* Enqueue a edge tuple */
  void MeldInterpretVM::enqueue_edge(NodeID neighbor){
    if(TYPE_EDGE == -1) return;
    tuple_t tuple = tuple_alloc(TYPE_EDGE);
    SET_TUPLE_FIELD(tuple, 0, &neighbor);
    enqueueNewTuple(tuple, 1);
  }

  /* Enqueue a id tuple */
  void MeldInterpretVM::enqueue_id(meld_int id){
    if(TYPE_ID == -1) return;
    tuple_t tuple = tuple_alloc(TYPE_ID);
    SET_TUPLE_FIELD(tuple, 0, &id);
    enqueueNewTuple(tuple, 1);
  }

  /* Enqueue a position tuple */
  void MeldInterpretVM::enqueue_position(meld_int x, meld_int y, meld_int z) {
    if (TYPE_POSITION == -1)
      return;
    tuple_t tuple = tuple_alloc(TYPE_POSITION);
    SET_TUPLE_FIELD(tuple, 0, &x);
    SET_TUPLE_FIELD(tuple, 1, &y);
    SET_TUPLE_FIELD(tuple, 2, &z);
    enqueueNewTuple(tuple, 1);
  }

  /* Enqueue a neighborCount tuple */
  void MeldInterpretVM::enqueue_count(meld_int count, int isNew){
    if(TYPE_NEIGHBORCOUNT == -1) /* no such predicate in the program */
      return;

    tuple_t tuple = tuple_alloc(TYPE_NEIGHBORCOUNT);

    SET_TUPLE_FIELD(tuple, 0, &count);

    enqueueNewTuple(tuple, (record_type) isNew);
  }

  /* Enqueue a tap tuple */
  void MeldInterpretVM::enqueue_tap(void) {
    if(TYPE_TAP == -1) /* no such predicate in the program */
      return;

    tuple_t tuple = tuple_alloc(TYPE_TAP);

    enqueueNewTuple(tuple, (record_type) 1);
  }

  /* Enqueue init tuple, triggers derivation of RULE 0, which derives axioms */
  void MeldInterpretVM::enqueue_init(void) {
    if(TYPE_INIT == -1)
      return;

    tuple_t tuple = tuple_alloc(TYPE_INIT);
    enqueueNewTuple(tuple, (record_type) 1);
  }

  /* Saves the ID of useful types */
  void MeldInterpretVM::init_all_consts(void) {
    init_consts();
    tuple_type i;
    for (i = 0; i < NUM_TYPES; i++) {
      if (strcmp(TYPE_NAME(i), "tap") == 0)
        TYPE_TAP = i;
      else if (strcmp(TYPE_NAME(i), "neighbor") == 0)
        TYPE_NEIGHBOR = i;
      else if ( (strcmp(TYPE_NAME(i), "neighborCount" ) == 0) ||
          (strcmp(TYPE_NAME(i), "neighborcount" ) == 0) )
        TYPE_NEIGHBORCOUNT = i;
      else if (strcmp(TYPE_NAME(i), "vacant") == 0)
        TYPE_VACANT = i;
      else if (strcmp(TYPE_NAME(i), "setcolor") == 0)
        TYPE_SETCOLOR = i;
      else if (strcmp(TYPE_NAME(i), "setcolor2") == 0 )
        TYPE_SETCOLOR2 = i;
      else if (strcmp(TYPE_NAME(i), "position") == 0 )
        TYPE_POSITION = i;
      else if (strcmp(TYPE_NAME(i), "setposition") == 0)
        TYPE_SETPOSITION = i;
      else if (strcmp(TYPE_NAME(i), "moveto") == 0)
        TYPE_MOVETO = i;
      else if (strcmp(TYPE_NAME(i), "readycount") == 0)
        TYPE_READYCOUNT = i;
      else if (strcmp(TYPE_NAME(i), "edge") == 0)
        TYPE_EDGE = i;
      else if (strcmp(TYPE_NAME(i), "unbroadcasted") == 0)
        TYPE_UNBROADCASTED = i;
      else if (strcmp(TYPE_NAME(i), "id") == 0)
        TYPE_ID = i;
    }
  }


  /* The VM's main function */
  void MeldInterpretVM::processOneRule(void) {
    // processing new facts and updating axioms
    waiting = 0;

    //If there are new tuples
    if(!queue_is_empty(newTuples)) {
      int isNew = 0;
      //print_newTuples();
      tuple_t tuple = queue_dequeue(newTuples, &isNew);
      tuple_handle(tuple, isNew, reg);
      waiting = 1;
      //Else if there are new delayed tuple available
    } else if (!p_empty(delayedTuples) && p_peek(delayedTuples)->priority <= myGetTime()) {
      tuple_pentry *entry = p_dequeue(delayedTuples);

      tuple_send(entry->tuple, entry->rt, 0, entry->records.count);
      free(entry);
      waiting = 1;
      //Else if there are new stratified tuple
    } else if (!(p_empty(newStratTuples))) {
      tuple_pentry *entry = p_dequeue(newStratTuples);
      tuple_handle(entry->tuple, entry->records.count, reg);

      free(entry);
      //Else if there are no tuple to process
      waiting = 1;
    } else {

      if (!p_empty(delayedTuples)) {
        waiting = 1;
      }
      /* If all tuples have been processed
       * update rule state and process them if they are ready */
      for (int i = 0; i < NUM_RULES; ++i) {
        //If a rule has all its predicate (considered ACTIVE)
        if (updateRuleState(i)) {
          waiting = 1;
          /* Set state byte used by DEBUG */
          byte processState = PROCESS_RULE | (i << 4);
          /* Don't process persistent rules (which is useless)
           * as they all have only a RETURN instruction.
           */
          if (!RULE_ISPERSISTENT(i)) {
            /* Trigger execution */
            process_bytecode (NULL, RULE_START(i), 1, NOT_LINEAR, reg, processState);

            /* After one rule is executed we set the VM on waiting until next call of scheduler*/
            i = NUM_RULES;
          }
        }
        /* else: Rule not ready yet, set status to not waiting until new fact appear */
      }
    }

    //updateAccel();

    // update axioms based upon any changes
    int newNumNeighbors = getNeighborCount();
    if (newNumNeighbors != numNeighbors) {
      enqueue_count(numNeighbors, -1);
      numNeighbors = newNumNeighbors;
      waiting = 1;
      enqueue_count(numNeighbors, 1);
    }

    for (int i = 0; i < NUM_PORTS; i++) {
      NodeID neighbor = get_neighbor_ID(i);

      if (neighbor == neighbors[i])
        continue;

      waiting = 1;
      enqueue_face(neighbors[i], i, -1);

      /* Delete received tuples from database
       * This may need to be reviewed,
       * I am not sure what LM is supposed to do with received tuples
       */
      while(!queue_is_empty(&(receivedTuples[i]))) {
        tuple_t tuple = queue_dequeue(&receivedTuples[i], NULL);
        printf("--%d--\tDelete received ", blockId);
        enqueueNewTuple(tuple, (record_type) -1);
      }

      neighbors[i] = neighbor;
      enqueue_face(neighbors[i], i, 1);
    }
  }

  bool MeldInterpretVM::isWaiting(){
    return waiting > 0;
  }


  /*void MeldInterpretVM::userRegistration(void) {
    registerHandler(SYSTEM_MAIN, (GenericHandler)&meldMain);
    registerHandler(EVENT_ACCEL_TAP, (GenericHandler)&enqueue_tap);
    }*/

  /* Receive a tuple and enqueue it to both receivedTuples and newTuples */
  void MeldInterpretVM::receive_tuple(int isNew, tuple_t tpl, byte face) {
    tuple_t rcvdTuple = (tuple_t)tpl;
    tuple_t tuple;
    tuple_type type = TUPLE_TYPE(rcvdTuple);
    size_t tuple_size = TYPE_SIZE(type);
    tuple_print(rcvdTuple, stderr);

    if(!TYPE_IS_LINEAR(type) && !TYPE_IS_ACTION(type) && face!=-1) {
      //tuple_queue *queue = receivedTuples + face;
      tuple_queue *queue = &(receivedTuples[face]);
      if(isNew > 0) {
        tuple = malloc(tuple_size);
        memcpy(tuple, rcvdTuple, tuple_size);
        queue_enqueue(queue, tuple, (record_type)isNew);
      } else {
        // delete tuple from queue because it must invalidate some other tuple
        tuple_entry **current;
        for (current = &queue->head; *current != NULL; current = &(*current)->next) {
          if(memcmp((*current)->tuple, rcvdTuple, tuple_size) == 0) {
            FREE_TUPLE(queue_dequeue_pos(queue, current));
            break;
          }
        }
      }
    }

    tuple = malloc(tuple_size);
    memcpy(tuple, rcvdTuple, tuple_size);
    enqueueNewTuple(tuple, (record_type)isNew);
  }

  /* Sends a tuple to Block of ID rt, with or without delay */
  void MeldInterpretVM::tuple_send(tuple_t tuple, NodeID rt, meld_int delay, int isNew) {
    myassert (TUPLE_TYPE(tuple) < NUM_TYPES);
    if (delay > 0) {
      p_enqueue(delayedTuples, myGetTime() + delay, tuple, rt, (record_type) isNew);
      return;
    }

    NodeID target = rt;

    if (target == blockId) {
      enqueueNewTuple(tuple, (record_type) isNew);
    }
    else {
      int face = -1;

      if (target == up())
        face = UP;
      else if (target == down())
        face = DOWN;
      else if (target == west())
        face = WEST;
      else if (target == east())
        face = EAST;
      else if (target == north())
        face = NORTH;
      else if (target == south())
        face = SOUTH;


      if (face != -1) {
        myassert(TYPE_SIZE(TUPLE_TYPE(tuple)) <= 17);
        MessagePtr ptr;
        if (isNew > 0) {
          ptr = (MessagePtr)(new AddTupleMessage(tuple, TYPE_SIZE(TUPLE_TYPE(tuple))));
        }
        else {
          ptr = (MessagePtr)(new RemoveTupleMessage(tuple, TYPE_SIZE(TUPLE_TYPE(tuple))));
        }
        P2PNetworkInterface *p2p = host->getP2PNetworkInterfaceByDestBlockId(get_neighbor_ID(face));
        /*Prepare message*/
        ptr->sourceInterface = p2p;
        if(p2p->connectedInterface != NULL)
          ptr->destinationInterface  = p2p->connectedInterface;
        MeldInterpret::getScheduler()->schedule(new VMSendMessageEvent(MeldInterpret::getScheduler()->now(), host, ptr, p2p));
      }
      else {
        MessagePtr ptr;
        if (isNew > 0) {
          ptr = (MessagePtr)(new AddTupleMessage(tuple, TYPE_SIZE(TUPLE_TYPE(tuple))));
        }
        else {
          ptr = (MessagePtr)(new RemoveTupleMessage(tuple, TYPE_SIZE(TUPLE_TYPE(tuple))));
        }
        list<BlinkyBlocks::BlinkyBlocksBlock*>::iterator it;
        BlinkyBlocks::BlinkyBlocksBlock* toblock = NULL;
        for (it = host->connected.begin(); it != host->connected.end(); ++ it) {
          if((*it)->blockId == target) {
            toblock = *it;
            break;
          }
        }
        if (toblock != NULL) {
          MeldInterpret::getScheduler()->schedule(new VMSendMessageEvent2(MeldInterpret::getScheduler()->now(), host, ptr, toblock));
        } else {
          printf("send message error\n");
        }

        /* This may happen when you delete a block in the simulator */
        //fprintf(stderr, "--%d--\tUNABLE TO ROUTE MESSAGE! To %d\n", (int)blockId, (int)target);
        //exit(EXIT_FAILURE);
      }
    }
  }

  /* Check if rule of ID rid is ready to be derived */
  /* Returns 1 if true, 0 otherwise */
  byte MeldInterpretVM::updateRuleState(byte rid) {
    int i;
    /* A rule is ready if all included predicates are present in the database */
    for (i = 0; i < RULE_NUM_INCLPREDS(rid); ++i) {
      if (TUPLES[RULE_INCLPRED_ID(rid, i)].length == 0)
        return INACTIVE_RULE;
    }

    /* Rule is ready, enqueue it or process it rightaway */
    return ACTIVE_RULE;
  }

  /* Simply calls tuple_do_handle located in core.c to handle tuple  */
  void MeldInterpretVM::tuple_handle(tuple_t tuple, int isNew, Register *registers) {
    tuple_type type = TUPLE_TYPE(tuple);
    myassert (type < NUM_TYPES);
    tuple_do_handle(type, tuple, isNew, registers);
  }

  /* VM initialization routine */
  void MeldInterpretVM::vm_init(void) {
    init_all_consts();
    init_fields();
  }

  /* Called upon block init (block.bb)
   * to ensure that data structures are allocated before
   * VM start in case other blocks send us tuples - Would seg fault otherwise */
  void MeldInterpretVM::vm_alloc(void) {

    // init stuff
    tuples = (tuple_queue*)calloc(NUM_TYPES, sizeof(tuple_queue));
    newTuples = (tuple_queue*)calloc(1, sizeof(tuple_queue));
    newStratTuples = (tuple_pqueue*)calloc(1, sizeof(tuple_pqueue));
    delayedTuples = (tuple_pqueue*)calloc(1, sizeof(tuple_pqueue));

    myassert(tuples!=NULL);
    myassert(newTuples!=NULL);
    myassert(newStratTuples!=NULL);
    myassert(delayedTuples!=NULL);

    /* Reset received tuples queue */
    memset(receivedTuples, 0, sizeof(tuple_queue) * NUM_PORTS);

  }

  void MeldInterpretVM::__myassert(string file, int line, string exp) {
    /*
       while (1) {
       setColor(RED);
       setColor(BLUE);
       }
       */
    OUTPUT << "Assert " <<  " " << file << " " << line << " " << exp << endl;
  }

  byte MeldInterpretVM::getNeighborCount() {
    uint8_t count, i;

    for(count = 0, i = 0; i < NUM_PORTS; ++i) {
      if(get_neighbor_ID(i) != VACANT) {
        count++;
      }
    }
    return count;
  }

  // simple functions to access geographic neighbors
  Uid MeldInterpretVM::down(void) {
    P2PNetworkInterface* temp = host->getInterface(BlinkyBlocks::NeighborDirection::Direction((int)DOWN))->connectedInterface;
    if(temp != NULL) return (temp->hostBlock->blockId);
    else return 0;
  }
  Uid MeldInterpretVM::up(void) {
    P2PNetworkInterface* temp = host->getInterface(BlinkyBlocks::NeighborDirection::Direction((int)UP))->connectedInterface;
    if(temp != NULL) return (temp->hostBlock->blockId);
    else return 0;
  }
  Uid MeldInterpretVM::north(void) {
    P2PNetworkInterface* temp = host->getInterface(BlinkyBlocks::NeighborDirection::Direction((int)NORTH))->connectedInterface;
    if(temp != NULL) return (temp->hostBlock->blockId);
    else return 0;
  }
  Uid MeldInterpretVM::south(void) {
    P2PNetworkInterface* temp = host->getInterface(BlinkyBlocks::NeighborDirection::Direction((int)SOUTH))->connectedInterface;
    if(temp != NULL) return (temp->hostBlock->blockId);
    else return 0;
  }
  Uid MeldInterpretVM::east(void) {
    P2PNetworkInterface* temp = host->getInterface(BlinkyBlocks::NeighborDirection::Direction((int)EAST))->connectedInterface;
    if(temp != NULL) return (temp->hostBlock->blockId);
    else return 0;
  }
  Uid MeldInterpretVM::west(void) {
    P2PNetworkInterface* temp = host->getInterface(BlinkyBlocks::NeighborDirection::Direction((int)WEST))->connectedInterface;
    if(temp != NULL) return (temp->hostBlock->blockId);
    else return 0;
  }

  NodeID MeldInterpretVM::getGUID(){
    return blockId;
  }

  void MeldInterpretVM::setColor(Color color){
    setLED(color[0]*255, color[1]*255, color[2]*255, color[3]*255);
  }
  void MeldInterpretVM::setColor(byte color){
    setLED(Colors[color][0]*255, Colors[color][1]*255, Colors[color][2]*255, Colors[color][3]*255);
  }
  void MeldInterpretVM::setLED(byte r, byte g, byte b, byte intensity){
    BaseSimulator::getScheduler()->schedule(new SetColorEvent(BaseSimulator::getScheduler()->now(), host , (float)r/255, (float)g/255, (float)b/255, (float)intensity/255));
  }
  void MeldInterpretVM::setPosition(int x, int y, int z) {
    BaseSimulator::getScheduler()->schedule(new SetPositionEvent(BaseSimulator::getScheduler()->now(), host , x, y, z));
  }
  void MeldInterpretVM::moveTo(int x, int y, int z) {
    BaseSimulator::getScheduler()->schedule(new MoveToEvent(BaseSimulator::getScheduler()->now()+500000, host, x, y, z));
  }


  bool MeldInterpretVM::equilibrium() {
    map<int, MeldInterpretVM*>::iterator it;
    for(it = vmMap.begin(); it != vmMap.end(); it++) {
      MeldInterpretVM *vm = it->second;
      BuildingBlock *buildb = vm->host;
      if (buildb->getState() < BuildingBlock::ALIVE) {
        continue;
      }
      if (vm->hasWork) {
        return false;
      }
    }
    return true;
  }

  void MeldInterpretVM::setConfiguration(string path, bool d){
    debugging = d;
    if(!configured){
      configured = true;
      readProgram(path);
    }
  }

  void MeldInterpretVM::readProgram(string path){
    ifstream ifs(path.c_str(), ios::in);
    stringstream strStream;
    strStream << ifs.rdbuf();//read the file
    string content = strStream.str();//convert stream to string

    //Separate the different variable value in several string
    int pos = content.find("{");
    string progString = content.substr(pos, content.find("}") - pos);
    pos = content.find("{", content.find("}", pos));
    string tupleString = content.substr(pos, content.find("}", pos) - pos);
    pos = content.find("{", content.find("}", pos));
    string ruleString = content.substr(pos, content.find("}", pos) - pos);

    //Reading meld_prog
    int byteCount = characterCount(progString, ',');
    unsigned char* outProg = (unsigned char*)malloc((byteCount + 1)*sizeof(unsigned char));
    int movingCursor = 0;
    int valByte = 0;
    int leftCursor = 0;
    int rightCursor = 0;
    int x = (int)'0';
    int multi = 1;
    for(int i = 0; i <= byteCount; i++){
      leftCursor = rightCursor;
      rightCursor = progString.find(',', leftCursor + 1);
      movingCursor = rightCursor - 1;
      //read the value between the ','
      for(; movingCursor >= leftCursor; movingCursor--){
        x = (int)'0';
        //Test if character is not part of a byte
        while(x <= ((int)'9')){ //Test for 0-9
          if((int)(progString[movingCursor]) == x ) break;
          x++;
        }
        if(x <= (int)'9'){
          valByte += ((((int)(progString[movingCursor])) - (int)'0')* multi);
          multi = multi * 16;
          continue;
        }
        x = (int)'a';
        while(x <= (int)'f'){ //Test for a-f
          if((int)(progString[movingCursor]) == x ) break;
          x++;
        }
        if(x <= (int)'f'){
          valByte += ((((int)(progString[movingCursor]) + 10) - (int)'a')* multi);
          multi = multi * 16;
          continue;
        }
        if((int)'0' < x && x > (int)'9' && (int)'a' < x && x > (int)'f') break; //if value is not hexadecimal
      }
      outProg[i] = (unsigned char)valByte;
      valByte = 0;
      multi = 1;
    }
    meld_prog = outProg;

    //Reading tuple_names
    int countingTuple = characterCount(tupleString, ',');
    char** outTuple = (char**)malloc((countingTuple + 1)*sizeof(char*));
    pos = tupleString.find("\"") + 1;
    int i = 0;
    while(i < countingTuple){
      outTuple[i] = strdup(tupleString.substr(pos, tupleString.find("\"", pos) - pos).c_str());
      pos = tupleString.find("\"", pos) + 1; //goes to the end of the string and jump the "
      pos = tupleString.find("\"", pos) + 1; // goes to the next string and jump the "
      i++;
    }
    tuple_names = outTuple;

    OUTPUT << "Program has been loaded" << endl;
  }

  int MeldInterpretVM::characterCount(string in, char character){
    int nFind = -1;
    int nCount = 0;
    while( -1 != ( nFind = in.find( character, nFind + 1 ))){
      nCount++;
    }
    return nCount;
  }


  /********************************************
   *******Below this line is the core of the VM
   *******It contains the basic function of the
   *******meld interpreter**********************
   *********************************************/

  NodeID MeldInterpretVM::getBlockId (void) { return blockId; }

  inline byte MeldInterpretVM::val_is_float(const byte x) {
    return x == 0x00;
  }
  inline byte MeldInterpretVM::val_is_int(const byte x) {
    return x == 0x01;
  }
  inline byte MeldInterpretVM::val_is_field(const byte x) {
    return x == 0x02;
  }

  /* ************* EVAL FUNCTIONS ************* */

  /* Returns the address of field number 'field_num' (extracted from byte code)
   * of the tuple given as argument.
   * Also increment pc past the field.
   */
  inline void* MeldInterpretVM::eval_field (tuple_t tuple, const unsigned char **pc) {
    const unsigned char field_num = VAL_FIELD_NUM(*pc);
    (*pc) += 2;

    return GET_TUPLE_FIELD(tuple, field_num);
  }
  /* Returns the address of register number 'value'
   * and increment pc past the reg byte.
   */
  inline void* MeldInterpretVM::eval_reg(const unsigned char value, const unsigned char **pc, Register *reg) {
    ++(*pc);
    return &(reg)[VAL_REG(value)];
  }

  /* Returns a pointer to the meld_int at address pointed at by pc
   * and increment it past the int.
   */
  inline void* MeldInterpretVM::eval_int (const unsigned char **pc) {
    void *ret = (void *)(*pc);
    *pc += sizeof(meld_int);

    return ret;
  }

  /* Returns a pointer to the meld_float (double) at address pointed at by pc
   * and increment it past the float.
   */
  inline void* MeldInterpretVM::eval_float (const unsigned char **pc) {
    void *ret = (void *)(*pc);
    *pc += sizeof(meld_float);

    return ret;
  }

  /* Set value of register number 'reg_index' as a pointer to tuple 'tuple' */
  inline void MeldInterpretVM::moveTupleToReg (const unsigned char reg_index, tuple_t tuple, Register *reg) {
    Register *dst = &(reg)[VAL_REG(reg_index)];
    *dst = (Register)tuple;


  }

  /* ************* INSTRUCTION EXECUTION FUNCTIONS ************* */

  /* Allocates a new tuple of type 'type' and set its type byte */
  inline void MeldInterpretVM::execute_alloc (const unsigned char *pc, Register *reg) {
    ++pc;
    tuple_type type = FETCH(pc++);
    byte reg_index = FETCH(pc);
    tuple_t *dst = (void**)eval_reg (reg_index, &pc, reg);
    *dst = ALLOC_TUPLE(TYPE_SIZE(type));

    memset (*dst, 0, TYPE_SIZE(type));
    TUPLE_TYPE(*dst) = type;
  }

  /* Either enqueue a tuple for derivation
   * or enqueue it for retraction
   */
  inline void MeldInterpretVM::execute_addtuple (const unsigned char *pc, Register *reg, int isNew) {
    ++pc;
    byte reg_index = FETCH(pc);
    Register tuple_reg = reg[reg_index];

    enqueueNewTuple((tuple_t)MELD_CONVERT_REG_TO_PTR(tuple_reg), (record_type) isNew);
  }

  /* Only used to notify user that a linear tuple has been updated
   * instead of removed during rule derivation.
   */
  inline void MeldInterpretVM::execute_update (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg_index = FETCH(pc);
    Register tuple_reg = reg[reg_index];
    (void)tuple_reg;
  }

  /* Send tuple pointed at by 'send_reg' to blockID designated by send_rt
   * NO DELAY!
   */
  inline void MeldInterpretVM::execute_send (const unsigned char *pc, Register *reg, int isNew) {
    ++pc;
    Register send_reg = reg[SEND_MSG(pc)];
    NodeID send_rt = reg[SEND_RT(pc)];


    tuple_send((tuple_t)MELD_CONVERT_REG_TO_PTR(send_reg), send_rt, 0, isNew);
  }

  /* Call an external function with one argument.
   * Not implemented yet, only used to support the node2int function
   * which is useless when using BB as nodeID's are not pointers.
   */
  inline void MeldInterpretVM::execute_call1 (const unsigned char *pc, Register *reg) {
    ++pc;
    byte functionID = FETCH(pc++);

    byte dst_index = FETCH(pc);
    Register *dst = (Register*)eval_reg (dst_index, &pc, reg);

    byte return_type = FETCH(pc++);
    byte garbage_collected = FETCH(pc++);

    byte arg1_index = FETCH(pc);
    Register *arg1 = (Register*)eval_reg (arg1_index, &pc, reg);

    if (functionID == NODE2INT_FUNC) {
      *dst = MELD_NODE_ID(arg1);
    } else {
      fprintf(stderr, "--%d--\t Error: call to function not implemented yet!\n",
          getBlockId());
    }

    /* Do nothing for now since no function are currently implemented */
    (void)arg1;
    (void)dst;
    (void)return_type;
    (void)garbage_collected;
  }

  /* Similar to send, but with a delay */
  inline void MeldInterpretVM::execute_send_delay (const unsigned char *pc, Register *reg, int isNew) {
    ++pc;

    const byte tpl = SEND_MSG(pc);
    const byte dst = SEND_RT(pc);
    Register send_reg = reg[tpl];
    NodeID send_rt = reg[dst];
    pc += 2;
    meld_int *delay = (meld_int*)eval_int(&pc);


    if(tpl == dst) {
      tuple_send((tuple_t)MELD_CONVERT_REG_TO_PTR(send_reg), getBlockId(), *delay, isNew);
    } else {
      tuple_send((tuple_t)MELD_CONVERT_REG_TO_PTR(send_reg), send_rt, *delay, isNew);
    }
  }

  /* Iterate through the database to find a match with tuple read from byte code
   * If there are matches, process the inside of the ITER with all matches sequentially.
   */
  int MeldInterpretVM::execute_iter (const unsigned char *pc, Register *reg, int isNew, int isLinear) {
    const unsigned char *inner_jump = pc + ITER_INNER_JUMP(pc);
    const tuple_type type = ITER_TYPE(pc);
    int i, k, length;
    void **list;

    /* Reg in which match will be stored during execution*/
    byte reg_store_index = FETCH(pc+10);

    /* produce a random ordering for all tuples of the appropriate type */
    tuple_entry *entry = TUPLES[type].head;

    length = queue_length(&TUPLES[ITER_TYPE(pc)]);
    list = (void**)malloc(sizeof(tuple_t) * length);

    for (i = 0; i < length; i++) {
      int j = host->getNextRandomNumber() % (i+1);

      list[i] = list[j];
      list[j] = entry->tuple;

      entry = entry->next;
    }

    if(length == 0) {
      free(list);
      /* no need to execute any further code, just jump! */
      return RET_NO_RET;
    }

    /* iterate over all tuples of the appropriate type */
    void *next_tuple;

    for (i = 0; i < length; i++) {
      next_tuple = list[i];

      unsigned char matched = 1;
      unsigned char num_args = ITER_NUM_ARGS(pc);
      const unsigned char *tmppc = pc + PERS_ITER_BASE;

      /* check to see if it matches */
      for (k = 0; k < num_args; ++k) {
        const unsigned char fieldnum = ITER_MATCH_FIELD(tmppc);
        const unsigned char fieldtype = TYPE_ARG_TYPE(type, fieldnum);
        const unsigned char type_size = TYPE_ARG_SIZE(type, fieldnum);
        const unsigned char value_type = ITER_MATCH_VAL(tmppc);

        Register *field = GET_TUPLE_FIELD(next_tuple, fieldnum);
        Register *val;

        if (val_is_int (value_type)) {
          tmppc += 2;
          val = (Register *)eval_int(&tmppc);
        } else if (val_is_float (value_type)) {
          tmppc += 2;
          val = (Register *)eval_float(&tmppc);
        } else if (val_is_field (value_type)) {
          tmppc += 2;
          byte reg_index = FETCH(tmppc+1);
          tuple_t tpl = (tuple_t)reg[reg_index];
          val = (Register *)eval_field(tpl, &tmppc);
        }  else {
          /* Don't know what to do */
          fprintf (stderr, "Type %d not supported yet - don't know what to do.\n", fieldtype);
          myassert (0);
          exit (2);
        }

        matched = matched && (memcmp(field, val, type_size) == 0);
      }

      if (matched) {
        /* We've got a match! */
        moveTupleToReg (reg_store_index, next_tuple, reg);
        /* Process it - And if we encounter a return instruction, return
         * Otherwise, look for another match.
         */
        int ret = process_bytecode(next_tuple, inner_jump,
            isNew, isLinear || TYPE_IS_LINEAR(TUPLE_TYPE(next_tuple)), reg, PROCESS_ITER);
        if(ret == RET_LINEAR) {
          free(list);
          return ret;
        }
        if(isLinear && ret == RET_DERIVED) {
          free(list);
          return ret;
        }
        if(ret == RET_RET) {
          free(list);
          return ret;
        }
      }
    }

    free(list);

    /* process next instructions */
    return RET_NO_RET;
  }

  inline void MeldInterpretVM::execute_run_action0 (tuple_t action_tuple, tuple_type type, int isNew) {
    if (type == TYPE_SETCOLOR) {
      if (isNew > 0) {

        /* Don't call it directly to avoid having to import led.bbh */
        setLEDWrapper(*(byte *)GET_TUPLE_FIELD(action_tuple, 0),
            *(byte *)GET_TUPLE_FIELD(action_tuple, 1),
            *(byte *)GET_TUPLE_FIELD(action_tuple, 2),
            *(byte *)GET_TUPLE_FIELD(action_tuple, 3));
      }
      FREE_TUPLE(action_tuple);
    } else if (type == TYPE_SETCOLOR2) {
      if (isNew > 0) {

        /* Don't call it directly to avoid having to import led.bbh */
        setColorWrapper(MELD_INT(GET_TUPLE_FIELD(action_tuple, 0)));
      }
      FREE_TUPLE(action_tuple);
    } else if (type == TYPE_SETPOSITION) {
      if (isNew > 0) {

        /* Don't call it directly to avoid having to import led.bbh */
        setPosition(MELD_INT(GET_TUPLE_FIELD(action_tuple, 0)),
            MELD_INT(GET_TUPLE_FIELD(action_tuple, 1)),
            0);
      }
      FREE_TUPLE(action_tuple);
    } else if (type == TYPE_MOVETO) {
      if (isNew > 0) {
        moveTo(MELD_INT(GET_TUPLE_FIELD(action_tuple, 0)),
            MELD_INT(GET_TUPLE_FIELD(action_tuple, 1)),
            0);
      }
      FREE_TUPLE(action_tuple);
    }
  }

  /* Run an action onto the block */
  inline void MeldInterpretVM::execute_run_action (const unsigned char *pc, Register *reg, int isNew) {
    ++pc;

    byte reg_index = FETCH(pc);

    tuple_t action_tuple = (tuple_t)reg[reg_index];
    tuple_type type = TUPLE_TYPE(action_tuple);
    execute_run_action0(action_tuple, type, isNew);
  }

  inline void MeldInterpretVM::execute_remove (const unsigned char *pc, Register *reg, int isNew) {
    if (isNew > 0) {
      ++pc;
      int reg_remove = REMOVE_REG(pc);
      tuple_type type = TUPLE_TYPE(MELD_CONVERT_REG_TO_PTR(reg[reg_remove]));
      int size = TYPE_SIZE(type);

      tuple_handle(memcpy(malloc(size),MELD_CONVERT_REG_TO_PTR(reg[reg_remove]), size), -1, reg);
      reg[REMOVE_REG(pc)] = 0;
    }
  }

  /* ************* MOVE INSTRUCTIONS FUNCTIONS ************* */

  /* Moves an int to a tuple field */
  inline void MeldInterpretVM::execute_mvintfield (const unsigned char *pc, Register *reg) {
    ++pc;

    Register *src = (Register*)eval_int (&pc);
    byte reg_index = FETCH(pc+1);
    byte field_num = FETCH(pc);

    tuple_t dst_tuple = (tuple_t)reg[reg_index];
    tuple_type type = TUPLE_TYPE(dst_tuple);

    Register *dst = (Register*)eval_field (dst_tuple, &pc);

    size_t size = TYPE_ARG_SIZE(type, field_num);

    memcpy(dst, src, size);
  }

  /* Moves pointer to an int to a register */
  inline void MeldInterpretVM::execute_mvintreg (const unsigned char *pc, Register *reg) {
    ++pc;

    Register *src = (Register*)eval_int (&pc);
    byte reg_index = FETCH(pc);
    Register *dst = (Register*)eval_reg (reg_index, &pc, reg);
    size_t size = sizeof(Register);
    memcpy(dst, src, size);
  }

  /* Moves pointer to a float to a register */
  inline void MeldInterpretVM::execute_mvfloatreg (const unsigned char *pc, Register *reg) {
    ++pc;

    Register *src = (Register*)eval_float (&pc);
    byte reg_index = FETCH(pc);
    Register *dst = (Register*)eval_reg (reg_index, &pc, reg);


    size_t size = sizeof(Register);
    memcpy(dst, src, size);
  }

  /* Moves pointer to a float to a tuple field */
  inline void MeldInterpretVM::execute_mvfloatfield (const unsigned char *pc, Register *reg) {
    ++pc;

    Register *src = (Register*)eval_float (&pc);
    byte reg_index = FETCH(pc+1);
    byte field_num = FETCH(pc);

    tuple_t dst_tuple = (tuple_t)reg[reg_index];
    tuple_type type = TUPLE_TYPE(dst_tuple);

    Register *dst = (Register*)eval_field (dst_tuple, &pc);

    size_t size = TYPE_ARG_SIZE(type, field_num);

    memcpy(dst, src, size);
  }

  /* Moves pointer to a tuple field to a register */
  inline void MeldInterpretVM::execute_mvfieldreg (const unsigned char *pc, Register *reg) {
    ++pc;
    byte field_reg = FETCH(pc+1);
    byte field_num = FETCH(pc);

    tuple_t tpl = (tuple_t)reg[field_reg];
    Register *src = (Register*)eval_field (tpl, &pc);

    byte reg_index = FETCH(pc);
    Register *dst = (Register*)eval_reg (reg_index, &pc, reg);

    (void)field_num;
    size_t size = TYPE_ARG_SIZE(TUPLE_TYPE(tpl), field_num);
    memcpy(dst, src, size);
  }

  /* Moves value pointed at by a register to a field */
  inline void MeldInterpretVM::execute_mvregfield (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg_index = FETCH(pc);
    Register *src = (Register*)eval_reg (reg_index, &pc, reg);

    byte field_reg = FETCH(pc+1);
    byte field_num = FETCH(pc);

    tuple_t field_tpl = (tuple_t)reg[field_reg];
    tuple_type type = TUPLE_TYPE(field_tpl);
    Register *dst = (Register*)eval_field (field_tpl, &pc);


    size_t size = TYPE_ARG_SIZE(type, field_num);

    memcpy(dst, src, size);
  }

  /* Moves content of a tuple field to another */
  inline void MeldInterpretVM::execute_mvfieldfield (const unsigned char *pc, Register *reg) {
    ++pc;
    byte src_field_reg = FETCH(pc+1);
    byte src_field_num = FETCH(pc);

    tuple_t src_field_tpl = (tuple_t)reg[src_field_reg];
    Register *src = (Register*)eval_field (src_field_tpl, &pc);

    byte dst_field_reg = FETCH(pc+1);
    byte dst_field_num = FETCH(pc);

    tuple_t dst_field_tpl = (tuple_t)reg[dst_field_reg];
    tuple_type type = TUPLE_TYPE(dst_field_tpl);
    Register *dst = (Register*)eval_field (dst_field_tpl, &pc);

    (void) src_field_num;

    size_t size = TYPE_ARG_SIZE(type, dst_field_num);

    memcpy(dst, src, size);
  }

  /* Moves blockId to a tuple field */
  inline void MeldInterpretVM::execute_mvhostfield (const unsigned char *pc, Register *reg) {
    ++pc;

    Register *src = (Register*)EVAL_HOST;

    byte field_reg = FETCH(pc+1);
    byte field_num = FETCH(pc);

    tuple_t field_tpl = (tuple_t)reg[field_reg];
    tuple_type type = TUPLE_TYPE(field_tpl);
    Register *dst = (Register*)eval_field (field_tpl, &pc);

    size_t size = TYPE_ARG_SIZE(type, field_num);

    memcpy(dst, src, size);
  }

  /* Moves blockId to a register */
  inline void MeldInterpretVM::execute_mvhostreg (const unsigned char *pc, Register *reg) {
    ++pc;

    Register *src = (Register*)EVAL_HOST;

    byte reg_index = FETCH(pc);
    Register *dst = (Register*)eval_reg (reg_index, &pc, reg);


    size_t size = sizeof(Register);

    memcpy(dst, src, size);
  }

  /* Moves content of a reg to another */
  inline void MeldInterpretVM::execute_mvregreg (const unsigned char *pc, Register *reg) {
    ++pc;

    byte src_reg_index = FETCH(pc);
    Register *src = (Register*)eval_reg (src_reg_index, &pc, reg);

    byte dst_reg_index = FETCH(pc);
    Register *dst = (Register*)eval_reg (dst_reg_index, &pc, reg);


    size_t size = sizeof(Register);

    memcpy(dst, src, size);
  }

  /* ************* OPERATION INSTRUCTIONS FUNCTIONS ************* */

  /* Perform boolean NOT operation */
  inline void MeldInterpretVM::execute_not (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);

    Register *arg = (Register*)eval_reg (reg1, &pc, reg);
    Register *dest = (Register*)eval_reg (reg2, &pc, reg);

    if (MELD_BOOL(arg) > 0)
      *dest = 0;
    else
      *dest = 1;

  }

  /* Perform boolean OR operation */
  inline void MeldInterpretVM::execute_boolor (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);

    *dest = (MELD_BOOL(arg1) | MELD_BOOL(arg2));

  }

  inline void MeldInterpretVM::execute_boolequal (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);

    *dest = (MELD_BOOL(arg1) == MELD_BOOL(arg2));
  }

  inline void MeldInterpretVM::execute_boolnotequal (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);

    *dest = (MELD_BOOL(arg1) != MELD_BOOL(arg2));

  }

  /* Compares two blockId and store the result to 'dest' */
  inline void MeldInterpretVM::execute_addrequal (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_NODE_ID(arg1) == MELD_NODE_ID(arg2));
  }

  /* Compares two blockId and store the result to 'dest' */
  inline void MeldInterpretVM::execute_addrnotequal (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_NODE_ID(arg1) != MELD_NODE_ID(arg2));
  }

  /* Same with and int... */
  inline void MeldInterpretVM::execute_intequal (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_INT(arg1) == MELD_INT(arg2));
  }

  inline void MeldInterpretVM::execute_intnotequal (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_INT(arg1) != MELD_INT(arg2));
  }

  inline void MeldInterpretVM::execute_intgreater (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_INT(arg1) > MELD_INT(arg2));
  }

  inline void MeldInterpretVM::execute_intgreaterequal (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_INT(arg1) >= MELD_INT(arg2));
  }

  inline void MeldInterpretVM::execute_intlesser (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_INT(arg1) < MELD_INT(arg2));
  }

  inline void MeldInterpretVM::execute_intlesserequal (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_INT(arg1) <= MELD_INT(arg2));
  }

  inline void MeldInterpretVM::execute_intmul (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_INT(arg1) * MELD_INT(arg2));
  }

  inline void MeldInterpretVM::execute_intdiv (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_INT(arg1) / MELD_INT(arg2));
  }

  inline void MeldInterpretVM::execute_intmod (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_INT(arg1) % MELD_INT(arg2));

  }
  inline void MeldInterpretVM::execute_intplus (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_INT(arg1) + MELD_INT(arg2));
  }

  inline void MeldInterpretVM::execute_intminus (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_INT(arg1) - MELD_INT(arg2));
  }

  inline void MeldInterpretVM::execute_floatplus (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_FLOAT(arg1) + MELD_FLOAT(arg2));

  }

  inline void MeldInterpretVM::execute_floatminus (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_FLOAT(arg1) - MELD_FLOAT(arg2));

  }

  inline void MeldInterpretVM::execute_floatmul (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_FLOAT(arg1) * MELD_FLOAT(arg2));

  }

  inline void MeldInterpretVM::execute_floatdiv (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_FLOAT(arg1) / MELD_FLOAT(arg2));

  }

  inline void MeldInterpretVM::execute_floatequal (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_FLOAT(arg1) == MELD_FLOAT(arg2));

  }

  inline void MeldInterpretVM::execute_floatnotequal (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_FLOAT(arg1) != MELD_FLOAT(arg2));

  }

  inline void MeldInterpretVM::execute_floatlesser (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_FLOAT(arg1) < MELD_FLOAT(arg2));

  }

  inline void MeldInterpretVM::execute_floatlesserequal (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_FLOAT(arg1) <= MELD_FLOAT(arg2));

  }

  inline void MeldInterpretVM::execute_floatgreater (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_FLOAT(arg1) > MELD_FLOAT(arg2));

  }

  inline void MeldInterpretVM::execute_floatgreaterequal (const unsigned char *pc, Register *reg) {
    ++pc;

    byte reg1 = FETCH(pc);
    byte reg2 = FETCH(pc+1);
    byte reg3 = FETCH(pc+2);

    Register *arg1 = (Register*)eval_reg (reg1, &pc, reg);
    Register *arg2 = (Register*)eval_reg (reg2, &pc, reg);
    Register *dest = (Register*)eval_reg (reg3, &pc, reg);
    *dest = (MELD_FLOAT(arg1) >= MELD_FLOAT(arg2));

  }

  /* END OF INSTR EXECUTION FUNCTIONS */

  /* ************* QUEUE MANAGEMENT FUNCTIONS ************* */

  int MeldInterpretVM::queue_length (tuple_queue *queue) {
    int i;
    tuple_entry *entry = queue->head;

    for (i = 0; entry != NULL; entry = entry->next, i++);

    return i;
  }

  bool MeldInterpretVM::queue_is_empty(tuple_queue *_queue) {
    return _queue->head == NULL;
  }

  void MeldInterpretVM::queue_push_tuple(tuple_queue *queue, tuple_entry *entry) {
    if(queue->head == NULL){
      queue->head = entry;
      queue->tail = entry;
    }
    else {
      queue->tail->next = entry;
      queue->tail = entry;
    }
  }

  tuple_t MeldInterpretVM::queue_pop_tuple(tuple_queue *_queue) {
    tuple_entry *entry = NULL;

    if (_queue->head) {
      entry = _queue->head;
      _queue->head = _queue->head->next;

      if (_queue->head == NULL)
        _queue->tail = NULL;
    }

    return entry;
  }

  tuple_t MeldInterpretVM::queue_dequeue_pos(tuple_queue *_queue, tuple_entry **pos) {
    tuple_entry *entry = *pos;
    tuple_entry *next = (*pos)->next;
    _queue->length--;

    if (entry == _queue->tail) {
      if(entry == _queue->head)
        _queue->tail = NULL;
      else
        _queue->tail = (tuple_entry *)pos; /* previous */
    }

    *pos = next;

    tuple_t tuple = entry->tuple;
    free(entry);

    return tuple;
  }

  tuple_entry* MeldInterpretVM::queue_enqueue(tuple_queue *_queue, tuple_t tuple, record_type isNew) {
    tuple_entry *entry = (tuple_entry*)malloc(sizeof(tuple_entry));
    entry->tuple = tuple;
    entry->records = isNew;
    entry->next = NULL;
    queue_push_tuple(_queue, entry);
    _queue->length++;
    return entry;
  }

  tuple_t MeldInterpretVM::queue_dequeue(tuple_queue *queue, int *isNew) {
    tuple_entry *entry = (tuple_entry*)queue_pop_tuple(queue);
    queue->length--;

    tuple_t tuple = entry->tuple;

    if(isNew)
      *isNew = entry->records.count;

    free(entry);

    return tuple;
  }

  tuple_pentry* MeldInterpretVM::p_dequeue(tuple_pqueue *q) {
    tuple_pentry *ret = q->queue;

    if(q->queue != NULL)
      q->queue = q->queue->next;

    return ret;
  }

  void MeldInterpretVM::p_enqueue(tuple_pqueue *queue, Time priority, tuple_t tuple,
      NodeID rt, record_type isNew) {
    tuple_pentry *entry = (tuple_pentry*)malloc(sizeof(tuple_pentry));

    entry->tuple = tuple;
    entry->records = isNew;
    entry->priority = priority;
    entry->rt = rt;

    tuple_pentry **spot;
    for (spot = &(queue->queue);
        *spot != NULL &&
        (*spot)->priority <= priority;
        spot = &((*spot)->next));

    entry->next = *spot;
    *spot = entry;
  }

  /* ************* VM INITIALIZATION FUNCTIONS ************* */

  static int type;
  void MeldInterpretVM::init_fields(void) {
    size_t total = 2*NUM_TYPES;
    int i, j;

    for(i = 0; i < NUM_TYPES; ++i)
      total += TYPE_NUMARGS(i) * 2;

    arguments = (unsigned char*)malloc(total);
    unsigned char *start = arguments + 2*NUM_TYPES;
    unsigned char offset, size;

    for(i = 0; i < NUM_TYPES; ++i) {
      arguments[i*2] = start - arguments; /* start */
      offset = 0;

      for(j = 0; j < TYPE_NUMARGS(i); ++j) {
        type = TYPE_ARG_TYPE(i, j);
        switch (type) {

          case (int)FIELD_INT:
          case (int)FIELD_TYPE:
            size = sizeof(meld_int);
            break;

          case (int)FIELD_FLOAT:
            size = sizeof(meld_float);
            break;

          case (int)FIELD_BOOL:
            size = sizeof(byte);
            break;

          case (int)FIELD_ADDR:
            size = sizeof(NodeID);
            break;

          case (int)FIELD_LIST_INT:
          case (int)FIELD_LIST_FLOAT:
          case (int)FIELD_LIST_ADDR:
          case (int)FIELD_SET_INT:
          case (int)FIELD_SET_FLOAT:
          case (int)FIELD_STRING:
            size = sizeof(void*);
            break;

          default:
            myassert(0);
            size = 0;
            break;
        }

        start[0] = size; /* argument size */
        start[1] = offset; /* argument offset */

        offset += size;
        start += 2;
      }
      arguments[i*2+1] = offset + TYPE_FIELD_SIZE; /* tuple size */
    }
  }

  /* Get TYPE id for useful types */
  void MeldInterpretVM::init_consts() {
    tuple_type i;
    for (i = 0; i < NUM_TYPES; i++) {
      if(strcmp(TYPE_NAME(i), "edge") == 0)
        TYPE_EDGE = i;
      else if(strcmp(TYPE_NAME(i), "terminate") == 0)
        TYPE_TERMINATE = i;
    }
  }

  /* ************* AGGREGATE RELATED FUNCTIONS ************* */

  inline bool MeldInterpretVM::aggregate_accumulate(int agg_type, void *acc, void *obj, int count) {
    switch (agg_type) {
      case AGG_SET_UNION_INT:
      case AGG_SET_UNION_FLOAT:
        myassert(false);
        return false;

      case AGG_FIRST:
        return false;

      case AGG_MAX_INT:
        if (MELD_INT(obj) > MELD_INT(acc)) {
          MELD_INT(acc) = MELD_INT(obj);
          return true;
        } else
          return false;

      case AGG_MIN_INT:
        if (MELD_INT(obj) < MELD_INT(acc)) {
          MELD_INT(acc) = MELD_INT(obj);
          return true;
        } else
          return false;

      case AGG_SUM_INT:
        MELD_INT(acc) += MELD_INT(obj) * count;
        return false;

      case AGG_MAX_FLOAT:
        if(MELD_FLOAT(obj) > MELD_FLOAT(acc)) {
          MELD_FLOAT(acc) = MELD_FLOAT(obj);
          return true;
        } else
          return false;

      case AGG_MIN_FLOAT:
        if(MELD_FLOAT(obj) < MELD_FLOAT(acc)) {
          MELD_FLOAT(acc) = MELD_FLOAT(obj);
          return true;
        } else
          return false;

      case AGG_SUM_FLOAT:
        MELD_FLOAT(acc) += MELD_FLOAT(obj) * (meld_float)count;
        return false;

      case AGG_SUM_LIST_INT:
      case AGG_SUM_LIST_FLOAT:
        myassert(false);
        return false;
    }

    myassert(0);
    while(1);
  }

  inline bool MeldInterpretVM::aggregate_changed(int agg_type, void *v1, void *v2) {
    switch(agg_type) {
      case AGG_FIRST:
        return false;

      case AGG_MIN_INT:
      case AGG_MAX_INT:
      case AGG_SUM_INT:
        return MELD_INT(v1) != MELD_INT(v2);

      case AGG_MIN_FLOAT:
      case AGG_MAX_FLOAT:
      case AGG_SUM_FLOAT:
        return MELD_FLOAT(v1) != MELD_FLOAT(v2);

      case AGG_SET_UNION_INT:
      case AGG_SET_UNION_FLOAT:
        return false;

      case AGG_SUM_LIST_INT:
      case AGG_SUM_LIST_FLOAT:
        return false;

      default:
        myassert(0);
        return true;
    }

    myassert(0);
    while(1);
  }

  inline void MeldInterpretVM::aggregate_seed(int agg_type, void *acc, void *start, int count, size_t size) {
    switch(agg_type) {
      case AGG_FIRST:
        memcpy(acc, start, size);
        return;
      case AGG_MIN_INT:
      case AGG_MAX_INT:
        MELD_INT(acc) = MELD_INT(start);
        return;
      case AGG_SUM_INT:
        MELD_INT(acc) = MELD_INT(start) * count;
        return;
      case AGG_MIN_FLOAT:
      case AGG_MAX_FLOAT:
        MELD_FLOAT(acc) = MELD_FLOAT(start);
        return;
      case AGG_SUM_FLOAT:
        MELD_FLOAT(acc) = MELD_FLOAT(start) * count;
        return;
      case AGG_SET_UNION_INT:
      case AGG_SET_UNION_FLOAT:
      case AGG_SUM_LIST_INT:
      case AGG_SUM_LIST_FLOAT:
        myassert(false);
        return;
    }

    myassert(0);
    while(1);
  }

  inline void MeldInterpretVM::aggregate_free(tuple_t tuple, unsigned char field_aggregate, unsigned char type_aggregate) {
    switch(type_aggregate) {
      case AGG_FIRST:
      case AGG_MIN_INT:
      case AGG_MAX_INT:
      case AGG_SUM_INT:
      case AGG_MIN_FLOAT:
      case AGG_MAX_FLOAT:
      case AGG_SUM_FLOAT:
        /* nothing to do */
        break;

      case AGG_SET_UNION_INT:
      case AGG_SET_UNION_FLOAT:
      case AGG_SUM_LIST_INT:
      case AGG_SUM_LIST_FLOAT:
        myassert(false);
        break;

      default:
        myassert(0);
        break;
    }
  }

  inline void MeldInterpretVM::aggregate_recalc(tuple_entry *agg, Register *reg, bool first_run) {
    tuple_type type = TUPLE_TYPE(agg->tuple);

    tuple_entry *cur;

    int agg_type = AGG_AGG(TYPE_AGGREGATE(type));
    int agg_field = AGG_FIELD(TYPE_AGGREGATE(type));
    tuple_queue *agg_queue = agg->records.agg_queue;
    tuple_entry *agg_list = agg_queue->head;
    tuple_t tuple = agg_list->tuple;

    void* start = GET_TUPLE_FIELD(tuple, agg_field);

    /* make copy */
    size_t size = TYPE_ARG_SIZE(type, agg_field);
    void* accumulator = malloc(size);

    aggregate_seed(agg_type, accumulator, start, agg_list->records.count, size);

    /* calculate offsets to copy right side to aggregated tuple */
    size_t size_offset = TYPE_FIELD_SIZE + TYPE_ARG_OFFSET(type, agg_field) + TYPE_ARG_SIZE(type, agg_field);
    size_t total_copy = TYPE_SIZE(type) - size_offset;
    tuple_t target_tuple = NULL;

    if (total_copy > 0)
      target_tuple = tuple;

    for (cur = agg_list->next; cur != NULL; cur = cur->next) {
      if(aggregate_accumulate(agg_type, accumulator,
            GET_TUPLE_FIELD(cur->tuple, agg_field), cur->records.count))
        target_tuple = cur->tuple;
    }

    void *acc_area = GET_TUPLE_FIELD(agg->tuple, agg_field);

    if(first_run)
      memcpy(acc_area, accumulator, size);
    else if (aggregate_changed(agg_type, acc_area, accumulator)) {
      process_bytecode(agg->tuple, TYPE_START(type), -1, NOT_LINEAR, reg, PROCESS_TUPLE);
      aggregate_free(agg->tuple, agg_field, agg_type);
      memcpy(acc_area, accumulator, size);
      if (total_copy > 0) /* copy right side from target tuple */
        memcpy(((unsigned char *)agg->tuple) + size_offset, ((unsigned char *)target_tuple) + size_offset, total_copy);
      process_bytecode(agg->tuple, TYPE_START(type), 1, NOT_LINEAR, reg, PROCESS_TUPLE);
    }

    free(accumulator);
  }

  /* ************* PROCESSING FUNCTIONS ************* */

  /* Handles a tuple, if it is a derivation tuple (isNew > 0):
   * -> Process tuple and add it to the database
   * If it is a retraction tuple (isNew < 0):
   * -> Iterate through the database and find corresponding tuple, dequeue and
   * process it with a isNew of -1 to perform retraction, and free both tuples.
   */
  void MeldInterpretVM::tuple_do_handle(tuple_type type, tuple_t tuple, int isNew, Register *reg) {
    if(type == TYPE_TERMINATE) {
      FREE_TUPLE(tuple);
      TERMINATE_CURRENT();
      return;
    }


    if (TYPE_IS_ACTION(type)) {
      if(isNew > 0)
        execute_run_action0(tuple, type, isNew);
      else
        FREE_TUPLE(tuple);
      return;
    }

    if (!TYPE_IS_AGG(type) || TYPE_IS_LINEAR(type)) {
      tuple_queue *queue = &TUPLES[type];
      tuple_entry** current;
      tuple_entry* cur;

      for (current = &queue->head; *current != NULL; current = &(*current)->next) {
        cur = *current;
        if (memcmp(cur->tuple, tuple, TYPE_SIZE(type)) == 0) {
          cur->records.count += isNew;

          if (cur->records.count <= 0) {
            /* Remove fact from database */
            if (!TYPE_IS_LINEAR(type))
              process_bytecode(tuple, TYPE_START(TUPLE_TYPE(tuple)), isNew,
                  NOT_LINEAR, reg, PROCESS_TUPLE);

            FREE_TUPLE(queue_dequeue_pos(queue, current));
            /* Also free retraction fact */
            FREE_TUPLE(tuple);

            return;
          }

          if(isNew > 0 && !TYPE_IS_LINEAR(type)) {
            /* tuple found, no need to rederive */
            FREE_TUPLE(tuple);
            return;
          }
        }
      }

      // if deleting, return
      if (isNew <= 0) {

        FREE_TUPLE(tuple);
        return;
      }

      queue_enqueue(queue, tuple, (record_type) isNew);

      process_bytecode(tuple, TYPE_START(TUPLE_TYPE(tuple)), isNew, TYPE_IS_LINEAR(TUPLE_TYPE(tuple)), reg, PROCESS_TUPLE);
      return;
    }

    unsigned char type_aggregate = TYPE_AGGREGATE(type);
    unsigned char field_aggregate = AGG_FIELD(type_aggregate);

    tuple_entry **current;
    tuple_entry *cur;
    tuple_queue *queue = &(TUPLES[type]);

    for (current = &queue->head;
        (*current) != NULL;
        current = &(*current)->next) {
      cur = *current;

      size_t sizeBegin = TYPE_FIELD_SIZE + TYPE_ARG_OFFSET(type, field_aggregate);
      char *start = (char*)(cur->tuple);

      if(memcmp(start, tuple, sizeBegin))
        continue;

      size_t sizeOffset = sizeBegin + TYPE_ARG_SIZE(type, field_aggregate);
      size_t sizeEnd = TYPE_SIZE(type) - sizeOffset;

      if (memcmp(start + sizeOffset, (char*)tuple + sizeOffset, sizeEnd))
        continue;
      tuple_queue *agg_queue = cur->records.agg_queue;

      /* AGG_FIRST aggregate optimization */
      if(AGG_AGG(type_aggregate) == AGG_FIRST
          && isNew > 0
          && !queue_is_empty(agg_queue)) {
        FREE_TUPLE(tuple);
        return;
      }

      tuple_entry** current2;
      tuple_entry* cur2;

      for (current2 = &agg_queue->head;
          *current2 != NULL;
          current2 = &(*current2)->next) {
        cur2 = *current2;

        if (memcmp(cur2->tuple, tuple, TYPE_SIZE(type)) == 0) {
          cur2->records.count += isNew;

          if (cur2->records.count <= 0) {
            // remove it
            FREE_TUPLE(queue_dequeue_pos(agg_queue, current2));

            if (queue_is_empty(agg_queue)) {
              /* aggregate is removed */
              void *aggTuple = queue_dequeue_pos(queue, current);

              /* delete queue */
              free(agg_queue);

              process_bytecode(aggTuple, TYPE_START(TUPLE_TYPE(aggTuple)),
                  -1, NOT_LINEAR, reg, PROCESS_TUPLE);
              aggregate_free(aggTuple, field_aggregate, AGG_AGG(type_aggregate));
              FREE_TUPLE(aggTuple);
            } else
              aggregate_recalc(cur, reg, false);
          } else
            aggregate_recalc(cur, reg, false);


          FREE_TUPLE(tuple);
          return;
        }
      }

      // if deleting, return
      if (isNew <= 0) {

        FREE_TUPLE(tuple);
        return;
      }

      queue_enqueue(agg_queue, tuple, (record_type) isNew);
      aggregate_recalc(cur, reg, false);

      return;
    }

    // if deleting, return
    if (isNew <= 0) {
      FREE_TUPLE(tuple);
      return;
    }

    // So now we know we have a new tuple
    tuple_t tuple_cpy = ALLOC_TUPLE(TYPE_SIZE(type));
    memcpy(tuple_cpy, tuple, TYPE_SIZE(type));

    /* create aggregate queue */
    tuple_queue *agg_queue = (tuple_queue*)malloc(sizeof(tuple_queue));

    queue_init(agg_queue);

    queue_enqueue(agg_queue, tuple, (record_type) isNew);
    tuple_entry *entry = queue_enqueue(&TUPLES[type], tuple_cpy, (record_type)agg_queue);

    aggregate_recalc(entry, reg, true);
    process_bytecode(tuple, TYPE_START(type), isNew, NOT_LINEAR, reg, PROCESS_TUPLE);
  }


  int MeldInterpretVM::process_bytecode (tuple_t tuple, const unsigned char *pc, int isNew, int isLinear, Register *reg, byte state) {


    /* Move tuple to register 0 so it can be accessed */
    if (state == PROCESS_TUPLE)
      moveTupleToReg (0, tuple, reg);
    /* Only if process_bytecode not called by iter, */
    /* because otherwise the tuple is already in a register */

    for (;;) {
eval_loop:


      switch (*(const unsigned char*)pc) {
        case RETURN_INSTR: {	/* 0x0 */

                             return RET_RET;
                           }

        case NEXT_INSTR: {	/* 0x1 */

                           return RET_NEXT;
                         }

#define DECIDE_NEXT_ITER()                      \
                         if(ret == RET_LINEAR)                  \
                         return RET_LINEAR;                  \
                         if(ret == RET_DERIVED && isLinear)     \
                         return RET_DERIVED;                 \
                         if(ret == RET_RET)                     \
                         return RET_RET;                     \
                         pc = npc; goto eval_loop;

        case PERS_ITER_INSTR: {	/* 0x02 */
                                const byte *npc = pc + ITER_OUTER_JUMP(pc);
                                const int ret = execute_iter (pc, reg, isNew, isLinear);
                                DECIDE_NEXT_ITER();
                              }

        case LINEAR_ITER_INSTR: {	/* 0x05 */
                                  const byte *npc = pc + ITER_OUTER_JUMP(pc);
                                  const int ret = execute_iter (pc, reg, isNew, isLinear);
                                  DECIDE_NEXT_ITER();
                                }

        case NOT_INSTR: {	/* 0x07 */
                          const byte *npc = pc + NOT_BASE;
                          execute_not (pc, reg);
                          pc = npc;
                          goto eval_loop;
                        }

        case SEND_INSTR: {	/* 0x08 */
                           const byte *npc = pc + SEND_BASE;
                           execute_send (pc, reg, isNew);
                           pc = npc;
                           goto eval_loop;
                         }

        case RESET_LINEAR_INSTR: { /* 0x0e */
                                   int ret = process_bytecode(tuple, pc + RESET_LINEAR_BASE, isNew, NOT_LINEAR, reg, PROCESS_ITER);
                                   (void)ret;
                                   pc += RESET_LINEAR_JUMP(pc);
                                   goto eval_loop;
                                 }
                                 break;

        case END_LINEAR_INSTR: /* 0x0f */
                                 return RET_LINEAR;

        case RULE_INSTR: {	/* 0x10 */
                           const byte *npc = pc + RULE_BASE;

                           pc = npc;
                           goto eval_loop;
                         }

        case RULE_DONE_INSTR: {	/* 0x11 */

                                const byte *npc = pc + RULE_DONE_BASE;
                                pc = npc;
                                goto eval_loop;
                              }

                              /* NOT TESTED */
        case SEND_DELAY_INSTR: {	/* 0x15 */
                                 const byte *npc = pc + SEND_DELAY_BASE;
                                 execute_send_delay (pc, reg, isNew);
                                 pc = npc;
                                 goto eval_loop;
                               }

        case RETURN_LINEAR_INSTR: {		/* 0xd0 */

                                    return RET_LINEAR;
                                  }

        case RETURN_DERIVED_INSTR: {		/* 0xf0 */

                                     return RET_DERIVED;
                                   }

        case MVINTFIELD_INSTR: {	/* 0x1e */
                                 const byte *npc = pc + MVINTFIELD_BASE;
                                 execute_mvintfield (pc, reg);
                                 pc = npc;
                                 goto eval_loop;
                               }

        case MVINTREG_INSTR: {	/* 0x1f */
                               const byte *npc = pc + MVINTREG_BASE;
                               execute_mvintreg (pc, reg);
                               pc = npc;
                               goto eval_loop;
                             }

        case MVFIELDFIELD_INSTR: {	/* 0x21 */
                                   const byte *npc = pc + MVFIELDFIELD_BASE;
                                   execute_mvfieldfield (pc, reg);
                                   pc = npc;
                                   goto eval_loop;
                                 }

        case MVFIELDREG_INSTR: {	/* 0x22 */
                                 const byte *npc = pc + MVFIELDREG_BASE;
                                 execute_mvfieldreg (pc, reg);
                                 pc = npc;
                                 goto eval_loop;
                               }

        case MVPTRREG_INSTR: {	/* 0x23 */
                               const byte *npc = pc + MVPTRREG_BASE;

                               /* TODO: Do something if used elsewhere than axiom derivation */
                               pc = npc;
                               goto eval_loop;
                             }


        case MVFIELDFIELDR_INSTR: { /* 0x25 */
                                    const byte *npc = pc + MVFIELDFIELD_BASE;
                                    execute_mvfieldfield (pc, reg);
                                    pc = npc;
                                    goto eval_loop;
                                  }

        case MVREGFIELD_INSTR: {	/* 0x26 */
                                 const byte *npc = pc + MVREGFIELD_BASE;
                                 execute_mvregfield (pc, reg);
                                 pc = npc;
                                 goto eval_loop;
                               }

        case MVHOSTFIELD_INSTR: {	/* 0x28 */
                                  const byte *npc = pc + MVHOSTFIELD_BASE;
                                  execute_mvhostfield (pc, reg);
                                  pc = npc;
                                  goto eval_loop;
                                }

                                /* NOT TESTED */
        case MVFLOATFIELD_INSTR: {	/* 0x2d */
                                   const byte *npc = pc + MVFLOATFIELD_BASE;
                                   execute_mvfloatfield (pc, reg);
                                   pc = npc;
                                   goto eval_loop;
                                 }

                                 /* NOT TESTED */
        case MVFLOATREG_INSTR: {	/* 0x2e */
                                 const byte *npc = pc + MVFLOATREG_BASE;
                                 execute_mvfloatreg (pc, reg);
                                 pc = npc;
                                 goto eval_loop;
                               }

                               /* NOT TESTED */
        case MVHOSTREG_INSTR: {	/* 0x37 */
                                const byte *npc = pc + MVHOSTREG_BASE;
                                execute_mvhostreg (pc, reg);
                                pc = npc;
                                goto eval_loop;
                              }

        case ADDRNOTEQUAL_INSTR: {	/* 0x38 */
                                   const byte *npc = pc + OP_BASE;
                                   execute_addrnotequal (pc, reg);
                                   pc = npc;
                                   goto eval_loop;
                                 }

        case ADDREQUAL_INSTR: {	/* 0x39 */
                                const byte *npc = pc + OP_BASE;
                                execute_addrequal (pc, reg);
                                pc = npc;
                                goto eval_loop;
                              }

        case INTMINUS_INSTR: {	/* 0x3a */
                               const byte *npc = pc + OP_BASE;
                               execute_intminus (pc, reg);
                               pc = npc;
                               goto eval_loop;
                             }

        case INTEQUAL_INSTR: {	/* 0x3b */
                               const byte *npc = pc + OP_BASE;
                               execute_intequal (pc, reg);
                               pc = npc;
                               goto eval_loop;
                             }

        case INTNOTEQUAL_INSTR: {	/* 0x3c */
                                  const byte *npc = pc + OP_BASE;
                                  execute_intnotequal (pc, reg);
                                  pc = npc;
                                  goto eval_loop;
                                }

        case INTPLUS_INSTR: {	/* 0x3d */
                              const byte *npc = pc + OP_BASE;
                              execute_intplus (pc, reg);
                              pc = npc;
                              goto eval_loop;
                            }

        case INTLESSER_INSTR: {	/* 0x3e */
                                const byte *npc = pc + OP_BASE;
                                execute_intlesser (pc, reg);
                                pc = npc;
                                goto eval_loop;
                              }

        case INTGREATEREQUAL_INSTR: {	/* 0x3f */
                                      const byte *npc = pc + OP_BASE;
                                      execute_intgreaterequal (pc, reg);
                                      pc = npc;
                                      goto eval_loop;
                                    }

        case ALLOC_INSTR: {	/* 0x40 */
                            const byte *npc = pc + ALLOC_BASE;
                            execute_alloc (pc, reg);
                            pc = npc;
                            goto eval_loop;
                          }

                          /* NOT TESTED */
        case BOOLOR_INSTR: {	/* 0x41 */
                             const byte *npc = pc + OP_BASE;
                             execute_boolor (pc, reg);
                             pc = npc;
                             goto eval_loop;
                           }

        case INTLESSEREQUAL_INSTR: {	/* 0x42 */
                                     const byte *npc = pc + OP_BASE;
                                     execute_intlesserequal (pc, reg);
                                     pc = npc;
                                     goto eval_loop;
                                   }

        case INTGREATER_INSTR: {	/* 0x43 */
                                 const byte *npc = pc + OP_BASE;
                                 execute_intgreater (pc, reg);
                                 pc = npc;
                                 goto eval_loop;
                               }

        case INTMUL_INSTR: {	/* 0x44 */
                             const byte *npc = pc + OP_BASE;
                             execute_intmul (pc, reg);
                             pc = npc;
                             goto eval_loop;
                           }

        case INTDIV_INSTR: {	/* 0x45 */
                             const byte *npc = pc + OP_BASE;
                             execute_intdiv (pc, reg);
                             pc = npc;
                             goto eval_loop;
                           }

        case FLOATPLUS_INSTR: {	/* 0x46 */
                                const byte *npc = pc + OP_BASE;
                                execute_floatplus (pc, reg);
                                pc = npc;
                                goto eval_loop;
                              }

        case FLOATMINUS_INSTR: {	/* 0x47 */
                                 const byte *npc = pc + OP_BASE;
                                 execute_floatminus (pc, reg);
                                 pc = npc;
                                 goto eval_loop;
                               }

        case FLOATMUL_INSTR: {	/* 0x48 */
                               const byte *npc = pc + OP_BASE;
                               execute_floatmul (pc, reg);
                               pc = npc;
                               goto eval_loop;
                             }

        case FLOATDIV_INSTR: {	/* 0x49 */
                               const byte *npc = pc + OP_BASE;
                               execute_floatdiv (pc, reg);
                               pc = npc;
                               goto eval_loop;
                             }

        case FLOATEQUAL_INSTR: {	/* 0x4a */
                                 const byte *npc = pc + OP_BASE;
                                 execute_floatequal (pc, reg);
                                 pc = npc;
                                 goto eval_loop;
                               }

        case FLOATNOTEQUAL_INSTR: {	/* 0x4b */
                                    const byte *npc = pc + OP_BASE;
                                    execute_floatnotequal (pc, reg);
                                    pc = npc;
                                    goto eval_loop;
                                  }

        case FLOATLESSER_INSTR: {	/* 0x4c */
                                  const byte *npc = pc + OP_BASE;
                                  execute_floatlesser (pc, reg);
                                  pc = npc;
                                  goto eval_loop;
                                }

        case FLOATLESSEREQUAL_INSTR: {	/* 0x4d */
                                       const byte *npc = pc + OP_BASE;
                                       execute_floatlesserequal (pc, reg);
                                       pc = npc;
                                       goto eval_loop;
                                     }

        case FLOATGREATER_INSTR: {	/* 0x4e */
                                   const byte *npc = pc + OP_BASE;
                                   execute_floatgreater (pc, reg);
                                   pc = npc;
                                   goto eval_loop;
                                 }

        case FLOATGREATEREQUAL_INSTR: {	/* 0x4f */
                                        const byte *npc = pc + OP_BASE;
                                        execute_floatgreaterequal (pc, reg);
                                        pc = npc;
                                        goto eval_loop;
                                      }

                                      /* NOT TESTED */
        case MVREGREG_INSTR: {	/* 0x50 */
                               const byte *npc = pc + MVREGREG_BASE;
                               execute_mvregreg (pc, reg);
                               pc = npc;
                               goto eval_loop;
                             }

                             /* NOT TESTED */
        case BOOLEQUAL_INSTR: {	/* 0x51 */
                                const byte *npc = pc + OP_BASE;
                                execute_boolequal (pc, reg);
                                pc = npc;
                                goto eval_loop;
                              }

                              /* NOT TESTED */
        case BOOLNOTEQUAL_INSTR: {	/* 0x51 */
                                   const byte *npc = pc + OP_BASE;;
                                   execute_boolnotequal (pc, reg);
                                   pc = npc;
                                   goto eval_loop;
                                 }

        case IF_INSTR: {	/* 0x60 */
                         const byte *npc = pc + IF_BASE;
                         byte *base = (byte*)pc;
                         ++pc;
                         byte reg_index = FETCH(pc);
                         Register *if_reg = (Register*)eval_reg (reg_index, &pc, reg);

                         if (!(unsigned char)(*if_reg)) {


                           pc = base + IF_JUMP(pc);
                           goto eval_loop;
                         }
                         /* else process if content */

                         pc = npc;
                         goto eval_loop;
                       }

        case CALL1_INSTR: {	/* 0x69 */
                            const byte *npc = pc + CALL1_BASE;
                            execute_call1 (pc, reg);
                            pc = npc;
                            goto eval_loop;
                          }

        case ADDLINEAR_INSTR: {	/* 0x77 */
                                const byte *npc = pc + ADDLINEAR_BASE;
                                execute_addtuple (pc, reg, isNew);
                                pc = npc;
                                goto eval_loop;
                              }

        case ADDPERS_INSTR: {	/* 0x78 */
                              const byte *npc = pc + ADDPERS_BASE;
                              execute_addtuple (pc, reg, isNew);
                              pc = npc;
                              goto eval_loop;
                            }

        case RUNACTION_INSTR: {	/* 0x79 */
                                const byte *npc = pc + RUNACTION_BASE;
                                execute_run_action (pc, reg, isNew);
                                pc = npc;
                                goto eval_loop;
                              }

        case UPDATE_INSTR: {	/* 0x7b */
                             const byte *npc = pc + UPDATE_BASE;
                             if (PROCESS_TYPE(state) == PROCESS_ITER)
                               execute_update (pc, reg);
                             pc = npc;
                             goto eval_loop;
                           }

        case REMOVE_INSTR: {	/* 0x80 */
                             const byte *npc = pc + REMOVE_BASE;
                             execute_remove (pc, reg, isNew);
                             pc = npc;
                             goto eval_loop;
                           }

                           /* NOT TESTED */
                           /* CAUTION: I have no way to ensure that it is the correct way to handle
                            * this instruction at this moment, please review this when you encounter it.
                            */
        case IF_ELSE_INSTR: {	/* 0x81 */
                              const byte *npc = pc + IF_ELSE_BASE;
                              byte *base = (byte*)pc;
                              ++pc;
                              byte reg_index = FETCH(pc);
                              Register *if_reg = (Register*)eval_reg (reg_index, &pc, reg);

                              /* If false, jump to else */
                              if (!(unsigned char)(*if_reg)) {


                                pc = base + IF_JUMP(pc);
                                goto eval_loop;
                              } else {
                                /* Else, process if until a jump instruction is encountered
                                 * (it seems...)
                                 */


                                pc = npc;
                                goto eval_loop;
                              }
                            }

                            /* NOT TESTED */
        case JUMP_INSTR: {
                           ++pc;

                           pc += JUMP_BASE + IF_JUMP(pc);
                           goto eval_loop;
                         }

        case INTMOD_INSTR: {	/* 0x7d */
                             const byte *npc = pc + OP_BASE;
                             execute_intmod (pc, reg);
                             pc = npc;
                             goto eval_loop;
                           }

        default:
                           printf ("--%d--\t "
                               "INSTRUCTION NOT IMPLEMENTED YET: %#x %#x %#x %#x %#x\n",
                               getBlockId(),
                               (unsigned char)*pc, (unsigned char)*(pc+1),
                               (unsigned char)*(pc+2), (unsigned char)*(pc+3),
                               (unsigned char)*(pc+4));

                           exit(-2);
      }
    }
  }

#define MAX_STRING_SIZE 200
  /* Prints a tuple */
  void MeldInterpretVM::tuple_print(tuple_t tuple, FILE *fp) {
    unsigned char tuple_type = TUPLE_TYPE(tuple);
    int j;
    char str[MAX_STRING_SIZE];
    char tmp[MAX_STRING_SIZE];

    sprintf(str,"@%d: %s(", getBlockId(), TYPE_NAME(tuple_type));

    for(j = 0; j < TYPE_NUMARGS(tuple_type); ++j) {
      void *field = GET_TUPLE_FIELD(tuple, j);

      if (j > 0)
        strcat(str, ", ");

      switch(TYPE_ARG_TYPE(tuple_type, j)) {
        case FIELD_INT:
          sprintf(tmp, "%d", MELD_INT(field));
          break;
        case FIELD_FLOAT:
          sprintf(tmp, "%f", (double)MELD_FLOAT(field));
          break;
        case FIELD_ADDR:
          sprintf(tmp, "%u", *(uint16_t*)field);
          break;
        case FIELD_LIST_INT:
        case FIELD_LIST_FLOAT:
        case FIELD_LIST_ADDR:
        case FIELD_SET_INT:
        case FIELD_SET_FLOAT:
          myassert(false);
          break;
        case FIELD_TYPE:
          sprintf(tmp, "%s", TYPE_NAME(MELD_INT(field)));
          break;
        case FIELD_BOOL:
          if (MELD_BOOL(field))
            sprintf(tmp, "true");
          else
            sprintf(tmp, "false");
          break;
        default:
          myassert(0);
          break;
      }
      strcat(str,tmp);
    }
    strcat(str, ")");

    OUTPUT << str << endl;
    fprintf(fp,"%s",str);
  }

  /* Prints the content of the whole database */
  void MeldInterpretVM::facts_dump(void) {
    int i;
    char str[MAX_STRING_SIZE];
    char tmp[MAX_STRING_SIZE];

    for (i = 0; i < NUM_TYPES; i++) {
      // don't print fact types that don't exist
      if (TUPLES[i].head == NULL)
        continue;

      // don't print artificial tuple types
      /*
         if (tuple_names[i][0] == '_')
         continue;
         */

      snprintf(str,MAX_STRING_SIZE*sizeof(char),
          "tuple %s (type %d, size: %d)",
          tuple_names[i], i, (int)TYPE_SIZE(i));

      fprintf(stderr, "%s\n",str);

      tuple_entry *tupleEntry;
      for (tupleEntry = TUPLES[i].head; tupleEntry != NULL; tupleEntry = tupleEntry->next) {
        sprintf(str,"  ");
        tuple_print(tupleEntry->tuple, stderr);
        if (TYPE_IS_AGG(i)) {
          strcat(str, "\n    [[[");
          tuple_entry *tpE;
          for (tpE = tupleEntry->records.agg_queue->head;
              tpE != NULL;
              tpE = tpE->next) {
            tuple_print(tpE->tuple, stderr);
            sprintf(tmp, "x%d\n       ", tpE->records.count);
            strcat(str,tmp);
          }
          strcat(str, "\b\b\b]]]");

          fprintf(stderr, "%s\n",str);

        } else {
          sprintf(str, "x%d", tupleEntry->records.count);

          fprintf(stderr, "%s\n",str);

        }
      }
    }
  }

  /* Print program info, this was designed for the oldVM, not sure if it works or not,
   * looks like it should.
   */
  void MeldInterpretVM::print_program_info(void) {
    int i;
    char s[MAX_STRING_SIZE];
    char tmp[MAX_STRING_SIZE];

    for(i = 0; i < NUM_TYPES; ++i) {
      sprintf(s, "Tuple (%s:%d:%d) ", tuple_names[i], i, (int)TYPE_SIZE(i));
      //printf("Tuple (%s:%d:%d) ", tuple_names[i], i, TYPE_SIZE(i));

      strcat(s,"[");
      if(TYPE_IS_AGG(i))
        strcat(s,"agg");
      if(TYPE_IS_LINEAR(i))
        strcat(s,"linear");
      else
        strcat(s,"per");
      if(TYPE_IS_ROUTING(i))
        strcat(s,"route");
      strcat(s,"] ");


      //printf("num_args:%d off:%d ; args(offset, arg_size): ",
      //	   TYPE_NUMARGS(i), TYPE_OFFSET(i));

      snprintf(tmp, MAX_STRING_SIZE*sizeof(char),"num_args:%d off:%d ; args(offset, arg_size): ",
          TYPE_NUMARGS(i), TYPE_OFFSET(i));
      strcat(s,tmp);

      int j;
      for (j = 0; j < TYPE_NUMARGS(i); ++j) {
        //printf(" %d:%d", TYPE_ARG_OFFSET(i, j), TYPE_ARG_SIZE(i, j));
        snprintf(tmp, MAX_STRING_SIZE*sizeof(char), " %d:%d", TYPE_ARG_OFFSET(i, j), TYPE_ARG_SIZE(i, j));
        strcat(s,tmp);
      }

      s[MAX_STRING_SIZE-1] = '\0';

      printf("%s\n",s);

    }
  }

  /* If activated, call each blockTick to ensure that tuples such as vacant, neighbor,
   * or neighborCount or don't have duplicates in the database.
   * i.e. numberOfVacant + numberOfNeighbor < NUM_PORTS :
   * and numberOfNeighborCount == 1 are two conditions which must always be true
   */
  void MeldInterpretVM::databaseConsistencyChecker() {
    int i;
    byte neighborTCount = 0;
    byte vacantTCount = 0;
    for (i = 0; i < NUM_TYPES; i++) {
      if (TUPLES[i].head == NULL)
        continue;

      byte tupleCount = 0;
      tuple_entry *tupleEntry;
      for (tupleEntry = TUPLES[i].head;
          tupleEntry != NULL;
          tupleEntry = tupleEntry->next) {
        if (i == TYPE_NEIGHBOR)
          ++neighborTCount;
        else if (i == TYPE_VACANT)
          ++vacantTCount;
        else
          ++tupleCount;
      }

      if (i == TYPE_NEIGHBORCOUNT) {
        if (!tupleCount > 1) {
          fprintf(stderr, "\x1b[1;32m--%d--\ttuple %s (type %d, size: %d), count = %d\x1b[0m\n", getBlockId(), tuple_names[i], i, (int)TYPE_SIZE(i), tupleCount);
          /* *(char *)0 = 0; */
        }
      } else {
        if ( (neighborTCount + vacantTCount) > 6) {
          fprintf(stderr, "\x1b[31m--%d--\tToo many port tuples! count = %dn +%dv\x1b[0m\n", getBlockId(), neighborTCount, vacantTCount);
        }
        /* *(char *)0 = 0; */
      }
    }
  }

}
