/*
 * Copyright (c) 1995  Colin Plumb.  All rights reserved.
 * For licensing and other legal details, see the file legal.c.
 */
#ifndef PPCASM_H
#define PPCASM_H
/*
 * A PowerPC assembler in the C preprocessor.
 * This assumes that ints are 32 bits, and uses them for the values.
 *
 * An assembly-language routine is simply an array of unsigned ints,
 * initialized with the macros defined here.
 *
 * In the PowerPC, a generic function pointer does *not* point to the
 * first word of code, but to a two (or possibly more) word "transition
 * vector."  The first word of the TV points to the function's code.
 * The second word is the function's TOC (Table Of Contents) pointer,
 * which is loaded into r2.  The function's global variables are
 * accessed via the TOC pointed to by r2.  TOC pointers are changed,
 * for example, when a dynamically linked library is called, so the
 * library can have private global variables.
 *
 * Saving r2 and reloading r2 each function call is a hassle that
 * I'd really rather avoid, since a lot of useful assembly language routines
 * can be written without global variables at all, so they don't need a TOC
 * pointer.  But I haven't figured out how to persuade CodeWarrior 7 to
 * generate an intra-TOC call to an array.  (CodeWarrior 8 supports
 * PowerPC asm, which obviates the need to do the cast-to-function-pointer
 * trick, which obviates the need for cross-TOC calls.)
 *
 * The basic PowerPC calling conventions for integers are:
 * r0  - scratch.  May be modified by function calls.
 * r1  - stack pointer.  Must be preserved across function calls.
 *       See IMPORTANT notes on stack frame format below.
 *       This must *ALWAYS*, at every instruction boundary, be 16-byte
 *       aligned and point to a valid stack frame.  If a procedure
 *       needs to create a stack frame, the recommended way is to do:
 *       stwu r1,-frame_size(r1)
 *       and on exit, recover with one of:
 *       addi r1,r1,frame_size,   OR
 *       lwz r1,0(r1)
 * r2  - TOC pointer.  Points to the current table of contents.
 *       Must be preserved across function calls.
 * r3  - First argument register and return value register.
 *       Arguments are passed in r3 through r10, and values returned in
 *       r3 through r6, as needed.  (Usually only r3 for single word.)
 * r4-r10 - More argument registers
 * r11 - Scratch, may be modified by function calls.
 *       On entry to indirect function calls, this points to the
 *       transition vector, and additional words may be loaded
 *       at offsets from it.  Some conventions use r12 instead.
 * r12 - Scratch, may be modified by function calls.
 * r13-r31 - Callee-save registers, may not be modified by function
 *       calls.
 * The LR, CTR and XER may be modified by function calls, as may the MQ
 * register, on those processors for which it is implemented.
 * CR fields 0, 1, 5, 6 and 7 are scratch and may be modified by function
 * calls.  CR fields 2, 3 and 4 must be preserved across function calls.
 *
 * Stack frame format - READ
 *
 * r1 points to a stack frame, which must *ALWAYS*, meaning after each and
 * every instruction, without excpetion, point to a valid 16-byte-aligned
 * stack frame, defined as follows:
 * - The 296 bytes below r1 (from -296(r1) to -1(r1)) are the so-called Red
 *   Zone reserved for leaf procedures, which may use it without allocating
 *   a stack frame and without decrementing r1.  The size comes from the room
 *   needed to store all the callee-save registers: 19 64-bit integer registers
 *   and 18 64-bit floating-point registers. (18+19)*8 = 296.  So any
 *   procedure can save all the registers it needs to save before creating
 *   a stack frame and moving r1.
 *   The bytes at -297(r1) and below may be used by interrupt and exception
 *   handlers *at any time*.  Anything placed there may disappear before
 *   the next instruction.
 *   The word at 0(r1) is the previous r1, and so on in a linked list.
 *   This is the minimum needed to be a valid stack frame, but some other
 *   offsets from r1 are preallocated by the calling procedure for the called
 *   procedure's use.  These are:
 *   Offset 0:  Link to previous stack frame - saved r1, if the called
 *              procedure alters it.
 *   Offset 4:  Saved CR, if the called procedure alters the callee-save
 *              fields.  There's no important reason to save it here,
 *              but the space is reserved and you might as well use it
 *              for its intended purpose unless you have good reason to
 *              do otherwise.  (This may help some debuggers.)
 *   Offset 8:  Saved LR, if the called procedure needs to save it for
 *              later function return.  Saving the LR here helps a debugger
 *              track the chain of return addresses on the stack.
 *              Note that a called procedure does not need to preserve the
 *              LR for it's caller's sake, but it uually wants to preserve
 *              the value for its own sake until it finishes and it's
 *              time to return.  At that point, this is usually loaded
 *              back into the LR and the branch accomplished with BLR.
 *              However, if you want to be preverse, you could load it
 *              into the CTR and use BCTR instead.
 *   Offset 12: Reserved to compiler.  I can't find what this is for.
 *   Offset 16: Reserved to compiler.  I can't find what this is for.
 *   Offset 20: Saved TOC pointer.  In a cross-TOC call, the old TOC (r2)
 *              is saved here before r2 is loaded with the new TOC value.
 *              Again, it's not important to use this slot for this, but
 *              you might as well.
 * Beginning at offset 24 is the argument area.  This area is at least 8 words
 * (32 bytes; I don't know what happens with 64 bits) long, and may be longer,
 * up to the length of the longest argument list in a function called by
 * the function which allocated this stack frame.  Generally, arguments
 * to functions are passed in registers, but if those functions notice
 * the address of the arguments being taken, the registers are stored
 * into the space reserved for them in this area and then used from memory.
 * Additional arguments that will not fit into registers are also stored
 * here.  Variadic functions (like printf) generally start by saving
 * all the integer argument registers from the "..." onwards to this space.
 * For that reason, the space must be large enough to store all the argument
 * registers, even if they're never used.
 * (It could probably be safely shrunk if you're not calling any variadic
 * functions, but be careful!)
 * 
 * Offsets above that are private to the calling function and shouldn't
 * be messed with.  Generally, what appears there is locals, then saved
 * registers.
 *
 *
 * The floating-point instruction set isn't implemented yet (I'm too
 * lazy, as I don't need it yet), but for when it is, the register
 * usage convention is:
 * FPSCR - Scratch, except for floating point exception enable fields,
 * which should only be modified by functions defined to do so.
 * fr0  - scratch
 * fr1  - first floating point parameter and return value, scratch
 * fr2  - second floating point parameter and return value (if needed), scratch
 * fr3  - third floating point parameter and return value (if needed), scratch
 * fr4  - fourth floating point parameter and return value (if needed), scratch
 * fr5-fr13 - More floating point argument registers, scratch
 * fr14-fr31 - Callee-save registers, may not be modified across a function call
 *
 * Complex values store the real part in the lower-numberd register of a pair.
 * When mixing floating-point and integer arguments, reserve space (one register
 * for single-precision, two for double-precision values) in the integer
 * argument list for the floating-point values.  Those integer registers
 * generally have undefined values, UNLESS there is no prototype for the call,
 * in which case they should contain a copy of the floating-point value's
 * bit pattern to cope with wierd software.
 * If the floating point arguments go past the end of the integer registers,
 * they are stored in the argument area as well as being passed in here.
 *
 * After the argument area comes the calling function's private storage.
 * Typically, there are locals, followed by saved GP rgisters, followed
 * by saved FP registers.
 *
 * Suggested instruction for allocating a stack frame:
 *        stwu r1,-frame_size(r1)
 * Suggested instructions for deallocating a stack frame:
 *        addi r1,r1,frame_size
 * or
 *        lwz r1,0(r1)
 * If frame_size is too big, you'll have to load the offset into a temp
 * register, but be sure that r1 is updated atomically.
 *
 *
 * Basic PowerPC instructions look like this:
 *
 *                      1 1 1 1 1 1 1 1 1 1 2 2 2 2 2 2 2 2 2 2 3 3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Opcode  | | | | | | | | | | | | | | | | | | | | | | | | | | |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Branch instructions look like this:
 *
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Opcode  |             Branch offset                     |A|L|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * The L, or LK, or Link bit indicates that the return address for the
 * branch should be copied to the link register (LR).
 * The A, or AA, or absolute address bit, indicates that the address
 * of the current instruction (NOTE: not next instruction!) should NOT
 * be added to the branch offset; it is relative to address 0.
 *
 * Conditional branches looks like this:
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Opcode  |    BO   |   BI    |      Branch offset        |A|L|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * The BI field specifies the condition bit of interest (from the CR).
 * The BO field specifies what's interesting.  You can branch on a
 * combination of a bit of the condition register and --ctr, the CTR
 * register.  Two bits encode the branch condition to use:
 *   BRANCH IF
 * 00--- = Bit BI is 0
 * 01--- = Bit BI is 1
 * 1z--- = don't care about bit BI (always true)
 *   AND
 * --00- = --ctr != 0
 * --01- = --ctr == 0
 * --1z- = don't decrement ctr (always true)
 * The last bit us used as a branch prediction bit.  If set, it reverses
 * the usual backward-branch-taken heuristic.
 *
 * y = branch prediction bit.  z = unused, must be 0
 * 0000y - branch if --ctr != 0 && BI == 0
 *         don't branch if --ctr == 0 || BI != 0
 * 0001y - branch if --ctr == 0 && BI == 0
 *         don't branch if --ctr != 0 || BI != 0
 * 001zy - branch if BI == 0
 *         don't branch if BI != 0
 * 0100y - branch if --ctr != 0 && BI != 0
 *         don't branch if --ctr == 0 || BI == 0
 * 0101y - branch if --ctr == 0 && BI != 0
 *         don't branch if --ctr != 0 || BI == 0
 * 011zy - branch if BI != 0
 *         don't branch if BI == 0
 * 1z00y - branch if --ctr != 0
 *         don't branch if --ctr == 0
 * 1z01y - branch if --ctr == 0
 *         don't branch if --ctr != 0
 * 1z1zz - branch always
 * If y is 1, the usual branch prediction (usually not taken, taken for
 * backwards branches with immediate offsets) is reversed.
 *
 * Instructions with 2 operands and a 16-bit immediate field look like this:
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Opcode  |     D   |    A    |    16-bit immediate value     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Now, there are three variations of note.  In some instructions, the 16-bit
 * value is sign-extended.  In others, it's zero-extended.  These are noted
 * below as "simm" (signed immediate) and "uimm", respectively.  Also, which
 * field is the destination and which is the source sometimes switches.
 * Sometimes it's d = a OP imm, and sometimes it's a = s OP imm.  In the
 * latter cases, the "d" field is referred to as "s" ("source" instead of
 * "destination".  These are logical and shift instructions.  (Store also
 * refers to the s register, but that's the source of the value to be stored.)
 * The assembly mnemonics, however, always lists the destination first,
 * swapping the order in the instruction if necessary.
 * Third, quite often, if r0 is specified for the source a, then the constant
 * value 0 is used instead.  Thus, r0 is of limited use - it can be used for
 * some things, but not all.
 *
 * Instructions with three register operands look like this:
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   Opcode  |     D   |    A    |    B    |     Subopcode     |C|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * For most of the instructions of interest the Opcode is 31 and the subopcode
 * determines what the instruction does.  For a few instructions (mostly loads
 * and stores), if the A field is 0, the constant 0 is used.  The "C"
 * bit (also known as the "RC" bit) controls whether or not the condition
 * codes are updated.  If it is set (indicated by a "." suffix on the official
 * PowerPC opcodes, and a "_" suffix on these macros), condition code register
 * field 0 (for integer instructions; field 1 for floating point) is updated
 * to reflect the result of the operation.
 * Some arithmetic instructions use the most significant bit of the subopcode
 * field as an overflow enable bit (o suffix).
 *
 * Then there are the rotate and mask instructions, which have 5 operands, and
 * fill the subopcode field with 2 more 5-bit fields.  See below for them.
 *
 * NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE
 * These macros fully parenthesize their arguments, but are not themselves
 * fully parenthesized.  They are intended to be used for initializer lists,
 * and if you want to do tricks with their numeric values, wrap them in
 * parentheses.
 */

