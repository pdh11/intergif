        AREA |C$$code|, CODE, READONLY

        EXPORT OS_ReadMonotonicTime
OS_ReadMonotonicTime
        SWI 0x20042
        MOVS PC,R14
;
     END
