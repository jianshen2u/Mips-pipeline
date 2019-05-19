#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "util.h"
#include "run.h"

CPU_State CURRENT_STATE;
int NUM_INST;
uint32_t* INST_INFO;
int MEM[0x100000 / 4];
int REG[32];
bool isDebug = false;

int INSTRUCTION_COUNT;
int CYCLE_COUNT;

int main(void)
{
	int x;
	NUM_INST = 0;
	init_mem();
	x = count();
	loadprogram(x);

	if (isDebug)
	{
		for (int i = 0; i < NUM_INST; i++)
		{
			printf("INST_INFO	[0x%08X]: 0x%08X\n", i << 2, INST_INFO[i]);
		}
	}

	PC_buffer = 0;
	CURRENT_STATE.PC = 0;
	run(x);
	pdump();
	rdump();
	mdump();
	printf("Instruction count: %d \n", INSTRUCTION_COUNT);
	printf("Cycle count: %d \n", CYCLE_COUNT);
	printf("REGISTER 2 value : 0x%08x\n", CURRENT_STATE.REGS[2]);
	return 0;
}

/*=====================================================*/
/*					init_mem()						   */
/*				Initialize memory					   */
/*=====================================================*/

void init_mem() {
	int i;
	for (i = 0; i < (0x100000 / 4); i++)
	{
		MEM[i] = 0;
	}
}

/*=====================================================*/
/*					init_inst_info()				   */
/*			Initialize Instruction Memory			   */
/*=====================================================*/

void init_inst_info(int x)
{
	int i;

	for (i = 0; i < x; i++)
	{
		INST_INFO[i] = 0;
	}
}

int count()
{
	FILE* fp = NULL;
	int ret = 0;
	unsigned int data;
	NUM_INST = 0;

	fp = fopen("test_prog/simple2.bin", "rb");
	if (fp == NULL) {
		printf("no file: %s\n", "input4.bin");
		return 0;
	}
	while (1) {
		ret = fread(&data, sizeof(int), 1, fp);
		if (ret == 0) break;
		NUM_INST++;
	}
	fclose(fp);
	return NUM_INST;
}

/*=====================================================*/
/*					loadprogram()					   */
/*		Loads program into the instruction memory.	   */
/*=====================================================*/
void loadprogram(int x)
{
	FILE* fp = NULL;
	int ret = 0;
	uint32_t data;
	uint32_t inst2;
	int i = 0;
	INST_INFO = malloc(sizeof(unsigned int) * x);

	init_inst_info(x);

	fp = fopen("test_prog/simple2.bin", "rb");
	if (fp == NULL) {
		printf("no file: %s\n", "input4.bin");
		return;
	}
	while (i < x) {
		ret = fread(&data, sizeof(int), 1, fp);
		if (ret == 0) break;

		inst2 = (data & 0xff) << 24 | (data & 0xff00) << 8 | (data & 0xff0000) >> 8 | (data & 0xff000000) >> 24;
		INST_INFO[i] = inst2;
		MEM[i] = inst2;
		i++;
	}
	fclose(fp);
}

/*=====================================================*/
/*					run program						   */
/*			Runs the program until the end .		   */
/*=====================================================*/

void run(int num_inst)
{
	int i = 0;
	for (INSTRUCTION_COUNT = 0; i < num_inst; ) {

		if (reachedEnd) {
			cycle();
			pdump();
			cycle();
			pdump();
			cycle();
			pdump();
			cycle();
			pdump();
			break;
		}
		cycle();
	}
}


/*=====================================================*/
/*					cycle()							   */
/*			Execute programs once.					   */
/*=====================================================*/

void cycle()
{
	process_instruction();
	CYCLE_COUNT++;
}

/*=====================================================*/
/*					process_instruction()			   */
/*			Process the instructions in 5 stages	   */
/*=====================================================*/
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
	process_ID();
	process_EX();// Data Forwarding is done here 
	process_MEM();
	process_WB();

	if (stall_ID_EX_count) stall_ID_EX_count--;
	if (stall_IF_ID_count) stall_IF_ID_count--;
}

/*=====================================================*/
/*			process_IF()			     			   */
/*			Instruction fetch						   */
/*=====================================================*/

