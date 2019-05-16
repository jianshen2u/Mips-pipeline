#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "util.h"
#include "run.h"

CPU_State CURRENT_STATE;

int NUM_INST;
int INST_INFO[0x100000 / 4];
int INST_SIZE;

int INSTRUCTION_COUNT;
int CYCLE_COUNT;

/* memory will be dynamically allocated at initialization */
mem_region_t MEM_REGIONS[] = {
	{ MEM_TEXT_START, MEM_TEXT_SIZE, NULL },
	{ MEM_DATA_START, MEM_DATA_SIZE, NULL },
};

#define MEM_NREGIONS (sizeof(MEM_REGIONS)/sizeof(mem_region_t))


int count_inst() {
	FILE* fp = NULL;

	unsigned int data = 0;
	int ret = 0;
	int i = 0;
	fp = fopen("test_prog/simple.bin", "rb");
	if (fp == NULL) {
		printf("no file: %s\n", "input4.bin");
		return 0;
	}
	while (1) {
		ret = fread(&data, sizeof(int), 1, fp);
		if (ret == 0) break;
		i++;
	}
	fclose(fp);
	INST_SIZE = i;
	return i;
}

void load_program(int* INST_INFO, int NUM_INST)
{
	FILE* fp = NULL;
	
	unsigned int data = 0;
	int ret = 0;
	unsigned int inst2;
	int i = 0;
	fp = fopen("test_prog/simple.bin", "rb");
	if (fp == NULL) {
		printf("no file: %s\n", "input4.bin");
		return;
	}
	while (1) {
		ret = fread(&data, sizeof(int), 1, fp);
		if (ret == 0) break;

		inst2 = (data & 0xff) << 24 | (data & 0xff00) << 8 | (data & 0xff0000) >> 8 | (data & 0xff000000) >> 24;
		INST_INFO[i++] = inst2;
		//printf("Memory[0x%08X]: 0x%08X\n", i << 2, inst2);
	}
	fclose(fp);
	PC_buffer = MEM_TEXT_START;
}

void init_inst_info(int* INST_INFO,int NUM_INST)
{
	int i;

	for (i = 0; i < NUM_INST; i++)
	{
		INST_INFO[i] = 0;
	}
}

void cycle() {
	process_instruction();
	CYCLE_COUNT++;
}

void run(int num_inst)
{
	int i = 0;
	for (INSTRUCTION_COUNT = 0; i < num_inst;) {
		
		if (reachedEnd) {
			cycle();
			cycle();
			cycle();
			cycle();
			break;
		}
		cycle();
	}
}

void init_memory() 
{
	int i;
	for (i = 0; i < MEM_NREGIONS; i++)
	{
		MEM_REGIONS[i].mem = malloc(MEM_REGIONS[i].size);
		memset(MEM_REGIONS[i].mem, 0, MEM_REGIONS[i].size);
	}
}

int main(void) 
{
	int i = 0;
	int addr1 = 0;
	int addr2 = 0;
	init_memory();
	NUM_INST = count_inst();
	init_inst_info(INST_INFO,NUM_INST);

	FILE* fp = NULL;

	unsigned int data = 0;
	int ret = 0;
	unsigned int inst2;

	fp = fopen("test_prog/simple.bin", "rb");
	if (fp == NULL) {
		printf("no file: %s\n", "input4.bin");
		return;
	}
	while (1) {
		ret = fread(&data, sizeof(int), 1, fp);
		if (ret == 0) break;

		inst2 = (data & 0xff) << 24 | (data & 0xff00) << 8 | (data & 0xff0000) >> 8 | (data & 0xff000000) >> 24;
		INST_INFO[i++] = inst2;
		//printf("INST_INFO[0x%08X]: 0x%08X\n", i << 2, inst2);
	}
	fclose(fp);
	PC_buffer = MEM_TEXT_START;
	//load_program(INST_INFO,NUM_INST);

	/*for (i = 0; i < NUM_INST; i++)
	{
		printf("INST_INFO[0x%08X]: 0x%08X\n", i << 2, INST_INFO[i]);
	}*/

	run(NUM_INST);

	pdump();
	rdump();
	mdump(addr1, addr2);

	free(INST_INFO);
	return 0;
}

