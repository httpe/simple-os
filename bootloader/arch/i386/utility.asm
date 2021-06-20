; move cx bytes from es:si to es:di
memcpy:
    pusha
    rep movsb
    popa
    ret

; compare cx bytes from es:si and es:di
; set comparison flags accordingly
memcmp:
    pusha
    ; repe: rep while equal
    repe cmpsb
    popa
    ret
    