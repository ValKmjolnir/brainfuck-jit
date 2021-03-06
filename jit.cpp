#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>
#include <stack>
#include <fstream>
#include <sstream>
#include <ctime>

#include "amd64jit.h"

enum op{
    op_add,op_sub,
    op_addp,op_subp,
    op_jt,op_jf,
    op_in,op_out
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
    for(size_t i=0;i<s.length();++i){
        switch(s[i]){
            case '+':
                cnt=0;
                while(s[i]=='+'){
                    ++cnt;
                    ++i;
                }
                --i;
                while(cnt>=256)
                    cnt-=256;
                code.push_back({op_add,cnt});break;
            case '-':
                cnt=0;
                while(s[i]=='-'){
                    ++cnt;
                    ++i;
                }
                --i;
                while(cnt>=256)
                    cnt-=256;
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
    std::cout<<"interpreter time usage: "<<(clock()-begin)/(1.0*CLOCKS_PER_SEC)<<"s\n";
}

void jit(const std::vector<opcode>& code){
    amd64jit mem(65536);
    
    /* set stack and base pointer */
    mem.push({0x55});// pushq %rbp
    mem.push({0x48,0x89,0xe5});// mov %rsp,%rbp
    /* save register context */
    mem.push({0x41,0x57});// push %r15
    mem.push({0x41,0x56});// push %r14
    mem.push({0x41,0x55});// push %r13
    mem.push({0x41,0x54});// push %r12
    mem.push({0x41,0x53});// push %r11
    mem.push({0x41,0x52});// push %r10
    mem.push({0x41,0x51});// push %r9
    mem.push({0x41,0x50});// push %r8
    mem.push({0x53});// pushq %rbx
    mem.push({0x52});// pushq %rdx
    mem.push({0x51});// pushq %rcx
    mem.push({0x50});// pushq %rax
    /* set stack space */
    mem.push({0x48,0x81,0xec,0x00,0x00,0x02,0x00});// sub $0x20000,%rsp
    /* set bf machine's paper pointer */
    mem.push({0x48,0x89,0xe3});// movq %rsp,%rbx

    /* clear stack memory */
#ifndef _WIN32
    mem.push({0x48,0xb8});
    mem.push64((uint64_t)memset);// movabs $memset,%rax
    mem.push({0x48,0x89,0xe7});// mov %rsp,%rdi
    mem.push({0x31,0xf6});// xor %esi,%esi
    mem.push({0xba,0x00,0x00,0x02,0x00});// mov $0x20000,%edx
    mem.push({0xff,0xd0});// callq *%rax
#else
    mem.push({0x48,0xb8});
    mem.push64((uint64_t)memset);// movabs $memset,%rax
    mem.push({0x41,0xb8,0x00,0x00,0x02,0x00});// mov $0x20000,%r8d
    mem.push({0x31,0xd2});// xor %edx,%edx
    mem.push({0x48,0x89,0xe1});// mov %rsp,%rcx
    mem.push({0xff,0xd0});// callq *%rax
#endif

    for(auto& op:code){
        switch(op.op){
            case op_add:
                mem.push({0x80,0x03,(uint8_t)(op.num&0xff)});// addb $op.num,(%rbx)
                break;
            case op_sub:
                mem.push({0x80,0x2b,(uint8_t)(op.num&0xff)});// subb $op.num,(%rbx)
                break;
            case op_addp:
                mem.push({0x48,0x81,0xc3});// add $op.num,%rbx
                mem.push32(op.num);
                break;
            case op_subp:
                mem.push({0x48,0x81,0xeb});// sub $op.num,%rbx
                mem.push32(op.num);
                break;
            case op_jt:// if(al)
                mem.push({0x8a,0x03});// mov (%rbx),%al
                mem.push({0x84,0xc0});// test %al,%al
                mem.jne();
                break;
            case op_jf:// if(!al)
                mem.push({0x8a,0x03});// mov (%rbx),%al
                mem.push({0x84,0xc0});// test %al,%al
                mem.je();
                break;
            case op_in:break;
            case op_out:
#ifndef _WIN32
                mem.push({0x48,0xb8});
                mem.push64((uint64_t)putchar);// movabs $putchar,%rax
                mem.push({0x0f,0xbe,0x3b});// movsbl (%rbx),%edi
                mem.push({0xff,0xd0});// callq *%rax
#else
                mem.push({0x48,0xb8});
                mem.push64((uint64_t)putchar);// movabs $putchar,%rax
                mem.push({0x0f,0xbe,0x0b});// movsbl (%rbx),%ecx
                //mem.push({0x83,0xc1,0x60});// add $0x60,%ecx
                //mem.push({0xb9,0x61,0x00,0x00,0x00});
                mem.push({0x48,0xb8});
                mem.push64((uint64_t)putchar);// movabs $putchar,%rax
                mem.push({0xff,0xd0});// callq *%rax
#endif
                break;
        }
    }
    /* restore stack space */
    mem.push({0x48,0x81,0xc4,0x00,0x00,0x02,0x00});// add $0x20000,%rsp
    /* restore register context */
    mem.push({0x58});// popq %rax
    mem.push({0x59});// popq %rcx
    mem.push({0x5a});// popq %rdx
    mem.push({0x5b});// popq %rbx
    mem.push({0x41,0x58});// pop %r8
    mem.push({0x41,0x59});// pop %r9
    mem.push({0x41,0x5a});// pop %r10
    mem.push({0x41,0x5b});// pop %r11
    mem.push({0x41,0x5c});// pop %r12
    mem.push({0x41,0x5d});// pop %r13
    mem.push({0x41,0x5e});// pop %r14
    mem.push({0x41,0x5f});// pop %r15
    mem.push({0x5d});// popq %rbp
    mem.push({0xc3});// retq

    clock_t begin=clock();
    mem.exec();
    std::cout<<"jit time usage: "<<(clock()-begin)/(1.0*CLOCKS_PER_SEC)<<"s\n";
}

int main(){
    std::ifstream fin("mandelbrot.bf");
    std::stringstream ss;
    ss<<fin.rdbuf();
    std::vector<opcode> code=scanner(ss.str());
    interpreter(code);
    jit(code);
    return 0;
}