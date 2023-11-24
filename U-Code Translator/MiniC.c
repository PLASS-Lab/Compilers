#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

FILE *astFile;                          // AST file
FILE *sourceFile;                       // miniC source program
FILE *ucodeFile;                        // ucode file

#include "src\Scanner.c"
#include "src\Parser.c"
#include "src\sdt.c"
#include "src\EmitCode.c"
#include "src\SymTab.c"

void codeGen(Node *ptr);
void processDeclaration(Node *ptr);
void processFuncHeader(Node *ptr);
void processFunction(Node *ptr);
void icg_error(int n);
void processSimpleVariable(Node *ptr, int typeSpecifier, int typeQualifier);
void processArrayVariable(Node *ptr, int typeSpecifier, int typeQualifier);
void processStatement(Node *ptr);
void processOperator(Node *ptr);
void processCondition(Node *ptr);
void rv_emit(Node *ptr);
void genLabel(char *label);
int checkPredefined(Node *ptr);

int labelCount = 0;
int returnWithValue, lvalue;

void main(int argc, char *argv[])
{
	char fileName[30];
	Node *root;

	printf(" *** start of Mini C Compiler\n");
	if (argc != 2) {
		icg_error(1);
		exit(1);
	}
	strcpy(fileName, argv[1]);
	printf("   * source file name: %s\n", fileName);

	if ((sourceFile = fopen(fileName, "r")) == NULL) {
		icg_error(2);
		exit(1);
	}
	astFile = fopen(strcat(strtok(fileName, "."), ".ast"), "w");
	ucodeFile = fopen(strcat(strtok(fileName, "."), ".uco"), "w");

	printf(" === start of Parser\n");
    root = parser();
	printTree(root, 0);
	printf(" === start of ICG\n");
	codeGen(root);
	printf(" *** end   of Mini C Compiler\n");
} // end of main

void codeGen(Node *ptr)
{
	Node *p;
	int globalSize;

	initSymbolTable();
	// first, process the declaration part
    for (p=ptr->son; p; p=p->brother) {
		if (p->token.number == DCL) processDeclaration(p->son);
		else if (p->token.number == FUNC_DEF) processFuncHeader(p->son);
		else icg_error(3);
	}

//	dumpSymbolTable();
	globalSize = offset-1;
//	printf("size of global variables = %d\n", globalSize);

	genSym(base);

	// second, process the function part
    for (p=ptr->son; p; p=p->brother)
		if (p->token.number == FUNC_DEF) processFunction(p);
//	if (!mainExist) warningmsg("main does not exist");

	// generate codes for start routine
	//          bgn    globalSize
	//			ldp
    //          call    main
	//          end
	emit1(bgn, globalSize);
	emit0(ldp);
	emitJump(call, "main");
	emit0(endop);
}

void icg_error(int n)
{
	printf("icg_error: %d\n", n);
	//3:printf("A Mini C Source file must be specified.!!!\n");
	//"error in DCL_SPEC"
	//"error in DCL_ITEM"
}

void processDeclaration(Node *ptr)
{
	int typeSpecifier, typeQualifier;
	Node *p, *q;

	if (ptr->token.number != DCL_SPEC)
		icg_error(4);

//	printf("processDeclaration\n");
	// 1. process DCL_SPEC
	typeSpecifier = INT_TYPE;		// default type
	typeQualifier = VAR_TYPE;
	p = ptr->son;
	while (p) {
		if (p->token.number == INT_NODE)    typeSpecifier = INT_TYPE;
		else if (p->token.number == CONST_NODE)  typeQualifier = CONST_TYPE;
		else { // AUTO, EXTERN, REGISTER, FLOAT, DOUBLE, SIGNED, UNSIGNED
			   printf("not yet implemented\n");
		       return;
		}
		p = p->brother;
	}

	// 2. process DCL_ITEM
	p = ptr->brother;
	if (p->token.number != DCL_ITEM)
		icg_error(5);

	while (p) {
		q = p->son;    // SIMPLE_VAR or ARRAY_VAR

		switch (q->token.number) {
			case SIMPLE_VAR: {		// simple variable
				processSimpleVariable(q, typeSpecifier, typeQualifier);
				break;
			}
			case ARRAY_VAR: {		// one dimensional array
				processArrayVariable(q, typeSpecifier, typeQualifier);
				break;
			}
			default: printf("error in SIMPLE_VAR or ARRAY_VAR\n");
				break;
		} // end switch
		p = p->brother;
	} // end while
}

