/*
** vmbuilder.cpp
**
**---------------------------------------------------------------------------
** Copyright -2016 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "vmbuilder.h"
#include "codegeneration/codegen.h"
#include "info.h"
#include "m_argv.h"
#include "thingdef.h"

//==========================================================================
//
// VMFunctionBuilder - Constructor
//
//==========================================================================

VMFunctionBuilder::VMFunctionBuilder(int numimplicits)
{
	NumIntConstants = 0;
	NumFloatConstants = 0;
	NumAddressConstants = 0;
	NumStringConstants = 0;
	MaxParam = 0;
	ActiveParam = 0;
	NumImplicits = numimplicits;
}

//==========================================================================
//
// VMFunctionBuilder - Destructor
//
//==========================================================================

VMFunctionBuilder::~VMFunctionBuilder()
{
}

//==========================================================================
//
// VMFunctionBuilder :: MakeFunction
//
// Creates a new VMScriptFunction out of the data passed to this class.
//
//==========================================================================

void VMFunctionBuilder::MakeFunction(VMScriptFunction *func)
{
	func->Alloc(Code.Size(), NumIntConstants, NumFloatConstants, NumStringConstants, NumAddressConstants);

	// Copy code block.
	memcpy(func->Code, &Code[0], Code.Size() * sizeof(VMOP));

	// Create constant tables.
	if (NumIntConstants > 0)
	{
		FillIntConstants(func->KonstD);
	}
	if (NumFloatConstants > 0)
	{
		FillFloatConstants(func->KonstF);
	}
	if (NumAddressConstants > 0)
	{
		FillAddressConstants(func->KonstA, func->KonstATags());
	}
	if (NumStringConstants > 0)
	{
		FillStringConstants(func->KonstS);
	}

	// Assign required register space.
	func->NumRegD = Registers[REGT_INT].MostUsed;
	func->NumRegF = Registers[REGT_FLOAT].MostUsed;
	func->NumRegA = Registers[REGT_POINTER].MostUsed;
	func->NumRegS = Registers[REGT_STRING].MostUsed;
	func->MaxParam = MaxParam;

	// Technically, there's no reason why we can't end the function with
	// entries on the parameter stack, but it means the caller probably
	// did something wrong.
	assert(ActiveParam == 0);
}

//==========================================================================
//
// VMFunctionBuilder :: FillIntConstants
//
//==========================================================================

void VMFunctionBuilder::FillIntConstants(int *konst)
{
	TMapIterator<int, int> it(IntConstants);
	TMap<int, int>::Pair *pair;

	while (it.NextPair(pair))
	{
		konst[pair->Value] = pair->Key;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: FillFloatConstants
//
//==========================================================================

void VMFunctionBuilder::FillFloatConstants(double *konst)
{
	TMapIterator<double, int> it(FloatConstants);
	TMap<double, int>::Pair *pair;

	while (it.NextPair(pair))
	{
		konst[pair->Value] = pair->Key;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: FillAddressConstants
//
//==========================================================================

void VMFunctionBuilder::FillAddressConstants(FVoidObj *konst, VM_ATAG *tags)
{
	TMapIterator<void *, AddrKonst> it(AddressConstants);
	TMap<void *, AddrKonst>::Pair *pair;

	while (it.NextPair(pair))
	{
		konst[pair->Value.KonstNum].v = pair->Key;
		tags[pair->Value.KonstNum] = pair->Value.Tag;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: FillStringConstants
//
//==========================================================================

void VMFunctionBuilder::FillStringConstants(FString *konst)
{
	TMapIterator<FString, int> it(StringConstants);
	TMap<FString, int>::Pair *pair;

	while (it.NextPair(pair))
	{
		konst[pair->Value] = pair->Key;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: GetConstantInt
//
// Returns a constant register initialized with the given value, or -1 if
// there were no more constants free.
//
//==========================================================================

int VMFunctionBuilder::GetConstantInt(int val)
{
	int *locp = IntConstants.CheckKey(val);
	if (locp != NULL)
	{
		return *locp;
	}
	else
	{
		int loc = NumIntConstants++;
		IntConstants.Insert(val, loc);
		return loc;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: GetConstantFloat
//
// Returns a constant register initialized with the given value, or -1 if
// there were no more constants free.
//
//==========================================================================

int VMFunctionBuilder::GetConstantFloat(double val)
{
	int *locp = FloatConstants.CheckKey(val);
	if (locp != NULL)
	{
		return *locp;
	}
	else
	{
		int loc = NumFloatConstants++;
		FloatConstants.Insert(val, loc);
		return loc;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: GetConstantString
//
// Returns a constant register initialized with the given value, or -1 if
// there were no more constants free.
//
//==========================================================================

int VMFunctionBuilder::GetConstantString(FString val)
{
	int *locp = StringConstants.CheckKey(val);
	if (locp != NULL)
	{
		return *locp;
	}
	else
	{
		int loc = NumStringConstants++;
		StringConstants.Insert(val, loc);
		return loc;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: GetConstantAddress
//
// Returns a constant register initialized with the given value, or -1 if
// there were no more constants free.
//
//==========================================================================

int VMFunctionBuilder::GetConstantAddress(void *ptr, VM_ATAG tag)
{
	if (ptr == NULL)
	{ // Make all NULL pointers generic. (Or should we allow typed NULLs?)
		tag = ATAG_GENERIC;
	}
	AddrKonst *locp = AddressConstants.CheckKey(ptr);
	if (locp != NULL)
	{
		// There should only be one tag associated with a memory location.
		assert(locp->Tag == tag);
		return locp->KonstNum;
	}
	else
	{
		AddrKonst loc = { NumAddressConstants++, tag };
		AddressConstants.Insert(ptr, loc);
		return loc.KonstNum;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: ParamChange
//
// Adds delta to ActiveParam and keeps track of MaxParam.
//
//==========================================================================

void VMFunctionBuilder::ParamChange(int delta)
{
	assert(delta > 0 || -delta <= ActiveParam);
	ActiveParam += delta;
	if (ActiveParam > MaxParam)
	{
		MaxParam = ActiveParam;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: RegAvailability - Constructor
//
//==========================================================================

VMFunctionBuilder::RegAvailability::RegAvailability()
{
	memset(Used, 0, sizeof(Used));
	MostUsed = 0;
}

//==========================================================================
//
// VMFunctionBuilder :: RegAvailability :: Get
//
// Gets one or more unused registers. If getting multiple registers, they
// will all be consecutive. Returns -1 if there were not enough consecutive
// registers to satisfy the request.
//
// Preference is given to low-numbered registers in an attempt to keep
// the maximum register count low so as to preserve VM stack space when this
// function is executed.
//
//==========================================================================

int VMFunctionBuilder::RegAvailability::Get(int count)
{
	VM_UWORD mask;
	int i, firstbit;

	// Getting fewer than one register makes no sense, and
	// the algorithm used here can only obtain ranges of up to 32 bits.
	if (count < 1 || count > 32)
	{
		return -1;
	}

	mask = count == 32 ? ~0u : (1 << count) - 1;

	for (i = 0; i < 256/32; ++i)
	{
		// Find the first word with free registers
		VM_UWORD bits = Used[i];
		if (bits != ~0u)
		{
			// Are there enough consecutive bits to satisfy the request?
			// Search by 16, then 8, then 1 bit at a time for the first
			// free register.
			if ((bits & 0xFFFF) == 0xFFFF)
			{
				firstbit = ((bits & 0xFF0000) == 0xFF0000) ? 24 : 16;
			}
			else
			{
				firstbit = ((bits & 0xFF) == 0xFF) ? 8 : 0;
			}
			for (; firstbit < 32; ++firstbit)
			{
				if (((bits >> firstbit) & mask) == 0)
				{
					if (firstbit + count <= 32)
					{ // Needed bits all fit in one word, so we got it.
						if (firstbit + count > MostUsed)
						{
							MostUsed = firstbit + count;
						}
						Used[i] |= mask << firstbit;
						return i * 32 + firstbit;
					}
					// Needed bits span two words, so check the next word.
					else if (i < 256/32 - 1)
					{ // There is a next word.
						if (((Used[i + 1]) & (mask >> (32 - firstbit))) == 0)
						{ // The next word has the needed open space, too.
							if (firstbit + count > MostUsed)
							{
								MostUsed = firstbit + count;
							}
							Used[i] |= mask << firstbit;
							Used[i + 1] |= mask >> (32 - firstbit);
							return i * 32 + firstbit;
						}
						else
						{ // Skip to the next word, because we know we won't find
						  // what we need if we stay inside this one. All bits
						  // from firstbit to the end of the word are 0. If the
						  // next word does not start with the x amount of 0's, we
						  // need to satisfy the request, then it certainly won't
						  // have the x+1 0's we would need if we started at
						  // firstbit+1 in this one.
							firstbit = 32;
						}
					}
					else
					{ // Out of words.
						break;
					}
				}
			}
		}
	}
	// No room!
	return -1;
}

//==========================================================================
//
// VMFunctionBuilder :: RegAvailibity :: Return
//
// Marks a range of registers as free again.
//
//==========================================================================

void VMFunctionBuilder::RegAvailability::Return(int reg, int count)
{
	assert(count >= 1 && count <= 32);
	assert(reg >= 0 && reg + count <= 256);

	VM_UWORD mask, partialmask;
	int firstword, firstbit;

	mask = count == 32 ? ~0u : (1 << count) - 1;
	firstword = reg / 32;
	firstbit = reg & 31;

	if (firstbit + count <= 32)
	{ // Range is all in one word.
		mask <<= firstbit;
		// If we are trying to return registers that are already free,
		// it probably means that the caller messed up somewhere.
		assert((Used[firstword] & mask) == mask);
		Used[firstword] &= ~mask;
	}
	else
	{ // Range is in two words.
		partialmask = mask << firstbit;
		assert((Used[firstword] & partialmask) == partialmask);
		Used[firstword] &= ~partialmask;

		partialmask = mask >> (32 - firstbit);
		assert((Used[firstword + 1] & partialmask) == partialmask);
		Used[firstword + 1] &= ~partialmask;
	}
}

//==========================================================================
//
// VMFunctionBuilder :: RegAvailability :: Reuse
//
// Marks an unused register as in-use. Returns false if the register is
// already in use or true if it was successfully reused.
//
//==========================================================================

bool VMFunctionBuilder::RegAvailability::Reuse(int reg)
{
	assert(reg >= 0 && reg <= 255);
	assert(reg < MostUsed && "Attempt to reuse a register that was never used");

	VM_UWORD mask = 1 << (reg & 31);
	int word = reg / 32;

	if (Used[word] & mask)
	{ // It's already in use!
		return false;
	}
	Used[word] |= mask;
	return true;
}

//==========================================================================
//
// VMFunctionBuilder :: GetAddress
//
//==========================================================================

size_t VMFunctionBuilder::GetAddress()
{
	return Code.Size();
}

//==========================================================================
//
// VMFunctionBuilder :: Emit
//
// Just dumbly output an instruction. Returns instruction position, not
// byte position. (Because all instructions are exactly four bytes long.)
//
//==========================================================================

size_t VMFunctionBuilder::Emit(int opcode, int opa, int opb, int opc)
{
	assert(opcode >= 0 && opcode < NUM_OPS);
	assert(opa >= 0 && opa <= 255);
	assert(opb >= 0 && opb <= 255);
	assert(opc >= 0 && opc <= 255);
	if (opcode == OP_PARAM)
	{
		int chg;
		if (opb & REGT_MULTIREG2) chg = 2;
		else if (opb&REGT_MULTIREG3) chg = 3;
		else chg = 1;
		ParamChange(chg);
	}
	else if (opcode == OP_CALL || opcode == OP_CALL_K || opcode == OP_TAIL || opcode == OP_TAIL_K)
	{
		ParamChange(-opb);
	}
	VMOP op;
	op.op = opcode;
	op.a = opa;
	op.b = opb;
	op.c = opc;
	return Code.Push(op);
}

size_t VMFunctionBuilder::Emit(int opcode, int opa, VM_SHALF opbc)
{
	assert(opcode >= 0 && opcode < NUM_OPS);
	assert(opa >= 0 && opa <= 255);
	//assert(opbc >= -32768 && opbc <= 32767);	always true due to parameter's width
	VMOP op;
	op.op = opcode;
	op.a = opa;
	op.i16 = opbc;
	return Code.Push(op);
}

size_t VMFunctionBuilder::Emit(int opcode, int opabc)
{
	assert(opcode >= 0 && opcode < NUM_OPS);
	assert(opabc >= -(1 << 23) && opabc <= (1 << 24) - 1);
	if (opcode == OP_PARAMI)
	{
		ParamChange(1);
	}
	VMOP op;
	op.op = opcode;
	op.i24 = opabc;
	return Code.Push(op);
}

//==========================================================================
//
// VMFunctionBuilder :: EmitParamInt
//
// Passes a constant integer parameter, using either PARAMI and an immediate
// value or PARAM and a constant register, as appropriate.
//
//==========================================================================

size_t VMFunctionBuilder::EmitParamInt(int value)
{
	// Immediates for PARAMI must fit in 24 bits.
	if (((value << 8) >> 8) == value)
	{
		return Emit(OP_PARAMI, value);
	}
	else
	{
		return Emit(OP_PARAM, 0, REGT_INT | REGT_KONST, GetConstantInt(value));
	}
}

//==========================================================================
//
// VMFunctionBuilder :: EmitLoadInt
//
// Loads an integer constant into a register, using either an immediate
// value or a constant register, as appropriate.
//
//==========================================================================

size_t VMFunctionBuilder::EmitLoadInt(int regnum, int value)
{
	assert(regnum >= 0 && regnum < Registers[REGT_INT].MostUsed);
	if (value >= -32768 && value <= 32767)
	{
		return Emit(OP_LI, regnum, value);
	}
	else
	{
		return Emit(OP_LK, regnum, GetConstantInt(value));
	}
}

//==========================================================================
//
// VMFunctionBuilder :: EmitRetInt
//
// Returns an integer, using either an immediate value or a constant
// register, as appropriate.
//
//==========================================================================

size_t VMFunctionBuilder::EmitRetInt(int retnum, bool final, int value)
{
	assert(retnum >= 0 && retnum <= 127);
	if (value >= -32768 && value <= 32767)
	{
		return Emit(OP_RETI, retnum | (final << 7), value);
	}
	else
	{
		return Emit(OP_RET, retnum | (final << 7), REGT_INT | REGT_KONST, GetConstantInt(value));
	}
}

//==========================================================================
//
// VMFunctionBuilder :: Backpatch
//
// Store a JMP instruction at <loc> that points at <target>.
//
//==========================================================================

void VMFunctionBuilder::Backpatch(size_t loc, size_t target)
{
	assert(loc < Code.Size());
	int offset = int(target - loc - 1);
	assert(((offset << 8) >> 8) == offset);
	Code[loc].op = OP_JMP;
	Code[loc].i24 = offset;
}

//==========================================================================
//
// VMFunctionBuilder :: BackpatchToHere
//
// Store a JMP instruction at <loc> that points to the current code gen
// location.
//
//==========================================================================

void VMFunctionBuilder::BackpatchToHere(size_t loc)
{
	Backpatch(loc, Code.Size());
}

//==========================================================================
//
// FFunctionBuildList
//
// This list contains all functions yet to build.
// All adding functions return a VMFunction - either a complete one
// for native functions or an empty VMScriptFunction for scripted ones
// This VMScriptFunction object later gets filled in with the actual
// info, but we get the pointer right after registering the function
// with the builder.
//
//==========================================================================
FFunctionBuildList FunctionBuildList;

VMFunction *FFunctionBuildList::AddFunction(PFunction *functype, FxExpression *code, const FString &name, bool fromdecorate, int stateindex, int statecount)
{
	auto func = code->GetDirectFunction();
	if (func != nullptr)
	{
		delete code;
		return func;
	}

	//Printf("Adding %s\n", name.GetChars());

	Item it;
	it.Func = functype;
	it.Code = code;
	it.PrintableName = name;
	it.Function = new VMScriptFunction;
	it.Function->Name = functype->SymbolName;
	it.Function->ImplicitArgs = functype->GetImplicitArgs();
	it.Proto = nullptr;
	it.FromDecorate = fromdecorate;
	it.StateIndex = stateindex;
	it.StateCount = statecount;

	// set prototype for named functions.
	if (it.Func->SymbolName != NAME_None)
	{
		it.Function->Proto = it.Func->Variants[0].Proto;
	}

	mItems.Push(it);
	return it.Function;
}


void FFunctionBuildList::Build()
{
	int errorcount = 0;
	int codesize = 0;
	FILE *dump = nullptr;

	if (Args->CheckParm("-dumpdisasm")) dump = fopen("disasm.txt", "w");

	for (auto &item : mItems)
	{
		assert(item.Code != NULL);

		// We don't know the return type in advance for anonymous functions.
		FCompileContext ctx(item.Func, item.Func->SymbolName == NAME_None ? nullptr : item.Func->Variants[0].Proto, item.FromDecorate, item.StateIndex, item.StateCount);

		// Allocate registers for the function's arguments and create local variable nodes before starting to resolve it.
		VMFunctionBuilder buildit(item.Func->GetImplicitArgs());
		for(unsigned i=0;i<item.Func->Variants[0].Proto->ArgumentTypes.Size();i++)
		{
			auto type = item.Func->Variants[0].Proto->ArgumentTypes[i];
			auto name = item.Func->Variants[0].ArgNames[i];
			auto flags = item.Func->Variants[0].ArgFlags[i];
			// this won't get resolved and won't get emitted. It is only needed so that the code generator can retrieve the necessary info about this argument to do its work.
			auto local = new FxLocalVariableDeclaration(type, name, nullptr, flags, FScriptPosition());	
			local->RegNum = buildit.Registers[type->GetRegType()].Get(type->GetRegCount());
			ctx.FunctionArgs.Push(local);
		}

		FScriptPosition::StrictErrors = !item.FromDecorate;
		item.Code = item.Code->Resolve(ctx);

		// Make sure resolving it didn't obliterate it.
		if (item.Code != nullptr)
		{
			if (!item.Code->CheckReturn())
			{
				auto newcmpd = new FxCompoundStatement(item.Code->ScriptPosition);
				newcmpd->Add(item.Code);
				newcmpd->Add(new FxReturnStatement(nullptr, item.Code->ScriptPosition));
				item.Code = newcmpd->Resolve(ctx);
			}

			item.Proto = ctx.ReturnProto;
			if (item.Proto == nullptr)
			{
				item.Code->ScriptPosition.Message(MSG_ERROR, "Function %s without prototype", item.PrintableName.GetChars());
				continue;
			}

			// Generate prototype for anonymous functions.
			VMScriptFunction *sfunc = item.Function;
			// create a new prototype from the now known return type and the argument list of the function's template prototype.
			if (sfunc->Proto == nullptr)
			{
				sfunc->Proto = NewPrototype(item.Proto->ReturnTypes, item.Func->Variants[0].Proto->ArgumentTypes);
			}

			// Emit code
			item.Code->Emit(&buildit);
			buildit.MakeFunction(sfunc);
			sfunc->NumArgs = 0;
			// NumArgs for the VMFunction must be the amount of stack elements, which can differ from the amount of logical function arguments if vectors are in the list.
			// For the VM a vector is 2 or 3 args, depending on size.
			for (auto s : item.Func->Variants[0].Proto->ArgumentTypes)
			{
				sfunc->NumArgs += s->GetRegCount();
			}

			if (dump != nullptr)
			{
				DumpFunction(dump, sfunc, item.PrintableName.GetChars(), (int)item.PrintableName.Len());
				codesize += sfunc->CodeSize;
			}
			sfunc->PrintableName = item.PrintableName;
		}
		delete item.Code;
		if (dump != nullptr)
		{
			fflush(dump);
		}
	}
	if (dump != nullptr)
	{
		fprintf(dump, "\n*************************************************************************\n%i code bytes\n", codesize * 4);
		fclose(dump);
	}
	FScriptPosition::StrictErrors = false;

}