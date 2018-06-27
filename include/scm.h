#pragma once
#ifndef __EDK2_SCMLIB_H__
#define __EDK2_SCMLIB_H__

typedef struct {
	UINTN  Arg0;
	UINTN  Arg1;
	UINTN  Arg2;
	UINTN  Arg3;
	UINTN  Arg4;
	UINTN  Arg5;
	UINTN  Arg6;
	UINTN  Arg7;
} ARM_SMC_ARGS;

extern VOID
ArmCallSmcHardCoded(
	VOID
);

#endif

#ifndef __QUALCOMM_SCM_H__
#define __QUALCOMM_SCM_H__

#define SCM_SVC_MILESTONE_32_64_ID      0x1
#define SCM_SVC_MILESTONE_CMD_ID        0xf

typedef struct {
	uint64_t el1_x0;
	uint64_t el1_x1;
	uint64_t el1_x2;
	uint64_t el1_x3;
	uint64_t el1_x4;
	uint64_t el1_x5;
	uint64_t el1_x6;
	uint64_t el1_x7;
	uint64_t el1_x8;
	uint64_t el1_elr;
} EL1_SYSTEM_PARAM;

enum
{
	SMC_PARAM_TYPE_VALUE = 0,
	SMC_PARAM_TYPE_BUFFER_READ,
	SMC_PARAM_TYPE_BUFFER_READWRITE,
	SMC_PARAM_TYPE_BUFFER_VALIDATION,
} SCM_ARG_TYPE;

#define SIP_SVC_CALLS                          0x02000000
#define MAKE_SIP_SCM_CMD(svc_id, cmd_id)       ((((svc_id << 8) | (cmd_id)) & 0xFFFF) | SIP_SVC_CALLS)
#define MAKE_SCM_VAR_ARGS(num_args, t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, ...) (\
						  (((t0) & 0xff) << 4) | \
						  (((t1) & 0xff) << 6) | \
						  (((t2) & 0xff) << 8) | \
						  (((t3) & 0xff) << 10) | \
						  (((t4) & 0xff) << 12) | \
						  (((t5) & 0xff) << 14) | \
						  (((t6) & 0xff) << 16) | \
						  (((t7) & 0xff) << 18) | \
						  (((t8) & 0xff) << 20) | \
						  (((t9) & 0xff) << 22) | \
						  (num_args & 0xffff))
#define MAKE_SCM_ARGS(...)                     MAKE_SCM_VAR_ARGS(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
#define BIT(x)  (1<<(x))
#define SCM_ATOMIC_BIT                         BIT(31)
#define SCM_MAX_ARG_LEN                        5
#define SCM_INDIR_MAX_LEN                      10

#define ROUNDUP(a, b) (((a) + ((b)-1)) & ~((b)-1))
#define ROUNDDOWN(a, b) ((a) & ~((b)-1))

/* SCM support as per ARM spec */
/*
* Structure to define the argument for scm call
* x0: is the command ID
* x1: Number of argument & type of arguments
*   : Type can be any of
*   : SMC_PARAM_TYPE_VALUE             0
*   : SMC_PARAM_TYPE_BUFFER_READ       1
*   : SMC_PARAM_TYPE_BUFFER_READWRITE  2
*   : SMC_PARAM_TYPE_BUFFER_VALIDATION 3
*   @Note: Number of argument count starts from X2.
* x2-x4: Arguments
* X5[10]: if the number of argument is more, an indirect
*       : list can be passed here.
*/
typedef struct {
	uint32_t x0;/* command ID details as per ARMv8 spec :
				0:7 command, 8:15 service id
				0x02000000: SIP calls
				30: SMC32 or SMC64
				31: Standard or fast calls*/
	uint32_t x1; /* # of args and attributes for buffers
				 * 0-3: arg #
				 * 4-5: type of arg1
				 * 6-7: type of arg2
				 * :
				 * :
				 * 20-21: type of arg8
				 * 22-23: type of arg9
				 */
	uint32_t x2; /* Param1 */
	uint32_t x3; /* Param2 */
	uint32_t x4; /* Param3 */
	uint32_t x5[10]; /* Indirect parameter list */
	uint32_t atomic; /* To indicate if its standard or fast call */
} scmcall_arg;

/* Return value for the SCM call:
* SCM call returns values in register if its less than
* 12 bytes, anything greater need to be input buffer + input len
* arguments
*/
typedef struct
{
	uint32_t x1;
	uint32_t x2;
	uint32_t x3;
} scmcall_ret;

#endif