void process_instruction()
{
	if (PC_jump && !reachedEnd) 
	{
		CURRENT_STATE.PC = PC_jump;
		PC_jump = 0;
	}
	else {
		if (!stall_ID_EX_count && !reachedEnd) CURRENT_STATE.PC = PC_buffer;
	}

	if (!stall_ID_EX_count) CURRENT_STATE.IF_ID_pipeline = IF_ID_pipeline_buffer;

	CURRENT_STATE.ID_EX_pipeline = ID_EX_pipeline_buffer;
	CURRENT_STATE.EX_MEM_pipeline = EX_MEM_pipeline_buffer;
	CURRENT_STATE.MEM_WB_pipeline = MEM_WB_pipeline_buffer;

	if (stall_IF_ID_count && !stall_ID_EX_count) flush_IF_ID();
	// when both are enabled. We shouldn't flush IF/ID pipeline.
	if (stall_ID_EX_count) flush_ID_EX();

	process_IF();
	process_WB();

	if (branchFlush) {
		
		flush_IF_ID();
		flush_ID_EX();
		flush_EX_MEM();
		branchFlush = false;
	}

	process_ID(); // Data Hazards are detected here (if forwarding is not enabled)
	process_EX();                          // Data Forwarding is done here (if fowarding is enabled)
	process_MEM();

	if (stall_ID_EX_count) stall_ID_EX_count--;
	if (stall_IF_ID_count) stall_IF_ID_count--;
}
/***********************************
process_IF() is Instruction fetch ,
fetches instructions and store in 
the latch.
************************************/
void process_IF()
{
	//pc
	PC_buffer = CURRENT_STATE.PC + 4;
	IF_ID_pipeline_buffer.pc4 = CURRENT_STATE.PC;
	IF_ID_pipeline_buffer.npc = PC_buffer;

	//inst fetch
	unsigned long inst = get_inst_info(CURRENT_STATE.PC);
	IF_ID_pipeline_buffer.inst = inst;
}

void process_WB() {
	MEM_WB prevMEM_WB_pipeline = CURRENT_STATE.MEM_WB_pipeline;
	if (prevMEM_WB_pipeline.instr_debug) { // not no-op
		INSTRUCTION_COUNT++;
	}

	// MUX for which data to write.
	uint32_t writeData;
	if (prevMEM_WB_pipeline.MemToReg) {
		writeData = prevMEM_WB_pipeline.Mem_OUT;
	}
	else {
		writeData = prevMEM_WB_pipeline.ALU_OUT;
	}

	// Write data if (RegWrite == 1)
	if (prevMEM_WB_pipeline.RegWrite) {
		CURRENT_STATE.REGS[prevMEM_WB_pipeline.RegDstNum] = writeData;
	}
	
}

unsigned long get_inst_info(unsigned long pc)
{
	printf("get_inst_info :  pc value is : 0x%08x, this is : %d, text size is  %d\n", pc, ((pc - MEM_TEXT_START)), NUM_INST);
	if (((pc - MEM_TEXT_START)>>2) >= NUM_INST) {
		CURRENT_STATE.PC = 0;
		IF_ID_pipeline_buffer.pc4 = 0;
		//CURRENT_STATE.IF_ID_pipeline.CURRENTPC = 0;
		reachedEnd = true;
		return 0;
	}
	return INST_INFO[(pc - MEM_TEXT_START)>>2];
}

