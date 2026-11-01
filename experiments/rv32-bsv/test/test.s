	.file	"tmp.c"
	.option nopic
	.attribute arch, "rv32i2p1"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.section	.text.startup,"ax",@progbits
	.align	2
	.globl	main
	.type	main, @function
main:
.L2:
	j	.L2
	.size	main, .-main
	.globl	csmith_sink_
	.section	.sbss,"aw",@nobits
	.align	2
	.type	csmith_sink_, @object
	.size	csmith_sink_, 4
csmith_sink_:
	.zero	4
	.ident	"GCC: (g1b306039a) 15.1.0"
	.section	.note.GNU-stack,"",@progbits
