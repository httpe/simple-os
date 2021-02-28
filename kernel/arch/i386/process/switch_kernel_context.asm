; Source: xv6/swtch.S
; Context switch between kernel routine/stacks
;
;   void switch_kernel_context(struct context **old, struct context *new);
; 
; Save the current registers on the stack, creating
; a struct context, and save its address in *old.
; Switch stacks to new and pop previously-saved registers.

global switch_kernel_context
switch_kernel_context:

  mov eax, [esp + 4]; struct context **old
  mov edx, [esp + 8]; struct context *new

  ; Save old callee-saved registers
  push ebp
  push ebx
  push esi
  push edi

  ; Switch stacks
  mov [eax], esp; *old = esp, save pointer to old stack to *old
  mov esp, edx; esp = new, switch to new stack

  ; Load new callee-saved registers
  pop edi
  pop esi
  pop ebx
  pop ebp
  ret ; pop eip and jmp