void process_ID() {

	IF_ID prevIF_ID_pipeline = CURRENT_STATE.IF_ID_pipeline;
	ID_EX_pipeline_buffer.npc = prevIF_ID_pipeline.npc;
	ID_EX_pipeline_buffer.pc4 = prevIF_ID_pipeline.pc4;

	unsigned long inst = prevIF_ID_pipeline.inst;
	ID_EX_pipeline_buffer.instr_debug = inst;


	// control signals
	generate_control_signals(inst);

	// Read register rs and rt
	ID_EX_pipeline_buffer.REG1 = CURRENT_STATE.REGS[RS1(inst)];
	ID_EX_pipeline_buffer.REG2 = CURRENT_STATE.REGS[RT1(inst)];
	unsigned long z = RS1(inst);
	// sign-extend Immediate value
	if (OPCODE1(inst) == ANDI || OPCODE1(inst) == ORI) ID_EX_pipeline_buffer.IMM = ZERO_EX(IMM(inst));
	else ID_EX_pipeline_buffer.IMM = SIGN_EX(IMM(inst));

	// transfer possible register write destinations (11-15 and 16-20)
	ID_EX_pipeline_buffer.RS2 = RS1(inst); // inst [21-25]
	ID_EX_pipeline_buffer.RT2 = RT1(inst); // inst [16-20]
	ID_EX_pipeline_buffer.RD2 = RD1(inst); // inst [11-15]
	ID_EX_pipeline_buffer.SHAMT2 = SHAMT1(inst);

	if (globaljal) {
		ID_EX_pipeline_buffer.REG1 = 0;
		ID_EX_pipeline_buffer.REG2 = 0;
		ID_EX_pipeline_buffer.IMM = CURRENT_STATE.PC;
		ID_EX_pipeline_buffer.RD2 = 31;
	}
}
/***********************************************************************
generate_control_signals = generating all the required control signals.
***********************************************************************/
void generate_control_signals(unsigned long instr)
{
	bool jump = false;
	bool jal = false;
	bool jumpandreturn = false;
	bool ALUinstruction = true;


	if (OPCODE1(instr) == RINST) {                    // R-type INSTRUCTIONS
		ID_EX_pipeline_buffer.RegDst = 1;
		ID_EX_pipeline_buffer.ALUSrc = 0;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 0;
		switch (FUNCT1(instr)) {
		case ADDU:                          // addu
			ID_EX_pipeline_buffer.ALUControl = 0;
			break;
		case AND:                          // and
			ID_EX_pipeline_buffer.ALUControl = 2;
			break;
		case JR:                          // jr
			jump = true;
			jumpandreturn = true;
			stall_IF_ID_count = 1 + 1;
			ALUinstruction = false;
			break;
		case NOR:                          // nor
			ID_EX_pipeline_buffer.ALUControl = 7;
			break;
		case OR:                          // or
			ID_EX_pipeline_buffer.ALUControl = 3;
			break;
		case SLTU:                          // sltu
			ID_EX_pipeline_buffer.ALUControl = 8;
			break;
		case SLL:                          // sll
			ID_EX_pipeline_buffer.ALUControl = 4;
			break;
		case SRL:                          // srl
			ID_EX_pipeline_buffer.ALUControl = 5;
			break;
		case SUBU:                          // subu
			ID_EX_pipeline_buffer.ALUControl = 1;
			break;
		default:
			break;
		}                                                   // J-type INSTRUCTIONS
	}
	else if (OPCODE1(instr) == J) {             // j
		jump = true;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 0;
		stall_IF_ID_count = 1 + 1;
		ALUinstruction = false;

	}
	else if (OPCODE1(instr) == JAL) {             // jal
		jump = true;
		ID_EX_pipeline_buffer.RegDst = 1;
		ID_EX_pipeline_buffer.ALUControl = 0;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 0;
		jal = true;
		stall_IF_ID_count = 1 + 1;
		ALUinstruction = false;

		// I-type
	}
	else if (OPCODE1(instr) == LW) {          // lw
		ID_EX_pipeline_buffer.RegDst = 0;
		ID_EX_pipeline_buffer.ALUControl = 0;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 1;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 1;
		ALUinstruction = false;

	}
	else if (OPCODE1(instr) == SW) {          // sw
		ID_EX_pipeline_buffer.ALUControl = 0;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 1;
		ID_EX_pipeline_buffer.WB_RegWrite = 0;
		ALUinstruction = false;

	}
	else if (OPCODE1(instr) == BEQ) {           // beq
		ID_EX_pipeline_buffer.ALUControl = 1;
		ID_EX_pipeline_buffer.ALUSrc = 0;
		ID_EX_pipeline_buffer.MEM_Branch = 1;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 0;
		stall_IF_ID_count = 3 + 1;
	}
	else if (OPCODE1(instr) == BNE) {           // bne
		ID_EX_pipeline_buffer.ALUControl = 11;
		ID_EX_pipeline_buffer.ALUSrc = 0;
		ID_EX_pipeline_buffer.MEM_Branch = 1;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 0;
		stall_IF_ID_count = 3 + 1;
		

	}
	else if (OPCODE1(instr) == ADDIU) {           // addiu
		ID_EX_pipeline_buffer.RegDst = 0;
		ID_EX_pipeline_buffer.ALUControl = 0;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 0;

	}
	else if (OPCODE1(instr) == ANDI) {           // andi
		ID_EX_pipeline_buffer.RegDst = 0;
		ID_EX_pipeline_buffer.ALUControl = 2;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 0;

	}
	else if (OPCODE1(instr) == LUI) {           // lui
		ID_EX_pipeline_buffer.RegDst = 0;
		ID_EX_pipeline_buffer.ALUControl = 10;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 0;

	}
	else if (OPCODE1(instr) == ORI) {           // ori
		ID_EX_pipeline_buffer.RegDst = 0;
		ID_EX_pipeline_buffer.ALUControl = 3;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 0;

	}
	else if (OPCODE1(instr) == SLTIU) {           // sltiu
		ID_EX_pipeline_buffer.RegDst = 0;
		ID_EX_pipeline_buffer.ALUControl = 8;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 0;

	}
	else {
		
	}


	if (jump) {
		PC_jump = ((CURRENT_STATE.PC + 4) & 0xF0000000) + (JADDR(instr) << 2);
		if (globalJumpAndReturn) {
			PC_jump = CURRENT_STATE.REGS[31];
		}
	}

	if (jal) {
		globaljal = true;
	}
	else {
		globaljal = false;
	}

	if (jumpandreturn) {
		globalJumpAndReturn = true;
	}
	else {
		globalJumpAndReturn = false;
	}


		// MEM hazard

		if (CURRENT_STATE.EX_MEM_pipeline.WB_RegWrite) {
			if (CURRENT_STATE.EX_MEM_pipeline.RegDstNum != 0) {

				uint32_t tempRegDstNum; // ID/EX's RegDstNum
				if (CURRENT_STATE.ID_EX_pipeline.RegDst) {
					tempRegDstNum = CURRENT_STATE.ID_EX_pipeline.RD2;
				}
				else {
					tempRegDstNum = CURRENT_STATE.ID_EX_pipeline.RT2;
				}


				if (!(CURRENT_STATE.ID_EX_pipeline.WB_RegWrite && (tempRegDstNum != 0) && (tempRegDstNum == RS1(instr)))) {
					if (CURRENT_STATE.EX_MEM_pipeline.RegDstNum == RS1(instr)) {
						stall_ID_EX_count = 1 + 1;
					}
				}

				if (!(CURRENT_STATE.ID_EX_pipeline.WB_RegWrite && (tempRegDstNum != 0) && (tempRegDstNum == RT1(instr)))) {

					if (CURRENT_STATE.EX_MEM_pipeline.RegDstNum == RT1(instr)) {
						stall_ID_EX_count = 1 + 1;
					}
				}
			}
		}

	// ALU operation followed by lw instruction
	if (ALUinstruction) {
		if (CURRENT_STATE.ID_EX_pipeline.MEM_MemRead) {
			if ((CURRENT_STATE.ID_EX_pipeline.RT2 == RS1(instr)) | (CURRENT_STATE.ID_EX_pipeline.RT2 == RT1(instr))) {
					stall_ID_EX_count = 1 + 1;		
			}
		}
	}
}

