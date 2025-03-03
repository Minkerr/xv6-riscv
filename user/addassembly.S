.global main

.section .data
prompt:
    .string "Enter two numbers: "
read_error_msg:
    .string "Read error. Code: %d\n"
incorrect_input_msg:
    .string "Incorrect Input. Enter two numbers separated by a space.\n"
result_format:
    .string "%d\n"

.section .bss
.lcomm buf, 512
.lcomm num1, 4
.lcomm num2, 4

.section .text
main:
    addi sp, sp, -32
    sw ra, 28(sp)
    sw s0, 24(sp)
    addi s0, sp, 32
    la a0, prompt
    call printf

    li a0, 0 
    la a1, buf
    li a2, 511
    call read
    mv t0, a0
    blt t0, zero, read_error
    addi t1, t0, -1
    la t2, buf
    add t2, t2, t1
    sb zero, 0(t2)
    la t1, buf


search_loop:
    lb t2, 0(t1)
    li t3, ' '
    beq t2, t3, space_found
    beq t2, zero, no_space
    addi t1, t1, 1
    j search_loop


space_found:
    sb zero, 0(t1)
    addi t3, t1, 1
    la a0, buf
    call atoi
    sw a0, num1, t4
    mv a0, t3
    call atoi
    sw a0, num2, t4
    lw a0, num1
    lw a1, num2
    call add
    mv a1, a0
    la a0, result_format
    call printf
    li a0, 0
    j exit

read_error:
    la a0, read_error_msg
    mv a1, t0
    call printf
    li a0, -2
    j exit

no_space:
    la a0, incorrect_input_msg
    call printf
    li a0, -1

exit:
    lw ra, 28(sp)
    lw s0, 24(sp)
    addi sp, sp, 32
    ret