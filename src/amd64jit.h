#pragma once

#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <vector>
#include <stack>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

typedef void (*func)();

// must use in x86_64/amd64
class amd64jit{
private:
    uint8_t* mem;
    uint8_t* ptr;
    size_t size;
    std::stack<uint8_t*> stk;
public:
    amd64jit(const size_t);
    ~amd64jit();
    void err();
    void exec();
    void print();
    amd64jit& push(std::initializer_list<uint8_t>);
    amd64jit& push8(uint8_t);
    amd64jit& push16(uint16_t);
    amd64jit& push32(uint32_t);
    amd64jit& push64(uint64_t);
    amd64jit& je();
    amd64jit& jne();
};

amd64jit::amd64jit(const size_t _size){
    size=_size;
#ifdef _WIN32
    mem=(uint8_t*)VirtualAlloc(nullptr,size,
        MEM_COMMIT|MEM_RESERVE,
        PAGE_EXECUTE_READWRITE);
#else
    mem=(uint8_t*)mmap(nullptr,size,
        PROT_READ|PROT_WRITE|PROT_EXEC,
        MAP_PRIVATE|MAP_ANONYMOUS,
        -1,0);
#endif
    if(!mem){
        std::cout<<"failed to allocate memory\n";
        std::exit(-1);
    }
    memset(mem,0,size);
    ptr=mem;
}

amd64jit::~amd64jit(){
#ifdef _WIN32
    VirtualFree(mem,size,MEM_RELEASE);
#else
    munmap(mem,size);
#endif
    mem=nullptr;
}

void amd64jit::err(){
    std::cout<<"data overflow, please try a memory size greater than "<<size<<'\n';
    std::exit(-1);
}

void amd64jit::exec(){
    std::cout<<"putchar : 0x"<<std::hex<<std::setw(16)<<std::setfill('0')<<(uint64_t)putchar<<std::dec<<std::endl;
    std::cout<<"memset  : 0x"<<std::hex<<std::setw(16)<<std::setfill('0')<<(uint64_t)memset<<std::dec<<std::endl;
    std::cout<<"memory  : 0x"<<std::hex<<std::setw(16)<<std::setfill('0')<<(uint64_t)mem<<std::dec<<std::endl;
    ((func)mem)();
}

void amd64jit::print(){
    const char tbl[]="0123456789abcdef";
    std::cout<<"size: "<<(uint64_t)(ptr-mem)<<std::endl;
    for(uint8_t* i=mem;i<ptr;++i)
        printf("%c%c%c",tbl[((*i)>>4)&0x0f],tbl[(*i)&0x0f]," \n"[!((i-mem+1)&0xf)]);
    printf("\n");
}

amd64jit& amd64jit::push(std::initializer_list<uint8_t> codes){
    for(auto c:codes){
        ptr[0]=c;
        ++ptr;
        if(ptr>=mem+size)
            err();
    }
    return *this;
}

amd64jit& amd64jit::push8(uint8_t n){
    if(ptr+1>=mem+size)
        err();
    ptr[0]=n;
    ++ptr;
    return *this;
}

amd64jit& amd64jit::push16(uint16_t n){
    if(ptr+2>=mem+size)
        err();
    ptr[0]=n&0xff;
    ptr[1]=(n>>8)&0xff;
    ptr+=2;
    return *this;
}

amd64jit& amd64jit::push32(uint32_t n){
    if(ptr+4>=mem+size)
        err();
    ptr[0]=n&0xff;
    ptr[1]=(n>>8)&0xff;
    ptr[2]=(n>>16)&0xff;
    ptr[3]=(n>>24)&0xff;
    ptr+=4;
    return *this;
}

amd64jit& amd64jit::push64(uint64_t n){
    if(ptr+8>=mem+size)
        err();
    ptr[0]=n&0xff;
    ptr[1]=(n>>8)&0xff;
    ptr[2]=(n>>16)&0xff;
    ptr[3]=(n>>24)&0xff;
    ptr[4]=(n>>32)&0xff;
    ptr[5]=(n>>40)&0xff;
    ptr[6]=(n>>48)&0xff;
    ptr[7]=(n>>56)&0xff;
    ptr+=8;
    return *this;
}

amd64jit& amd64jit::je(){
    push({0x0f,0x84,0x00,0x00,0x00,0x00});// je
    stk.push(ptr);
    return *this;
}

amd64jit& amd64jit::jne(){
    push({0x0f,0x85,0x00,0x00,0x00,0x00});// jne
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