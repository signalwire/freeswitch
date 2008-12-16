/*
    File: exprpars.c
    Auth: Brian Allen Vanderburg II
    Date: Wednesday, April 30, 2003
    Desc: Actual parsing routines for this library

    This file is part of ExprEval.
*/

/* Includes */
#include "exprincl.h"

#include "exprpriv.h"
#include "exprmem.h"

/* Data structure used by parser */
typedef struct _exprToken {
	int type;					/* token type */
	int start;					/* token start position */
	int end;					/* token end position */

	union _tdata {
		char *str;				/* string data */
		EXPRTYPE val;			/* value data */
	} data;
} exprToken;

/* Defines for token types */
#define EXPR_TOKEN_UNKNOWN 0
#define EXPR_TOKEN_OPAREN 1
#define EXPR_TOKEN_CPAREN 2
#define EXPR_TOKEN_IDENTIFIER 3
#define EXPR_TOKEN_VALUE 4
#define EXPR_TOKEN_PLUS 5
#define EXPR_TOKEN_HYPHEN 6
#define EXPR_TOKEN_ASTERISK 7
#define EXPR_TOKEN_FSLASH 8
#define EXPR_TOKEN_AMPERSAND 9
#define EXPR_TOKEN_SEMICOLON 10
#define EXPR_TOKEN_COMMA 11
#define EXPR_TOKEN_EQUAL 12
#define EXPR_TOKEN_HAT 13

/* Internal functions */
int exprMultiParse(exprObj * obj, exprNode * node, exprToken * tokens, int count);
int exprInternalParse(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end);
int exprInternalParseAssign(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index);
int exprInternalParseAdd(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index);
int exprInternalParseSub(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index);
int exprInternalParseMul(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index);
int exprInternalParseDiv(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index);
int exprInternalParsePosNeg(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index);
int exprInternalParseExp(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index);
int exprInternalParseFunction(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int p1, int p2);
int exprInternalParseVarVal(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end);
int exprStringToTokenList(exprObj * obj, char *expr, exprToken ** tokens, int *count);
void exprFreeTokenList(exprToken * tokens, int count);

/* This frees a token list */
void exprFreeTokenList(exprToken * tokens, int count)
{
	int pos;

	if (tokens == NULL)
		return;

	for (pos = 0; pos < count; pos++) {
		if (tokens[pos].type == EXPR_TOKEN_IDENTIFIER)
			exprFreeMem(tokens[pos].data.str);
	}

	exprFreeMem(tokens);
}

