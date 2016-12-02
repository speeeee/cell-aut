#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <memory>
#include <tuple>
#include <vector>

#include <fstream>
#include <deque>

// WARNING: very much a WIP.  needs a lot of work and polishing.

#define READ_MODE 0
#define QUOT_MODE 1

#define R_WORD  0
#define R_DWORD 1
#define R_FUN   2
#define R_SYM   3

#define WORD_T  0
#define DWORD_T 1
#define FUN_T   2

#define QUOT_T  3
#define SYM_T   4

// TODO: change all char to int8_t.

// use vector if not C++11?
typedef struct Item { char *dat; std::vector<struct Item> quot;
                      int typ; int sz; } Item;
typedef struct { std::deque<Item> deq; int mode; } Program;

// TODO: configure to test for little- or big-endianness.
// little endian.
int dat__int(char *dat) {
  return (dat[3] << 24) | (dat[2] << 16) | (dat[1] << 8) | dat[0]; }

// TODO: quote-read mode.  make `[' which changes mode to quote mode and pushes to back of deque [].
//     : QUOT_MODE will read each token into back element.

// this is a temporary variable and will be removed when it is no longer needed.
char *cl_brkt = (char[1]){0x5D};

//typedef void (*SFun)(Program);
typedef std::function<void(Program *, Item)> PuFun; // push functions based on mode.
typedef std::function<Item(Program *)> PoFun; // pop functions based on mode.
typedef std::function<void(Program *, PuFun, PoFun)> SFun;

void print_int(Program *a, PuFun pu, PoFun po) {
  printf("%i",dat__int(po(a).dat/*a->deq.back().dat*/)); /*a->deq.pop_back();*/ }
// invoke only in READ_MODE.
void quote_read(Program *a, PuFun pu, PoFun po) { Item e; e.quot = std::vector<Item>();
  e.typ = QUOT_T; e.sz = 0; pu(a,e); /*a->deq.push_back(e);*/ a->mode = QUOT_MODE; }
void push(Program *a, Item subj) { a->deq.push_back(subj); }
Item pop(Program *a) { Item e = a->deq.back(); a->deq.pop_back(); return e; }

std::vector<SFun> funs;

// WARNING: returns newly allocated c-string.
std::tuple<char *, int> readf(const char *in) {
  std::ifstream ii(in, std::ifstream::ate | std::ifstream::binary);
  int len = ii.tellg(); char *buf = new char[len]; ii.read(buf,len); 
  return std::make_tuple(buf,len); }

// in case vector is needed and all indexing operations must be switched.
char ind(std::unique_ptr<char[]> dat, int i) { return dat[i]; }

// NOTE: use std::move when moving data array to struct.

void call(Program *prog, Item callee, PuFun pu, PoFun po) {
  if(callee.typ!=FUN_T) { /*prog->deq.push_front*/pu(prog,callee); }
  else { (funs.at(dat__int(callee.dat)))(prog, pu, po); } }

void invoke_mode(Program *prog, Item subj) {
  switch(prog->mode) {
  case READ_MODE: call(prog,subj,push,pop); break;
  case QUOT_MODE: // TODO: make this cleaner.
                  if(subj.typ==SYM_T&&subj.sz==1&&!memcmp(subj.dat,cl_brkt,1)) {
                    prog->mode = READ_MODE; }
                  else { prog->deq.back().quot.push_back(subj); } break; } }

// WARNING: do not use outside of read_bytecode due to dependence on bc and z.
#define IN_TOK(SZ, TYPE) \
    Item p; z++; char *dat = (new char[SZ]); \
    memcpy(dat,&std::get<0>(bc)[z],(SZ)); \
    p = (Item) { dat, std::vector<Item>(), (TYPE), (SZ) }; \
    z+=sizeof(int);

// TODO: make this less repetitive.
// TODO: try to use something better than char *.
void read_bytecode(std::tuple<char *,int> bc, Program *prog) { int z = 0;
  while(z<std::get<1>(bc)) { switch(std::get<0>(bc)[z]) {
    case R_WORD: { IN_TOK(sizeof(int),WORD_T) invoke_mode(prog,p); break; }
    case R_DWORD: { IN_TOK(sizeof(int64_t),DWORD_T) invoke_mode(prog,p); break; }
    case R_FUN: { IN_TOK(sizeof(int),FUN_T) invoke_mode(prog,p); break; }
    case R_SYM: { int sz; memcpy(&sz,&std::get<0>(bc)[++z],sizeof(int)); z+=sizeof(int);
                  IN_TOK(sz,SYM_T) invoke_mode(prog,p); break; } } } }

int main(int argc, char **argv) { funs.push_back(print_int); funs.push_back(quote_read);
  std::tuple<char *,int> bc = //readf("test.sm");
                              // test0
                              std::make_tuple((char[10]){0,4,0,0,0,2,0,0,0,0},10);
                              // test2
                              //std::make_tuple((char[16]){2,1,0,0,0,0,4,0,0,0,3,1,0,0,0,0x5D},16);
  Program *prog = new Program; prog->deq = std::deque<Item>(); prog->mode = READ_MODE;
  read_bytecode(bc,prog); /*delete[] std::get<0>(bc); delete prog;*/ return 0; }
