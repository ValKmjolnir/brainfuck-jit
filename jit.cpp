#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#include <stack>
#include <fstream>
#include <sstream>
#include <ctime>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

class codegen{
private:
    std::vector<uint8_t> code;
public:
    void func_head(){
        const uint8_t begin[]={
            0x55,          // pushq %rbp
            0x48,0x89,0xe5 // movq  %rsp, %rbp
        };
        for(int i=0;i<4;++i)
            code.push_back(begin[i]);
    }
    void func_end(){
        const uint8_t end[]={
            0x5d,// popq %rbp
            0xc3,// retq
        };
        for(int i=0;i<2;++i)
            code.push_back(end[i]);
    }
    void push8(uint8_t n){
        code.push_back(n);
    }
    void push16(uint16_t n){
        code.push_back(n&0xff);
        code.push_back((n>>8)&0xff);
    }
    void push32(uint32_t n){
        code.push_back(n&0xff);
        code.push_back((n>>8)&0xff);
        code.push_back((n>>16)&0xff);
        code.push_back((n>>24)&0xff);
    }
    void push64(uint64_t n){
        code.push_back(n&0xff);
        code.push_back((n>>8)&0xff);
        code.push_back((n>>16)&0xff);
        code.push_back((n>>24)&0xff);
        code.push_back((n>>32)&0xff);
        code.push_back((n>>40)&0xff);
        code.push_back((n>>48)&0xff);
        code.push_back((n>>56)&0xff);
    }
    const std::vector<uint8_t> get() const{
        return code;
    }
};

typedef void (*func)();

class jit{
// must use in x86_64
private:
    char* mem;
    size_t size;
public:
    jit(const std::vector<uint8_t>& codes){
        size=codes.size()+1;
#ifdef _WIN32
        mem=(char*)VirtualAlloc(nullptr,size,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE);
#else
        mem=(char*)mmap(nullptr,size,
            PROT_READ|PROT_WRITE|PROT_EXEC,
            MAP_PRIVATE|MAP_ANONYMOUS,
            -1,0);
#endif
        if(!mem){
            std::cout<<"failed to allocate memory\n";
            std::exit(-1);
        }
        memset(mem,0,size);
        memcpy(mem,codes.data(),codes.size());
    }
    ~jit(){
#ifdef _WIN32
        VirtualFree(mem,size,MEM_RELEASE);
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
        for(uint64_t i=0;i<size;++i)
            printf("%c%c %c",tbl[((mem[i])>>4)&0x0f],tbl[(mem[i])&0x0f]," \n"[!((i+1)&0xf)]);
        printf("\n");
    }
};

enum op{
    op_add,
    op_sub,
    op_addp,
    op_subp,
    op_jt,
    op_jf,
    op_in,
    op_out
};
struct opcode{
    uint8_t op;
    uint32_t num;
};
std::vector<opcode> scanner(std::string s){
    std::vector<opcode> code;
    std::stack<size_t> stk;
    uint32_t cnt=0;
    int line=0;
    for(int i=0;i<s.length();++i){
        switch(s[i]){
            case '+':
                cnt=0;
                while(s[i]=='+'){
                    ++cnt;
                    ++i;
                }
                --i;
                code.push_back({op_add,cnt});break;
            case '-':
                cnt=0;
                while(s[i]=='-'){
                    ++cnt;
                    ++i;
                }
                --i;
                code.push_back({op_sub,cnt});break;
            case '>':
                cnt=0;
                while(s[i]=='>'){
                    ++cnt;
                    ++i;
                }
                --i;
                code.push_back({op_addp,cnt});break;
            case '<':
                cnt=0;
                while(s[i]=='<'){
                    ++cnt;
                    ++i;
                }
                --i;
                code.push_back({op_subp,cnt});break;
            case '[':
                stk.push(code.size());
                code.push_back({op_jf,0});break;
            case ']':
                if(stk.empty()){
                    std::cout<<"empty stack at line "<<line<<"\n";
                    std::exit(-1);
                }
                code[stk.top()].num=code.size()&0xffffffff;
                code.push_back({op_jt,(uint32_t)stk.top()});
                stk.pop();break;
            case ',':code.push_back({op_in,0});break;
            case '.':code.push_back({op_out,0});break;
            case '\n':++line;break;
        }
    }
    if(!stk.empty()){
        std::cout<<"lack ]\n";
        std::exit(-1);
    }
    return code;
}

void interpreter(const std::vector<opcode>& code){
    clock_t begin=clock();
    uint8_t buf[131072]={0};
    uint32_t p=0;
    for(int i=0;i<code.size();++i){
        switch(code[i].op){
            case op_add:buf[p]+=code[i].num;break;
            case op_sub:buf[p]-=code[i].num;break;
            case op_addp:p+=code[i].num;break;
            case op_subp:p-=code[i].num;break;
            case op_jt:if(buf[p])i=(code[i].num)-1;break;
            case op_jf:if(!buf[p])i=(code[i].num)-1;break;
            case op_in:buf[p]=getchar();break;
            case op_out:std::cout<<(char)buf[p]<<std::flush;break;
        }
    }
    std::cout<<"interpreter time usage: "<<(clock()-begin)/(1.0*CLOCKS_PER_SEC)<<"s\n";
}

void jit_compiler(const std::vector<opcode>& code){
    codegen gen;
    clock_t begin=clock();
    gen.func_head();
    for(int i=0;i<code.size();++i){
        // unfinished
    }
    gen.func_end();
    jit mem(gen.get());
    mem.print();
    mem.exec();
    std::cout<<"jit time usage: "<<(clock()-begin)/(1.0*CLOCKS_PER_SEC)<<"s\n";
}

int main(){
    std::ifstream fin("bf.bf");
    std::stringstream ss;
    ss<<fin.rdbuf();
    std::vector<opcode> code=scanner(ss.str());
    interpreter(code);
    jit_compiler(code);
    return 0;
}