/* This converts an expression string to a token list */
int exprStringToTokenList(exprObj * obj, char *expr, exprToken ** tokens, int *count)
{
	int found;
	exprToken *list;
	int pass;
	int pos, len;
	int tpos;
	int comment;				/* Is a comment active */
	int start, ilen;
	char buf[EXPR_MAXIDENTSIZE + 1];

	/* Set initial variables */
	found = 0;
	tpos = 0;
	list = NULL;
	comment = 0;
	*tokens = NULL;
	*count = 0;


	/* Check string length */
	len = (int) strlen(expr);
	if (len == 0)
		return EXPR_ERROR_EMPTYEXPR;

	/* Two passes, one to count, one to tokenize */
	for (pass = 0; pass <= 1; pass++) {
		for (pos = 0; pos < len; pos++) {
			switch (expr[pos]) {
				/* Comment */
			case '#':
				{
					/* Only set it if a comment is not already active */
					if (!comment)
						comment = 1;

					break;
				}

				/* Newline characters turn off comments */
			case '\r':
			case '\n':
				{
					/* If a comment is active, unset it */
					if (comment)
						comment = 0;

					break;
				}

				/* Open parenthesis */
			case '(':
				{
					if (!comment) {
						if (pass == 0)
							found++;
						else {
							list[tpos].type = EXPR_TOKEN_OPAREN;
							list[tpos].start = pos;
							list[tpos].end = pos;
							tpos++;
						}
					}

					break;
				}

				/* Close parenthesis */
			case ')':
				{
					if (!comment) {
						if (pass == 0)
							found++;
						else {
							list[tpos].type = EXPR_TOKEN_CPAREN;
							list[tpos].start = pos;
							list[tpos].end = pos;
							tpos++;
						}
					}

					break;
				}

				/* Plus */
			case '+':
				{
					if (!comment) {
						if (pass == 0)
							found++;
						else {
							list[tpos].type = EXPR_TOKEN_PLUS;
							list[tpos].start = pos;
							list[tpos].end = pos;
							tpos++;
						}
					}

					break;
				}

				/* Hyphen */
			case '-':
				{
					if (!comment) {
						if (pass == 0)
							found++;
						else {
							list[tpos].type = EXPR_TOKEN_HYPHEN;
							list[tpos].start = pos;
							list[tpos].end = pos;
							tpos++;
						}
					}

					break;
				}

				/* Asterisk */
			case '*':
				{
					if (!comment) {
						if (pass == 0)
							found++;
						else {
							list[tpos].type = EXPR_TOKEN_ASTERISK;
							list[tpos].start = pos;
							list[tpos].end = pos;
							tpos++;
						}
					}

					break;
				}

				/* Forward slash */
			case '/':
				{
					if (!comment) {
						if (pass == 0)
							found++;
						else {
							list[tpos].type = EXPR_TOKEN_FSLASH;
							list[tpos].start = pos;
							list[tpos].end = pos;
							tpos++;
						}
					}

					break;
				}

				/* Hat */
			case '^':
				{
					if (!comment) {
						if (pass == 0)
							found++;
						else {
							list[tpos].type = EXPR_TOKEN_HAT;
							list[tpos].start = pos;
							list[tpos].end = pos;
							tpos++;
						}
					}

					break;
				}

				/* Ampersand */
			case '&':
				{
					if (!comment) {
						if (pass == 0)
							found++;
						else {
							list[tpos].type = EXPR_TOKEN_AMPERSAND;
							list[tpos].start = pos;
							list[tpos].end = pos;
							tpos++;
						}
					}

					break;
				}

				/* Semicolon */
			case ';':
				{
					if (!comment) {
						if (pass == 0)
							found++;
						else {
							list[tpos].type = EXPR_TOKEN_SEMICOLON;
							list[tpos].start = pos;
							list[tpos].end = pos;
							tpos++;
						}
					}

					break;
				}

				/* Comma */
			case ',':
				{
					if (!comment) {
						if (pass == 0)
							found++;
						else {
							list[tpos].type = EXPR_TOKEN_COMMA;
							list[tpos].start = pos;
							list[tpos].end = pos;
							tpos++;
						}
					}

					break;
				}

				/* Equal sign */
			case '=':
				{
					if (!comment) {
						if (pass == 0)
							found++;
						else {
							list[tpos].type = EXPR_TOKEN_EQUAL;
							list[tpos].start = pos;
							list[tpos].end = pos;
							tpos++;
						}
					}

					break;
				}

				/* Identifiers and values */
			default:
				{
					if (!comment) {
						if (expr[pos] == '.' || switch_isdigit(expr[pos])) {
							/* Value */
							start = pos;

							/* Find digits before a period */
							while (switch_isdigit(expr[pos]))
								pos++;

							/* Find a period */
							if (expr[pos] == '.')
								pos++;

							/* Find digits after a period */
							while (switch_isdigit(expr[pos]))
								pos++;

							/* pos is AFTER last item, back up */
							pos--;

							if (pass == 0)
								found++;
							else {
								ilen = pos - start + 1;

								/* Is the value to large */
								if (ilen > EXPR_MAXIDENTSIZE) {
									obj->starterr = start;
									obj->enderr = pos;
									exprFreeTokenList(list, found);
									return EXPR_ERROR_BADIDENTIFIER;
								}

								/* Create value token */
								strncpy(buf, expr + start, ilen);
								buf[ilen] = '\0';

								list[tpos].type = EXPR_TOKEN_VALUE;
								list[tpos].start = start;
								list[tpos].end = pos;
								list[tpos].data.val = (EXPRTYPE) atof(buf);
								tpos++;
							}
						} else if (expr[pos] == '_' || switch_isalpha(expr[pos])) {
							/* Identifier */
							start = pos;

							/* Find rest of identifier */
							while (expr[pos] == '_' || switch_isalnum(expr[pos]))
								pos++;

							/* pos is AFTER last item, back up */
							pos--;

							if (pass == 0)
								found++;
							else {
								ilen = pos - start + 1;

								/* Is the value to large */
								if (ilen > EXPR_MAXIDENTSIZE) {
									obj->starterr = start;
									obj->enderr = pos;
									exprFreeTokenList(list, found);
									return EXPR_ERROR_BADIDENTIFIER;
								}

								/* Create value token */
								strncpy(buf, expr + start, ilen);
								buf[ilen] = '\0';

								/* Allocate memory for identifier */
								list[tpos].data.str = exprAllocMem(ilen + 1);
								if (list[tpos].data.str == NULL) {
									exprFreeTokenList(list, found);
									return EXPR_ERROR_MEMORY;
								}

								list[tpos].type = EXPR_TOKEN_IDENTIFIER;
								list[tpos].start = start;
								list[tpos].end = pos;
								strcpy(list[tpos].data.str, buf);
								tpos++;
							}
						} else if (switch_isspace(expr[pos])) {
							/* Spaces are ignored, do nothing */
						} else {
							/* Unknown */
							obj->starterr = obj->enderr = pos;
							exprFreeTokenList(list, found);
							return EXPR_ERROR_INVALIDCHAR;
						}
					}

					break;
				}
			}
		}

		/* If pass is 0, allocate memory for next pass */
		if (pass == 0) {
			/* First, make sure all comments were ended */
			if (comment)
				comment = 0;

			/* Make sure the expression is not empty */
			if (found == 0)
				return EXPR_ERROR_EMPTYEXPR;

			/* Allocate memory for token list */
			list = exprAllocMem(found * sizeof(exprToken));
			if (list == NULL)
				return EXPR_ERROR_MEMORY;

			tpos = 0;
		}
	}

	*count = found;
	*tokens = list;
	return EXPR_ERROR_NOERROR;
}