void process_EX() {

	ID_EX prevID_EX_pipeline = CURRENT_STATE.ID_EX_pipeline;
	EX_MEM_pipeline_buffer.instr_debug = CURRENT_STATE.ID_EX_pipeline.instr_debug;

	// shift the pipelined control signals
	EX_MEM_pipeline_buffer.pc4 = prevID_EX_pipeline.pc4;
	EX_MEM_pipeline_buffer.WB_MemToReg = prevID_EX_pipeline.WB_MemToReg;
	EX_MEM_pipeline_buffer.WB_RegWrite = prevID_EX_pipeline.WB_RegWrite;

	EX_MEM_pipeline_buffer.MemWrite = prevID_EX_pipeline.MEM_MemWrite;
	EX_MEM_pipeline_buffer.MemRead = prevID_EX_pipeline.MEM_MemRead;
	EX_MEM_pipeline_buffer.Branch = prevID_EX_pipeline.MEM_Branch;

	// PC (for Branch instruction)
	EX_MEM_pipeline_buffer.npc = prevID_EX_pipeline.npc + (prevID_EX_pipeline.IMM << 2);

	int forwardA = 00;
	int forwardB = 00;

	// Forwarding unit.
	

		// EX hazard
		if (CURRENT_STATE.EX_MEM_pipeline.WB_RegWrite) {
			if (CURRENT_STATE.EX_MEM_pipeline.RegDstNum != 0) {
				if (CURRENT_STATE.EX_MEM_pipeline.RegDstNum == CURRENT_STATE.ID_EX_pipeline.RS2) {
					forwardA = 10;
				}
				if (CURRENT_STATE.EX_MEM_pipeline.RegDstNum == CURRENT_STATE.ID_EX_pipeline.RT2) {
					forwardB = 10;
				}
			}
		}

		// MEM hazard
		if (CURRENT_STATE.MEM_WB_pipeline.RegWrite) {
			if (CURRENT_STATE.MEM_WB_pipeline.RegDstNum != 0) {
				if (!(CURRENT_STATE.EX_MEM_pipeline.WB_RegWrite && (CURRENT_STATE.EX_MEM_pipeline.RegDstNum != 0) && (CURRENT_STATE.EX_MEM_pipeline.RegDstNum == CURRENT_STATE.ID_EX_pipeline.RS2))) {

					if (CURRENT_STATE.MEM_WB_pipeline.RegDstNum == CURRENT_STATE.ID_EX_pipeline.RS2) {
					
						forwardA = 01;
					}
				}

				if (!(CURRENT_STATE.EX_MEM_pipeline.WB_RegWrite && (CURRENT_STATE.EX_MEM_pipeline.RegDstNum != 0) && (CURRENT_STATE.EX_MEM_pipeline.RegDstNum == CURRENT_STATE.ID_EX_pipeline.RT2))) {

					if (CURRENT_STATE.MEM_WB_pipeline.RegDstNum == CURRENT_STATE.ID_EX_pipeline.RT2) {
						forwardB = 01;
					}
				}
			}
		}

	 // end of "forwardingEnabled"





	// For this section, refer to figure 4.57 !!! Other figures are wrong!
	// select ALU input
	uint32_t ALUinput1;
	uint32_t ALUinput2;

	// 3 to 1 MUX
	if (forwardA == 00) {
		ALUinput1 = prevID_EX_pipeline.REG1;
	}
	else if (forwardA == 10) {
		ALUinput1 = CURRENT_STATE.EX_MEM_pipeline.ALU_OUT;
	}
	else if (forwardA == 01) {
		// MUX for which data to write.
		uint32_t writeData;
		if (CURRENT_STATE.MEM_WB_pipeline.MemToReg) {
			writeData = CURRENT_STATE.MEM_WB_pipeline.Mem_OUT;
		}
		else {
			writeData = CURRENT_STATE.MEM_WB_pipeline.ALU_OUT;
		}
		ALUinput1 = writeData;
	}
	else {
		
	}

	// 3 to 1 MUX
	if (forwardB == 00) {
		ALUinput2 = prevID_EX_pipeline.REG2;
	}
	else if (forwardB == 10) {
		ALUinput2 = CURRENT_STATE.EX_MEM_pipeline.ALU_OUT;
	}
	else if (forwardB == 01) {
		// MUX for which data to write.
		uint32_t writeData;
		if (CURRENT_STATE.MEM_WB_pipeline.MemToReg) {
			writeData = CURRENT_STATE.MEM_WB_pipeline.Mem_OUT;
		}
		else {
			writeData = CURRENT_STATE.MEM_WB_pipeline.ALU_OUT;
		}
		ALUinput2 = writeData;
	}
	else {
		printf("unrecognized fowardB signal : %d\n", forwardB);
	}

	// transfer data2
	EX_MEM_pipeline_buffer.data2 = ALUinput2;

	// Another MUX inside
	// if ALUSrc == 1, the second ALU operand is the sign-extended, lower 16 bits of the instruction.
	// if ALUSrc == 0, the second ALU operand comes from the second register file output. (Read data 2).
	if (prevID_EX_pipeline.ALUSrc) {
		ALUinput2 = prevID_EX_pipeline.IMM;
	}



	// calculate ALU control
	int funct_field = (prevID_EX_pipeline.IMM & 63); // extract funct field from instruction[0~15]
	int control_line = prevID_EX_pipeline.ALUControl;

	// Special case for shift commands (srl, sll)
	if ((control_line == 4) || (control_line == 5)) {
		ALUinput1 = SHAMT1(prevID_EX_pipeline.instr_debug);
	}


	// process ALU
	uint32_t ALUresult = ALU(control_line, ALUinput1, ALUinput2);
	EX_MEM_pipeline_buffer.zero = !ALUresult;
	EX_MEM_pipeline_buffer.ALU_OUT = ALUresult;

	// Mux for Write Register
	// if RegDst == 1, the register destination number for the Write register comes from the rd field.(bits 15:11)
	// if RegDst == 0, the register destination number for the Write register comes from the rt field.(bits 20:16)
	if (prevID_EX_pipeline.RegDst) {
		EX_MEM_pipeline_buffer.RegDstNum = prevID_EX_pipeline.RD2;
	}
	else {
		EX_MEM_pipeline_buffer.RegDstNum = prevID_EX_pipeline.RT2;
	}
}