#define PPC_MAJOR(x)	((x)<<26)	/* Major opcode (0..63) */
#define PPC_MINOR(x)	((x)<<1)	/* Minor opcode (0..1023) */
#define PPC_RC	1		/* Record carry (. suffix, represented as _) */
#define PPC_OE	1024		/* Overflow enable (o suffix) */
#define PPC_DEST(reg)	((reg)<<21)	/* Dest register field */
#define PPC_SRCA(reg)	((reg)<<16)	/* First source register field */
#define PPC_SRCB(reg)	((reg)<<11)	/* Second source register field */
#define PPC_AA	2	/* Branch is absolute, relative to address 0 */
#define PPC_LK	1	/* Branch with link (L suffix) */

/* Unconditional branch (dest is 26 bits, +/- 2^25 bytes) */
#define PPC_B(dest)	PPC_MAJOR(18)|(((dest)<<2) & 0x03fffffc)
#define PPC_BA(dest)	PPC_B(dest)|PPC_AA
#define PPC_BL(dest)	PPC_B(dest)|PPC_LK
#define PPC_BLA(dest)	PPC_B(dest)|PPC_AA|PPC_LK

/* Three-operand instructions */
#define PPC_TYPE31(minor,d,a,b)	\
	PPC_MAJOR(31)|PPC_DEST(d)|PPC_SRCA(a)|PPC_SRCB(b)|PPC_MINOR(minor)