/* This is the main parsing routine */
int exprParse(exprObj * obj, char *expr)
{
	exprToken *tokens;
	int count;
	int err;
	exprNode *tmp;

	/* Make sure an object was passed */
	if (obj == NULL)
		return EXPR_ERROR_NULLPOINTER;

	/* Clear expression error position */
	obj->starterr = obj->enderr = -1;

	/* Have we already been parsed? */
	if (obj->parsedbad != 0)
		return EXPR_ERROR_ALREADYPARSEDBAD;

	if (obj->parsedgood != 0)
		return EXPR_ERROR_ALREADYPARSEDGOOD;

	/* Make sure an expression was passed */
	if (expr == NULL)
		return EXPR_ERROR_NULLPOINTER;

	/* Create token list */
	err = exprStringToTokenList(obj, expr, &tokens, &count);
	if (err != EXPR_ERROR_NOERROR)
		return err;

	/* Create head pointer */
	tmp = exprAllocNodes(1);
	if (tmp == NULL) {
		exprFreeTokenList(tokens, count);
		return EXPR_ERROR_MEMORY;
	}

	obj->headnode = tmp;

	/* Call the multiparse routine to parse subexpressions */
	err = exprMultiParse(obj, tmp, tokens, count);

	/* Free the token list */
	exprFreeTokenList(tokens, count);

	/* successful parse? */
	if (err == EXPR_ERROR_NOERROR) {
		obj->parsedgood = 1;
		obj->parsedbad = 0;
	} else {
		obj->parsedbad = 1;
		obj->parsedgood = 0;
	}

	return err;
}


/* Parse the subexpressions, each ending with semicolons */
int exprMultiParse(exprObj * obj, exprNode * node, exprToken * tokens, int count)
{
	int pos, plevel, last;
	int num, cur, err;
	exprNode *tmp;

	plevel = 0;
	num = 0;
	last = -1;

	/* First count the number of arguments */
	for (pos = 0; pos < count; pos++) {
		switch (tokens[pos].type) {
		case EXPR_TOKEN_OPAREN:
			/* increase plevel */
			plevel++;
			break;

		case EXPR_TOKEN_CPAREN:
			/* decrease plevel */
			plevel--;

			if (plevel < 0) {
				obj->starterr = tokens[pos].start;
				obj->enderr = tokens[pos].end;
				return EXPR_ERROR_UNMATCHEDPAREN;
			}

			break;

		case EXPR_TOKEN_SEMICOLON:
			if (plevel == 0) {
				if (last == pos - 1 || pos == 0) {
					/* last semicolon is before us or we are at the start */
					obj->starterr = tokens[pos].start;
					obj->enderr = tokens[pos].end;
					return EXPR_ERROR_SYNTAX;
				} else {
					/* last semicolon is not right before us */
					num++;
				}
			} else {
				/* Semicolon should not be in a parenthesis */
				obj->starterr = tokens[pos].start;
				obj->enderr = tokens[pos].end;
				return EXPR_ERROR_SYNTAX;
			}

			last = pos;			/* update position of last semicolon */
			break;
		}
	}

	/* plevel should be zero now */
	if (plevel != 0)
		return EXPR_ERROR_UNMATCHEDPAREN;

	/* the last character should be a semicolon */
	if (last != pos - 1)
		return EXPR_ERROR_MISSINGSEMICOLON;

	/* Now we know how many arguments there are */

	/* Allocate array of subnodes */
	tmp = exprAllocNodes(num);
	if (tmp == NULL)
		return EXPR_ERROR_MEMORY;

	/* Set the current node's data */
	node->type = EXPR_NODETYPE_MULTI;
	node->data.oper.nodes = tmp;
	node->data.oper.nodecount = num;

	/* now we parse each subexpression */
	last = 0;					/* Not for last semicolon, but for first char of subexpr */
	cur = 0;

	for (pos = 0; pos < count; pos++) {
		if (tokens[pos].type == EXPR_TOKEN_SEMICOLON) {
			/* Everything from last up to pos - 1 is a parameter */
			err = exprInternalParse(obj, &(tmp[cur]), tokens, last, pos - 1);
			if (err != EXPR_ERROR_NOERROR)
				return err;

			/* Update last position and current argument */
			last = pos + 1;
			cur++;
		}
	}

	return EXPR_ERROR_NOERROR;
}