void process_MEM() {
	EX_MEM prevEX_MEM_pipeline = CURRENT_STATE.EX_MEM_pipeline;
	MEM_WB_pipeline_buffer.instr_debug = prevEX_MEM_pipeline.instr_debug;

	// shift the pipelined control signals
	MEM_WB_pipeline_buffer.MemToReg = prevEX_MEM_pipeline.WB_MemToReg;
	MEM_WB_pipeline_buffer.RegWrite = prevEX_MEM_pipeline.WB_RegWrite;
	MEM_WB_pipeline_buffer.MemRead = prevEX_MEM_pipeline.MemRead;
	MEM_WB_pipeline_buffer.pc4 = prevEX_MEM_pipeline.pc4;

	// branching
	if (prevEX_MEM_pipeline.Branch) { // branch condition
		if (prevEX_MEM_pipeline.zero) {
			PC_jump = prevEX_MEM_pipeline.pc4;
		}
		else {
			PC_buffer = prevEX_MEM_pipeline.pc4 + 4;
		}
	}

	// forwarding
	bool forwardingDone = false;

	
		if (CURRENT_STATE.MEM_WB_pipeline.MemRead & CURRENT_STATE.EX_MEM_pipeline.MemWrite) {
			if (CURRENT_STATE.EX_MEM_pipeline.RegDstNum == CURRENT_STATE.MEM_WB_pipeline.RegDstNum) {
				mem_write_32(prevEX_MEM_pipeline.ALU_OUT, CURRENT_STATE.MEM_WB_pipeline.Mem_OUT);
				forwardingDone = true;
			}
		}
	

	// Read data from memory
	if (prevEX_MEM_pipeline.MemRead) {
		MEM_WB_pipeline_buffer.Mem_OUT = mem_read_32(prevEX_MEM_pipeline.ALU_OUT);
	}
	else {
		MEM_WB_pipeline_buffer.ALU_OUT = prevEX_MEM_pipeline.ALU_OUT;
	}

	// Write data to memory
	if (prevEX_MEM_pipeline.MemWrite & (!forwardingDone)) {
		mem_write_32(prevEX_MEM_pipeline.ALU_OUT, prevEX_MEM_pipeline.data2);
	}

	// shift Register destination number (on write)
	MEM_WB_pipeline_buffer.RegDstNum = prevEX_MEM_pipeline.RegDstNum;
}

