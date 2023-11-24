/********************************************************
 *         Code emitting functions                      *
 *                                       2003. 3. 18.   *
 ********************************************************/

#define numberOfOpcodes 40           // 35 + 4 + 1

typedef enum {
     notop, neg,   incop, decop, dup,  swp, add,  sub,   mult, divop,
	 modop, andop, orop,  gt,    lt,   ge,  le,   eq,    ne,
	 lod,   ldc,   lda,   ldi,   ldp,  str, sti,  ujp,   tjp,  fjp,
	 call,  ret,   retv,  chkh,  chkl, nop, proc, endop, bgn,  sym,
	 none
} opcode;

char *mnemonic[numberOfOpcodes] = {
     "notop", "neg",  "inc", "dec",  "dup", "swp",  "add", "sub",
	 "mult",  "div",  "mod", "and",  "or",  "gt",   "lt",  "ge",
	 "le",    "eq",   "ne",	 "lod",  "ldc", "lda",  "ldi", "ldp",
	 "str",   "sti",  "ujp", "tjp",  "fjp", "call", "ret", "retv",
	 "chkh",  "chkl", "nop", "proc", "end", "bgn",  "sym", "none"
};

void emit0(opcode op)
{
	fprintf (ucodeFile, "           ");
	fprintf (ucodeFile, "%-10s\n", mnemonic[op]);
}

void emit1(opcode op, int num)
{
	fprintf (ucodeFile, "           ");
	fprintf (ucodeFile, "%-10s %5d\n", mnemonic[op], num);
}

void emit2(opcode op, int base, int offset)
{
  // ...
  // ... need to implemented !!
  // ...

}
void emit3(opcode op, int p1, int p2, int p3)
{
  // ...
  // ... need to implemented !!
  // ...

}

void emitLabel(char *label)
{
	int i, noBlanks;

	fprintf (ucodeFile, "%s", label);
	noBlanks = 12-strlen(label);
	for (i=1; i<noBlanks; ++i)
		fprintf (ucodeFile, " ");
	fprintf (ucodeFile, "%-10s\n", mnemonic[nop]);

}

void emitJump(opcode op, char *label)
{
  // ...
  // ... need to implemented !!
  // ...

}

void emitSym(int base, int offset, int size)
{
	fprintf (ucodeFile, "           ");
	fprintf (ucodeFile, "%-10s %5d %5d %5d\n", mnemonic[sym], base, offset, size);
}

void emitFunc(char *label, int base, int offset, int size)
{
	int i, noBlanks;

	fprintf (ucodeFile, "%s", label);
	noBlanks = 12-strlen(label);
	for (i=1; i<noBlanks; ++i)
		fprintf (ucodeFile, " ");
	fprintf (ucodeFile, "%-10s", mnemonic[proc]);
	fprintf (ucodeFile, " ");
	fprintf (ucodeFile, "%5d %5d %5d\n", base, offset, size);
}
