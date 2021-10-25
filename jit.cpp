#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

typedef void (*func)();

class jit{
private:
    char* mem;
    uint64_t cnt;
    size_t size;
public:
    jit(size_t s,const std::vector<unsigned char> codes){
        size=s;
#ifdef _WIN32
        mem=(char*)VirtualAlloc(
            nullptr,
            size,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );
#else
        mem=(char*)mmap(
            nullptr,
            size,
            PROT_READ|PROT_WRITE|PROT_EXEC,
            MAP_PRIVATE|MAP_ANONYMOUS,
            -1,
            0);
#endif
        if(!mem){
            perror("failed to allocate memory");
            std::exit(-1);
        }
        memset(mem,0,size);
        memcpy(mem,codes.data(),codes.size());
        cnt=codes.size();
    }
    ~jit(){
#ifdef _WIN32
        VirtualFree(mem,0,MEM_RELEASE);
#else
        munmap(mem,size);
#endif
        mem=nullptr;
    }
    void exec(){
        func entry=(func)mem;
        entry();
    }
    void print(){
        const char* tbl="0123456789abcdef";
        for(uint64_t i=0;i<cnt;++i)
            printf("%c%c %c",tbl[((mem[i])>>4)&0x0f],tbl[(mem[i])&0x0f]," \n"[!((i+1)&0xf)]);
        printf("\n");
    }
};

unsigned char func_begin[]={
    0x55,               //	pushq	%rbp
    0x48,0x89,0xe5,     //	movq	%rsp, %rbp
    0x00
};
unsigned char func_end[]={
    0x5d,               //	popq	%rbp
    0xc3,               //  retq
    0x00
};

unsigned char paper[32768]={0};
uint32_t p=0;

void subfunc(){
    std::vector<unsigned char> codes;
    for(int i=0;func_begin[i];++i)
        codes.push_back(func_begin[i]);
    for(uint8_t* i=(uint8_t*)&&add;i<(uint8_t*)&&sub;++i)
        codes.push_back(*i);
    for(uint8_t* i=(uint8_t*)&&add;i<(uint8_t*)&&sub;++i)
        codes.push_back(*i);
    for(uint8_t* i=(uint8_t*)&&add;i<(uint8_t*)&&sub;++i)
        codes.push_back(*i);
    for(uint8_t* i=(uint8_t*)&&sub;i<(uint8_t*)&&mov;++i)
        codes.push_back(*i);
    for(int i=0;func_end[i];++i)
        codes.push_back(func_end[i]);
    jit mem(1024,codes);
    mem.print();
add:
    ++paper[p];
sub:
    --paper[p];
mov:
    ++p;
neg:
    --p;
end:
    mem.exec();
    printf("%d\n",paper[p]);
    return;
}
int main(){
    subfunc();
    return 0;
}