/* This function parses each subnode and recurses if needed */
int exprInternalParse(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end)
{
	int pos;
	int plevel = 0;				/* Paren level */
	int fgopen = -1;			/* First paren group open index */
	int fgclose = -1;			/* First paren group close index */
	int assignindex = -1;		/* First = at plevel 0 for assignment */
	int addsubindex = -1;		/* Last + or - at plevel 0 for adding or subtracting */
	int muldivindex = -1;		/* Last * or / at plevel 0 for multiplying or dividing */
	int expindex = -1;			/* Last ^ fount at plevel 0 for exponents */
	int posnegindex = -1;		/* First +,- at plevel 0 for positive,negative */

	/* Make sure some conditions are right */
	if (start > end)
		return EXPR_ERROR_UNKNOWN;

	/* Scan the string for certain characters */
	for (pos = start; pos <= end; pos++) {
		switch (tokens[pos].type) {
		case EXPR_TOKEN_OPAREN:
			plevel++;

			/* First group open? */
			if (plevel == 1 && fgopen == -1)
				fgopen = pos;
			break;

		case EXPR_TOKEN_CPAREN:
			plevel--;

			/* First group close? */
			if (plevel == 0 && fgclose == -1)
				fgclose = pos;

			if (plevel < 0) {
				obj->starterr = tokens[pos].start;
				obj->enderr = tokens[pos].end;
				return EXPR_ERROR_UNMATCHEDPAREN;
			}
			break;

		case EXPR_TOKEN_EQUAL:
			/* Assignment found */
			if (plevel == 0) {
				if (assignindex == -1)
					assignindex = pos;
			}
			break;

		case EXPR_TOKEN_ASTERISK:
		case EXPR_TOKEN_FSLASH:
			/* Multiplication or division */
			if (plevel == 0)
				muldivindex = pos;
			break;

		case EXPR_TOKEN_HAT:
			/* Exponent */
			if (plevel == 0)
				expindex = pos;
			break;


		case EXPR_TOKEN_PLUS:
		case EXPR_TOKEN_HYPHEN:
			/* Addition or positive or subtraction or negative */
			if (plevel == 0) {
				if (pos == start) {
					/* At the start area, positive/negative */
					if (posnegindex == -1)
						posnegindex = pos;
				} else {
					/* Not at start, check item in front */
					switch (tokens[pos - 1].type) {
					case EXPR_TOKEN_EQUAL:	/* Equal sign */
					case EXPR_TOKEN_PLUS:	/* Add/positive sign */
					case EXPR_TOKEN_HYPHEN:	/* Subtract/negative sign */
					case EXPR_TOKEN_ASTERISK:	/* Multiply sign */
					case EXPR_TOKEN_FSLASH:	/* Divide sign */
					case EXPR_TOKEN_HAT:	/* Exponent sign */

						/* After theses, it is positive/negative */
						if (posnegindex == -1)
							posnegindex = pos;

						break;

					default:
						/* Otherwise it is addition/subtraction */
						addsubindex = pos;
						break;
					}
				}
			}
			break;

		}
	}

	/* plevel should now be zero */
	if (plevel != 0)
		return EXPR_ERROR_UNMATCHEDPAREN;

	/* We must parse the data in a certain order to maintain the
	   correct order of operators at evaluation time */

	/* First, take care of assignment */
	if (assignindex != -1)
		return exprInternalParseAssign(obj, node, tokens, start, end, assignindex);

	/* Addition or subtraction is next */
	if (addsubindex != -1) {
		if (tokens[addsubindex].type == EXPR_TOKEN_PLUS)
			return exprInternalParseAdd(obj, node, tokens, start, end, addsubindex);
		else
			return exprInternalParseSub(obj, node, tokens, start, end, addsubindex);
	}


	/* Multiplycation or division */
	if (muldivindex != -1) {
		if (tokens[muldivindex].type == EXPR_TOKEN_ASTERISK)
			return exprInternalParseMul(obj, node, tokens, start, end, muldivindex);
		else
			return exprInternalParseDiv(obj, node, tokens, start, end, muldivindex);
	}

	/* Exponent */
	if (expindex != -1)
		return exprInternalParseExp(obj, node, tokens, start, end, expindex);

	/* Negation */
	if (posnegindex != -1)
		return exprInternalParsePosNeg(obj, node, tokens, start, end, posnegindex);


	/* Grouped parenthesis */
	if (fgopen == start) {
		/* Closing paren. should be at the end */
		if (fgclose == end) {
			/* Anything between them */
			if (fgclose > fgopen + 1) {
				return exprInternalParse(obj, node, tokens, fgopen + 1, fgclose - 1);
			} else {
				/* Nothing between them */
				obj->starterr = tokens[fgopen].start;
				obj->enderr = tokens[fgclose].end;
				return EXPR_ERROR_SYNTAX;
			}
		} else					/* Closing paren not at the end */
			return EXPR_ERROR_SYNTAX;
	}

	/* Functions */
	if (fgopen > start) {
		/* Closing paren should be at end */
		if (fgclose == end) {
			return exprInternalParseFunction(obj, node, tokens, start, end, fgopen, fgclose);
		} else					/* Closing paren not at end */
			return EXPR_ERROR_SYNTAX;
	}

	/* If it was none of the above, it must be a variable or value */
	return exprInternalParseVarVal(obj, node, tokens, start, end);
}