void process_IF()
{
	//pc
	PC_buffer = CURRENT_STATE.PC + 4;
	IF_ID_pipeline_buffer.CURRENTPC = CURRENT_STATE.PC;
	IF_ID_pipeline_buffer.NPC = PC_buffer;

	//inst fetch
	uint32_t inst = get_inst_info(CURRENT_STATE.PC);
	IF_ID_pipeline_buffer.inst = inst;
	if (isDebug)
	{
		printf("*debug process_IF: CURRENTPC 0x%08x, instr 0x%08x\n", IF_ID_pipeline_buffer.CURRENTPC, inst);
	}
}

/*=====================================================*/
/*			get_inst_info()			     			   */
/*			fetches instruction						   */
/*=====================================================*/

uint32_t get_inst_info(uint32_t pc)
{
	if (isDebug)
	{
		printf("get_inst_info :  pc value is : 0x%08x, this is : %d, text size is  %d\n", pc, ((pc - MEM_TEXT_START) >> 2), NUM_INST);
	}
	if (((pc) >> 2) >= NUM_INST)
	{
		CURRENT_STATE.PC = 0;
		IF_ID_pipeline_buffer.CURRENTPC = 0;
		//CURRENT_STATE.IF_ID_pipeline.CURRENTPC = 0;
		reachedEnd = true;
		return 0;
	}
	return INST_INFO[pc >> 2];
}

/*=====================================================*/
/*			process_ID()			     			   */
/*			Instruction decode stage.				   */
/*=====================================================*/

void process_ID() {

	int rs;
	int rt;
	int rd;
	int s_imm;
	int opcode;
	int func;
	int addr;
	int imm;
	int shamt;
	IF_ID prevIF_ID_pipeline = CURRENT_STATE.IF_ID_pipeline;
	ID_EX_pipeline_buffer.NPC = prevIF_ID_pipeline.NPC;
	ID_EX_pipeline_buffer.CURRENTPC = prevIF_ID_pipeline.CURRENTPC;

	uint32_t inst = prevIF_ID_pipeline.inst;
	ID_EX_pipeline_buffer.instr_debug = inst;

	opcode = (inst & OPCODE) >> 26;
	rs = (inst & RS) >> 21;
	rt = (inst & RT) >> 16;
	rd = (inst & RD) >> 11;
	func = (inst & FUNCT);
	addr = (inst & 0x3ffffff);
	shamt = (inst & SHAMT) >> 6;

	if ((inst & 0xffff) >= 0x8000)//shift immediate value
	{
		imm = ((inst & 0xffff) | 0xffff0000);
	}
	else
	{
		imm = (inst & 0xffff);
	}

	s_imm = imm;

	// control signals
	generate_control_signals(inst, opcode, func, addr, rs, rt);

	// Read register rs and rt
	ID_EX_pipeline_buffer.v1 = REG[rs];
	ID_EX_pipeline_buffer.v2 = REG[rt];

	// sign-extend Immediate value
	//if (opcode == ANDI || opcode == ORI) ID_EX_pipeline_buffer.imm = ZERO_EX(IMM(inst));
	//else ID_EX_pipeline_buffer.IMM = SIGN_EX(IMM(inst));

	// transfer possible register write destinations (11-15 and 16-20)
	ID_EX_pipeline_buffer.rs = rs; // inst [21-25]
	ID_EX_pipeline_buffer.rt = rt; // inst [16-20]
	ID_EX_pipeline_buffer.rd = rd; // inst [11-15]
	ID_EX_pipeline_buffer.imm = imm;
	ID_EX_pipeline_buffer.opcode = opcode; // inst [21-25]
	ID_EX_pipeline_buffer.func = func; // inst [16-20]
	ID_EX_pipeline_buffer.addr = addr; // inst [11-15]
	ID_EX_pipeline_buffer.shamt = shamt;

	if (globaljal) {
		ID_EX_pipeline_buffer.v1 = 0;
		ID_EX_pipeline_buffer.v2 = 0;
		ID_EX_pipeline_buffer.imm = CURRENT_STATE.PC;
		ID_EX_pipeline_buffer.rd = 31;
	}
	if (isDebug) {
		printf("*debug process_ID: CURRENTPC 0x%08x, instr 0x%08x\n", ID_EX_pipeline_buffer.CURRENTPC, inst);
		printf("    RS %d, REG1 %d, RT %d, REG2 %d, RD %d, imm %08x, shamt %d\n", rs, REG[ID_EX_pipeline_buffer.rs], rt, REG[ID_EX_pipeline_buffer.rd], rd, ID_EX_pipeline_buffer.imm, ID_EX_pipeline_buffer.shamt);
	}
}