uint32_t ALU(int control_line, uint32_t data1, uint32_t data2) {
	if (control_line == 0) {        // 0000 : add
		return data1 + data2;
	}
	else if (control_line == 1) { // 0001 : sub
		return data1 - data2;
	}
	else if (control_line == 2) { // 0010 : AND
		return data1 & data2;
	}
	else if (control_line == 3) { // 0011 : OR
		return data1 | data2;
	}
	else if (control_line == 4) { // 0100 : SLL (shift left logical)
		return (data2 << data1);
	}
	else if (control_line == 5) { // 0101 : SRL (shift right logical)
		return (data2 >> data1);
	}
	else if (control_line == 7) { // 0111 : NOR
		return ~(data1 | data2);
	}
	else if (control_line == 8) { // 1000 : SLT (set on less than)
		return data1 < data2;
	}
	else if (control_line == 10) { // 1010 : LUI
		return (data2 << 16);
	}
	else if (control_line == 11) {  // 1011 : not equal
		return !(data1 - data2);
	}
	else {
		printf("Error in ALU. ALU control line value is : %d\n", control_line);
	}
	return 0;
}

/***************************************************************/
/*                                                             */
/* Procedure: mem_read_32                                      */
/*                                                             */
/* Purpose: Read a 32-bit word from memory                     */
/*                                                             */
/***************************************************************/
uint32_t mem_read_32(uint32_t address)
{
	int i;
	int valid_flag = 0;

	for (i = 0; i < MEM_NREGIONS; i++) {
		if (address >= MEM_REGIONS[i].start &&
			address < (MEM_REGIONS[i].start + MEM_REGIONS[i].size)) {
			uint32_t offset = address - MEM_REGIONS[i].start;

			valid_flag = 1;

			return
				(MEM_REGIONS[i].mem[offset + 3] << 24) |
				(MEM_REGIONS[i].mem[offset + 2] << 16) |
				(MEM_REGIONS[i].mem[offset + 1] << 8) |
				(MEM_REGIONS[i].mem[offset + 0] << 0);
		}
	}

	if (!valid_flag) {
		printf("Memory Read Error: Exceed memory boundary 0x%x\n", address);
		exit(1);
	}


	return 0;
}

