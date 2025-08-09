# Brainfuck Just-In-Time Compiler

## __Introduction__

Brainfuck is a very interesting programming language that has only 8 operators:

|Operator|Code in C/C++|
|:----|:----|
|`+`|`buff[ptr]++`|
|`-`|`buff[ptr]--`|
|`>`|`ptr++`|
|`<`|`ptr--`|
|`[`|`if (!buff[ptr]) goto ']'`|
|`]`|`if (buff[ptr] goto '['`|
|`,`|`buff[ptr] = getchar()`|
|`.`|`putchar(buff[ptr])`|

This simple syntax makes brainfuck a great language for me to learn how to build an interpreter and __JIT(just-in-time)__ compiler.

## __Brainfuck Interpreter__

This project has a simple interpreter for brainfuck,
using switch-threading:

```C++
for (size_t i = 0; i < code.size(); ++i) {
    switch (code[i].op) {
        case op_add: buff[p] += code[i].num; break;
        case op_sub: buff[p] -= code[i].num; break;
        case op_addp: p += code[i].num; break;
        case op_subp: p -= code[i].num; break;
        case op_jt: if (buff[p]) i = code[i].num; break;
        case op_jf: if (!buff[p]) i = code[i].num; break;
        case op_in: buff[p] = getchar(); break;
        case op_out: putchar(buff[p]); break;
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
mem.push({0x48, 0xbb}).push64((uint64_t)buff); // movq $buff, %rbx
```

### __Add & Sub Operations__

These four operators are not so difficult to translate to machine codes:

```C++
case op_add:  mem.push({0x80, 0x03, (uint8_t)(op.num & 0xff)}); break; // addb $op.num, (%rbx)
case op_sub:  mem.push({0x80, 0x2b, (uint8_t)(op.num & 0xff)}); break; // subb $op.num, (%rbx)
case op_addp: mem.push({0x48, 0x81, 0xc3}).push32(op.num);    break; // add $op.num, %rbx
case op_subp: mem.push({0x48, 0x81, 0xeb}).push32(op.num);    break; // sub $op.num, %rbx
```

### __Library Function `putchar` & `getchar`__

#### __putchar__

```C++
int putchar(int);
```

`op_out` uses the `putchar`,
write a demo and use objdump to see how the gcc and clang generate the machine code that calls the function,
then just copy them :)

```C++
mem.push({0x48, 0xb8}).push64((uint64_t)putchar); // movabs $putchar, %rax
#ifndef _WIN32
mem.push({0x0f, 0xbe, 0x3b}); // movsbl (%rbx), %edi
#else
mem.push({0x0f, 0xbe, 0x0b}); // movsbl (%rbx), %ecx
#endif
mem.push({0xff, 0xd0}); // callq *%rax
```

You may find that there's a small difference between generated machine code on Windows platform.
This is because the rule of parameter passing in __call convention__ of Windows is different from Linux/macOS/Unix.
And Linux/macOS/Unix use `rdi` to get the first parameter, but Windows uses `rcx`.

Although JIT-compiler developers should remember this rule,
it is quite easier to remember x86_64/amd64 call convention than x86_32...

#### __getchar__

```C++
int getchar();
```

`op_in` uses the `getchar`,
also we just use the objdump to see how gcc/clang generate the code,
and just copy them :)

Luckily, on Windows/Linux/macOS/Unix platform, the return value `int` will all be stored in register `rax`. And we just need to mov the low 8-bits of `rax` to `rbx[0]` (aka `movsbl %al,(%rbx)`).

```C++
mem.push({0x48, 0xb8})
   .push64((uint64_t)getchar); // movabs $getchar, %rax
mem.push({0xff, 0xd0});        // callq *%rax
mem.push({0x88, 0x03});        // movsbl %al, (%rbx)
```

So we don't need to write `#ifndef _WIN32` and so on :)

### __Jump Operation__

`je` and `jne` are two difficulties in this project.
You must calculate the distance of two jump labels to make sure they work correctly.

```C++
amd64jit& amd64jit::je() {
    push({0x0f, 0x84}).push32(0x0); // je
    stk.push(ptr);
    return *this;
}

amd64jit& amd64jit::jne() {
    push({0x0f, 0x85}).push32(0x0); // jne
    uint8_t* je_next = stk.top();
    stk.pop();
    
    uint8_t* jne_next = ptr;
    uint64_t p0 = jne_next - je_next;
    uint64_t p1 = je_next - jne_next;
    jne_next[-4] = p1 & 0xff;
    jne_next[-3] = (p1 >> 8) & 0xff;
    jne_next[-2] = (p1 >> 16) & 0xff;
    jne_next[-1] = (p1 >> 24) & 0xff;
    je_next[-4] = p0 & 0xff;
    je_next[-3] = (p0 >> 8) & 0xff;
    je_next[-2] = (p0 >> 16) & 0xff;
    je_next[-1] = (p0 >> 24) & 0xff;
    return *this;
}
```

op_jf(`[`) uses the `je` and op_jt(`]`) uses the `jne`.

### __Conclusion__

|bf code|opcode|machine code|
|:----|:----|:----|
|`+`|op_add|`addb $op.num, (%rbx)`|
|`-`|op_sub|`subb $op.num, (%rbx)`|
|`>`|op_addp|`add $op.num, %rbx`|
|`<`|op_subp|`sub $op.num, %rbx`|
|`[`|op_jf|`je label`|
|`]`|op_jt|`jne label`|
|`,`|op_in|`callq *%rax` & `movsbl %al, (%rbx)`|
|`.`|op_out|`callq *%rax`|

## __Simple Optimization__

Here's a simple pattern that could be optimized:

```bf
[-]
```

Or:

```bf
[+]
```

This means we should set `buff[p]` to zero.
So we could add another opcode `op_setz`:

|bf code|opcode|machine code|
|:----|:----|:----|
|`[-]`|op_setz|`movb $0, (%rbx)`|
|`[+]`|op_setz|`movb $0, (%rbx)`|

```c++
// movb $0, (%rbx)
case op_setz: mem.push({0xc6, 0x03, 0x00}); break;
```

## Advanced Optimization

Not implemented here, but need to mention.

```bf
-<++>-
```

Could be optimized to:

```bf
--<++>
```

Which is called `canonicalization`. This needs the compiler to have the ability to analyze memory/variable access patterns in bf.

And more possible patterns are waiting to be optimized:

```bf
[-<+>]
[-<<->>]
[->>+<<]
[->++>>>+++++>++>+<<<<<<]
```

## __More__

Want to check the output machine code of different CPU arch?

You may need this website: [__godbolt.org__](https://godbolt.org/)