/*=====================================================*/
/*			generate_control_signals()				   */
/*			Generate all control signals			   */
/*=====================================================*/

void generate_control_signals(uint32_t instr, int opcode, int func, int addr, int rs, int rt) {
	bool jump = false;
	bool jal = false;
	bool jumpandreturn = false;
	bool ALUinstruction = true;


	if (opcode == 0) {                               // R-type
		ID_EX_pipeline_buffer.RegDst = 1;
		ID_EX_pipeline_buffer.ALUSrc = 0;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 0;
		switch (func) {
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
			printf("Unrecognized input in 'generate_control_signals' with input : %08x\n", instr);
			break;
		}                                                   // J-type
	}
	else if (opcode == J) {             // j
		jump = true;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 0;
		stall_IF_ID_count = 1 + 1;
		ALUinstruction = false;

	}
	else if (opcode == JAL) {             // jal
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
	else if (opcode == LW) {          // lw
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
	else if (opcode == SW) {          // sw
		ID_EX_pipeline_buffer.ALUControl = 0;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 1;
		ID_EX_pipeline_buffer.WB_RegWrite = 0;
		ALUinstruction = false;

	}
	else if (opcode == BEQ) {           // beq
		ID_EX_pipeline_buffer.ALUControl = 1;
		ID_EX_pipeline_buffer.ALUSrc = 0;
		ID_EX_pipeline_buffer.MEM_Branch = 1;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 0;
		stall_IF_ID_count = 3 + 1;

	}
	else if (opcode == BNE) {           // bne
		ID_EX_pipeline_buffer.ALUControl = 11;
		ID_EX_pipeline_buffer.ALUSrc = 0;
		ID_EX_pipeline_buffer.MEM_Branch = 1;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 0;
		stall_IF_ID_count = 3 + 1;
	}
	else if (opcode == ADDIU) {           // addiu
		ID_EX_pipeline_buffer.RegDst = 0;
		ID_EX_pipeline_buffer.ALUControl = 0;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 0;
	}
	else if (opcode == ANDI) {           // andi
		ID_EX_pipeline_buffer.RegDst = 0;
		ID_EX_pipeline_buffer.ALUControl = 2;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 0;

	}
	else if (opcode == LUI) {           // lui
		ID_EX_pipeline_buffer.RegDst = 0;
		ID_EX_pipeline_buffer.ALUControl = 10;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 0;

	}
	else if (opcode == ORI) {           // ori
		ID_EX_pipeline_buffer.RegDst = 0;
		ID_EX_pipeline_buffer.ALUControl = 3;
		ID_EX_pipeline_buffer.ALUSrc = 1;
		ID_EX_pipeline_buffer.MEM_Branch = 0;
		ID_EX_pipeline_buffer.MEM_MemRead = 0;
		ID_EX_pipeline_buffer.MEM_MemWrite = 0;
		ID_EX_pipeline_buffer.WB_RegWrite = 1;
		ID_EX_pipeline_buffer.WB_MemToReg = 0;

	}
	else if (opcode == SLTIU) {           // sltiu
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
		printf("Unrecognized OPCODE : %d\n", opcode);
	}


	if (jump) {
		PC_jump = ((CURRENT_STATE.PC + 4) & 0xF0000000) + (addr << 2);
		if (globalJumpAndReturn) {
			PC_jump = REG[31];
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
	// ALU operation followed by lw instruction
	if (ALUinstruction) {
		if (CURRENT_STATE.ID_EX_pipeline.MEM_MemRead) {
			if ((CURRENT_STATE.ID_EX_pipeline.rt == rs) | (CURRENT_STATE.ID_EX_pipeline.rt == rt))
			{
				stall_ID_EX_count = 1 + 1;
			}
		}
	}

	if (isDebug) {
		printf("*debug generate_control_signals: jump=%d, jl=%d, jr=%d, alu=%d\n", jump, jal, jumpandreturn, ALUinstruction);
		printf("    RegDst %d, ALUControl %d, ALUSrc %d\n", ID_EX_pipeline_buffer.RegDst, ID_EX_pipeline_buffer.ALUControl, ID_EX_pipeline_buffer.ALUSrc);
		printf("    Branch %d, MemRead %d, MemWrite %d\n", ID_EX_pipeline_buffer.MEM_Branch, ID_EX_pipeline_buffer.MEM_MemRead, ID_EX_pipeline_buffer.MEM_MemWrite);
		printf("    RegWrite %d, MemToReg %d\n", ID_EX_pipeline_buffer.WB_RegWrite, ID_EX_pipeline_buffer.WB_MemToReg);
		printf("    stall_IF_ID_count %d, stall_ID_EX_count %d\n", stall_IF_ID_count, stall_ID_EX_count);
		printf("    PC_jump %08x\n", PC_jump);
	}
}

/*=====================================================*/
/*			process_EX()							   */
/*			Executes ALU operations					   */
/*=====================================================*/
void process_EX() {

	ID_EX prevID_EX_pipeline = CURRENT_STATE.ID_EX_pipeline;
	EX_MEM_pipeline_buffer.instr_debug = CURRENT_STATE.ID_EX_pipeline.instr_debug;

	// shift the pipelined control signals
	EX_MEM_pipeline_buffer.CURRENTPC = prevID_EX_pipeline.CURRENTPC;
	EX_MEM_pipeline_buffer.WB_MemToReg = prevID_EX_pipeline.WB_MemToReg;
	EX_MEM_pipeline_buffer.WB_RegWrite = prevID_EX_pipeline.WB_RegWrite;

	EX_MEM_pipeline_buffer.MemWrite = prevID_EX_pipeline.MEM_MemWrite;
	EX_MEM_pipeline_buffer.MemRead = prevID_EX_pipeline.MEM_MemRead;
	EX_MEM_pipeline_buffer.Branch = prevID_EX_pipeline.MEM_Branch;

	// PC (for Branch instruction)
	EX_MEM_pipeline_buffer.NPC = prevID_EX_pipeline.NPC + (prevID_EX_pipeline.imm << 2);

	//shift data
	// transfer possible register write destinations (11-15 and 16-20)
	EX_MEM_pipeline_buffer.rs = prevID_EX_pipeline.rs; // inst [21-25]
	EX_MEM_pipeline_buffer.rt = prevID_EX_pipeline.rt; // inst [16-20]
	EX_MEM_pipeline_buffer.rd = prevID_EX_pipeline.rd; // inst [11-15]
	EX_MEM_pipeline_buffer.imm = prevID_EX_pipeline.imm;
	EX_MEM_pipeline_buffer.opcode = prevID_EX_pipeline.opcode; // inst [21-25]
	EX_MEM_pipeline_buffer.func = prevID_EX_pipeline.func; // inst [16-20]
	EX_MEM_pipeline_buffer.addr = prevID_EX_pipeline.addr; // inst [11-15]
	EX_MEM_pipeline_buffer.shamt = prevID_EX_pipeline.shamt;
	EX_MEM_pipeline_buffer.v1 = prevID_EX_pipeline.v1;
	EX_MEM_pipeline_buffer.v2 = prevID_EX_pipeline.v2;

	int forwardA = 00;
	int forwardB = 00;

	// Forwarding unit.

	if (isDebug) printf("RegDstNum %d, RS %d, RT %d\n", CURRENT_STATE.EX_MEM_pipeline.RegDstNum, CURRENT_STATE.ID_EX_pipeline.rs, CURRENT_STATE.ID_EX_pipeline.rt);

	// EX hazard
	if (CURRENT_STATE.EX_MEM_pipeline.WB_RegWrite) {
		if (CURRENT_STATE.EX_MEM_pipeline.RegDstNum != 0) {
			if (CURRENT_STATE.EX_MEM_pipeline.RegDstNum == CURRENT_STATE.ID_EX_pipeline.rs) {
				if (isDebug) printf("EX Hazard detected!!!!! \n");
				forwardA = 10;
			}
			if (CURRENT_STATE.EX_MEM_pipeline.RegDstNum == CURRENT_STATE.ID_EX_pipeline.rt) {
				if (isDebug) printf("EX Hazard detected!!!!! \n");
				forwardB = 10;
			}
		}
	}

	// MEM hazard
	if (CURRENT_STATE.MEM_WB_pipeline.RegWrite) {
		if (CURRENT_STATE.MEM_WB_pipeline.RegDstNum != 0) {
			if (!(CURRENT_STATE.EX_MEM_pipeline.WB_RegWrite && (CURRENT_STATE.EX_MEM_pipeline.RegDstNum != 0) && (CURRENT_STATE.EX_MEM_pipeline.RegDstNum == CURRENT_STATE.ID_EX_pipeline.rs))) {

				if (CURRENT_STATE.MEM_WB_pipeline.RegDstNum == CURRENT_STATE.ID_EX_pipeline.rs) {
					if (isDebug) printf("MEM Hazard detected!!!!! \n");
					forwardA = 01;
				}
			}

			if (!(CURRENT_STATE.EX_MEM_pipeline.WB_RegWrite && (CURRENT_STATE.EX_MEM_pipeline.RegDstNum != 0) && (CURRENT_STATE.EX_MEM_pipeline.RegDstNum == CURRENT_STATE.ID_EX_pipeline.rt))) {

				if (CURRENT_STATE.MEM_WB_pipeline.RegDstNum == CURRENT_STATE.ID_EX_pipeline.rt) {
					if (isDebug) printf("MEM Hazard detected!!!!! \n");
					forwardB = 01;
				}
			}
		}
	}

	// end of "forwardingEnabled"

	// select ALU input
	uint32_t ALUinput1;
	uint32_t ALUinput2;

	// 3 to 1 MUX
	if (forwardA == 00) {
		ALUinput1 = prevID_EX_pipeline.v1;
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
		printf("unrecognized fowardA signal : %d\n", forwardA);
	}

	// 3 to 1 MUX
	if (forwardB == 00) {
		ALUinput2 = prevID_EX_pipeline.v2;
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
		ALUinput2 = prevID_EX_pipeline.imm;
	}



	// calculate ALU control
	int funct_field = (prevID_EX_pipeline.imm & FUNCT); // extract funct field from instruction[0~15]
	int control_line = prevID_EX_pipeline.ALUControl;

	// Special case for shift commands (srl, sll)
	if ((control_line == 4) || (control_line == 5)) {
		ALUinput1 = (prevID_EX_pipeline.instr_debug & SHAMT) >> 6;
	}

	// process ALU
	uint32_t ALUresult = ALU(control_line, ALUinput1, ALUinput2);
	EX_MEM_pipeline_buffer.zero = !ALUresult;
	EX_MEM_pipeline_buffer.ALU_OUT = ALUresult;
	if (isDebug) printf("ALU result is : %d\n", ALUresult);

	// Mux for Write Register
	// if RegDst == 1, the register destination number for the Write register comes from the rd field.(bits 15:11)
	// if RegDst == 0, the register destination number for the Write register comes from the rt field.(bits 20:16)
	if (prevID_EX_pipeline.RegDst) {
		EX_MEM_pipeline_buffer.RegDstNum = prevID_EX_pipeline.rd;
	}
	else {
		EX_MEM_pipeline_buffer.RegDstNum = prevID_EX_pipeline.rt;
	}
	if (isDebug) {
		printf("*debug process_EX: CURRENTPC 0x%08x, instr %08x\n", EX_MEM_pipeline_buffer.CURRENTPC, EX_MEM_pipeline_buffer.instr_debug);
		printf("    forwardA %d, forwardB %d, ALUinput1 %d, ALUinput2 %d, ALU_OUT %d\n", forwardA, forwardB, ALUinput1, ALUinput2, ALUresult);
	}
}

/*=====================================================*/
/*			ALU()									   */
/*			Execute ALU operations					   */
/*=====================================================*/
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

/*=====================================================*/
/*			process_MEM()							   */
/*			Decide which memory to read or write	   */
/*=====================================================*/

void process_MEM() {
	EX_MEM prevEX_MEM_pipeline = CURRENT_STATE.EX_MEM_pipeline;
	MEM_WB_pipeline_buffer.instr_debug = prevEX_MEM_pipeline.instr_debug;

	// shift the pipelined control signals
	MEM_WB_pipeline_buffer.MemToReg = prevEX_MEM_pipeline.WB_MemToReg;
	MEM_WB_pipeline_buffer.RegWrite = prevEX_MEM_pipeline.WB_RegWrite;
	MEM_WB_pipeline_buffer.MemRead = prevEX_MEM_pipeline.MemRead;
	MEM_WB_pipeline_buffer.CURRENTPC = prevEX_MEM_pipeline.CURRENTPC;

	// branching
	if (prevEX_MEM_pipeline.Branch) { // branch condition
		if (prevEX_MEM_pipeline.zero) {
			// if the branch Prediction was enabled, this is handled by generatecontrolsignal in ID stage.

			PC_jump = prevEX_MEM_pipeline.NPC;

		}
		else {
			PC_buffer = prevEX_MEM_pipeline.CURRENTPC + 4;
		}
	}

	// forwarding
	bool forwardingDone = false;


	if (CURRENT_STATE.MEM_WB_pipeline.MemRead & CURRENT_STATE.EX_MEM_pipeline.MemWrite)
	{
		if (CURRENT_STATE.EX_MEM_pipeline.RegDstNum == CURRENT_STATE.MEM_WB_pipeline.RegDstNum) {
			mem_write(prevEX_MEM_pipeline.ALU_OUT, CURRENT_STATE.MEM_WB_pipeline.Mem_OUT);
			forwardingDone = true;
		}
	}


	// Read data from memory
	if (prevEX_MEM_pipeline.MemRead) {
		MEM_WB_pipeline_buffer.Mem_OUT = mem_read(prevEX_MEM_pipeline.ALU_OUT);
	}
	else {
		MEM_WB_pipeline_buffer.ALU_OUT = prevEX_MEM_pipeline.ALU_OUT;
	}

	// Write data to memory
	if (prevEX_MEM_pipeline.MemWrite & (!forwardingDone)) {
		mem_write(prevEX_MEM_pipeline.ALU_OUT, prevEX_MEM_pipeline.data2);
	}

	// shift Register destination number (on write)
	MEM_WB_pipeline_buffer.RegDstNum = prevEX_MEM_pipeline.RegDstNum;
	if (isDebug) {
		printf("*debug process_MEM: CURRENTPC 0x%08x, instr %08x\n", MEM_WB_pipeline_buffer.CURRENTPC, MEM_WB_pipeline_buffer.instr_debug);
		printf("    jump %d, Branch %d, zero %d, PC_jump 0x%08x\n", globaljump, prevEX_MEM_pipeline.Branch, prevEX_MEM_pipeline.zero, PC_jump);
	}
}

/*=====================================================*/
/*			mem_write()								   */
/*			Write memory.							   */
/*=====================================================*/
void mem_write(uint32_t address, uint32_t value)
{
	value = (value & 0xff) << 24 | (value & 0xff00) << 8 | (value & 0xff0000) >> 8 | (value & 0xff000000) >> 24;
	MEM[address / 0x4] = value;
	return;
}

/*=====================================================*/
/*			mem_read()								   */
/*			read memory.							   */
/*=====================================================*/
uint32_t mem_read(uint32_t address)
{
	return (MEM[address / 4]);
}

/*=====================================================*/
/*			flush_IF_ID()							   */
/*			flush IF_ID latch.						   */
/*=====================================================*/
void flush_IF_ID() {
	CURRENT_STATE.IF_ID_pipeline.NPC = 0;
	CURRENT_STATE.IF_ID_pipeline.CURRENTPC = 0;
	CURRENT_STATE.IF_ID_pipeline.inst = 0;
}

/*=====================================================*/
/*			flush_ID_EX()							   */
/*			flush ID_EX latch.						   */
/*=====================================================*/
void flush_ID_EX() {
	CURRENT_STATE.ID_EX_pipeline.NPC = 0;
	CURRENT_STATE.ID_EX_pipeline.CURRENTPC = 0;
	CURRENT_STATE.ID_EX_pipeline.WB_MemToReg = false;
	CURRENT_STATE.ID_EX_pipeline.WB_RegWrite = false;
	CURRENT_STATE.ID_EX_pipeline.MEM_MemWrite = false;
	CURRENT_STATE.ID_EX_pipeline.MEM_MemRead = false;
	CURRENT_STATE.ID_EX_pipeline.MEM_Branch = false;
	CURRENT_STATE.ID_EX_pipeline.RegDst = false;
	CURRENT_STATE.ID_EX_pipeline.ALUSrc = false;
	CURRENT_STATE.ID_EX_pipeline.jump = false;
	CURRENT_STATE.ID_EX_pipeline.v1 = 0;
	CURRENT_STATE.ID_EX_pipeline.v2 = 0;
	CURRENT_STATE.ID_EX_pipeline.imm = 0;
	CURRENT_STATE.ID_EX_pipeline.rs = 0;
	CURRENT_STATE.ID_EX_pipeline.rt = 0;
	CURRENT_STATE.ID_EX_pipeline.rd = 0;
	CURRENT_STATE.ID_EX_pipeline.instr_debug = 0;
}

/*=====================================================*/
/*			flush_EX_MEM()							   */
/*			flush EX_MEM latch.						   */
/*=====================================================*/
void flush_EX_MEM() {
	CURRENT_STATE.EX_MEM_pipeline.NPC = 0;
	CURRENT_STATE.EX_MEM_pipeline.CURRENTPC = 0;
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

void mdump() {
	int address;
	int start = 0;
	int stop = NUM_INST * 4;
	printf("Memory content [0x%08x..0x%08x] :\n", start, stop);
	printf("-------------------------------------\n");
	for (address = 0; address < stop; address += 4)
		printf("0x%08x: 0x%08x\n", address, mem_read(address));
	printf("\n");
}

void pdump() {
	//int k;

	printf("Current pipeline PC state :\n");
	printf("-------------------------------------\n");
	printf("CYCLE %d:", CYCLE_COUNT);

	if ((!CURRENT_STATE.PC) || ((CURRENT_STATE.PC) >> 2) >= NUM_INST) {
		printf("          ");
	}
	else {
		printf("0x%08x", CURRENT_STATE.PC);
	}
	printf("|");
	if (CURRENT_STATE.IF_ID_pipeline.CURRENTPC) printf("0x%08x", CURRENT_STATE.IF_ID_pipeline.CURRENTPC);
	else printf("          ");
	printf("|");
	if (CURRENT_STATE.ID_EX_pipeline.CURRENTPC) printf("0x%08x", CURRENT_STATE.ID_EX_pipeline.CURRENTPC);
	else printf("          ");
	printf("|");
	if (CURRENT_STATE.EX_MEM_pipeline.CURRENTPC) printf("0x%08x", CURRENT_STATE.EX_MEM_pipeline.CURRENTPC);
	else printf("          ");
	printf("|");
	if (CURRENT_STATE.MEM_WB_pipeline.CURRENTPC) printf("0x%08x", CURRENT_STATE.MEM_WB_pipeline.CURRENTPC);
	else printf("          ");


	/*
	 for(k = 0; k < 5; k++)
	 {
		if(CURRENT_STATE.PIPE[k])
		printf("0x%08x", CURRENT_STATE.PIPE[k]);
	 else
		printf("          ");

	 if( k != PIPE_STAGE - 1 )
		printf("|");
	 }
	 */

	printf("\n\n");
}

/*=====================================================*/
/*			process_wb()			     			   */
/*			Writeback stage.						   */
/*=====================================================*/

void process_WB()
{
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
		REG[prevMEM_WB_pipeline.RegDstNum] = writeData;
	}
	if (isDebug)
	{
		printf("*debug process_WB: PC 0x%08x, instr %08x\n", prevMEM_WB_pipeline.CURRENTPC, prevMEM_WB_pipeline.instr_debug);
	}
}