void processSimpleVariable(Node *ptr, int typeSpecifier, int typeQualifier)
{
	int stIndex, width, initialValue;
	int sign = 1;
	Node *p = ptr->son;          // variable name(=> identifier)
	Node *q = ptr->brother;      // initial value part

	if (ptr->token.number != SIMPLE_VAR)
		printf("error in SIMPLE_VAR\n");

	if (typeQualifier == CONST_TYPE) {		// constant type
		if (q == NULL) {
 		     printf("%s must have a constant value\n", ptr->son->token.value.id);
			 return;
		}
		if (q->token.number == UNARY_MINUS) {
			sign = -1;
			q = q->son;
		}
		initialValue = sign * q->token.value.num;

		stIndex = insert(p->token.value.id, typeSpecifier, typeQualifier,
			             0/*base*/, 0/*offset*/, 0/*width*/, initialValue);
	} else {  // variable type
		width = 1;
		stIndex = insert(p->token.value.id, typeSpecifier, typeQualifier,
			             base, offset, width, 0);
		offset++;
	}
}

void processArrayVariable(Node *ptr, int typeSpecifier, int typeQualifier)
{
	int stIndex, size;
	Node *p = ptr->son;          // variable name(=> identifier)

	if (ptr->token.number != ARRAY_VAR) {
		printf("error in ARRAY_VAR\n");
		return;
	}
	if (p->brother == NULL)			// no size
		printf("array size must be specified\n");
	else size = p->brother->token.value.num;

	stIndex = insert(p->token.value.id, typeSpecifier, typeQualifier,
		             base, offset, size, 0);
	offset += size;
}

void processFuncHeader(Node *ptr)
{
	int noArguments, returnType;
	int stIndex;
	Node *p;

//	printf("processFuncHeader\n");
	if (ptr->token.number != FUNC_HEAD)
		printf("error in processFuncHeader\n");

	// 1. process the function return type
	p = ptr->son->son;
	while (p) {
		if (p->token.number == INT_NODE) returnType = INT_TYPE;
		  else if (p->token.number == VOID_NODE) returnType = VOID_TYPE;
		     else printf("invalid function return type\n");
		p = p->brother;
	}

	// 2. process formal parameters
	p = ptr->son->brother->brother;		// FORMAL_PARA
	p = p->son;							// PARAM_DCL

	noArguments = 0;
	while (p) {
		noArguments++;
		p = p->brother;
	}

	// 3. insert the function name
	stIndex = insert(ptr->son->brother->token.value.id, returnType, FUNC_TYPE,
		             1/*base*/, 0/*offset*/, noArguments/*width*/, 0/*initialValue*/);
//	if (!strcmp("main", functionName)) mainExist = 1;

}

void processFunction(Node *ptr)
{
	int paraType, noArguments;
	int typeSpecifier, returnType;
	int p1, p2, p3;
	int stIndex;
	Node *p, *q;
	char *functionName;
//	int i, j;

//	printf("processFunction\n");
	if (ptr->token.number != FUNC_DEF)
		printf("error in processFunction\n");

  // set symbol table for the function
	set();

	// 1. process formal parameters
	// ...
	// ... need to implemented !!
	// ...

	// 2. process the declaration part in function body
	p = ptr->son->brother;			// COMPOUND_ST
	p = p->son->son;				// DCL
	// ...
	// ... need to implemented !!
	// ...


	// 3. emit the function start code
		// fname       proc      p1 p2 p3
		// p1 = size of local variables + size of arguments
		// p2 = block number
		// p3 = lexical level
	p1 = offset-1;
	p2 = p3 = base;
	functionName = ptr->son->son->brother->token.value.id;
	emitFunc(functionName, p1, p2, p3);


//	dumpSymbolTable();
	genSym(base);

	// 4. process the statement part in function body
	p = ptr->son->brother->son->brother;	// STAT_LIST
	returnWithValue = 0;
	// ...
	// ... need to implemented !!
	// ...


	// 5. check if return type and return value
	stIndex = lookup(functionName);
	if (stIndex == -1) return;
	returnType = symbolTable[stIndex].typeSpecifier;
	if ((returnType == VOID_TYPE) && returnWithValue)
		printf("void return type must not return a value\n");
	if ((returnType == INT_TYPE) && !returnWithValue)
		printf("int return type must return a value\n");

	// 6. generate the ending codes
	if (!returnWithValue) emit0(ret);
	emit0(endop);

	// reset symbol table
	reset();
}