#define PPC_ADD(d,a,b)  	PPC_TYPE31(266,d,a,b)
#define PPC_ADD_(d,a,b) 	PPC_TYPE31(266,d,a,b)|PPC_RC
#define PPC_ADDO(d,a,b) 	PPC_TYPE31(266,d,a,b)|PPC_OE
#define PPC_ADDO_(d,a,b)	PPC_TYPE31(266,d,a,b)|PPC_OE|PPC_RC
#define PPC_ADDC(d,a,b) 	PPC_TYPE31(10,d,a,b)
#define PPC_ADDC_(d,a,b)	PPC_TYPE31(10,d,a,b)|PPC_RC
#define PPC_ADDCO(d,a,b)	PPC_TYPE31(10,d,a,b)|PPC_OE
#define PPC_ADDCO_(d,a,b)	PPC_TYPE31(10,d,a,b)|PPC_OE|PPC_RC
#define PPC_ADDE(d,a,b) 	PPC_TYPE31(138,d,a,b)
#define PPC_ADDE_(d,a,b)	PPC_TYPE31(138,d,a,b)|PPC_RC
#define PPC_ADDEO(d,a,b)	PPC_TYPE31(138,d,a,b)|PPC_OE
#define PPC_ADDEO_(d,a,b)	PPC_TYPE31(138,d,a,b)|PPC_OE|PPC_RC
#define PPC_ADDME(d,a)  	PPC_TYPE31(234,d,a,0)
#define PPC_ADDME_(d,a) 	PPC_TYPE31(234,d,a,0)|PPC_RC
#define PPC_ADDMEO(d,a) 	PPC_TYPE31(234,d,a,0)|PPC_OE
#define PPC_ADDMEO_(d,a)	PPC_TYPE31(234,d,a,0)|PPC_OE|PPC_RC
#define PPC_ADDZE(d,a)  	PPC_TYPE31(202,d,a,0)
#define PPC_ADDZE_(d,a) 	PPC_TYPE31(202,d,a,0)|PPC_RC
#define PPC_ADDZEO(d,a) 	PPC_TYPE31(202,d,a,0)|PPC_OE
#define PPC_ADDZEO_(d,a)	PPC_TYPE31(202,d,a,0)|PPC_OE|PPC_RC
#define PPC_AND(a,s,b)  	PPC_TYPE31(28,s,a,b)
#define PPC_AND_(a,s,b) 	PPC_TYPE31(28,s,a,b)|PPC_RC
#define PPC_ANDC(a,s,b) 	PPC_TYPE31(60,s,a,b)
#define PPC_ANDC_(a,s,b)	PPC_TYPE31(60,s,a,b)|PPC_RC
#define PPC_CMP(cr,a,b) 	PPC_TYPE31(0,(cr)<<2,a,b)
#define PPC_CMPL(cr,a,b)	PPC_TYPE31(32,(cr)<<2,a,b)
#define PPC_CNTLZW(a,s) 	PPC_TYPE31(26,s,a,0)
#define PPC_CNTLZW_(a,s)	PPC_TYPE31(26,s,a,0)|PPC_RC
#define PPC_DCBF(a,b)   	PPC_TYPE31(86,0,a,b)
#define PPC_DCBI(a,b)   	PPC_TYPE31(470,0,a,b)
#define PPC_DCBST(a,b)  	PPC_TYPE31(54,0,a,b)
#define PPC_DCBT(a,b)   	PPC_TYPE31(278,0,a,b)
#define PPC_DCBTST(a,b) 	PPC_TYPE31(246,0,a,b)
#define PPC_DCBZ(a,b)   	PPC_TYPE31(1014,0,a,b)
#define PPC_DIVW(d,a,b) 	PPC_TYPE31(491,d,a,b)
#define PPC_DIVW_(d,a,b)	PPC_TYPE31(491,d,a,b)|PPC_RC
#define PPC_DIVWO(d,a,b)	PPC_TYPE31(491,d,a,b)|PPC_OE
#define PPC_DIVWO_(d,a,b)	PPC_TYPE31(491,d,a,b)|PPC_OE|PPC_RC
#define PPC_DIVWU(d,a,b)	PPC_TYPE31(459,d,a,b)
#define PPC_DIVWU_(d,a,b)	PPC_TYPE31(459,d,a,b)|PPC_RC
#define PPC_DIVWUO(d,a,b)	PPC_TYPE31(459,d,a,b)|PPC_OE
#define PPC_DIVWUO_(d,a,b)	PPC_TYPE31(459,d,a,b)|PPC_OE|PPC_RC
#define PPC_EIEIO()     	PPC_TYPE31(854,0,0,0)
#define PPC_EQV(a,s,b)  	PPC_TYPE31(284,s,a,b)
#define PPC_EQV_(a,s,b) 	PPC_TYPE31(284,s,a,b)|PPC_RC
#define PPC_EXTSB(a,s,b)	PPC_TYPE31(954,s,a,b)
#define PPC_EXTSB_(a,s,b)	PPC_TYPE31(954,s,a,b)|PPC_RC
#define PPC_EXTSH(a,s,b)	PPC_TYPE31(922,s,a,b)
#define PPC_EXTSH_(a,s,b)	PPC_TYPE31(922,s,a,b)|PPC_RC
#define PPC_ICBI(a,b)   	PPC_TYPE31(982,0,a,b)
#define PPC_ISYNC()     	PPC_TYPE31(150,0,0,0)
#define PPC_LBZUX(d,a,b)	PPC_TYPE31(119,d,a,b)
#define PPC_LBZX(d,a,b) 	PPC_TYPE31(87,d,a,b)
#define PPC_LHAUX(d,a,b)	PPC_TYPE31(375,d,a,b)
#define PPC_LHAX(d,a,b) 	PPC_TYPE31(343,d,a,b)
#define PPC_LHBRX(d,a,b)	PPC_TYPE31(790,d,a,b)
#define PPC_LHZUX(d,a,b)	PPC_TYPE31(311,d,a,b)
#define PPC_LHZX(d,a,b) 	PPC_TYPE31(279,d,a,b)
#define PPC_LSWI(d,a,nb)	PPC_TYPE31(597,d,a,nb)
#define PPC_LSWX(d,a,b) 	PPC_TYPE31(533,d,a,b)
#define PPC_LSARX(d,a,b) 	PPC_TYPE31(20,d,a,b)
#define PPC_LSBRX(d,a,b) 	PPC_TYPE31(534,d,a,b)
#define PPC_MCRXR(crd)  	PPC_TYPE31(512,(crd)<<2,0,0)
#define PPC_MFCR(d)     	PPC_TYPE31(19,d,0,0)
#define PPC_MFSPR(d,spr)     	PPC_TYPE31(339,d,(spr)&31,(spr)>>5)
#define PPC_MFTB(d)     	PPC_TYPE31(371,d,12,8)
#define PPC_MFTBU(d)     	PPC_TYPE31(371,d,13,8)
#define PPC_MTCRF(mask,s)     	PPC_TYPE31(144,s,0,(mask)&0xff)
#define PPC_MTSPR(s,spr)     	PPC_TYPE31(467,s,(spr)&31,(spr)>>5)
#define PPC_MULHW(d,a,b) 	PPC_TYPE31(75,d,a,b)
#define PPC_MULHW_(d,a,b) 	PPC_TYPE31(75,d,a,b)|PPC_RC
#define PPC_MULHWU(d,a,b) 	PPC_TYPE31(11,d,a,b)
#define PPC_MULHWU_(d,a,b) 	PPC_TYPE31(11,d,a,b)|PPC_RC
#define PPC_MULLW(d,a,b) 	PPC_TYPE31(235,d,a,b)
#define PPC_MULLW_(d,a,b) 	PPC_TYPE31(235,d,a,b)|PPC_RC
#define PPC_MULLWO(d,a,b) 	PPC_TYPE31(235,d,a,b)|PPC_OE
#define PPC_MULLWO_(d,a,b) 	PPC_TYPE31(235,d,a,b)|PPC_OE|PPC_RC
#define PPC_NAND(a,s,b)  	PPC_TYPE31(476,s,a,b)
#define PPC_NAND_(a,s,b) 	PPC_TYPE31(476,s,a,b)|PPC_RC
#define PPC_NEG(d,a)    	PPC_TYPE31(104,d,a,b)
#define PPC_NEG_(d,a)    	PPC_TYPE31(104,d,a,b)|PPC_RC
#define PPC_NEGO(d,a)    	PPC_TYPE31(104,d,a,b)|PPC_OE
#define PPC_NEGO_(d,a)    	PPC_TYPE31(104,d,a,b)|PPC_OE|PPC_RC
#define PPC_NOR(a,s,b)  	PPC_TYPE31(124,s,a,b)
#define PPC_NOR_(a,s,b) 	PPC_TYPE31(124,s,a,b)|PPC_RC
#define PPC_OR(a,s,b)   	PPC_TYPE31(444,s,a,b)
#define PPC_OR_(a,s,b)  	PPC_TYPE31(444,s,a,b)|PPC_RC
#define PPC_ORC(a,s,b)   	PPC_TYPE31(412,s,a,b)
#define PPC_ORC_(a,s,b)  	PPC_TYPE31(412,s,a,b)|PPC_RC
#define PPC_SLW(a,s,b)   	PPC_TYPE31(24,s,a,b)
#define PPC_SLW_(a,s,b)  	PPC_TYPE31(24,s,a,b)|PPC_RC
#define PPC_SRAW(a,s,b)   	PPC_TYPE31(792,s,a,b)
#define PPC_SRAW_(a,s,b)  	PPC_TYPE31(792,s,a,b)|PPC_RC
#define PPC_SRAWI(a,s,sh)   	PPC_TYPE31(824,s,a,sh)
#define PPC_SRAWI_(a,s,sh)  	PPC_TYPE31(824,s,a,sh)|PPC_RC
#define PPC_SRW(a,s,b)   	PPC_TYPE31(536,s,a,b)
#define PPC_SRW_(a,s,b)  	PPC_TYPE31(536,s,a,b)|PPC_RC
#define PPC_STBUX(s,a,b)   	PPC_TYPE31(247,s,a,b)
#define PPC_STBX(s,a,b)   	PPC_TYPE31(215,s,a,b)
#define PPC_STHBRX(s,a,b)   	PPC_TYPE31(918,s,a,b)
#define PPC_STHUX(s,a,b)   	PPC_TYPE31(439,s,a,b)
#define PPC_STHX(s,a,b)   	PPC_TYPE31(407,s,a,b)
#define PPC_STSWI(s,a,nb)   	PPC_TYPE31(725,s,a,nb)
#define PPC_STSWX(s,a,b)   	PPC_TYPE31(661,s,a,b)
#define PPC_STWBRX(s,a,b)   	PPC_TYPE31(662,s,a,b)
#define PPC_STWCX_(s,a,b)   	PPC_TYPE31(150,s,a,b)|PPC_RC
#define PPC_STWUX(s,a,b)   	PPC_TYPE31(183,s,a,b)
#define PPC_STWX(s,a,b)   	PPC_TYPE31(151,s,a,b)
#define PPC_SUBF(d,a,b) 	PPC_TYPE31(40,d,a,b)
#define PPC_SUBF_(d,a,b) 	PPC_TYPE31(40,d,a,b)|PPC_RC
#define PPC_SUBFO(d,a,b) 	PPC_TYPE31(40,d,a,b)|PPC_OE
#define PPC_SUBFO_(d,a,b) 	PPC_TYPE31(40,d,a,b)|PPC_OE|PPC_RC
#define PPC_SUB(d,b,a)		PPC_SUBF(d,a,b)
#define PPC_SUB_(d,b,a)		PPC_SUBF_(d,a,b)
#define PPC_SUBO(d,b,a)		PPC_SUBFO(d,a,b)
#define PPC_SUBO_(d,b,a)	PPC_SUBFO_(d,a,b)
#define PPC_SUBFC(d,a,b) 	PPC_TYPE31(8,d,a,b)
#define PPC_SUBFC_(d,a,b) 	PPC_TYPE31(8,d,a,b)|PPC_RC
#define PPC_SUBFCO(d,a,b) 	PPC_TYPE31(8,d,a,b)|PPC_OE
#define PPC_SUBFCO_(d,a,b) 	PPC_TYPE31(8,d,a,b)|PPC_OE|PPC_RC
#define PPC_SUBFE(d,a,b) 	PPC_TYPE31(136,d,a,b)
#define PPC_SUBFE_(d,a,b) 	PPC_TYPE31(136,d,a,b)|PPC_RC
#define PPC_SUBFEO(d,a,b) 	PPC_TYPE31(136,d,a,b)|PPC_OE
#define PPC_SUBFEO_(d,a,b) 	PPC_TYPE31(136,d,a,b)|PPC_OE|PPC_RC
#define PPC_SUBFME(d,a) 	PPC_TYPE31(232,d,a,0)
#define PPC_SUBFME_(d,a) 	PPC_TYPE31(232,d,a,0)|PPC_RC
#define PPC_SUBFMEO(d,a) 	PPC_TYPE31(232,d,a,0)|PPC_OE
#define PPC_SUBFMEO_(d,a) 	PPC_TYPE31(232,d,a,0)|PPC_OE|PPC_RC
#define PPC_SUBFZE(d,a) 	PPC_TYPE31(200,d,a,0)
#define PPC_SUBFZE_(d,a) 	PPC_TYPE31(200,d,a,0)|PPC_RC
#define PPC_SUBFZEO(d,a) 	PPC_TYPE31(200,d,a,0)|PPC_OE
#define PPC_SUBFZEO_(d,a) 	PPC_TYPE31(200,d,a,0)|PPC_OE|PPC_RC
#define PPC_SYNC()		PPC_TYPE31(598,0,0,0)
#define PPC_TW(to,a,b)   	PPC_TYPE31(4,to,a,b)
#define PPC_XOR(a,s,b)   	PPC_TYPE31(316,s,a,b)	

