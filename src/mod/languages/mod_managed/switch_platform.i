// MANUALLY GENERATED
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned long in_addr_t;

// TODO: C# chars are 2 bytes, C is one. The marshalling copies two bytes (Value+whatever) into the C# char
//		causing corruption. We should figure this out. It's as simple as ((char)(byte)<thebyte>) whever
//		we define char as byte.
// TODO: Possible? It'd be nice to do the whole char*->IntPtr->Marshal/Free thing here instead of swigStringFix

%typemap(imtype, out="string") char **   "ref string"
%typemap(cstype, out="string") char **   "ref string"
%typemap(csin) char **     "ref $csinput"
%typemap(csvarin) char **
%{
  set { $imcall; }
%}
%typemap(csvarout) char **
%{
  get {
    return $imcall;
  }
%}

%newobject EventConsumer::pop;
%newobject Session;
%newobject CoreSession;
%newobject Event;
%newobject Stream;

#define SWITCH_DECLARE(type) type
#define SWITCH_DECLARE_NONSTD(type) type
#define SWITCH_MOD_DECLARE(type) type
#define SWITCH_MOD_DECLARE_NONSTD(type) type
#define SWITCH_DECLARE_DATA
#define SWITCH_MOD_DECLARE_DATA
#define SWITCH_THREAD_FUNC
#define SWITCH_DECLARE_CONSTRUCTOR SWITCH_DECLARE_DATA

#define _In_
#define _In_z_
#define _In_opt_z_
#define _In_opt_
#define _Printf_format_string_
#define _Ret_opt_z_
#define _Ret_z_
#define _Out_opt_
#define _Out_
#define _Check_return_
#define _Inout_
#define _Inout_opt_
#define _In_bytecount_(x)
#define _Out_opt_bytecapcount_(x)
#define _Out_bytecapcount_(x)
#define _Ret_
#define _Post_z_
#define _Out_cap_(x)
#define _Out_z_cap_(x)
#define _Out_ptrdiff_cap_(x)
#define _Out_opt_ptrdiff_cap_(x)
#define _Post_count_(x)
