/* https://stackoverflow.com/questions/16721164/x86-linux-assembler-get-program-parameters-from-start */
	
.globl	_start

_start:	sub	$8, %esp
	mov	8(%esp), %ecx
	lea	12(%esp), %edx
	push	%edx
	push	%ecx
	call	main
	mov	%eax, %ebx
	mov	$1, %eax
	int	$0x80