void processStatement(Node *ptr)
{
	Node *p;

	if (ptr == NULL) return;		// null statement

	switch (ptr->token.number) {
	case COMPOUND_ST:
		p = ptr->son->brother;		// STAT_LIST
		p = p->son;
		while (p) {
			processStatement(p);
			p = p->brother;
		}
		break;
	case EXP_ST:
		if (ptr->son != NULL) processOperator(ptr->son);
		break;
	case RETURN_ST:
		if (ptr->son != NULL) {
			returnWithValue = 1;
			p = ptr->son;
			if (p->noderep == nonterm)
				processOperator(p); // return value
			else rv_emit(p);
			emit0(retv);
		} else
			emit0(ret);
		break;
	case IF_ST:
		{
			char label[LABEL_SIZE];

			genLabel(label);
			processCondition(ptr->son);				// condition
			emitJump(fjp, label);
			processStatement(ptr->son->brother);	// true part
			emitLabel(label);
		}
		break;
	case IF_ELSE_ST:
		{
			char label1[LABEL_SIZE], label2[LABEL_SIZE];

			genLabel(label1);
			genLabel(label2);
			processCondition(ptr->son);				// condition
			emitJump(fjp, label1);
			processStatement(ptr->son->brother);	// true part
			emitJump(ujp, label2);
			emitLabel(label1);
			processStatement(ptr->son->brother->brother);	// false part
			emitLabel(label2);
		}
		break;
	case WHILE_ST:
		{
			char label1[LABEL_SIZE], label2[LABEL_SIZE];

			genLabel(label1);
			genLabel(label2);
			emitLabel(label1);
			processCondition(ptr->son);				// condition
			emitJump(fjp, label2);
			processStatement(ptr->son->brother);	// loop body
			emitJump(ujp, label1);
			emitLabel(label2);
		}
		break;
	default:
		printf("not yet implemented.\n");
		break;
	} //end switch
}

void genLabel(char *label)
{
	sprintf(label, "$$%d", labelCount++);
}

void processCondition(Node *ptr)
{
	if (ptr->noderep == nonterm) processOperator(ptr);
		else rv_emit(ptr);
}

void rv_emit(Node *ptr)
{
	int stIndex;

	if (ptr->token.number == tnumber)		// number
		emit1(ldc, ptr->token.value.num);
	else {									// identifier
		stIndex = lookup(ptr->token.value.id);
		if (stIndex == -1) return;
		if (symbolTable[stIndex].typeQualifier == CONST_TYPE)		// constant
			emit1(ldc, symbolTable[stIndex].initialValue);
		else if (symbolTable[stIndex].width > 1)					// array var
			emit2(lda, symbolTable[stIndex].base, symbolTable[stIndex].offset);
		else														// simple var
			emit2(lod, symbolTable[stIndex].base, symbolTable[stIndex].offset);
	}
}

