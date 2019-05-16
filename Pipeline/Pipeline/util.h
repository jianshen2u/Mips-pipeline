
#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
//#include "run.h"

#define FALSE	0
#define TRUE	1
#define OPCODE	0xfc000000
#define RS		0x3e00000
#define RT		0x1f0000
#define RD		0xf800
#define FUNCT	0x3f
#define SHAMT	0x7c0
#define MEM_TEXT_START	0x00000000
#define MEM_TEXT_SIZE	0x00100000
#define MEM_DATA_START	0x00000000
#define MEM_DATA_SIZE	0x00100000

//R-inst
#define RINST   0
#define ADD		0x20
#define ADDU	0x21
#define AND		0x24
#define JR		0x08
#define NOR		0x27
#define OR		0x25
#define SLT		0x2A
#define SLTU	0x2B
#define SLL		0x00
#define SRL		0x02
#define SUB		0x22
#define SUBU	0x23

//I-inst
#define ADDI	0x08
#define ADDIU	0x09
#define ANDI	0x0C
#define BEQ		0x04
#define BNE		0x05
#define LBU		0x24
#define LL		0x30
#define LUI		0x0F
#define LW		0x23
#define ORI		0x0D
#define SLTI	0x0A
#define SLTIU	0x0B
#define SW		0x2B

//J-Inst
#define J		0x02
#define JAL		0x03

// instruction information extracting macros
#define FUNCT1(INST)    (INST & FUNCT)
#define SHAMT1(INST)    (INST & SHAMT)>>6
#define RD1(INST)       (INST & RD)>>11
#define RT1(INST)       (INST & RT)>>16
#define RS1(INST)       (INST & RS)>>21
#define OPCODE1(INST)   (INST & OPCODE)>>26
#define IMM(INST)		(INST & 0x0000FFFF)
#define JADDR(INST) 	(INST & 0x03FFFFFF)

/* Basic Information */

#define MIPS_REGS	32
#define PIPE_STAGE	5

/* Sign Extension */
#define SIGN_EX(X) (((X) & 0x8000) ? ((X) | 0xffff0000) : (X))
#define ZERO_EX(X) (X)

typedef struct ifid_latch {
	//pc
	int CURRENTPC;
	int NPC;

	//reg
	unsigned int inst;

}IF_ID;


typedef struct idex_latch {
	//PC
	int NPC;
	int CURRENTPC;

	// Control Signals
	bool WB_MemToReg;   // WB
	bool WB_RegWrite;   // WB

	bool MEM_MemWrite;  // MEM
	bool MEM_MemRead;   // MEM
	bool MEM_Branch;    // MEM

	bool RegDst;        // EX
	int ALUControl;     // EX
	bool ALUSrc;        // EX

	bool jump;

	// reg
	uint32_t REG1;      // Reg[rs]
	uint32_t REG2;      // Reg[rt]
	uint32_t IMM; 		// immediate value

	uint32_t RS2;		// rs, inst[21:25]
	uint32_t RT2;		// rt, inst[16:20]
	uint32_t RD2;		// rd, inst[11:15]
	uint32_t SHAMT2;	// shamt, inst[6:10]

	uint32_t instr_debug;
}ID_EX;


typedef struct exmm_latch {
	//PC
	int NPC;
	int CURRENTPC;

	// Control signals
	bool WB_MemToReg; // WB
	bool WB_RegWrite; // WB

	bool MemWrite;
	bool MemRead;
	bool Branch; // branch -> eventually PCSrc

	// reg
	bool zero;
	uint32_t ALU_OUT;
	uint32_t data2;             // untouched data2 from register.
	uint32_t RegDstNum; // 5 bit Register destination (if write)
	uint32_t instr_debug;
}EX_MEM;


typedef struct mmwb_latch {
	//PC
	int NPC;
	int CURRENTPC;

	// Control signals
	bool MemRead;
	bool MemToReg;
	bool RegWrite;

	// Reg
	uint32_t Mem_OUT;
	uint32_t ALU_OUT;

	uint32_t RegDstNum; // 5 bit Register destination (if write)
	uint32_t instr_debug;
}MEM_WB;


typedef struct CPU_State_Struct {
	unsigned long PC;                /* program counter */
	uint32_t REGS[MIPS_REGS];	/* register file */
	uint32_t PIPE[PIPE_STAGE]; /* pipeline stage */
// Pipelines
	IF_ID IF_ID_pipeline;
	ID_EX ID_EX_pipeline;
	EX_MEM EX_MEM_pipeline;
	MEM_WB MEM_WB_pipeline;

} CPU_State;

typedef struct {
	uint32_t start, size;
	uint8_t* mem;
} mem_region_t;


extern CPU_State CURRENT_STATE;

/* Pipelines */
IF_ID IF_ID_pipeline_buffer;
ID_EX ID_EX_pipeline_buffer;
EX_MEM EX_MEM_pipeline_buffer;
MEM_WB MEM_WB_pipeline_buffer;

bool globaljump;
bool globaljal;
bool globalJumpAndReturn;
bool branchFlush;
bool reachedEnd;

uint32_t PC_buffer;
uint32_t PC_jump;
int stall_IF_ID_count; // stall count for stalling IF stage only
int stall_ID_EX_count; // stall count for stalling IF, ID stages

extern mem_region_t MEM_REGIONS[2];
extern int INSTRUCTION_COUNT;
extern int CYCLE_COUNT;
/* Functions */

uint32_t	mem_read_32(uint32_t address);
void		mem_write_32(uint32_t address, uint32_t value);
void		run(int num_cycles);
void		cycle();
void		init_inst_info();
void		rdump();
void		pdump();
void		mdump(int start, int stop);

extern void process_instruction();
/*flush functions*/
void flush_IF_ID();
void flush_ID_EX();
void flush_EX_MEM();

#endif