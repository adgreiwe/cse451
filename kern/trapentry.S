/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */

TRAPHANDLER_NOEC(i_t_0, T_DIVIDE);
TRAPHANDLER_NOEC(i_t_1, T_DEBUG);
TRAPHANDLER_NOEC(i_t_2, T_NMI);
TRAPHANDLER_NOEC(i_t_3, T_BRKPT);
TRAPHANDLER_NOEC(i_t_4, T_OFLOW);
TRAPHANDLER_NOEC(i_t_5, T_BOUND);
TRAPHANDLER_NOEC(i_t_6, T_ILLOP);
TRAPHANDLER_NOEC(i_t_7, T_DEVICE);
TRAPHANDLER(i_t_8, T_DBLFLT);
TRAPHANDLER(i_t_10, T_TSS);
TRAPHANDLER(i_t_11, T_SEGNP);
TRAPHANDLER(i_t_12, T_STACK);
TRAPHANDLER(i_t_13, T_GPFLT);
TRAPHANDLER(i_t_14, T_PGFLT);
TRAPHANDLER_NOEC(i_t_16, T_FPERR);
TRAPHANDLER_NOEC(i_t_17, T_ALIGN);
TRAPHANDLER_NOEC(i_t_18, T_MCHK);
TRAPHANDLER_NOEC(i_t_19, T_SIMDERR);

/*
 * Lab 3: Your code here for _alltraps
 */

_alltraps:
	push %ds;	
	push %es;
	pushal;
	movl $GD_KD, %eax;
	movw %ax, %ds;
	movw %ax, %es;
	pushl %esp;
	call trap;
