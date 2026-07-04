// Copyright (c) 2013-2025 John Seamons, ZL4VO/KF6VO

#pragma once

typedef struct {
	const char *str;
	token_type_e ttype;
	int val;
    u4_t flags;     // TF_* flags
    u4_t mask;      // opcode mask for TT_OPC entries
	u4_t stats;
} dict_t;

#include <cpu.h>

// dictionary of reserved symbols and tokens
dict_t dict[] = {
	{ "DEF",		TT_PRE,		PP_DEF },
	{ "DEFc",		TT_PRE,		PP_DEF,			TF_CFG_H },		// gens RX_CFG in kiwi.cfg.vh
	{ "DEFh",		TT_PRE,		PP_DEF,			TF_DOT_H },		// gens a '#define' in .h and '`define' in .vh
	{ "DEFp",		TT_PRE,		PP_DEF,			TF_DOT_VP },	// gens a 'localparam' in .vh and '#define' in .h
	{ "DEFb",		TT_PRE,		PP_DEF,			TF_DOT_VB },	// gens a 'localparam' of bit position in .vh
	{ "MACRO",		TT_PRE,		PP_MACRO },
	{ "ENDM",		TT_PRE,		PP_ENDM },
	{ "REPEAT",		TT_PRE,		PP_REPEAT },
	{ "ENDR",		TT_PRE,		PP_ENDR },
	{ "STRUCT",		TT_PRE,		PP_STRUCT },
	{ "ENDS",		TT_PRE,		PP_ENDS },
	{ "FACTOR",		TT_PRE,		PP_FACTOR },
	{ "ENDF",		TT_PRE,		PP_ENDF },
	{ "FCALL",		TT_PRE,		PP_FCALL },
	{ "#if",		TT_PRE,		PP_IF },        // supports '#if NUM|SYM (implied != 0)' and '#if SYM|NUM OPR SYM|NUM'
	{ "#elif",		TT_PRE,		PP_ELIF },
	{ "#else",		TT_PRE,		PP_ELSE },
	{ "#endif",		TT_PRE,		PP_ENDIF },
	{ "#error",		TT_PRE,		PP_ERROR },
	{ "#warning",   TT_PRE,		PP_WARNING },
	{ "#display",   TT_PRE,		PP_DISPLAY },

	{ "push",		TT_OPC,		OC_PUSH,            0,              OCM_CONST },
	{ "nop",		TT_OPC,		OC_NOP },
	{ "ret",		TT_OPC,		OC_NOP | OPT_RET,   TF_RET },
	{ "dup",		TT_OPC,		OC_DUP,             TF_RET },
	{ "swap",		TT_OPC,		OC_SWAP,            TF_RET },
	{ "swap16",		TT_OPC,		OC_SWAP16,          TF_RET },
	{ "over",		TT_OPC,		OC_OVER,            TF_RET },
	{ "pop",		TT_OPC,		OC_POP,             TF_RET },
	{ "drop",		TT_OPC,		OC_POP,             TF_RET },
	{ "rot",		TT_OPC,		OC_ROT,             TF_RET },
	{ "addi",		TT_OPC,		OC_ADDI,            TF_RET },
	{ "add",		TT_OPC,		OC_ADD,             TF_RET | TF_CIN },
	{ "add.cin",    TT_STATS,   OC_ADD | OPT_CIN,   TF_RET | TF_CIN },  // only for benefit of stats
	{ "sub",		TT_OPC,		OC_SUB,             TF_RET },
	{ "mult",		TT_OPC,		OC_MULT,            TF_RET },
	{ "mult20",		TT_OPC,		OC_MULT20,          TF_RET },
	{ "and",		TT_OPC,		OC_AND,             TF_RET },
	{ "or",			TT_OPC,		OC_OR,              TF_RET },
	{ "xor",		TT_OPC,		OC_XOR,             TF_RET },
	{ "not",		TT_OPC,		OC_NOT,             TF_RET },
	{ "shl64",		TT_OPC,		OC_SHL64,           TF_RET | TF_LOOP },
	{ "shl",		TT_OPC,		OC_SHL,             TF_RET },
	{ "rol",		TT_OPC,		OC_ROL,             TF_RET },
	{ "shr",		TT_OPC,		OC_SHR,             TF_RET },
	{ "ror",		TT_OPC,		OC_ROR,             TF_RET },
	{ "usr",		TT_OPC,		OC_USR,             TF_RET },
	{ "rdBit0",		TT_OPC,		OC_RDBIT0,          TF_RET | TF_LOOP },
	{ "rdBit1",		TT_OPC,		OC_RDBIT1,          TF_RET | TF_LOOP },
	{ "rdBit2",		TT_OPC,		OC_RDBIT2,          TF_RET | TF_LOOP },
	{ "fetch16",	TT_OPC,		OC_FETCH16,         TF_RET },
	{ "store16",	TT_OPC,		OC_STORE16,         TF_RET },
	{ "stk_rd",	    TT_OPC,		OC_STK_RD,          TF_RET },
	{ "stk_wr",	    TT_OPC,		OC_STK_WR,          TF_RET },
	{ "sp_rp",      TT_OPC,		OC_SP_RP,           TF_RET },

	{ "r",			TT_OPC,		OC_R,               0 },
	{ "r_from",		TT_OPC,		OC_R_FROM,          0 },
	{ "to_r",		TT_OPC,		OC_TO_R,            0 },
	{ "call",		TT_OPC,		OC_CALL,            0,          OCM_ADDR },
	{ "br",			TT_OPC,		OC_BR,              0,          OCM_ADDR },
	{ "brZ",		TT_OPC,		OC_BRZ,             0,          OCM_ADDR },
	{ "brNZ",		TT_OPC,		OC_BRNZ,            0,          OCM_ADDR },
	{ "loop",		TT_OPC,		OC_LOOP,            0,          OCM_ADDR },
	{ "loop2",		TT_OPC,		OC_LOOP2,           0,          OCM_ADDR },
	{ "to_loop",    TT_OPC,		OC_TO_LOOP,         TF_RET | TF_OPC9 },
	{ "to_loop2",   TT_OPC,		OC_TO_LOOP2,        TF_RET | TF_OPC9 },
	{ "loop_from",  TT_OPC,		OC_LOOP_FROM,       TF_RET | TF_OPC9 },
	{ "loop2_from", TT_OPC,		OC_LOOP2_FROM,      TF_RET | TF_OPC9 },
	{ "rdReg",		TT_OPC,		OC_RDREG,           0,          OCM_IO },
	{ "wrEvtL",		TT_OPC,		OC_WREVTL,          0,          OCM_IO },
	{ "wrReg",		TT_OPC,		OC_WRREG,           0,          OCM_IO },
	{ "wrReg2",		TT_OPC,		OC_WRREG2,          0,          OCM_IO },
	{ "wrEvt",		TT_OPC,		OC_WREVT,           0,          OCM_IO },
	{ "wrEvt2",		TT_OPC,		OC_WREVT2,          0,          OCM_IO },
	
	{ "ALIGN",		TT_ALIGN,   0 },

	{ "u8",			TT_DATA,	1 },
	{ "u16",		TT_DATA,	2 },
	{ "u32",		TT_DATA,	4 },
	{ "u64",		TT_DATA,	8 },
	
	{ "++",			TT_OPR,		OPR_INC,	TF_1OPR },
	{ "+",			TT_OPR,		OPR_ADD,	TF_2OPR },
	{ "--",			TT_OPR,		OPR_DEC,	TF_1OPR },
	{ "-",			TT_OPR,		OPR_SUB,	TF_2OPR },
	{ "*",			TT_OPR,		OPR_MUL,	TF_2OPR },
	{ "/",			TT_OPR,		OPR_DIV,	TF_2OPR },
	{ "<<",			TT_OPR,		OPR_SHL,	TF_2OPR },
	{ ">>",			TT_OPR,		OPR_SHR,	TF_2OPR },
	{ "&&",			TT_OPR,		OPR_LAND,	TF_2OPR },
	{ "&",			TT_OPR,		OPR_AND,	TF_2OPR },
	{ "||",			TT_OPR,		OPR_LOR,    TF_2OPR },
	{ "|",			TT_OPR,		OPR_OR,		TF_2OPR },
	{ "==",			TT_OPR,		OPR_EQ,	    TF_2OPR },
	{ "!=",			TT_OPR,		OPR_NEQ,	TF_2OPR },
	{ "~",			TT_OPR,		OPR_NOT,	TF_1OPR },      // NB: currently done postfix
	{ "max",        TT_OPR,		OPR_MAX,    TF_2OPR },
	{ "min",        TT_OPR,		OPR_MIN,    TF_2OPR },
	{ "sizeof",		TT_OPR,		OPR_SIZEOF },
	{ "#",			TT_OPR,		OPR_CONCAT },
	{ ":",			TT_OPR,		OPR_LABEL },
	{ "(",			TT_OPR,		OPR_OPEN },
	{ ")",			TT_OPR,		OPR_CLOSE },
	
	{ "<iter>",		TT_ITER,	0 },
	
	{ 0,			TT_EOL,     0 }
};