/* Immediate-operand instructions.  Take a 16-bit immediate operand */
#define PPC_IMM(major,d,a,imm) \
	PPC_MAJOR(major)|PPC_DEST(d)|PPC_SRCA(a)|((imm)&0xffff)
/* Trap word immediate */
#define PPV_TWI(to,a,simm)	PPC_IMM(3,to,a,simm)
/* Integer arithmetic */
#define PPC_MULLI(d,a,simm)	PPC_IMM(7,d,a,simm)
#define PPC_SUBFIC(s,a,simm)	PPC_IMM(8,s,a,simm)
#define PPC_CMPLI(cr,a,uimm)	PPC_IMM(10,(cr)<<2,a,uimm)
#define PPC_CMPI(cr,a,simm)	PPC_IMM(11,(cr)<<2,a,simm)
#define PPC_ADDIC(d,a,simm)	PPC_IMM(12,d,a,simm)
#define PPC_ADDIC_(d,a,simm)	PPC_IMM(13,d,a,simm)
#define PPC_ADDI(d,a,simm)	PPC_IMM(14,d,a,simm)
#define PPC_ADDIS(d,a,simm)	PPC_IMM(15,d,a,simm)

/* Conditional branch (dest is 16 bits, +/- 2^15 bytes) */
#define PPC_BC(bo,bi,dest)	PPC_IMM(16,bo,bi,((dest)<<2)&0xfffc)
#define PPC_BCA(bo,bi,dest)	PPC_BC(bo,bi,dest)|PPC_AA
#define PPC_BCL(bo,bi,dest)	PPC_BC(bo,bi,dest)|PPC_LK
#define PPC_BCLA(bo,bi,dest)	PPC_BC(bo,bi,dest)|PPC_AA|PPC_LK

