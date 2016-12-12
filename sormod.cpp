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
#define SCOPE_T 5

#define FALSE   6

// TODO: change all char to int8_t.

// DONE: create dump function.
// TODO: create general mode change function.

// use vector if not C++11?
// DONE: create some kind of destructor for Item.
typedef struct Item { std::shared_ptr<char> dat; std::deque<struct Item> quot;
                      int typ; int sz; } Item;
struct ItemDeleter { void operator()(char *c) { delete[] c; } };
Item item(std::shared_ptr<char> dat, std::deque<Item> quot, int typ, int sz) {
  return (Item) { dat, quot, typ, sz }; }
//void item_free(Item e) { if(e.dat) { delete e.dat; } /* TODO: destroy queue? */ }
typedef struct { std::deque<Item> deq; std::stack<int> p_modes; int mode; } Program;

// TODO: configure to test for little- or big-endianness.
// little endian.
int dat__int(char *dat) {
  return (dat[3] << 24) | (dat[2] << 16) | (dat[1] << 8) | dat[0]; }
double dat__flt(char *dat) { return *(double *)dat; }

// TODO: runtime error for attempt to pop empty stack.
// DONE: function definition using quotes.
//     : function definition will involve the usage of lambda functions to be more
//         convenient to write.  Though, since these functions will be just a wrapper around
//         the quote itself, it would probably be better to store in separate vector.

// DONE: quote-read mode.  make `[' which changes mode to quote mode and pushes to back of deque [].
//     : QUOT_MODE will read each token into back element.

// TODO: add file importing.

// this is a temporary variable and will be removed when it is no longer needed.
char *cl_brkt = (char[1]){0x5D};

//typedef void (*SFun)(Program);
typedef std::function<void(Program *, Item)> PuFun; // push functions based on mode.
typedef std::function<Item(Program *)> PoFun; // pop functions based on mode.
typedef std::function<void(Program *, PuFun, PoFun)> SFun;

/* == IMPORTANT! this is the amount of pre-loaded functions. == */
#define FSZ 25
std::vector<SFun> funs; // functions defined from the start.  also contains functions from
                        //   DLLs wrapped in a lambda function (unimplemented).
std::vector<Item> dfuns; // functions defined by user at runtime.  stored as quotes.

void read_parse(std::deque<Item>, Program *);

// TODO: work on standard lib:
/*     * print_float
       * print (symbol)
       * read_char [from stdin for now]
       * curry
       * if (true/false are int)
       * =, <, and >
       * +, -, *, /, [pow]
       * float64 -> int32, int32 -> float64
       * swap, dup, drop, pick */

Item item_ptr(char *a, int t, int sz) {
  return item(std::shared_ptr<char>(a,std::default_delete<char[]>())
             ,std::deque<Item>(), t, sz); }

/* push_false -- pushes the false item to the stack.
              --  (note, all other values are evaluated as true) */
void push_false(Program *a, PuFun pu, PoFun po) {
  pu(a,item(NULL,std::deque<Item>(),FALSE,0)); }

/* print_int, print_flt -- standard printing functions. */
void print_int(Program *a, PuFun pu, PoFun po) {
  printf("%i",dat__int(po(a).dat.get()/*a->deq.back().dat*/)); /*a->deq.pop_back();*/ }
void print_flt(Program *a, PuFun pu, PoFun po) {
  printf("%g",dat__flt(po(a).dat.get())); }
void print_sym(Program *a, PuFun pu, PoFun po) { } // TODO
void read_char(Program *a, PuFun pu, PoFun po) { } // TODO
// same as cons.
/* curry -- given a quote at the top and any item below, push item below to the front of quote. */
void curry(Program *a, PuFun pu, PoFun po) { Item q = po(a); Item b = po(a);
  q.quot.push_front(b); pu(a,q); }
/* if_f -- if pred then t else f */
void if_f(Program *a, PuFun pu, PoFun po) { Item b = po(a); Item c = po(a); Item p = po(a);
  if(p.typ!=FALSE) { pu(a,b); } else { pu(a,c); } }
// Literals using 'dat' only.
/* eq -- checks if two topmost items are equal by data comparison. */
void eq(Program *a, PuFun pu, PoFun po) { Item b = po(a); Item c = po(a);
  if(b.sz==c.sz&&b.sz!=0&&!memcmp(b.dat.get(),c.dat.get(),b.sz)) { pu(a,b); }
  else { push_false(a,pu,po); } }

/* add, sub, mul, divi -- standard arithmetic.  type (int32 or float64) determined by
                       --   __left__ argument. */
