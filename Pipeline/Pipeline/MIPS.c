#include <stdio.h>
#include <string.h>

#define OPCODE 0xfc000000
#define RS 0x3e00000
#define RT 0x1f0000
#define RD 0xf800
#define FUNCT 0x3f
#define SHAMT 0x7c0

//R-inst
#define RINST 0
#define ADD 0x20
#define ADDU 0x21
#define AND 0x24
#define JR 0x08
#define NOR 0x27
#define OR 0x25
#define SLT 0x2A
#define SLTU 0x2B
#define SLL 0x00
#define SRL 0x02
#define SUB 0x22
#define SUBU 0x23

//I-inst
#define ADDI 0x08
#define ADDIU 0x09
#define ANDI 0x0C
#define BEQ 0x04
#define BNE 0x05
#define LBU 0x24
#define LL 0x30
#define LUI 0x0F
#define LW 0x23
#define ORI 0x0D
#define SLTI 0x0A
#define SLTIU 0x0B
#define SW 0x2B

//J-Inst
#define J 0x02
#define JAL 0x03

int Memory[0x100000 / 4];
int pc;
int gClock;
int Reg[32];
int npc;
int iform = 0;
int rform = 0;
int jform = 0;
int br = 0;
int total;
unsigned int opcode;
int rs;
int rt;
int rd;
int s_imm;
int v1; //reg read 1
int v2; //reg read 2
unsigned int funct;
unsigned int shamt;
int addr;
int writereg;
signed int imm;
int target_j;
int target_brq;
int inst;
int i;
int num_inst = 0;

struct controlsig
{
	int RegDest;
	int Jump;
	int Branch;
	int MemRead;
	int MemtoReg;
	int AluOp;
	int MemWrite;
	int AluSrc;
	int RegWrite;
} control_sig;

void loadprogram();
void ifetch();
void runprog();
void progexecute();
void init_reg();
int inst_fetch(int pc);
void idec();
void iex();
void generate_controlsig();

void main()
{
	pc = 0;
	npc = 0;
	for (i = 0; i < 32; i++)
	{
		Reg[i] = 0;
	}

	init_reg();

	loadprogram();
	runprog();


	printf("Total R Format: %d\n", rform);
	printf("Total I Format: %d\n", iform);
	printf("Total J Format: %d\n", jform);
	printf("Total of Branches: %d\n", br);
	total = iform + rform + jform + br;
	printf("Total Executed Instruction: %d\n", total);
	printf("Final Result: R[2] = %d\n", Reg[2]);
}

void loadprogram()
{
	FILE* fp = NULL;
	unsigned int data = 0;
	int ret = 0;
	int i = 0;
	unsigned int inst2;

	fp = fopen("test_prog/input4.bin", "rb");
	if (fp == NULL)
	{
		printf("no file: %s\n", "input4.bin");
		return;
	}

	while (1)
	{
		ret = fread(&data, sizeof(int), 1, fp);
		if (ret == 0)
			break;

		inst2 = (data & 0xff) << 24 | (data & 0xff00) << 8 | (data & 0xff0000) >> 8 | (data & 0xff000000) >> 24;
		Memory[i++] = inst2;
		//printf("Memory[0x%08X]: 0x%08X\n", i << 2, inst);
		num_inst++;
	}
	fclose(fp);
}


void runprog()
{
	while(1)
	{
		
		if (pc == 0xFFFFFFFF)
			return;
		progexecute();
	}
}

void progexecute()
{
	ifetch();
	idec();
	iex();
}

void init_reg()
{
	//init
	Reg[29] = 0x100000;   //sp
	Reg[31] = 0xFFFFFFFF; //return addr
}

void idec()
{
	//local var
	//printf("========= Cycle 0x%08X =======\n", gClock);
	npc = 0;
	//fetch
	//printf("pc: 0x%08x, inst: 0x%08X ", pc, inst);
	//inst = Memory[pc / 4];
	opcode = (inst & OPCODE) >> 26;
	rs = (inst & RS) >> 21;
	rt = (inst & RT) >> 16;
	rd = (inst & RD) >> 11;
	funct = (inst & FUNCT);
	shamt = (inst & SHAMT) >> 6;
	addr = (inst & 0x3ffffff);

	if ((inst & 0xffff) >= 0x8000) //shift immediate value
	{
		imm = ((inst & 0xffff) | 0xffff0000);
	}
	else
	{
		imm = (inst & 0xffff);
	}

	s_imm = imm;
	v1 = Reg[rs];
	v2 = Reg[rt];
	writereg = Reg[rd];
	target_j = addr * 4;
	target_brq = s_imm << 2;

	generate_controlsig();

	//printf("0x%08X (Opcode: 0x%x)", inst, opcode);
	//printf("opcode: 0x%X rs:%d rt:%d rd:%d func:0x%x\n", opcode, rs, rt, rd, funct);
	//printf("s_imm: 0x%08X v1: 0x%08X v2:0x%08x\n", s_imm, v1, v2);
}