/* Logical operations */
#define PPC_ORI(a,s,uimm)	PPC_IMM(24,s,a,uimm)
#define PPC_ORIS(a,s,uimm)	PPC_IMM(25,s,a,uimm)
#define PPC_XORI(a,s,uimm)	PPC_IMM(26,s,a,uimm)
#define PPC_XORIS(a,s,uimm)	PPC_IMM(27,s,a,uimm)
#define PPC_ANDI_(a,s,uimm)	PPC_IMM(28,s,a,uimm)
#define PPC_ANDIS(a,s,uimm)	PPC_IMM(29,s,a,uimm)

/* Load/store */
#define PPC_LWZ(d,a,simm)	PPC_IMM(32,d,a,simm)
#define PPC_LWZU(d,a,simm)	PPC_IMM(33,d,a,simm)
#define PPC_LBZ(d,a,simm)	PPC_IMM(34,d,a,simm)
#define PPC_LBZU(d,a,simm)	PPC_IMM(35,d,a,simm)
#define PPC_STW(s,a,simm)	PPC_IMM(36,s,a,simm)
#define PPC_STWU(s,a,simm)	PPC_IMM(37,s,a,simm)
#define PPC_STB(s,a,simm)	PPC_IMM(38,s,a,simm)
#define PPC_STBU(s,a,simm)	PPC_IMM(39,s,a,simm)
#define PPC_LHZ(d,a,simm)	PPC_IMM(40,d,a,simm)
#define PPC_LHZU(d,a,simm)	PPC_IMM(41,d,a,simm)
#define PPC_LHA(d,a,simm)	PPC_IMM(42,d,a,simm)
#define PPC_STH(s,a,simm)	PPC_IMM(44,s,a,simm)
#define PPC_STHU(s,a,simm)	PPC_IMM(45,s,a,simm)
#define PPC_LHAU(d,a,simm)	PPC_IMM(43,d,a,simm)
#define PPC_LMW(d,a,simm)	PPC_IMM(46,d,a,simm)
#define PPC_STMW(s,a,simm)	PPC_IMM(47,s,a,simm)

