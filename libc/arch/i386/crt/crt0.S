.section .text

.global _start
_start:
	# Pop fake return address pushed by our own process loader
	popl %eax

	# Run the global constructors.
	call _init

	# Run main
	xor %eax, %eax
	call main

	# Terminate the process with the exit code.
	pushl %eax
	call exit
.size _start, . - _start