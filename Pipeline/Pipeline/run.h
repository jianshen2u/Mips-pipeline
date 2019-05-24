#ifndef _RUN_H_
#define _RUN_H_

#include <stdio.h>

#include "util.h"


/* functions */
uint32_t get_inst_info(uint32_t pc);
void process_instruction();
void process_IF();
void process_ID();
void generate_control_signals(uint32_t inst, int opcode, int func, int addr, int rs, int rt);
void process_EX();
void process_MEM();
void process_WB();
uint32_t ALU(int control_line, uint32_t data1, uint32_t data2);



#endif