/* Major number = 19 - condition register operations.  d, a and b are CR bits */
#define PPC_TYPE19(minor,d,a,b) \
	PPC_MAJOR(19)|PPC_DEST(d)|PPC_SRCA(a)|PPC_SRCB(b)|PPC_MINOR(minor)
#define PPC_MCRF(d,s)   	PPC_TYPE19(0,(d)<<2,(s)<<2,0)
#define PPC_CRNOR(d,a,b)	PPC_TYPE19(33,d,a,b)
#define PPC_CRANDC(d,a,b)	PPC_TYPE19(129,d,a,b)
#define PPC_CRXOR(d,a,b)	PPC_TYPE19(193,d,a,b)
#define PPC_CRNAND(d,a,b)	PPC_TYPE19(225,d,a,b)
#define PPC_CRAND(d,a,b)	PPC_TYPE19(257,d,a,b)
#define PPC_CREQV(d,a,b)	PPC_TYPE19(289,d,a,b)
#define PPC_CRORC(d,a,b)	PPC_TYPE19(417,d,a,b)
#define PPC_CROR(d,a,b) 	PPC_TYPE19(449,d,a,b)

/* Indirect conditional branch */
#define PPC_BCLR(bo,bi) 	PPC_TYPE19(16,bo,bi,0)
#define PPC_BCLRL(bo,bi)	PPC_TYPE19(16,bo,bi,0)|PPC_LK
#define PPC_BCCTR(bo,bi)	PPC_TYPE19(528,bo,bi,0)
#define PPC_BCCTRL(bo,bi)	PPC_TYPE19(528,bo,bi,0)|PPC_LK
#define PPC_BLR()           	PPC_BCLR(20,31)
#define PPC_BCTR()           	PPC_BCCTR(20,31)