/***************************************************************/
/*                                                             */
/* Procedure: mem_write_32                                     */
/*                                                             */
/* Purpose: Write a 32-bit word to memory                      */
/*                                                             */
/***************************************************************/
void mem_write_32(uint32_t address, uint32_t value)
{
	int i;
	int valid_flag = 0;

	for (i = 0; i < MEM_NREGIONS; i++) {
		if (address >= MEM_REGIONS[i].start &&
			address < (MEM_REGIONS[i].start + MEM_REGIONS[i].size)) {
			uint32_t offset = address - MEM_REGIONS[i].start;

			valid_flag = 1;

			MEM_REGIONS[i].mem[offset + 3] = (value >> 24) & 0xFF;
			MEM_REGIONS[i].mem[offset + 2] = (value >> 16) & 0xFF;
			MEM_REGIONS[i].mem[offset + 1] = (value >> 8) & 0xFF;
			MEM_REGIONS[i].mem[offset + 0] = (value >> 0) & 0xFF;
			return;
		}
	}
	if (!valid_flag) {
		printf("Memory Write Error: Exceed memory boundary 0x%x\n", address);
		exit(1);
	}
}

// Flush functions
void flush_IF_ID() {
	CURRENT_STATE.IF_ID_pipeline.npc = 0;
	CURRENT_STATE.IF_ID_pipeline.pc4 = 0;
	CURRENT_STATE.IF_ID_pipeline.inst = 0;
}

void flush_ID_EX() {
	CURRENT_STATE.ID_EX_pipeline.npc = 0;
	CURRENT_STATE.ID_EX_pipeline.pc4 = 0;
	CURRENT_STATE.ID_EX_pipeline.WB_MemToReg = false;
	CURRENT_STATE.ID_EX_pipeline.WB_RegWrite = false;
	CURRENT_STATE.ID_EX_pipeline.MEM_MemWrite = false;
	CURRENT_STATE.ID_EX_pipeline.MEM_MemRead = false;
	CURRENT_STATE.ID_EX_pipeline.MEM_Branch = false;
	CURRENT_STATE.ID_EX_pipeline.RegDst = false;
	CURRENT_STATE.ID_EX_pipeline.ALUSrc = false;
	CURRENT_STATE.ID_EX_pipeline.jump = false;
	CURRENT_STATE.ID_EX_pipeline.REG1 = 0;
	CURRENT_STATE.ID_EX_pipeline.REG2 = 0;
	CURRENT_STATE.ID_EX_pipeline.IMM = 0;
	CURRENT_STATE.ID_EX_pipeline.RS2 = 0;
	CURRENT_STATE.ID_EX_pipeline.RT2 = 0;
	CURRENT_STATE.ID_EX_pipeline.RD2 = 0;
	CURRENT_STATE.ID_EX_pipeline.instr_debug = 0;
}

