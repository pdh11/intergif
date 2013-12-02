; Dynamic.s

R0 RN 0
r0 RN 0
R1 RN 1
r1 RN 1
R2 RN 2
r2 RN 2
R3 RN 3
r3 RN 3
R4 RN 4
r4 RN 4
R5 RN 5
r5 RN 5
R6 RN 6
r6 RN 6
R7 RN 7
r7 RN 7
R8 RN 8
r8 RN 8
R9 RN 9
r9 RN 9
R10 RN 10
r10 RN 10
R11 RN 11
r11 RN 11
R12 RN 12
r12 RN 12
SP  RN 13
sp  RN 13
R13 RN 13
r13 RN 13
LR  RN 14
lr  RN 14
R14 RN 14
r14 RN 14
PC  RN 15
pc  RN 15

        AREA |ASM$$code|, CODE, READONLY

;---------------------------------------------------------------------------
; DynamicArea_Alloc
;
;   os_error *DynamicArea_Alloc( int nSize, char *pAreaName,
;                                DynamicArea *pArea,
;                                void **ppStart );

            EXPORT DynamicArea_Alloc
DynamicArea_Alloc
            STMFD   R13!,{R2-R8,R14}        ;   8 words
            MOV     R8,R1                   ;   area name
            MOV     R7,#0
            MOV     R6,#0
            MOV     R5,#-1
            MOV     R4,#&80                 ;   green bar please
            MOV     R3,#-1
            MOV     R2,R0                   ;   initial size
            MOV     R1,#-1
            MOV     R0,#0                   ; create
            SWI     &20066                  ; XOS_DynamicArea
            LDMVSFD R13!,{R2-R8,PC}^
            LDR     R0,[R13,#4]             ; R3 passed in
            STR     R3,[R0]
            LDR     R0,[R13]                ; R2 passed in
            STR     R1,[R0]
            MOV     R0,#0
            LDMFD   R13!,{R2-R8,PC}^

;---------------------------------------------------------------------------
; DynamicArea_Realloc
;
;   os_error *DynamicArea_Realloc( DynamicArea area, int *pSize );
;
            EXPORT DynamicArea_Realloc
DynamicArea_Realloc
            STMFD   R13!,{R1,R14}
            LDR     R1,[R1]
            ;MOV    R0,R0
            SWI     &2002A                  ; XOS_ChangeDynamicArea
            LDMFD   R13!,{R2}               ; saved R1
            STR     R1,[R2]                 ; *not* STRVC
            MOVVC   R0,#0
            LDMFD   R13!,{PC}^

;---------------------------------------------------------------------------
; DynamicArea_Free
;
;   os_error *DynamicArea_Free( DynamicArea *pArea );
;
            EXPORT DynamicArea_Free
DynamicArea_Free
            STMFD   R13!,{R14}
            MOV     R12,R0
            LDR     R1,[R12]
            MOV     R0,#1
            SWI     &20066                  ; XOS_DynamicArea
            MOVVC   R0,#0
            STRVC   R0,[R12]
            LDMFD   R13!,{PC}^

;---------------------------------------------------------------------------
; MemoryPageSize
;
;   int MemoryPageSize(void)
;
            EXPORT MemoryPageSize
MemoryPageSize
            STMFD   R13!,{R14}
            SWI     &20051                  ; XOS_ReadMemMapInfo
            LDMFD   R13!,{PC}^

;
            END
