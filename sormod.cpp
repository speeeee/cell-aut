#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <memory>
#include <tuple>
#include <queue>
#include <stack>

#include <fstream>
#include <deque>

// WARNING: very much a WIP.  needs a lot of work and polishing.

#define READ_MODE 0
#define QUOT_MODE 1
#define REV_MODE  2

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
// TODO: create some kind of destructor for Item.
typedef struct Item { char *dat; std::queue<struct Item> quot;
                      int typ; int sz; } Item;
Item item(char *dat, std::queue<Item> quot, int typ, int sz) {
  return (Item) { dat, quot, typ, sz }; }
void item_free(Item e) { if(e.dat) { delete e.dat; } /* TODO: destroy queue? */ }
typedef struct { std::deque<Item> deq; std::stack<int> p_modes; int mode; } Program;

// TODO: configure to test for little- or big-endianness.
// little endian.
int dat__int(char *dat) {
  return (dat[3] << 24) | (dat[2] << 16) | (dat[1] << 8) | dat[0]; }

// TODO: runtime error for attempt to pop empty stack.
// TODO: function definition using quotes.
//     : function definition will involve the usage of lambda functions to be more
//         convenient to write.  Though, since these functions will be just a wrapper around
//         the quote itself, it would probably be better to store in separate vector.

// DONE: quote-read mode.  make `[' which changes mode to quote mode and pushes to back of deque [].
//     : QUOT_MODE will read each token into back element.

// this is a temporary variable and will be removed when it is no longer needed.
char *cl_brkt = (char[1]){0x5D};

//typedef void (*SFun)(Program);
typedef std::function<void(Program *, Item)> PuFun; // push functions based on mode.
typedef std::function<Item(Program *)> PoFun; // pop functions based on mode.
typedef std::function<void(Program *, PuFun, PoFun)> SFun;

#define FSZ 6
std::vector<SFun> funs; // functions defined from the start.  also contains functions from
                        //   DLLs wrapped in a lambda function.
std::vector<Item> dfuns; // functions defined by user at runtime.  stored as quotes.

void read_parse(std::queue<Item>, Program *);

void print_int(Program *a, PuFun pu, PoFun po) {
  printf("%i",dat__int(po(a).dat/*a->deq.back().dat*/)); /*a->deq.pop_back();*/ }
// invoke only in READ_MODE.
void quote_read(Program *a, PuFun pu, PoFun po) { Item e; e.quot = std::queue<Item>();
  e.typ = QUOT_T; e.sz = 1; pu(a,e); /*a->deq.push_back(e);*/ 
  a->p_modes.push(a->mode); a->mode = QUOT_MODE; }
void read_mode(Program *a, PuFun pu, PoFun po) { a->mode = READ_MODE; }
void rev_mode(Program *a, PuFun pu, PoFun po) { if(a->mode==READ_MODE) { a->mode = REV_MODE; } 
  else { a->mode = READ_MODE; } }
// reminder that `po' is destructive.
void f_call(Program *a, PuFun pu, PoFun po) { read_parse(po(a).quot,a); }
void d_fun(Program *a, PuFun pu, PoFun po) { dfuns.push_back(po(a)); }

void push(Program *a, Item subj) { a->deq.push_back(subj); }
Item pop(Program *a) { Item e = a->deq.back(); a->deq.pop_back(); return e; }
void fpush(Program *a, Item subj) { a->deq.push_front(subj); }
Item fpop(Program *a) { Item e = a->deq.front(); a->deq.pop_front(); return e; }

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
  else { int f = dat__int(callee.dat);
         if(f>=FSZ) { read_parse(dfuns.at(f-FSZ).quot,prog); }
         else { (funs.at(f))(prog, pu, po); } } }

void invoke_mode(Program *prog, Item subj) {
  switch(prog->mode) {
  case READ_MODE: call(prog,subj,push,pop); break;
  case REV_MODE: call(prog,subj,fpush,fpop); break;
  case QUOT_MODE: // TODO: make this cleaner.
                  // DONE: allow for embedded quotes.
                  // !!! WARNING: irregular use of `subj.sz' as counter for embedded brackets.
                  if(subj.typ==FUN_T&&dat__int(subj.dat)==1/*quote_read mode*/) { subj.sz++; }
                  if(subj.typ==SYM_T&&subj.sz==1&&!memcmp(subj.dat,cl_brkt,1)) { subj.sz--; }
                  if(!subj.sz) { prog->mode = prog->p_modes.top(); prog->p_modes.pop(); }
                  else { prog->deq.back().quot.push(subj); } break; } }

// WARNING: do not use outside of read_bytecode due to dependence on bc and z.
#define IN_TOK(SZ, TYPE) \
    Item p; /*z++*/ char *dat = (new char[SZ]); \
    memcpy(dat,&std::get<0>(bc)[z],(SZ)); \
    p = (Item) { dat, std::queue<Item>(), (TYPE), (SZ) }; \
    z+=(SZ);

// DONE: write call function and parse with just queue.
void read_parse(std::queue<Item> bc, Program *prog) {
  while(!bc.empty()) { invoke_mode(prog,bc.back()); bc.pop(); } }

// DONE: make this less repetitive.
// TODO: try to use something better than char *.
void read_bytecode(std::tuple<char *,int> bc, Program *prog) { int z = 0;
  while(z<std::get<1>(bc)) { switch(std::get<0>(bc)[z]) {
    case R_WORD: { z++; IN_TOK(sizeof(int),WORD_T) invoke_mode(prog,p); break; }
    case R_DWORD: { z++; IN_TOK(sizeof(int64_t),DWORD_T) invoke_mode(prog,p); break; }
    case R_FUN: { z++; IN_TOK(sizeof(int),FUN_T) invoke_mode(prog,p); break; }
    case R_SYM: { int sz; memcpy(&sz,&std::get<0>(bc)[++z],sizeof(int)); z+=sizeof(int);
                  IN_TOK(sz,SYM_T) invoke_mode(prog,p); break; } } } }

int main(int argc, char **argv) { funs.push_back(print_int); funs.push_back(quote_read);
                                  funs.push_back(f_call); funs.push_back(read_mode);
                                  funs.push_back(rev_mode); funs.push_back(d_fun);
  std::tuple<char *,int> bc = //readf("test.sm");
                              // test0
                              //std::make_tuple((char[10]){0,4,0,0,0,2,0,0,0,0},10);
                              // test2
                              //std::make_tuple((char[16]){2,1,0,0,0,0,4,0,0,0,3,1,0,0,0,0x5D},16);
                              // test3
                              //std::make_tuple((char[26]){0,4,0,0,0,2,1,0,0,0,2,0,0,0,0,3,1,0,0,0,0x5D
                              //                          ,2,2,0,0,0},26);
                              // test4
                              std::make_tuple((char[31]){0,4,0,0,0,2,1,0,0,0,2,0,0,0,0,3,1,0,0,0,0x5D
                                                        ,2,5,0,0,0,2,6,0,0,0},31);
                              
  Program *prog = new Program; prog->deq = std::deque<Item>(); prog->p_modes = std::stack<int>();
                               prog->mode = READ_MODE;
  read_bytecode(bc,prog); /*delete[] std::get<0>(bc); delete prog;*/ return 0; }
