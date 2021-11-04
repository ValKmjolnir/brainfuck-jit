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
    for(int i=0;i<s.length();++i){
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
    for(int i=0;i<code.size();++i){
        switch(code[i].op){
            case op_add:buf[p]+=code[i].num;break;
            case op_sub:buf[p]-=code[i].num;break;
            case op_addp:p+=code[i].num;break;
            case op_subp:p-=code[i].num;break;
            case op_jt:if(buf[p])i=(code[i].num)-1;break;
            case op_jf:if(!buf[p])i=(code[i].num)-1;break;
            case op_in:buf[p]=getchar();break;
            case op_out:putchar(buf[p]);break;
        }
    }
    std::cout<<"interpreter time usage: "<<(clock()-begin)/(1.0*CLOCKS_PER_SEC)<<"s\n";
}

void jit_compiler(const std::vector<opcode>& code){
    amd64jit mem(65536);
    
    mem.push({0x53});// pushq %rbx
    mem.push({0x48,0x81,0xec,0x00,0x80,0x00,0x00});// sub $0x8000,%rsp

    mem.push({0x48,0xb9});
    mem.push64((uint64_t)putchar);// movabs $putchar,%rcx

    for(auto& op:code){
        switch(op.op){
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
            case op_jt:// al!=0
                mem.push({0x8a,0x03});// mov (%rbx),%al
                mem.push({0x84,0xc0});// test %al,%al
                mem.jne();
                break;
            case op_jf:// al==0
                mem.push({0x8a,0x03});// mov (%rbx),%al
                mem.push({0x84,0xc0});// test %al,%al
                mem.je();
                break;
            case op_in:break;
            case op_out:
                mem.push({0x0f,0xbe,0x3b});// movsbl (%rbx),%edi
                mem.push({0xff,0xd1});// callq *%rcx
                break;
        }
    }
    mem.push({0x48,0x81,0xc4,0x00,0x80,0x00,0x00});// add $0x8000,%rsp
    mem.push({0x5b});// popq %rbx
    mem.push({0xc3});// retq
    
    //mem.print();
    
    //clock_t begin=clock();
    mem.exec();
    //std::cout<<"jit time usage: "<<(clock()-begin)/(1.0*CLOCKS_PER_SEC)<<"s\n";
}

extern "C" void testfunc(){
    printf("putc %p\n",putchar);
    asm(
        "pushq %rbx\n"        // store rbx on stack
        "subq $0x8000,%rsp\n" // uint8_t buff[32768<<4];
        "movq %rsp,%rbx\n"    // uint8_t* p=buff;
        "movabs $putchar,%rcx\n"
        "movb $0x41,(%rbx)\n" // p[0]='A';
        "movsbl (%rbx),%edi\n"
        "callq *%rcx\n"
        "addq $0x8000,%rsp\n" // delete buff
        "popq %rbx"
    );
    putchar('\n');
}

int main(){
    std::ifstream fin("mandelbrot.bf");
    std::stringstream ss;
    ss<<fin.rdbuf();
    std::vector<opcode> code=scanner(ss.str());
    //interpreter(code);
    jit_compiler(code);
    testfunc();
    return 0;
}