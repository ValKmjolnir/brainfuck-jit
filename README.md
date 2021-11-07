# Brainfuck

## Introduction

Brainfuck is a very interesting programming language that has only 8 operators:

|Operator|Code in C/C++|
|:----|:----|
|`+`|`buff[ptr]++`|
|`-`|`buff[ptr]--`|
|`>`|`ptr++`|
|`<`|`ptr--`|
|`[`|`if(!buff[ptr]) goto ']'`|
|`]`|`if(buff[ptr] goto '['`|
|`,`|`buff[ptr]=getchar()`|
|`.`|`putchar(buff[ptr])`|

This simple syntax makes brainfuck a great language for me to learn how to build an interpreter and jit(just-in-time) compiler.

## Interpreter

This project has a simple interpreter for brainfuck,
using switch-threading:

```C++
for(size_t i=0;i<code.size();++i){
    switch(code[i].op){
        case op_add:buf[p]+=code[i].num;break;
        case op_sub:buf[p]-=code[i].num;break;
        case op_addp:p+=code[i].num;break;
        case op_subp:p-=code[i].num;break;
        case op_jt:if(buf[p])i=code[i].num;break;
        case op_jf:if(!buf[p])i=code[i].num;break;
        case op_in:buf[p]=getchar();break;
        case op_out:putchar(buf[p]);break;
    }
}
```

To optimize the efficiency of the interpreter,
i count consecutive operators instead of just translating operators into an opcode.
You will see the structure of opcode in `jit.cpp`.

For example:

|bf code|opcode|
|:----|:----|
|`+++`|`buf[p]+=3`|
|`----`|`buf[p]-=4`|
|`>>>>>`|`p+=5`|
|`<<`|`p-=2`|

## Just-In-Time Compiler

__Caution__: this compiler only works on x86_64(amd64) system V(linux/macOS)

After generating opcodes,
it's quite easy for us to generate machine codes into a memory space allocated by `mmap`,
this memory space must be `read/write/exec` so we could execute the machine codes in this memory space.
You could see the `mmap` in `amd64jit::amd64jit(const size_t)` in file `amd64jit.h`.

I use the stack to be the paper of brainfuck machine(and `rbx` be the pointer),
and use memset to clean the stack space to zero-filled.

```C++
mem.push({0x48,0x81,0xec,0x00,0x00,0x02,0x00});// sub $0x20000,%rsp
mem.push({0x48,0x89,0xe3});// movq %rsp,%rbx

/* clear stack memory */
mem.push({0x48,0xb9});
mem.push64((uint64_t)memset);// movabs $memset,%rcx
mem.push({0x48,0x89,0xe7});// mov %rsp,%rdi
mem.push({0x31,0xf6});// xor %esi,%esi
mem.push({0xba,0x00,0x00,0x02,0x00});// mov $0x20000,%edx
mem.push({0xff,0xd1});// callq *%rcx
```

These four operators are not so difficult to translate to machine codes:

```C++
case op_add:
    mem.push({0x80,0x03,(uint8_t)(op.num&0xff)});// addb $op.num,(%rbx)
    break;
case op_sub:
    mem.push({0x80,0x2b,(uint8_t)(op.num&0xff)});// subb $op.num,(%rbx)
    break;
case op_addp:
    mem.push({0x48,0x81,0xc3});// add $op.num %rbx
    mem.push32(op.num);
    break;
case op_subp:
    mem.push({0x48,0x81,0xeb});// sub $op.num %rbx
    mem.push32(op.num);
    break;
```

And op_out uses the `putchar`,
write a demo and use objdump to see how the gcc and clang generate the machine code that calls the function,
then just copy them :)

The test file doesn't use `getchar` so i haven't wrote the machine code to call this function.

`je` and `jne` are two difficulties in this project.
You must calculate the distance of two jump labels to make sure they work correctly.

```C++
void amd64jit::je(){
    push({0x0f,0x84,0x00,0x00,0x00,0x00});// je
    stk.push(ptr);
}

void amd64jit::jne(){
    push({0x0f,0x85,0x00,0x00,0x00,0x00});// jne
    uint8_t* je_next=stk.top();stk.pop();
    uint8_t* jne_next= ptr;
    uint64_t p0=jne_next-je_next;
    uint64_t p1=je_next-jne_next;
    jne_next[-4]=(p1&0xff);
    jne_next[-3]=((p1>>8)&0xff);
    jne_next[-2]=((p1>>16)&0xff);
    jne_next[-1]=((p1>>24)&0xff);
    je_next[-4]=(p0&0xff);
    je_next[-3]=((p0>>8)&0xff);
    je_next[-2]=((p0>>16)&0xff);
    je_next[-1]=((p0>>24)&0xff);
}
```

Be careful that op_jf(`[`) uses the `je` and op_jt(`]`) uses the `jne`.

|bf code|opcode|machine code|
|:----|:----|:----|
|`+`|op_add|`addb $op.num,(%rbx)`|
|`-`|op_sub|`subb $op.num,(%rbx)`|
|`>`|op_addp|`add $op.num %rbx`|
|`<`|op_subp|`sub $op.num %rbx`|
|`[`|op_jf|`je label`|
|`]`|op_jt|`jne label`|
|`,`|op_in|`nop`|
|`.`|op_out|`callq *%rcx`|

Hope you enjoy it.