/* Function to parse an assignment node */
int exprInternalParseAssign(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index)
{
	exprNode *tmp;
	exprValList *l;
	EXPRTYPE *addr;

	/* Make sure the equal sign is not at the start or end */
	if (index != start + 1 || index >= end) {
		obj->starterr = tokens[index].start;
		obj->enderr = tokens[index].end;
		return EXPR_ERROR_SYNTAX;
	}

	/* Make sure item before equal sign is an identifier */
	if (tokens[index - 1].type != EXPR_TOKEN_IDENTIFIER) {
		obj->starterr = tokens[index - 1].start;
		obj->enderr = tokens[index].end;
		return EXPR_ERROR_SYNTAX;
	}

	/* Create expression subnode */
	tmp = exprAllocNodes(1);
	if (tmp == NULL) {
		return EXPR_ERROR_MEMORY;
	}


	/* Set the data */
	node->type = EXPR_NODETYPE_ASSIGN;
	node->data.assign.node = tmp;


	/*
	   The fast access method directly accesses the memory address
	   of the variable's value at evaluation time.  Because of this,
	   we must make sure the variable does exists in the variable list.
	 */

	/* Make sure name is not a constant name */
	l = exprGetConstList(obj);
	if (l) {
		exprValListGetAddress(l, tokens[index - 1].data.str, &addr);
		if (addr) {
			obj->starterr = tokens[index - 1].start;
			obj->enderr = tokens[index].end;
			return EXPR_ERROR_CONSTANTASSIGN;
		}
	}

	/* Get the variable list */
	l = exprGetVarList(obj);
	if (l == NULL)
		return EXPR_ERROR_NOVARLIST;

	/* Get variable address if already in the list */
	exprValListGetAddress(l, tokens[index - 1].data.str, &addr);
	if (addr == NULL) {			/* Variable not in the list, add it */
		exprValListAdd(l, tokens[index - 1].data.str, 0.0);

		/* Try to get address again */
		exprValListGetAddress(l, tokens[index - 1].data.str, &addr);
		if (addr == NULL)		/* Could not add variable */
			return EXPR_ERROR_MEMORY;	/* Could not add variable to list */
	}

	node->data.assign.vaddr = addr;

	/* Parse the subnode */
	return exprInternalParse(obj, tmp, tokens, index + 1, end);
}

/* Function to parse an addition operator */
int exprInternalParseAdd(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index)
{
	exprNode *tmp;
	int err;

	/* Make sure plus sign is at a good place */
	if (index <= start || index >= end) {
		obj->starterr = tokens[index].start;
		obj->enderr = tokens[index].end;
		return EXPR_ERROR_SYNTAX;
	}

	/* Allocate space for 2 subnodes */
	tmp = exprAllocNodes(2);
	if (tmp == NULL)
		return EXPR_ERROR_MEMORY;


	/* Set the data */
	node->type = EXPR_NODETYPE_ADD;
	node->data.oper.nodes = tmp;
	node->data.oper.nodecount = 2;

	/* parse the left side */
	err = exprInternalParse(obj, &(tmp[0]), tokens, start, index - 1);
	if (err != EXPR_ERROR_NOERROR)
		return err;

	/* parse the right side */
	return exprInternalParse(obj, &(tmp[1]), tokens, index + 1, end);
}

/* Function to parse a subtraction operator */
int exprInternalParseSub(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index)
{
	exprNode *tmp;
	int err;

	/* Make sure minus sign is at a good place */
	if (index <= start || index >= end) {
		obj->starterr = tokens[index].start;
		obj->enderr = tokens[index].end;
		return EXPR_ERROR_SYNTAX;
	}

	/* Allocate space for 2 subnodes */
	tmp = exprAllocNodes(2);
	if (tmp == NULL)
		return EXPR_ERROR_MEMORY;


	/* Set the data */
	node->type = EXPR_NODETYPE_SUBTRACT;
	node->data.oper.nodes = tmp;
	node->data.oper.nodecount = 2;

	/* parse the left side */
	err = exprInternalParse(obj, &(tmp[0]), tokens, start, index - 1);
	if (err != EXPR_ERROR_NOERROR)
		return err;

	/* parse the right side */
	return exprInternalParse(obj, &(tmp[1]), tokens, index + 1, end);
}

/* Function to parse a multiplication operator */
int exprInternalParseMul(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index)
{
	exprNode *tmp;
	int err;

	/* Make sure times sign is at a good place */
	if (index <= start || index >= end) {
		obj->starterr = tokens[index].start;
		obj->enderr = tokens[index].end;
		return EXPR_ERROR_SYNTAX;
	}


	/* Allocate space for 2 subnodes */
	tmp = exprAllocNodes(2);
	if (tmp == NULL)
		return EXPR_ERROR_MEMORY;


	/* Set the data */
	node->type = EXPR_NODETYPE_MULTIPLY;
	node->data.oper.nodes = tmp;
	node->data.oper.nodecount = 2;

	/* parse the left side */
	err = exprInternalParse(obj, &(tmp[0]), tokens, start, index - 1);
	if (err != EXPR_ERROR_NOERROR)
		return err;

	/* parse the right side */
	return exprInternalParse(obj, &(tmp[1]), tokens, index + 1, end);
}