// VERY ugly but unfortunately operators cannot be used directly in macros.  Definitely needs
//   fixing.
void add(Program *a, PuFun pu, PoFun po) { Item b = po(a); Item c = po(a); switch(b.typ) {
  case WORD_T: { int *r = new int; *r = dat__int(c.dat.get())+dat__int(b.dat.get());
                 pu(a,item_ptr((char *)r, WORD_T, 4)); }
  case DWORD_T: { double *r = new double; *r = dat__flt(c.dat.get())+dat__flt(b.dat.get());
                 pu(a,item_ptr((char *)r, DWORD_T, 8)); } } }
void sub(Program *a, PuFun pu, PoFun po) { Item b = po(a); Item c = po(a); switch(b.typ) {
  case WORD_T: { int *r = new int; *r = dat__int(c.dat.get())-dat__int(b.dat.get());
                 pu(a,item_ptr((char *)r, WORD_T, 4)); }
  case DWORD_T: { double *r = new double; *r = dat__flt(c.dat.get())-dat__flt(b.dat.get());
                 pu(a,item_ptr((char *)r, DWORD_T, 8)); } } }
void mul(Program *a, PuFun pu, PoFun po) { Item b = po(a); Item c = po(a); switch(b.typ) {
  case WORD_T: { int *r = new int; *r = dat__int(c.dat.get())*dat__int(b.dat.get());
                 pu(a,item_ptr((char *)r, WORD_T, 4)); }
  case DWORD_T: { double *r = new double; *r = dat__flt(c.dat.get())*dat__flt(b.dat.get());
                 pu(a,item_ptr((char *)r, DWORD_T, 8)); } } }
void divi(Program *a, PuFun pu, PoFun po) { Item b = po(a); Item c = po(a); switch(b.typ) {
  case WORD_T: { int *r = new int; *r = dat__int(c.dat.get())/dat__int(b.dat.get());
                 pu(a,item_ptr((char *)r, WORD_T, 4)); }
  case DWORD_T: { double *r = new double; *r = dat__flt(c.dat.get())/dat__flt(b.dat.get());
                 pu(a,item_ptr((char *)r, DWORD_T, 8)); } } }

/* swap, dup, drop, pick --
   ( a b -- b a ), ( a -- a a ), ( a -- ), ( a b c -- a b c a ) respectively. */
void swap(Program *a, PuFun pu, PoFun po) { Item b = po(a); Item c = po(a); pu(a,b); pu(a,c); }
void dup(Program *a, PuFun pu, PoFun po) { Item b = po(a); pu(a,b); pu(a,b); }
void drop(Program *a, PuFun pu, PoFun po) { po(a); }
void pick(Program *a, PuFun pu, PoFun po) { } // TODO

void from_back(Program *a, PuFun pu, PoFun po) { Item e = po(a); pu(a,a->deq[dat__int(e.dat.get())]); }

// pushes SCOPE_T to front.
/* scope -- push scope item to front of stack.  done by read_parse as a way to simulate local scope
         --   in quotes. */
void scope(Program *a, PuFun pu, PoFun po) {
  a->deq.push_front(item(NULL,std::deque<Item>(),SCOPE_T,0)); }
/* scope_eq -- check if topmost item is of type SCOPE_T. */
void scope_eq(Program *a, PuFun pu, PoFun po) { Item e = po(a);
  if(e.typ==SCOPE_T) { pu(a,e); } else { push_false(a,pu,po); } }
// not included function.
/* destroy_scope -- destroys front of stack up to and including scope type. */
void destroy_scope(Program *a) { while(a->deq.front().typ!=SCOPE_T) { a->deq.pop_front(); }
  a->deq.pop_front(); }

// invoke only in READ_MODE.
/* quote_read -- pushes new quote to stack and changes mode to QUOT_MODE. */
void quote_read(Program *a, PuFun pu, PoFun po) { Item e; e.quot = std::deque<Item>();
  e.typ = QUOT_T; e.sz = 1; pu(a,e); /*a->deq.push_back(e);*/ 
  a->p_modes.push(a->mode); a->mode = QUOT_MODE; }
void read_mode(Program *a, PuFun pu, PoFun po) { a->mode = READ_MODE; }
// ! WARNING: deprecated.
void rev_mode(Program *a, PuFun pu, PoFun po) { if(a->mode==READ_MODE) { a->mode = REV_MODE; }
  else { a->mode = READ_MODE; } }
// reminder that `po' is destructive.
// pushes new scope and quote itself to back for possible recursion.
// expects READ_MODE
/* f_call -- calls read_parse on quote at top of stack. */
void f_call(Program *a, PuFun pu, PoFun po) { read_parse(po(a).quot,a); }
/* d_fun -- pushes quote at top of stack to function table */
void d_fun(Program *a, PuFun pu, PoFun po) { dfuns.push_back(po(a)); }

