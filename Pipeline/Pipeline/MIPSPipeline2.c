#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "util.h"
#include "run.h"

CPU_State CURRENT_STATE;

int NUM_INST;
uint32_t *INST_INFO;
int MEM[0x100000 / 4];

bool isDebug = true;

int INSTRUCTION_COUNT;
int CYCLE_COUNT;

int main(void)
{
	int x;
	NUM_INST = 0;
	init_mem();
	NUM_INST = count();
	loadprogram(NUM_INST);
	
	if (isDebug)
	{
		for (int i = 0; i < NUM_INST ; i++)
		{
			printf("INST_INFO	[0x%08X]: 0x%08X\n", i << 2, INST_INFO[i]);
		}
	}
	x = NUM_INST;
	run(x);

	return 0;
}
/*=====================================================*/
/*					run program						   */
/*			Runs the program until the end .		   */
/*=====================================================*/

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
/*					count()							   */
/*			Counts number of instructions.			   */
/*=====================================================*/

int count()
{
	FILE* fp = NULL;
	int ret = 0;
	unsigned int data;
	NUM_INST = 0;

	fp = fopen("test_prog/simple.bin", "rb");
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
	return NUM_INST++;
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

void init_inst_info(x)
{
	int i;

	for (i = 0; i < x; i++)
	{
		INST_INFO[i] = 0;
	}
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
	INST_INFO = malloc(sizeof(uint32_t) * x);

	init_inst_info(x);

	fp = fopen("test_prog/simple.bin", "rb");
	if (fp == NULL) {
		printf("no file: %s\n", "input4.bin");
		return;
	}
	while (i < x) {
		ret = fread(&data, sizeof(int), 1, fp);
		if (ret == 0) break;

		inst2 = (data & 0xff) << 24 | (data & 0xff00) << 8 | (data & 0xff0000) >> 8 | (data & 0xff000000) >> 24;
		INST_INFO[i] = inst2;
		i++;
	}
	fclose(fp);
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