/* Function to parse a division operator */
int exprInternalParseDiv(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index)
{
	exprNode *tmp;
	int err;

	/* Make sure slash sign is at a good place */
	if (index <= start || index >= end) {
		obj->starterr = tokens[index].start;
		obj->enderr = tokens[index].end;
		return EXPR_ERROR_SYNTAX;
	}


	/* Allocate space for 2 subnodes */
	tmp = exprAllocNodes(2);
	if (tmp == NULL)
		return EXPR_ERROR_MEMORY;


	/* Set the data */
	node->type = EXPR_NODETYPE_DIVIDE;
	node->data.oper.nodes = tmp;
	node->data.oper.nodecount = 2;

	/* parse the left side */
	err = exprInternalParse(obj, &(tmp[0]), tokens, start, index - 1);
	if (err != EXPR_ERROR_NOERROR)
		return err;

	/* parse the right side */
	return exprInternalParse(obj, &(tmp[1]), tokens, index + 1, end);
}

/* Function to parse an exponent operator */
int exprInternalParseExp(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index)
{
	exprNode *tmp;
	int err;

	/* Make sure exponent sign is at a good place */
	if (index <= start || index >= end) {
		obj->starterr = tokens[index].start;
		obj->enderr = tokens[index].end;
		return EXPR_ERROR_SYNTAX;
	}


	/* Allocate space for 2 subnodes */
	tmp = exprAllocNodes(2);
	if (tmp == NULL)
		return EXPR_ERROR_MEMORY;


	/* Set the data */
	node->type = EXPR_NODETYPE_EXPONENT;
	node->data.oper.nodes = tmp;
	node->data.oper.nodecount = 2;

	/* parse the left side */
	err = exprInternalParse(obj, &(tmp[0]), tokens, start, index - 1);
	if (err != EXPR_ERROR_NOERROR)
		return err;

	/* parse the right side */
	return exprInternalParse(obj, &(tmp[1]), tokens, index + 1, end);
}

/* Function to parse for positive and negative */
int exprInternalParsePosNeg(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int index)
{
	exprNode *tmp;

	/* Position should be the same as start */
	if (index != start) {
		obj->starterr = tokens[index].start;
		obj->enderr = tokens[index].end;
		return EXPR_ERROR_UNKNOWN;
	}

	/* If it is a positive, just parse the internal of it */
	if (tokens[index].type == EXPR_TOKEN_PLUS)
		return exprInternalParse(obj, node, tokens, index + 1, end);
	else {
		/* Allocate subnode */
		tmp = exprAllocNodes(1);
		if (tmp == NULL)
			return EXPR_ERROR_NOERROR;


		/* Set data */
		node->type = EXPR_NODETYPE_NEGATE;
		node->data.oper.nodes = tmp;
		node->data.oper.nodecount = 1;

		/* Parse the subnode */
		return exprInternalParse(obj, tmp, tokens, index + 1, end);
	}
}