void generate_controlsig()
{
	if (inst == 0x00000000)
	{
		gClock = gClock + 1;
		pc = pc + 4;
		//printf("\nDo nothing\n\n");
	}
	else if (opcode == RINST)
	{
		//printf("[Type R Format]\n");
		control_sig.RegDest = 1;
		rform++;
	}
	else if (opcode == J || opcode == JAL)
	{
		//printf("[Type J Format]\n");
		control_sig.Jump = 1;
		jform++;
	}
	else
	{
		//printf("[Type I Format]\n");
		if (opcode == BEQ || opcode == BNE)
		{
			control_sig.Branch = 1;
			br++;
		}
		else if (opcode == LW)
		{
			control_sig.MemtoReg = 1;
			control_sig.MemRead = 1;
		}
		else if (opcode == SW)
		{
			control_sig.MemWrite = 1;
		}
		else if (opcode != RINST && opcode != BEQ && opcode != BNE)
		{
			control_sig.AluSrc = 1;
		}
		else
		{
			control_sig.RegWrite = 1;
		}
		iform++;
	}
}

void iex()
{
	
	if (control_sig.AluSrc == 1)
	{
		switch (opcode)
		{
		case ADDI:
			Reg[rt] = Reg[rs] + s_imm;
			//printf("addi R[%d:rt] = R[%d:rs] + %d:Imm\n",
			//rt, rs, s_imm);
			//printf("R[%d] = %d\n\n", rt, Reg[rt]);
			pc = pc + 4;
			break;

		case ADDIU:
			Reg[rt] = Reg[rs] + s_imm;
			//printf("addiu R[%d:rt] = R[%d:rs] + %d:Imm\n",
			//rt, rs, s_imm);
			//printf("R[%d] = %d\n\n", rt, Reg[rt]);
			pc = pc + 4;
			break;
		case ANDI:
			Reg[rt] = Reg[rs] & s_imm;
			//printf("andi R[%d:rt] = R[%d:rs] & 0x%x:ZeroExtImm\n\n",
			//rt, rs, s_imm);
			pc = pc + 4;
			break;
		case LL:
			Reg[rt] = Memory[Reg[rs] + s_imm];
			//printf("ll R[%d:rt] = M[R[%d:rs] + %d:Imm]\n\n",
			//rt, rs, s_imm);
			pc = pc + 4;
			break;
		case LUI:
			Reg[rt] = s_imm << 16;
			//printf("lui R[%d:rt] = {0x%x:imm, 16'b0} (0x%08x)\n\n",
			//	rt, s_imm, (s_imm << 16));
			pc = pc + 4;
			break;
		case ORI:
			Reg[rt] = Reg[rs] | (0xFFFF & imm);
			//printf("ori R[%d:rt] = R[%d:rs] | 0x%x:ZeroExtImm\n\n",
			//	rt, rs, (0xFFFF & imm));
			pc = pc + 4;
			break;
		case SLTI:
			Reg[rt] = Reg[rs] < s_imm;
			//printf("slti R[%d:rt] = R[%d:rs] < 0x%x:SignExtImm) ? 1 : 0(%d)\n\n",
			//	rt, rs, s_imm, (v1 < s_imm) ? 1 : 0);
			pc = pc + 4;
			break;
		case SLTIU:
			Reg[rt] = Reg[rs] < s_imm;
			//printf("sltiu R[%d:rt] = R[%d:rs] < 0x%x:SignExtImm) ? 1 : 0 (%d)\n\n",
			//	rt, rs, s_imm, (v1 < s_imm) ? 1 : 0);
			pc = pc + 4;
			break;
		}
	}
	if (control_sig.Branch == 1)
	{
		if (opcode == BEQ)
		{
			if (Reg[rs] == Reg[rt])
			{
				pc = pc + 4 + (s_imm * 4);
			}
			else
			{
				pc = pc + 4;
			}
			//printf("beq if(R[%d:rs] == R[%d:rt] (%d)) pc = pc+4+BranchAddr\n\n",
			//	rs, rt, (v1 == v2) ? 1 : 0);
		}
		else if (opcode == BNE)
		{
			if (Reg[rs] != Reg[rt])
			{
				pc = pc + 4 + (s_imm * 4);
			}
			else
			{
				pc = pc + 4;
			}
			//printf("bne if (R[%d:rs] != R[%d:rt] (%d) ) pc = pc+4+BranchAddr\n\n",
			//	rs, rt, (v1 != v2) ? 1 : 0);
		}
	}
	if (control_sig.RegDest == 1 && opcode == 0x0 && inst != 0x00000000)
	{
		writereg = (inst & 0xF800) >> 11;
		switch (funct)
		{
		case SLL:
			Reg[rd] = Reg[rt] << shamt;
			//printf("sll R[%d:rd] = R[%d:rt] << 0x%x:shamt\n\n",
			//	rd, rt, shamt);
			pc = pc + 4;
			break;
		case SRL:
			Reg[rd] = Reg[rt] >> shamt;
			//printf("srl R[%d:rd] = R[%d:rt] >> 0x%x:shamt 90x%08x)\n\n",
			//	rd, rt, shamt, shamt);
			pc = pc + 4;
			break;
		case SLT:
			Reg[rd] = (Reg[rs] < Reg[rt]);
			//printf("slt R[%d:rd] = (R[%d:rs] < R[%d:rt]) ? 1 : 0\n\n",
			//	rd, rs, rd);
			pc = pc + 4;
			break;
		case SLTU:
			Reg[rd] = (Reg[rs] < Reg[rt]);
			//printf("sltu R[%d:rd] = (R[%d:rs] < R[%d:rt]) ? 1 : 0\n\n",
			//	rd, rs, rd);
			pc = pc + 4;
			break;
		case ADD:
			Reg[rd] = Reg[rs] + Reg[rt];
			//printf("add R[%d:rd] = R[%d:rs] + R[%d:rt]\n", rd, rs, rt);
			//printf("R[%d] = %d\n\n", rd, Reg[rd]);
			pc = pc + 4;
			break;
		case ADDU:
			Reg[rd] = Reg[rs] + Reg[rt];
			//printf("addu R[%d:rd] = R[%d:rs] + R[%d:rt]\n", rd, rs, rt);
			//printf("R[%d] = %d\n\n", rd, Reg[rd]);
			pc = pc + 4;
			break;
		case SUB:
			Reg[rd] = Reg[rs] - Reg[rt];
			//printf("sub R[%d:rd] = R[%d:rs] - R[%d:rt]\n\n", rd, rs, rt);
			pc = pc + 4;
			break;
		case SUBU:
			Reg[rd] = Reg[rs] - Reg[rt];
			//printf("subu R[%d:rd] = R[%d:rs] + R[%d:rt]\n\n", rd, rs, rt);
			pc = pc + 4;
			break;
		case AND:
			Reg[rd] = Reg[rs] & Reg[rt];
			//printf("and R[%d:rd] = R[%d:rs] & R[%d:rt]\n\n", rd, rs, rt);
			pc = pc + 4;
			break;
		case OR:
			Reg[rd] = Reg[rs] | Reg[rt];
			//printf("or R[%d:rd] = R[%d:rs] | R[%d:rt]\n\n", rd, rs, rt);
			pc = pc + 4;
			break;
		case NOR:
			Reg[rd] = ~(Reg[rs] + Reg[rt]);
			//printf("nor R[%d:rd] = ~(R[%d:rs] | R[%d:rt])\n\n", rd, rs, rt);
			pc = pc + 4;
			break;
		}
	}
	else
	{
		writereg = rt;
	}

	if (opcode == SW)
	{
		Memory[(Reg[rs] + s_imm) / 0x4] = Reg[rt];
		//printf("sw M[R[%d:rs] + 0x%x:Imm] = R[%d:rt]\n", rs, s_imm, rt);
		//printf("Mem[0x%x] = R[%d]\n\n", Memory[(Reg[rs] + s_imm) / 4], rt);
		pc = pc + 4;
	}
	else if (opcode == LW)
	{
		Reg[rt] = Memory[(Reg[rs] + s_imm) / 0x4];
		//printf("lw R[%d:rt] = M[R[%d:rs]+ 0x%x:Imm]\n", rt, rs, s_imm);
		//printf("R[%d] = 0x%x\n\n", rt, Reg[rt]);
		pc = pc + 4;
		//printf("%d\n", pc);
	}
	if (opcode == RINST && funct == JR)
	{
		pc = Reg[rs];
		//printf("jr PC = R[%d:rs]\n\n", rs);
	}
	else if (opcode == J)
	{
		pc = target_j;
		//printf("j PC = 0x%x\n\n", target_j);
	}
	else if (opcode == JAL)
	{
		Reg[31] = pc + 8;
		pc = target_j;
		//printf("jal R[31] = 0x%x:JumpAddr 0x%x\n\n", Reg[31], pc);
	}
	gClock++;
}

void ifetch()
{
	inst = Memory[pc/4];
}