// expects READ_MODE
/* recur -- brings to front quote stored in scope and calls it. */
void recur(Program *a, PuFun pu, PoFun po) { int i;
  for(i = 0;i<a->deq.size()&&a->deq[i].typ!=SCOPE_T;i++);
  pu(a,a->deq[i-1]); f_call(a,pu,po); }

/* push, pop, fpush, fpop -- standard stack operations.  f-prefixed operations perform on the
                          --   opposite side. */
void push(Program *a, Item subj) { a->deq.push_back(subj); }
Item pop(Program *a) { Item e = a->deq.back(); a->deq.pop_back(); return e; }
void fpush(Program *a, Item subj) { a->deq.push_front(subj); }
Item fpop(Program *a) { Item e = a->deq.front(); a->deq.pop_front(); return e; }

// WARNING: returns newly allocated c-string.
/* readf -- reads file into (bytestring, size) */
std::tuple<char *, int> readf(const char *in) {
  std::ifstream ii(in, std::ifstream::ate | std::ifstream::binary);
  int len = ii.tellg(); ii.seekg(0,std::ios::beg); char *buf = new char[len]; ii.read(buf,len); 
  ii.close(); return std::make_tuple(buf,len); }

/*#define WORD_T  0
#define DWORD_T 1
#define FUN_T   2

#define QUOT_T  3
#define SYM_T   4*/
/* encode_out -- performs the reverse of the decoding done on read_bytecode.
              -- QUOT_T is handled specially where simply function quote_read ([)
              --   and symbol ] are pushed, with the encoding of all within the quote in between. */
void encode_out(Item f, FILE *o) { switch(f.typ) {
  case WORD_T: fputc(R_WORD,o); fwrite(f.dat.get(),sizeof(int),1,o); break;
  case DWORD_T: fputc(R_DWORD,o); fwrite(f.dat.get(),sizeof(int64_t),1,o); break;
  case FUN_T: fputc(R_FUN,o); fwrite(f.dat.get(),sizeof(int),1,o); break;
  case QUOT_T: { fputc(R_FUN,o); int a = 1; fwrite(&a,sizeof(int),1,o); int sz = f.quot.size();
    fwrite(&sz,sizeof(int),1,o); std::deque<Item> bc = f.quot;
    while(!bc.empty()) { encode_out(bc.front(),o); bc.pop_front(); }
    fputc(R_SYM,o); int csz = 1; fwrite(&csz,sizeof(int),1,o); fputc(0x5D,o); break; }
  case SYM_T: fputc(R_SYM,o); fwrite(&f.sz,sizeof(int),1,o);
    fwrite(f.dat.get(),sizeof(char),f.sz,o); break; } }

/* dumpf -- dumps the program in its current state:
     contents of dump: all function definitions defined at runtime
                     : all of the contents of the stack */
void dumpf(std::tuple<char *, int> dump, Program *prog, const char *out) {
  FILE *o = fopen(out,"wb");
  // TODO: figure out how to restore dll-bound functions.
  for(Item i : dfuns) { encode_out(i,o); }
  fwrite(std::get<0>(dump),sizeof(char),std::get<1>(dump),o); }

// in case vector is needed and all indexing operations must be switched.
char ind(std::unique_ptr<char[]> dat, int i) { return dat[i]; }

// NOTE: use std::move when moving data array to struct.

/* call -- if callee is a function (__not__ a quote), it will be called on the stack.
           else it will be pushed to the stack. */
void call(Program *prog, Item callee, PuFun pu, PoFun po) {
  if(callee.typ!=FUN_T) { /*prog->deq.push_front*/pu(prog,callee); }
  else { int f = dat__int(callee.dat.get());
         if(f>=FSZ) { read_parse(dfuns.at(f-FSZ).quot,prog); }
         else { (funs.at(f))(prog, pu, po); } } }

/* invoke_mode -- decides what to do with subj by checking the mode.
               -- it might be possible later to define new modes to functions TODO.
     cases: READ_MODE -- simply calls call on the stack with Item subj.
          : REV_MODE  -- reverses the stack. __deprecated__
          : QUOT_MODE -- will simply push subj to the topmost quote (as pushed by quote_read).
                      -- embedded quotes will not be evaluated within the quote. */
void invoke_mode(Program *prog, Item subj) {
  switch(prog->mode) {
  case READ_MODE: call(prog,subj,push,pop); break;
  // ! WARNING: REV_MODE is no longer supported, and is specifically broken by anything involving
  //          : scope.
  case REV_MODE: call(prog,subj,fpush,fpop); break;
  case QUOT_MODE: // TODO: make this cleaner.
                  // DONE: allow for embedded quotes.
                  // !!! WARNING: irregular use of `subj.sz' as counter for embedded brackets.
                  //            : subj.sz will always be 0 given that the quotes are well-formed.
                  if(subj.typ==FUN_T&&dat__int(subj.dat.get())==1/*quote_read mode*/) { subj.sz++; }
                  if(subj.typ==SYM_T&&subj.sz==1&&!memcmp(subj.dat.get(),cl_brkt,1)) { subj.sz--; }
                  if(!subj.sz) { prog->mode = prog->p_modes.top(); prog->p_modes.pop(); }
                  else { prog->deq.back().quot.push_back(subj); } break; } }

