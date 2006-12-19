/* Automatically generated.  Do not edit */
/* See the mkopcodeh.awk script for details */
#define OP_VRowid                               1
#define OP_VFilter                              2
#define OP_ContextPop                           3
#define OP_IntegrityCk                          4
#define OP_DropTrigger                          5
#define OP_DropIndex                            6
#define OP_IdxInsert                            7
#define OP_Delete                               8
#define OP_MoveGt                               9
#define OP_OpenEphemeral                       10
#define OP_VerifyCookie                        11
#define OP_Push                                12
#define OP_Dup                                 13
#define OP_Blob                                14
#define OP_FifoWrite                           15
#define OP_IdxGT                               17
#define OP_RowKey                              18
#define OP_IsUnique                            19
#define OP_SetNumColumns                       20
#define OP_Eq                                  69   /* same as TK_EQ       */
#define OP_VUpdate                             21
#define OP_Expire                              22
#define OP_IdxIsNull                           23
#define OP_NullRow                             24
#define OP_OpenPseudo                          25
#define OP_OpenWrite                           26
#define OP_OpenRead                            27
#define OP_Transaction                         28
#define OP_AutoCommit                          29
#define OP_Negative                            85   /* same as TK_UMINUS   */
#define OP_Pop                                 30
#define OP_Halt                                31
#define OP_Vacuum                              32
#define OP_IfMemNeg                            33
#define OP_RowData                             34
#define OP_NotExists                           35
#define OP_MoveLe                              36
#define OP_SetCookie                           37
#define OP_Variable                            38
#define OP_VNext                               39
#define OP_VDestroy                            40
#define OP_TableLock                           41
#define OP_MemMove                             42
#define OP_LoadAnalysis                        43
#define OP_IdxDelete                           44
#define OP_Sort                                45
#define OP_ResetCount                          46
#define OP_NotNull                             67   /* same as TK_NOTNULL  */
#define OP_Ge                                  73   /* same as TK_GE       */
#define OP_Remainder                           83   /* same as TK_REM      */
#define OP_Divide                              82   /* same as TK_SLASH    */
#define OP_Integer                             47
#define OP_AggStep                             48
#define OP_CreateIndex                         49
#define OP_NewRowid                            50
#define OP_MoveLt                              51
#define OP_Explain                             52
#define OP_And                                 62   /* same as TK_AND      */
#define OP_ShiftLeft                           77   /* same as TK_LSHIFT   */
#define OP_Real                               126   /* same as TK_FLOAT    */
#define OP_Return                              53
#define OP_MemLoad                             54
#define OP_IdxLT                               55
#define OP_Rewind                              56
#define OP_MakeIdxRec                          57
#define OP_Gt                                  70   /* same as TK_GT       */
#define OP_AddImm                              58
#define OP_Subtract                            80   /* same as TK_MINUS    */
#define OP_Null                                59
#define OP_VColumn                             60
#define OP_MemNull                             63
#define OP_MemIncr                             64
#define OP_Clear                               65
#define OP_IsNull                              66   /* same as TK_ISNULL   */
#define OP_If                                  74
#define OP_ToBlob                             140   /* same as TK_TO_BLOB  */
#define OP_RealAffinity                        86
#define OP_Callback                            89
#define OP_AggFinal                            90
#define OP_IfMemZero                           91
#define OP_Last                                92
#define OP_Rowid                               93
#define OP_Sequence                            94
#define OP_NotFound                            95
#define OP_MakeRecord                          96
#define OP_ToText                             139   /* same as TK_TO_TEXT  */
#define OP_BitAnd                              75   /* same as TK_BITAND   */
#define OP_Add                                 79   /* same as TK_PLUS     */
#define OP_HexBlob                            127   /* same as TK_BLOB     */
#define OP_String                              97
#define OP_Goto                                98
#define OP_VCreate                             99
#define OP_MemInt                             100
#define OP_IfMemPos                           101
#define OP_DropTable                          102
#define OP_IdxRowid                           103
#define OP_Insert                             104
#define OP_Column                             105
#define OP_Noop                               106
#define OP_Not                                 16   /* same as TK_NOT      */
#define OP_Le                                  71   /* same as TK_LE       */
#define OP_BitOr                               76   /* same as TK_BITOR    */
#define OP_Multiply                            81   /* same as TK_STAR     */
#define OP_String8                             88   /* same as TK_STRING   */
#define OP_VOpen                              107
#define OP_CreateTable                        108
#define OP_Found                              109
#define OP_Distinct                           110
#define OP_Close                              111
#define OP_Statement                          112
#define OP_IfNot                              113
#define OP_ToInt                              142   /* same as TK_TO_INT   */
#define OP_Pull                               114
#define OP_VBegin                             115
#define OP_MemMax                             116
#define OP_MemStore                           117
#define OP_Next                               118
#define OP_Prev                               119
#define OP_MoveGe                             120
#define OP_Lt                                  72   /* same as TK_LT       */
#define OP_Ne                                  68   /* same as TK_NE       */
#define OP_MustBeInt                          121
#define OP_ForceInt                           122
#define OP_ShiftRight                          78   /* same as TK_RSHIFT   */
#define OP_CollSeq                            123
#define OP_Gosub                              124
#define OP_ContextPush                        125
#define OP_FifoRead                           128
#define OP_ParseSchema                        129
#define OP_Destroy                            130
#define OP_IdxGE                              131
#define OP_ReadCookie                         132
#define OP_BitNot                              87   /* same as TK_BITNOT   */
#define OP_AbsValue                           133
#define OP_Or                                  61   /* same as TK_OR       */
#define OP_ToReal                             143   /* same as TK_TO_REAL  */
#define OP_ToNumeric                          141   /* same as TK_TO_NUMERIC*/
#define OP_Function                           134
#define OP_Concat                              84   /* same as TK_CONCAT   */
#define OP_Int64                              135

/* The following opcode values are never used */
#define OP_NotUsed_136                        136
#define OP_NotUsed_137                        137
#define OP_NotUsed_138                        138

/* Opcodes that are guaranteed to never push a value onto the stack
** contain a 1 their corresponding position of the following mask
** set.  See the opcodeNoPush() function in vdbeaux.c  */
#define NOPUSH_MASK_0 0x9fec
#define NOPUSH_MASK_1 0xfffb
#define NOPUSH_MASK_2 0x7bbb
#define NOPUSH_MASK_3 0x65a9
#define NOPUSH_MASK_4 0xffff
#define NOPUSH_MASK_5 0x9eef
#define NOPUSH_MASK_6 0xed6c
#define NOPUSH_MASK_7 0x3fff
#define NOPUSH_MASK_8 0xf80a
#define NOPUSH_MASK_9 0x0000