void flush_EX_MEM() {
	CURRENT_STATE.EX_MEM_pipeline.npc = 0;
	CURRENT_STATE.EX_MEM_pipeline.pc4 = 0;
	CURRENT_STATE.EX_MEM_pipeline.WB_MemToReg = false;
	CURRENT_STATE.EX_MEM_pipeline.WB_RegWrite = false;
	CURRENT_STATE.EX_MEM_pipeline.MemWrite = false;
	CURRENT_STATE.EX_MEM_pipeline.MemRead = false;
	CURRENT_STATE.EX_MEM_pipeline.Branch = false;
	CURRENT_STATE.EX_MEM_pipeline.zero = false;
	CURRENT_STATE.EX_MEM_pipeline.ALU_OUT = 0;
	CURRENT_STATE.EX_MEM_pipeline.data2 = 0;
	CURRENT_STATE.EX_MEM_pipeline.RegDstNum = 0;
	CURRENT_STATE.EX_MEM_pipeline.instr_debug = 0;
}

/***************************************************************/
/*                                                             */
/* Procedure : mdump                                           */
/*                                                             */
/* Purpose   : Dump a word-aligned region of memory to the     */
/*             output file.                                    */
/*                                                             */
/***************************************************************/
void mdump(int start, int stop) {
	int address;

	printf("Memory content [0x%08x..0x%08x] :\n", start, stop);
	printf("-------------------------------------\n");
	for (address = start; address <= stop; address += 4)
		printf("0x%08x: 0x%08x\n", address, mem_read_32(address));
	printf("\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : rdump                                           */
/*                                                             */
/* Purpose   : Dump current register and bus values to the     */
/*             output file.                                    */
/*                                                             */
/***************************************************************/
void rdump() {
	int k;

	printf("Current register values :\n");
	printf("-------------------------------------\n");
	printf("PC: 0x%08x\n", CURRENT_STATE.PC + 4);       // adjusted
	printf("Registers:\n");
	for (k = 0; k < MIPS_REGS; k++)
		printf("R%d: 0x%08x\n", k, CURRENT_STATE.REGS[k]);
	printf("\n");
}

/***************************************************************/
/*                                                             */
/* Procedure : pdump                                           */
/*                                                             */
/* Purpose   : Dump current pipeline PC state                  */
/*                                                             */
/***************************************************************/
void pdump() {
	int k;

	printf("Current pipeline PC state :\n");
	printf("-------------------------------------\n");
	printf("CYCLE %d:", CYCLE_COUNT);

	if ((!CURRENT_STATE.PC) || ((CURRENT_STATE.PC - MEM_TEXT_START) >> 2) >= NUM_INST) {
		printf("          ");
	}
	else {
		printf("0x%08x", CURRENT_STATE.PC);
	}
	printf("|");
	if (CURRENT_STATE.IF_ID_pipeline.pc4) printf("0x%08x", CURRENT_STATE.IF_ID_pipeline.pc4);
	else printf("          ");
	printf("|");
	if (CURRENT_STATE.ID_EX_pipeline.pc4) printf("0x%08x", CURRENT_STATE.ID_EX_pipeline.pc4);
	else printf("          ");
	printf("|");
	if (CURRENT_STATE.EX_MEM_pipeline.pc4) printf("0x%08x", CURRENT_STATE.EX_MEM_pipeline.pc4);
	else printf("          ");
	printf("|");
	if (CURRENT_STATE.MEM_WB_pipeline.pc4) printf("0x%08x", CURRENT_STATE.MEM_WB_pipeline.pc4);
	else printf("          ");

	printf("\n\n");
}