// WARNING: do not use outside of read_bytecode due to dependence on bc and z.
#define IN_TOK(SZ, TYPE) \
    Item p; /*z++*/ char *dat = (new char[SZ]); \
    memcpy(dat,&std::get<0>(bc)[z],(SZ)); \
    p = (Item) { std::shared_ptr<char>(dat,std::default_delete<char[]>()) \
               , std::deque<Item>(), (TYPE), (SZ) }; \
    z+=(SZ);

// DONE: write call function and parse with just queue.
/* read_parse -- iterates through quote bcc, where invoke_mode is called for each Item.
              -- it also pushes a new scope to the __front__ of the stack, where
              --   that same scope is destroyed after calling the quote.
              -- after pushing the scope, it will push the quote itself.  This quote
              -- exists if the user were to call recur (note that the quote is destroyed along with
              --   the rest of the scope). */
void read_parse(std::deque<Item> bcc, Program *prog) { 
  scope(prog,push,pop); prog->deq.push_front(item(NULL,bcc,QUOT_T,0)); std::deque<Item> bc = bcc;
  while(!bc.empty()) { invoke_mode(prog,bc.front()); bc.pop_front(); } destroy_scope(prog); }

// DONE: make this less repetitive.
// DONE: try to use something better than char *.
/* read_bytecode -- essentially the exact same as read_parse but does not act on any scope and
                 -- is only usable at the toplevel.  The program will read bytes from a bytestring
                 -- and based off of the byte read, it will read more and construct an Item to
                 -- use.
     cases: R_WORD  -- reads 4 more bytes and stores in an Item of WORD_T.
          : R_DWORD -- reads 8 bytes and stores in Item of DWORD_T.
          : R_FUN   -- reads 4 bytes that represent the address of a function from
                    --   funs (if address < FSZ) or dfuns.  stored in FUN_T.
                    -- this function is called in READ_MODE.
          : R_SYM   -- reads 4 bytes to represent size n and reads n more bytes and stores them
                    --   in Item of SYM_T. */
void read_bytecode(std::tuple<char *,int> bc, Program *prog) { int z = 0;
  while(z<std::get<1>(bc)) { switch(std::get<0>(bc)[z]) {
    case R_WORD: { z++; IN_TOK(sizeof(int),WORD_T) invoke_mode(prog,p); break; }
    case R_DWORD: { z++; IN_TOK(sizeof(int64_t),DWORD_T) invoke_mode(prog,p); break; }
    case R_FUN: { z++; IN_TOK(sizeof(int),FUN_T) invoke_mode(prog,p); break; }
    case R_SYM: { int sz; memcpy(&sz,&std::get<0>(bc)[++z],sizeof(int)); z+=sizeof(int);
                  IN_TOK(sz,SYM_T) invoke_mode(prog,p); break; } } } }

int main(int argc, char **argv) { /*funs.push_back(print_int); funs.push_back(quote_read);
                                  funs.push_back(f_call); funs.push_back(read_mode);
                                  funs.push_back(rev_mode); funs.push_back(d_fun);*/
  SFun fs[] = { print_int,quote_read,f_call,read_mode,rev_mode,d_fun
              , print_flt,print_sym,read_char,curry,if_f,eq
              , add,sub,mul,divi,swap,dup,drop,pick,from_back
              , scope,scope_eq,recur,push_false };
  funs = std::vector<SFun>(fs,&fs[FSZ-1]);
  std::tuple<char *,int> bc = readf("test.sm");
                              // test0
                              //std::make_tuple((char[10]){0,4,0,0,0,2,0,0,0,0},10);
                              // test2
                              //std::make_tuple((char[16]){2,1,0,0,0,0,4,0,0,0,3,1,0,0,0,0x5D},16);
                              // test3
                              //std::make_tuple((char[26]){0,4,0,0,0,2,1,0,0,0,2,0,0,0,0,3,1,0,0,0,0x5D
                              //                          ,2,2,0,0,0},26);
                              // test4
                              //std::make_tuple((char[31]){0,4,0,0,0,2,1,0,0,0,2,0,0,0,0,3,1,0,0,0,0x5D
                              //                          ,2,5,0,0,0,2,6,0,0,0},31);
                              
  Program *prog = new Program; prog->deq = std::deque<Item>(); prog->p_modes = std::stack<int>();
                               prog->mode = READ_MODE;
  read_bytecode(bc,prog); delete[] std::get<0>(bc); delete prog; return 0; }
