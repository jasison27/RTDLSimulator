const unsigned char meld_prog[] = {
/* NUMBER OF PREDICATES */
0x2, 
/* NUMBER OF RULES */
0x1, 
/* OFFSETS TO PREDICATE DESCRIPTORS */
0x8, 0, 
0xe, 0, 
/* OFFSETS TO RULE DESCRIPTORS */
0x16, 0, 
/* PREDICATE DESCRIPTORS */
0x1b, 0, 0x4, 0, 0, 0, 
0x1c, 0, 0x12, 0, 0x1, 0x2, 0, 0, 
/* RULE DESCRIPTORS */
0x1d, 0, 0, 0x1, 0, 
/* PREDICATE BYTECODE */
/* Predicate 0: */0, 
/* Predicate 1: */0, 
/* RULE BYTECODE */
/* Rule 0: */0x10, 0, 0, 0, 0, 0x5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1, 0x15, 0, 0, 0, 0x37, 0, 0, 0, 0, 0x11, 0x80, 0, 0x40, 0x1, 0, 0x1e, 0x2, 0, 0, 0, 0, 0, 0x1e, 0x3, 0, 0, 0, 0x1, 0, 0x79, 0, 0x23, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xf0, 0x1, 0, };

char *tuple_names[] = {"_init", "setposition", };

char *rule_names[] = {"_init -o node-axioms.", };

#include "extern_functions.bbh"
Register (*extern_functs[])() = {};

int extern_functs_args[] = {};
