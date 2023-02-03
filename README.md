# Brainfuck Just-In-Time Compiler

## __Introduction__

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

## __Brainfuck Interpreter__

This project has a simple interpreter for brainfuck,
using switch-threading:

```C++
for(size_t i=0;i<code.size();++i){
    switch(code[i].op){
        case op_add:buff[p]+=code[i].num;break;
        case op_sub:buff[p]-=code[i].num;break;
        case op_addp:p+=code[i].num;break;
        case op_subp:p-=code[i].num;break;
        case op_jt:if(buff[p])i=code[i].num;break;
        case op_jf:if(!buff[p])i=code[i].num;break;
        case op_in:buff[p]=getchar();break;
        case op_out:putchar(buff[p]);break;
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

## __Just-In-Time Compiler__

### __mmap__

After generating opcodes,
it's quite easy for us to generate machine codes into a memory space allocated by `mmap`.

This memory space must be `read/write/exec` so we could execute the machine codes in this memory space.
You could see the `mmap` in `amd64jit::amd64jit(const size_t)` in file `amd64jit.h`.

I use a global u8 array `buff[0x20000]` to be the paper of brainfuck machine(and `rbx` stores the pointer),
and remember to use memset to clean the stack space to zero-filled.

```C++
/* set bf machine's paper pointer */
mem.push({0x48,0xbb}).push64((uint64_t)buff); // movq $buff,%rbx
```

### __Add & Sub Operations__

These four operators are not so difficult to translate to machine codes:

```C++
case op_add: mem.push({0x80,0x03,(uint8_t)(op.num&0xff)}); break; // addb $op.num,(%rbx)
case op_sub: mem.push({0x80,0x2b,(uint8_t)(op.num&0xff)}); break; // subb $op.num,(%rbx)
case op_addp: mem.push({0x48,0x81,0xc3}).push32(op.num); break;   // add $op.num,%rbx
case op_subp: mem.push({0x48,0x81,0xeb}).push32(op.num); break;   // sub $op.num,%rbx
```

### __Library Function putchar & getchar__

And op_out uses the `putchar`,
write a demo and use objdump to see how the gcc and clang generate the machine code that calls the function,
then just copy them :)

The test file doesn't use `getchar` so i haven't wrote the machine code to call this function. (TODO)

```C++
#ifndef _WIN32
    mem.push({0x48,0xb8}).push64((uint64_t)putchar); // movabs $putchar,%rax
    mem.push({0x0f,0xbe,0x3b}); // movsbl (%rbx),%edi
    mem.push({0xff,0xd0}); // callq *%rax
#else
    mem.push({0x48,0xb8}).push64((uint64_t)putchar); // movabs $putchar,%rax
    mem.push({0x0f,0xbe,0x0b}); // movsbl (%rbx),%ecx
    mem.push({0xff,0xd0}); // callq *%rax
#endif
```

Ok, you may find that there's a small difference between generated machine code on Windows platform.
This is because the rule of parameter passing in __call convention__ of Windows is different from Linux/macOS/Unix.
And Linux/macOS/Unix use `rdi` to get the first parameter, but Windows uses `rcx`.

Although JIT-compiler developers should remember this rule,
it is quite easier to remember x86_64/amd64 call convention than x86_32...

### __Jump Operation__

`je` and `jne` are two difficulties in this project.
You must calculate the distance of two jump labels to make sure they work correctly.

```C++
amd64jit& amd64jit::je(){
    push({0x0f,0x84,0x00,0x00,0x00,0x00}); // je
    stk.push(ptr);
    return *this;
}

amd64jit& amd64jit::jne(){
    push({0x0f,0x85,0x00,0x00,0x00,0x00}); // jne
    uint8_t* je_next=stk.top();stk.pop();
    uint8_t* jne_next=ptr;
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
    return *this;
}
```

Be careful that op_jf(`[`) uses the `je` and op_jt(`]`) uses the `jne`.

### __Conclusion__

|bf code|opcode|machine code|
|:----|:----|:----|
|`+`|op_add|`addb $op.num,(%rbx)`|
|`-`|op_sub|`subb $op.num,(%rbx)`|
|`>`|op_addp|`add $op.num %rbx`|
|`<`|op_subp|`sub $op.num %rbx`|
|`[`|op_jf|`je label`|
|`]`|op_jt|`jne label`|
|`,`|op_in|`callq *%rax`|
|`.`|op_out|`callq *%rax`|

Hope you enjoy it.