/* Other */
#define  PPC_RLWIMI(a,s,sh,mb,me) \
	PPC_MAJOR(20)|PPC_DEST(s)|PPC_SRCA(A)|PPC_SRCB(sh)|(mb)<<6|(me)<<1 
#define  PPC_RLWIMI_(a,s,sh,mb,me)	PPC_RLWIMI(a,s,sh,mb,me)|PPC_RC
#define  PPC_RLWINM(a,s,sh,mb,me) \
	PPC_MAJOR(21)|PPC_DEST(s)|PPC_SRCA(A)|PPC_SRCB(sh)|(mb)<<6|(me)<<1 
#define  PPC_RLWINM_(a,s,sh,mb,me)	PPC_RLWINM(a,s,sh,mb,me)|PPC_RC
#define  PPC_RLWNM(a,s,b,mb,me) \
 	PPC_MAJOR(23)|PPC_DEST(s)|PPC_SRCA(A)|PPC_SRCB(b)|(mb)<<6|(me)<<1 
#define  PPC_RLWNM_(a,s,b,mb,me)	PPC_RLWNM(a,s,b,mb,me)|PPC_RC

#define PPC_SC()			PPC_MAJOR(17)|2
/* Major number = 63 Floating-point operations (not implemented for now) */

/* Simplified Mnemonics */
/* Fabricate immediate subtract out of add negative */
#define PPC_SUBI(d,a,simm)	PPC_ADDI(d,a,-(simm))
#define PPC_SUBIS(d,a,simm)	PPC_ADDIS(d,a,-(simm))
#define PPC_SUBIC(d,a,simm)	PPC_ADDIC(d,a,-(simm))
#define PPC_SUBIC_(d,a,simm)	PPC_ADDIC_(d,a,-(simm))
/* Fabricate subtract out of subtract from */
#define PPC_SUBC(d,b,a)		PPC_SUBFC(d,a,b)
#define PPC_SUBC_(d,b,a)	PPC_SUBFC_(d,a,b)
#define PPC_SUBCO(d,b,a)	PPC_SUBFCO(d,a,b)
#define PPC_SUBCO_(d,b,a)	PPC_SUBFCO_(d,a,b)
/* Messy compare bits omitted */
/* Shift and rotate omitted */
/* Branch coding omitted */
#define PPC_CRSET(d)		PPC_CREQV(d,d,d)
#define PPC_CRCLR(d)		PPC_CRXOR(d,d,d)
#define PPC_CRMOVE(d,s)		PPC_CROR(d,s,s)
#define PPC_CRNOT(d,s)		PPC_CRNOR(d,s,s)
/* Trap menmonics omitted */
/* Menmonics for user-accessible SPRs */
#define PPC_MFXER(d)    	PPC_MFSPR(d,1)		
#define PPC_MFLR(d)     	PPC_MFSPR(d,8)		
#define PPC_MFCTR(d)    	PPC_MFSPR(d,9)		
#define PPC_MTXER(s)    	PPC_MTSPR(s,1)		
#define PPC_MTLR(s)     	PPC_MTSPR(s,8)		
#define PPC_MTCTR(s)    	PPC_MTSPR(s,9)		
/* Recommended mnemonics */
#define PPC_NOP()		PPC_ORI(0,0,0)
#define PPC_LI(d,simm)		PPC_ADDI(d,0,simm)
#define PPC_LIS(d,simm)		PPC_ADDIS(d,0,simm)
#define PPC_LA(d,a,simm)	PPC_ADDI(d,a,simm)
#define PPC_MR(d,s)		PPC_OR(d,s,s)
#define PPC_NOT(d,s)		PPC_NOR(d,s,s)
#define PPC_MTCR(s)		PPC_MTCRF(0xff,s)

#endif /* PPCASM_H */

/* 45678901234567890123456789012345678901234567890123456789012345678901234567 */
