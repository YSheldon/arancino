#include "PatternMatchModule.h"


//----------------------------- PATCH FUNCTIONS -----------------------------//

//avoid the leak of the modified ip by pin
VOID patchInt2e(ADDRINT ip, CONTEXT *ctxt, ADDRINT cur_eip ){
	MYTEST("[POSSIBLE EVASIVE BEHAVIOR] int2e instruction detected ( possible leak of real EIP )\n");
	//set the return value of the int2e (stored in edx) as the current ip
	PIN_SetContextReg(ctxt, REG_EDX, cur_eip);	
} 

//avoid the leak of the modified ip by pin
VOID patchFsave(ADDRINT ip, CONTEXT *ctxt, ADDRINT cur_eip ){
	//set the return value of the int2e (stored in edx) as the current ip
	FPSTATE a;
	//get the current fp unit state
	PIN_GetContextFPState(ctxt, &a);
	//set the correct ip and save the state
	a.fxsave_legacy._fpuip = cur_eip;
	PIN_SetContextFPState(ctxt, &a);
} 

//fake the result of an rdtsc operation by dividing it by RDTSC_DIVISOR
VOID patchRtdsc(ADDRINT ip, CONTEXT *ctxt, ADDRINT cur_eip ){
	//get the two original values ()
	UINT32 eax_value = PIN_GetContextReg(ctxt, REG_EAX);
	UINT32 edx_value = PIN_GetContextReg(ctxt, REG_EDX);
	//store the value of edx in a 64 bit data in order to shift this value correctly
	UINT64 tmp_edx = edx_value;
	//we have to compose the proper returned value (EDX:EAX) so let's shift the value of EDX by 32 bit on the left (tmp_edx00..0) and add to this value eax_value (tmp_edxeax_value) and divide the result by a proper divisor
	UINT64 divided_time = ( (tmp_edx << 32) + eax_value ) / Config::RDTSC_DIVISOR;
	//get the right parts 
	UINT32 eax_new_value = divided_time; 
	UINT32 edx_new_value = divided_time >> 32;	
	//MYINFO("Detected a rdtsc, EAX before = %08x , EAX after = %08x , EDX before: %08x , EDX after: %08x\n", eax_value, le_fighe_bianche, edx_value, edx_new_value);
	//set the registerss
	PIN_SetContextReg(ctxt, REG_EAX,eax_new_value);
	PIN_SetContextReg(ctxt, REG_EDX,edx_new_value);
} 

//----------------------------- END PATCH FUNCTIONS -----------------------------//


PatternMatchModule::PatternMatchModule(void)
{
	//set the initial patch pointer to zero (an invalid address) 
	this->curPatchPointer = 0x0;
	//create the map for our our patches
	//ex : if i find an int 2e instruction we have the functon pointer for the right patch 
	this->patchesMap.insert( std::pair<string,AFUNPTR>("int 0x2e",(AFUNPTR)patchInt2e) );
	//this->patchesMap.insert( std::pair<string,AFUNPTR>("fsave",(AFUNPTR)patchFsave) );
	//this->patchesMap.insert( std::pair<string,AFUNPTR>("rdtsc ",(AFUNPTR)patchRtdsc) );	
}


PatternMatchModule::~PatternMatchModule(void)
{
}

//search if we have a patch for the current instruction and if yes insert the patch in the next round
bool PatternMatchModule::patchDispatcher(INS ins, ADDRINT curEip){	
	//if we have found an instruction that has to be patched in the previous round then we have a correct function pointer end we can instrument the code
	//we have to use this trick because some instructions, such as int 2e, don't have a fall throug and is not possible to insert an analysis routine with the IPOINT_AFTER attribute
	if(this->curPatchPointer){
		//all the register in the context can be modified
		REGSET regsIn;
		REGSET_AddAll(regsIn);
		REGSET regsOut;
		REGSET_AddAll(regsOut);
		//add the analysis rtoutine (the patch)
		INS_InsertCall(ins, IPOINT_BEFORE, this->curPatchPointer, IARG_INST_PTR, IARG_PARTIAL_CONTEXT, &regsIn, &regsOut, IARG_ADDRINT, curEip, IARG_END);
		//invalidate the function pointer for the next round
		this->curPatchPointer = 0x0;
		return true;
	}	
	//disasseble the instruction
	std::string disass_instr = INS_Disassemble(ins);
	//if we find an fsave instruction or similar we have to patch it immediately
	
	/*
	std::regex rx("^f(.*)[save|env](.*)");	
	if (std::regex_match(disass_instr.cbegin(), disass_instr.cend(), rx)){
		//all the register in the context can be modified
		REGSET regsIn;
		REGSET_AddAll(regsIn);
		REGSET regsOut;
		REGSET_AddAll(regsOut);
		//add the analysis rtoutine (the patch)
		INS_InsertCall(ins, IPOINT_BEFORE,  this->patchesMap.at("fsave"), IARG_INST_PTR, IARG_PARTIAL_CONTEXT, &regsIn, &regsOut, IARG_ADDRINT, curEip, IARG_END);
		return true;
	}
	*/
	//search if we have a patch foir this instruction
	std::map<string, AFUNPTR>::iterator item = this->patchesMap.find(disass_instr);
	if(item != this->patchesMap.end()){
		//if so retrieve the correct function pointer for the analysis routine at the next round
		this->curPatchPointer = this->patchesMap.at(disass_instr);
		return true;
	}
	//otherwiae continue the analysis in the class PINshield
	return false;

}