; VExtension.asm

.CODE

get_rdx_register PROC
  mov qword ptr [rcx],rdx
  ret
get_rdx_register ENDP

set_rdx_register PROC
  mov rdx,qword ptr [rcx]
  ret
set_rdx_register ENDP

END
