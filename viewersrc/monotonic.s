        AREA |C$$code|, CODE, READONLY

        EXPORT OS_ReadMonotonicTime
OS_ReadMonotonicTime
        SWI 0x20042
        MOV PC,R14
;
     END