/* Function will parse a call to a function */
int exprInternalParseFunction(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end, int p1, int p2)
{
	int pos;
	int num, cur;
	int refnum, refcur;
	int plevel = 0;
	int lv, err;
	exprNode *tmp;
	exprFuncType fptr;
	int argmin, argmax;
	int refargmin, refargmax;
	int type;
	exprFuncList *l;
	exprValList *vars;
	EXPRTYPE *addr;
	EXPRTYPE **reftmp;

	/* We should have a function list */
	l = exprGetFuncList(obj);
	if (l == NULL)
		return EXPR_ERROR_NOSUCHFUNCTION;

	/* check paren. location */
	if (p2 <= p1)
		return EXPR_ERROR_SYNTAX;

	/* second paren. should not be after the end */
	if (p2 > end)
		return EXPR_ERROR_SYNTAX;

	/* Item before parenthesis should be an identifier */
	if (tokens[p1 - 1].type != EXPR_TOKEN_IDENTIFIER) {
		obj->starterr = tokens[p1 - 1].start;
		obj->enderr = tokens[p1].end;
		return EXPR_ERROR_SYNTAX;
	}


	/* Look up the function */
	err = exprFuncListGet(l, tokens[p1 - 1].data.str, &fptr, &type, &argmin, &argmax, &refargmin, &refargmax);
	if (err != EXPR_ERROR_NOERROR) {
		if (err == EXPR_ERROR_NOTFOUND) {
			obj->starterr = tokens[p1 - 1].start;
			obj->enderr = tokens[p1 - 1].end;
			return EXPR_ERROR_NOSUCHFUNCTION;
		} else
			return err;
	}

	/* Make sure the function exists */
	if (fptr == NULL && type == 0) {
		obj->starterr = tokens[p1 - 1].start;
		obj->enderr = tokens[p1 - 1].end;
		return EXPR_ERROR_NOSUCHFUNCTION;
	}

	/* Count arguments */
	if (p2 == p1 + 1) {
		num = 0;
		refnum = 0;
	} else {
		num = 1;
		refnum = 0;


		/* count commas */
		for (pos = p1 + 1; pos < p2; pos++) {
			switch (tokens[pos].type) {
			case EXPR_TOKEN_OPAREN:
				plevel++;
				break;

			case EXPR_TOKEN_CPAREN:
				plevel--;
				if (plevel < 0) {
					obj->starterr = tokens[pos].start;
					obj->enderr = tokens[pos].end;
					return EXPR_ERROR_UNMATCHEDPAREN;
				}
				break;

			case EXPR_TOKEN_COMMA:
				/* Found comma */
				if (plevel == 0)
					num++;
				break;

			case EXPR_TOKEN_AMPERSAND:
				/* Found reference mark */
				if (plevel == 0) {
					/* This may only occur after the open parenthesis or comma */
					if (tokens[pos - 1].type == EXPR_TOKEN_OPAREN || tokens[pos - 1].type == EXPR_TOKEN_COMMA)
						refnum++;
					else
						return EXPR_ERROR_SYNTAX;
				}
				break;
			}
		}

		/* plevel should be zero */
		if (plevel != 0)
			return EXPR_ERROR_UNMATCHEDPAREN;
	}

	/* We now have the number of total arguments and
	   number of ref arguments.  Get number of normal
	   arguments */
	num = num - refnum;

	/* Make sure number of arguments is correct */
	/* Here we make sure the limits are greater
	   or equal to zero because any negative number
	   could be used to specify no limit */
	if (argmin >= 0 && num < argmin) {
		obj->starterr = tokens[p1 - 1].start;
		obj->enderr = tokens[p2].end;
		return EXPR_ERROR_BADNUMBERARGUMENTS;
	}

	if (argmax >= 0 && num > argmax) {
		obj->starterr = tokens[p1 - 1].start;
		obj->enderr = tokens[p2].end;
		return EXPR_ERROR_BADNUMBERARGUMENTS;
	}

	if (refargmin >= 0 && refnum < refargmin) {
		obj->starterr = tokens[p1 - 1].start;
		obj->enderr = tokens[p2].end;
		return EXPR_ERROR_BADNUMBERARGUMENTS;
	}

	if (refargmax >= 0 && refnum > refargmax) {
		obj->starterr = tokens[p1 - 1].start;
		obj->enderr = tokens[p2].end;
		return EXPR_ERROR_BADNUMBERARGUMENTS;
	}

	/* Set tmp to null in case of no arguments */
	tmp = NULL;
	reftmp = NULL;

	if (num > 0) {
		/* Allocate subnodes */
		tmp = exprAllocNodes(num);
		if (tmp == NULL)
			return EXPR_ERROR_MEMORY;
	}

	if (refnum > 0) {
		/* Allocate ref pointers */
		reftmp = exprAllocMem(sizeof(EXPRTYPE *) * refnum);
		if (reftmp == NULL) {
			exprFreeMem(tmp);
			return EXPR_ERROR_MEMORY;
		}
	}



	/* Set this node's data */
	node->type = EXPR_NODETYPE_FUNCTION;
	node->data.function.fptr = fptr;
	node->data.function.nodecount = num;
	node->data.function.nodes = tmp;
	node->data.function.refcount = refnum;
	node->data.function.refs = reftmp;
	node->data.function.type = type;

	/* parse each subnode */
	if (num + refnum > 0) {
		plevel = 0;
		cur = 0;
		refcur = 0;
		lv = p1 + 1;

		/* look for commas if more than 1 arg */
		if (num + refnum > 1) {
			for (pos = p1 + 1; pos < p2; pos++) {
				switch (tokens[pos].type) {
				case EXPR_TOKEN_OPAREN:
					plevel++;
					break;

				case EXPR_TOKEN_CPAREN:
					plevel--;
					break;		/* Already checked paren nesting above */

				case EXPR_TOKEN_COMMA:
					/* Found comma */
					if (plevel == 0) {
						/* parse inside */
						if (tokens[lv].type == EXPR_TOKEN_AMPERSAND) {
							if (lv != pos - 2) {
								obj->starterr = tokens[lv].start;
								obj->enderr = tokens[pos].end;
								return EXPR_ERROR_SYNTAX;
							}

							/* It is a reference */
							if (tokens[lv + 1].type != EXPR_TOKEN_IDENTIFIER) {
								obj->starterr = tokens[lv].start;
								obj->enderr = tokens[lv + 1].end;
								return EXPR_ERROR_SYNTAX;
							}


							/* Make sure it is not a constant */
							vars = exprGetConstList(obj);
							if (vars) {
								exprValListGetAddress(vars, tokens[lv + 1].data.str, &addr);
								if (addr) {
									obj->starterr = tokens[lv].start;
									obj->enderr = tokens[lv + 1].start;
									return EXPR_ERROR_REFCONSTANT;
								}
							}

							/* Get variable list */
							vars = exprGetVarList(obj);
							if (vars == NULL)
								return EXPR_ERROR_NOVARLIST;

							/* Get variable address */
							exprValListGetAddress(vars, tokens[lv + 1].data.str, &addr);
							if (addr == NULL) {
								/* Add variable to list */
								exprValListAdd(vars, tokens[lv + 1].data.str, 0.0);

								/* Try to get address again */
								exprValListGetAddress(vars, tokens[lv + 1].data.str, &addr);
								if (addr == NULL)
									return EXPR_ERROR_MEMORY;	/* Could not add variable */
							}

							/* Set reference item */
							reftmp[refcur] = addr;

							/* increase ref arg number and lv position */
							refcur++;
							lv = pos + 1;
						} else {
							err = exprInternalParse(obj, &(tmp[cur]), tokens, lv, pos - 1);
							if (err != EXPR_ERROR_NOERROR)
								return err;

							/* increase arg number and lv position */
							lv = pos + 1;
							cur++;
						}
					}
					break;
				}
			}
		}

		/* lv should point after the last comma, or open paren. if only 1 arg */
		if (tokens[lv].type == EXPR_TOKEN_AMPERSAND) {
			if (lv != p2 - 2) {
				obj->starterr = tokens[lv].start;
				obj->enderr = tokens[p2].end;
				return EXPR_ERROR_SYNTAX;
			}

			/* It is a reference */
			if (tokens[lv + 1].type != EXPR_TOKEN_IDENTIFIER) {
				obj->starterr = tokens[lv].start;
				obj->enderr = tokens[lv + 1].end;
				return EXPR_ERROR_SYNTAX;
			}

			/* Make sure it is not a constant */
			vars = exprGetConstList(obj);
			if (vars) {
				exprValListGetAddress(vars, tokens[lv + 1].data.str, &addr);
				if (addr) {
					obj->starterr = tokens[lv].start;
					obj->enderr = tokens[lv + 1].start;
					return EXPR_ERROR_REFCONSTANT;
				}
			}

			/* Get variable list */
			vars = exprGetVarList(obj);
			if (vars == NULL)
				return EXPR_ERROR_NOVARLIST;

			/* Get variable address */
			exprValListGetAddress(vars, tokens[lv + 1].data.str, &addr);
			if (addr == NULL) {
				/* Add variable to list */
				exprValListAdd(vars, tokens[lv + 1].data.str, 0.0);

				/* Try to get address again */
				exprValListGetAddress(vars, tokens[lv + 1].data.str, &addr);
				if (addr == NULL)
					return EXPR_ERROR_MEMORY;	/* Could not add variable */
			}

			/* Set reference item */
			reftmp[refcur] = addr;
		} else {
			err = exprInternalParse(obj, &(tmp[cur]), tokens, lv, p2 - 1);
			if (err != EXPR_ERROR_NOERROR)
				return err;
		}
	}


	return EXPR_ERROR_NOERROR;
}