void processOperator(Node *ptr)
{
	int stIndex;

	if (ptr->noderep == terminal) {
		printf("illegal expression\n");
		return;
	}

	switch (ptr->token.number) {
	case ASSIGN_OP:
	{
		Node *lhs = ptr->son, *rhs = ptr->son->brother;

		// generate instructions for left-hane side if INDEX node.
		if (lhs->noderep == nonterm) {		// index variable
			lvalue = 1;
			processOperator(lhs);
			lvalue = 0;
		}

		// generate instructions for right-hane side
		if (rhs->noderep == nonterm) processOperator(rhs);
			else rv_emit(rhs);

		// generate a store instruction
		if (lhs->noderep == terminal) {		// simple variable
			stIndex = lookup(lhs->token.value.id);
			if (stIndex == -1) {
				printf("undefined variable : %s\n", lhs->token.value.id);
				return;
			}
			emit2(str, symbolTable[stIndex].base, symbolTable[stIndex].offset);
		} else								// array variable
			emit0(sti);
		break;
	}
	case ADD_ASSIGN: case SUB_ASSIGN: case MUL_ASSIGN: case DIV_ASSIGN:
	case MOD_ASSIGN:
	{
		Node *lhs = ptr->son, *rhs = ptr->son->brother;
		int nodeNumber = ptr->token.number;

		ptr->token.number = ASSIGN_OP;
		if (lhs->noderep == nonterm) {	// code generation for left hand side
			lvalue = 1;
			processOperator(lhs);
			lvalue = 0;
		}
		ptr->token.number = nodeNumber;
		if (lhs->noderep == nonterm)	// code generation for repeating part
			processOperator(lhs);
			else rv_emit(lhs);
		if (rhs->noderep == nonterm) 	// code generation for right hand side
			processOperator(rhs);
			else rv_emit(rhs);

		switch (ptr->token.number) {
			case ADD_ASSIGN: emit0(add);	break;
			case SUB_ASSIGN: emit0(sub);	break;
			case MUL_ASSIGN: emit0(mult);	break;
			case DIV_ASSIGN: emit0(divop);	break;
			case MOD_ASSIGN: emit0(modop);	break;
		}
		if (lhs->noderep == terminal) {	// code generation for store code
			stIndex = lookup(lhs->token.value.id);
			if (stIndex == -1) {
				printf("undefined variable : %s\n", lhs->son->token.value.id);
				return;
			}
			emit2(str, symbolTable[stIndex].base, symbolTable[stIndex].offset);
		} else
			emit0(sti);
		break;
	}
/*
	// logical operators(new computation of and/or operators: 2001.10.21)
	case AND: case OR:
		{
			Node *lhs = ptr->son, *rhs = ptr->son->brother;
			char label[LABEL_SIZE];

			genLabel(label);

			if (lhs->noderep == nonterm) processOperator(lhs);
				else rv_emit(lhs);
			emit0(dup);

			if (ptr->token.number == AND) emitJump(fjp, label);
			else if (ptr->token.number == OR) emitJump(tjp, label);

			// pop the first operand and push the second operand
			emit1(popz, 15);	// index 15 => swReserved7(dummy)
			if (rhs->noderep == nonterm) processOperator(rhs);
				else rv_emit(rhs);

			emitLabel(label);
			break;
		}
*/
	// arithmetic operators
	case ADD: case SUB: case MUL: case DIV: case MOD:
	// relational operators
	case EQ:  case NE: case GT: case LT: case GE: case LE:
	// logical operators
	case LOGICAL_AND: case LOGICAL_OR:
		{
			Node *lhs = ptr->son, *rhs = ptr->son->brother;

			if (lhs->noderep == nonterm) processOperator(lhs);
				else rv_emit(lhs);
			if (rhs->noderep == nonterm) processOperator(rhs);
				else rv_emit(rhs);
			switch (ptr->token.number) {
				case ADD: emit0(add);	break;			// arithmetic operators
				case SUB: emit0(sub);	break;
				case MUL: emit0(mult);	break;
				case DIV: emit0(divop);	break;
				case MOD: emit0(modop);	break;
				case EQ:  emit0(eq);	break;			// relational operators
				case NE:  emit0(ne);	break;
				case GT:  emit0(gt);	break;
				case LT:  emit0(lt);	break;
				case GE:  emit0(ge);	break;
				case LE:  emit0(le);	break;
				case LOGICAL_AND: emit0(andop);	break;	// logical operators
				case LOGICAL_OR : emit0(orop);	break;
			}
			break;
		}
	// unary operators
	case UNARY_MINUS: case LOGICAL_NOT:
		{
			Node *p = ptr->son;

			if (p->noderep == nonterm) processOperator(p);
				else rv_emit(p);
			switch (ptr->token.number) {
				case UNARY_MINUS: emit0(neg);
							      break;
				case LOGICAL_NOT: emit0(notop);
							      break;
			}
		break;
		}
	// increment/decrement operators
	case PRE_INC: case PRE_DEC: case POST_INC: case POST_DEC:
		{
			Node *p = ptr->son;
			Node *q;
			int stIndex;
			int amount = 1;

			if (p->noderep == nonterm) processOperator(p);		// compute value
				else rv_emit(p);

			q = p;
			while (q->noderep != terminal) q = q->son;
			if (!q || (q->token.number != tident)) {
				printf("increment/decrement operators can not be applied in expression\n");
				break;
			}
			stIndex = lookup(q->token.value.id);
			if (stIndex == -1) return;

			switch (ptr->token.number) {
				case PRE_INC: emit0(incop);
//							  if (isOperation(ptr)) emit0(dup);
							  break;
				case PRE_DEC: emit0(decop);
//							  if (isOperation(ptr)) emit0(dup);
							  break;
				case POST_INC:
//							   if (isOperation(ptr)) emit0(dup);
							   emit0(incop);
							   break;
				case POST_DEC:
//							   if (isOperation(ptr)) emit0(dup);
							   emit0(decop);
							   break;
			}
			if (p->noderep == terminal) {
				stIndex = lookup(p->token.value.id);
				if (stIndex == -1) return;
				emit2(str, symbolTable[stIndex].base, symbolTable[stIndex].offset);
			} else if (p->token.number == INDEX) {				// compute index
				lvalue = 1;
				processOperator(p->son);
				lvalue = 1;
				emit0(swp);
				emit0(sti);
			}
			else printf("error in prefix/postfix operator\n");
		break;
		}
	case INDEX:
		{
			Node *indexExp = ptr->son->brother;

			if (indexExp->noderep == nonterm) processOperator(indexExp);
				else rv_emit(indexExp);
			stIndex = lookup(ptr->son->token.value.id);
			if (stIndex == -1) {
				printf("undefined variable : %s\n", ptr->son->token.value.id);
				return;
			}
			emit2(lda, symbolTable[stIndex].base, symbolTable[stIndex].offset);
			emit0(add);
			if (!lvalue) emit0(ldi);	// rvalue
			break;
		}
	case CALL:
	{
		Node *p = ptr->son;		// function name
		char *functionName;
		int stIndex;
		int noArguments;

		if (checkPredefined(p)) // library functions
			break;

		// handle for user function
		functionName = p->token.value.id;
		stIndex = lookup(functionName);
		if (stIndex == -1) break;			// undefined function !!!
		noArguments = symbolTable[stIndex].width;

		emit0(ldp);
		p = p->brother;			// ACTUAL_PARAM
		while (p) {				// processing actual arguments
			if (p->noderep == nonterm) processOperator(p);
				else rv_emit(p);
			noArguments--;
			p = p->brother;
		}
		if (noArguments > 0) printf("%s: too few actual arguments", functionName);
		if (noArguments < 0) printf("%s: too many actual arguments", functionName);
		emitJump(call, ptr->son->token.value.id);
		break;
	}
	} //end switch
}

int checkPredefined(Node *ptr)
{
	char *functionName;
	int stIndex;

	functionName = ptr->token.value.id;

	if (!strcmp(functionName, "read")) {	// read procedure : call by address
		stIndex = lookup(ptr->brother->token.value.id);
		if (stIndex == -1) return 1;
		emit0(ldp);
		emit2(lda, symbolTable[stIndex].base, symbolTable[stIndex].offset);
		emitJump(call, "read");
		return 1;
	}
	if (!strcmp(functionName, "write")) {	// write procedure
		emit0(ldp);
		if (ptr->brother->noderep == nonterm) processOperator(ptr->brother);
			else rv_emit(ptr->brother);
		emitJump(call, "write");
		return 1;
	}
	return 0;
}