/* Parse a variable or value */
int exprInternalParseVarVal(exprObj * obj, exprNode * node, exprToken * tokens, int start, int end)
{
	exprValList *l;
	EXPRTYPE *addr;


	/* Make sure positions are correct */
	if (start != end) {
		return EXPR_ERROR_UNKNOWN;
	}


	/* Are we an identifier */
	if (tokens[start].type == EXPR_TOKEN_IDENTIFIER) {
		/* we are an identifier */

		/* check to see if it is a constant */
		l = exprGetConstList(obj);
		if (l != NULL) {
			if (exprValListGetAddress(l, tokens[start].data.str, &addr) == EXPR_ERROR_NOERROR) {
				/* We found it in the constant list */

				/*
				   Treat is like a variable node so application can change
				   constant value and it will reflect in expression
				 */

				node->type = EXPR_NODETYPE_VARIABLE;
				node->data.variable.vaddr = addr;
				return EXPR_ERROR_NOERROR;
			}
		}

		/* Not found in the constant list, so it must be a variable */

		/* Set node type */
		node->type = EXPR_NODETYPE_VARIABLE;

		/*
		   The fast access method directly accesses the memory address
		   of the variable's value at evaluation time.  Because of this,
		   we must make sure the variable does exists in the variable list.
		 */

		/* Get the variable list */
		l = exprGetVarList(obj);
		if (l == NULL)
			return EXPR_ERROR_NOVARLIST;

		/* Get variable address if already in the list */
		exprValListGetAddress(l, tokens[start].data.str, &addr);
		if (addr == NULL) {		/* Variable not in the list, add it */
			exprValListAdd(l, tokens[start].data.str, 0.0);

			/* Try to get address again */
			exprValListGetAddress(l, tokens[start].data.str, &addr);
			if (addr == NULL)	/* Could not add variable */
				return EXPR_ERROR_MEMORY;	/* Could not add variable to list */
		}

		node->data.variable.vaddr = addr;

		return EXPR_ERROR_NOERROR;
	} else if (tokens[start].type == EXPR_TOKEN_VALUE) {
		/* we are a value */
		node->type = EXPR_NODETYPE_VALUE;
		node->data.value.value = tokens[start].data.val;
		return EXPR_ERROR_NOERROR;
	} else {
		obj->starterr = tokens[start].start;
		obj->enderr = tokens[end].end;
		return EXPR_ERROR_UNKNOWN;
	}
}
