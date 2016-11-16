#include <glad/glad.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>

#include "portaudio.h"
#include <math.h>
#include <sndfile.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <time.h>

//#include "glob_structs.h"

#define SZ (100)

#define SAMPLE_RATE (44100)
#define FPB (64)

typedef struct CAut CAut;
typedef struct Cell Cell;
typedef Cell (*CFun2D)(Cell,CAut,int,int);
typedef Cell (*CFun)(Cell,CAut,int *);
struct CAut { Cell *cells; int *fra; int rank;
              union { CFun f; CFun2D f2d; } fs; };
// more efficient 1-generation lookback.
typedef struct { CAut *curr; CAut *next; } SwapCAut;
void swap(SwapCAut *cc) { /* optionally set all c.next to 0 after swap. */
  CAut *t = cc->curr; cc->curr = cc->next; cc->next = t; }

struct Cell { int state; void *data; /* data is for non-integer states */};
Cell cell(int state) { return (Cell) { state, NULL }; }
//typedef struct { void *data; } GCell;

int *int_arr(int sz, ...) { int *r = malloc(sz*sizeof(int)); va_list vl; va_start(vl,sz);
  for(int i=0;i<sz;i++) { r[i] = va_arg(vl,int); } va_end(vl); return r; }

int prod(int *a, int e) { int p = 1; for(int i=e;e>0;e--) { p*=a[e]; } return p; }

CAut c_aut2D(Cell *cells, int *fra, int rank, CFun2D f2d) { CAut r;
  r.cells = cells; r.fra = fra; r.rank = rank; r.fs.f2d = f2d; return r; }
CAut c_aut(Cell *cells, int *fra, int rank, CFun f) { CAut r;
  r.cells = cells; r.fra = fra; r.rank = rank; r.fs.f = f; return r; }
  
int len(CAut c) { int s = 1; for(int i=0;i<c.rank;i++) { s*=c.fra[i]; } return s; }
// TODO: generalize to all dimensions: (xm)(1)+(ym)(fra[0])+(zm)(fra[1]*fra[0])+ ...
Cell index2(CAut a, int x, int y) { int xm = x%a.fra[0]; int ym = y%a.fra[1];
  xm += (x<0)*a.fra[0]; ym += (y<0)*a.fra[1];
  return a.cells[xm+ym*a.fra[0]]; }
Cell index_g(CAut a, int *coords) { int total = 0;
  for(int i=0;i<a.rank;i++) { total += coords[i]%a.fra[i]+((coords[i]<0)*a.fra[i])*prod(a.fra,i); }
  return a.cells[total]; }

void d_rect(GLfloat x, GLfloat y, GLfloat w, GLfloat h) {
  glVertex3f(x,y,0); glVertex3f(x+w,y,0); glVertex3f(x+w,y+h,0);
  glVertex3f(x,y+h,0); }
void d_conway(CAut c, GLFWwindow *win) { int ww, wh;
  glfwGetFramebufferSize(win,&ww,&wh);
  for(int i=0;i<len(c);i++) { if(c.cells[i].state) {
    d_rect(4.0/(GLfloat)c.fra[0]*(i%c.fra[0])-2
          ,2-4.0/(GLfloat)c.fra[1]*(i/c.fra[1])
          ,4.0/(GLfloat)c.fra[0],4.0/(GLfloat)c.fra[1]); } } }

void free_c_aut(CAut c) { free(c.cells); free(c.fra); }
// does not copy data; only frame.
CAut copy_c(CAut c) { CAut n; n.cells = malloc(len(c)*sizeof(Cell));
  n.fra = malloc(c.rank*sizeof(Cell)); memcpy(n.fra,c.fra,c.rank*sizeof(Cell));
  n.rank = c.rank; n.fs = c.fs; return n; }

int get2(CAut c, int x, int y) { return index2(c,x,y).state; }
int get(CAut c, ...) { va_list vl; va_start(vl,c); int *coords = malloc(c.rank*sizeof(int));
  for(int i=0;i<c.rank;i++) { coords[i] = va_arg(vl,int); }
  int a = index_g(c,coords).state; free(coords); va_end(vl); return a; }

Cell id(Cell a, CAut c, int x, int y) { return a; }
Cell f0(Cell a, CAut c, int x, int y) { return cell(!a.state); }
Cell conway(Cell a, CAut c, int x, int y) {
  int neighbors = get2(c,x-1,y)+get2(c,x-1,y-1)+get2(c,x,y-1)+get2(c,x+1,y-1)+get2(c,x+1,y)
                + get2(c,x+1,y+1)+get2(c,x,y+1)+get2(c,x-1,y+1);
  if(a.state) { return cell(neighbors>1&&neighbors<4); }
  return cell(neighbors==3); }
Cell conway_g(Cell a, CAut c, int *co) {
  int neighbors = get(c,co[0]-1,co[1])+get(c,co[0]-1,co[1]-1)
                + get(c,co[0],co[1]-1)+get(c,co[0]+1,co[1]-1)+get(c,co[0]+1,co[1])
                + get(c,co[0]+1,co[1]+1)+get(c,co[0],co[1]+1)+get(c,co[0]-1,co[1]+1);
  if(a.state) { return cell(neighbors>1&&neighbors<4); }
  return cell(neighbors==3); }

// DONE: make this faster since reallocating every time isn't ideal.
// TODO: Generalize to all dimensions.
//   int_arr(i%c->fra[0],i%(c->fra[1]*c->fra[0])/c->fra[0]
//          ,i%(c->fra[2]*c->fra[1]*c->fra[0])/(c->fra[1]*c->fra[0]))
void next_state2D(CAut *c, CAut *n) {
  for(int i=0;i<len(*c);i++) {
    n->cells[i] = c->fs.f2d(c->cells[i],*c,i%c->fra[0],i/c->fra[0]); } }
// TODO: test this.
void next_state(CAut *c, CAut *n) { int *coords = malloc(c->rank*sizeof(int));
  for(int i=0;i<len(*c);i++) {
    for(int q=0;q<c->rank;q++) { coords[q] = i%prod(c->fra,q+1)/prod(c->fra,q); }
    n->cells[i] = c->fs.f(c->cells[i],*c,coords); } free(coords); }

const char *ver_v =
  "void main(void) { vec4 v = vec4(gl_Vertex);"
  "gl_FrontColor = gl_Color; gl_Position = gl_ModelViewProjectionMatrix*v; }";
const char *frag_v = "void main(void) { vec4 v = vec4(gl_Color);"
  "gl_FragColor = v; }";

static GLuint mk_shader(GLenum type, const char *src) {
  GLuint ok; GLsizei ll; char info_log[8192];
  GLuint shader = glCreateShader(type); glShaderSource(shader, 1, (const GLchar **)&src, NULL);
  glCompileShader(shader); glGetShaderiv(shader, GL_COMPILE_STATUS, &ok); 
  if(ok != GL_TRUE) { glGetShaderInfoLog(shader, 8192, &ll, info_log);
    fprintf(stderr, "ERROR: \n%s\n\n", info_log); glDeleteShader(shader); shader = 0u; }
  return shader; }
static GLuint mk_shader_program(const char *vs_src, const char *fs_src) { GLuint prog = 0u;
  GLuint vs = 0u; GLuint fs = 0u; vs = mk_shader(GL_VERTEX_SHADER, vs_src);
  fs = mk_shader(GL_FRAGMENT_SHADER, fs_src); prog = glCreateProgram();
  glAttachShader(prog,vs); glAttachShader(prog,fs); glLinkProgram(prog); return prog; }

// remember to free.
// WARNING: arguments passed to the function MUST be GLdoubles.
GLfloat *farr(int sz, ...) { va_list vl; va_start(vl,sz); GLfloat *a = malloc(sz*sizeof(GLfloat));
  for(int i=0;i<sz;i++) { a[i] = (GLfloat)va_arg(vl,GLdouble); } va_end(vl); return a; }

void perspective(GLdouble fovy, GLdouble asp, GLdouble znear, GLdouble zfar) {
  const GLdouble pi = 3.14159265358979323846264338327;
  GLdouble top = znear*tan(pi/360.*fovy);
  glFrustum(-top*asp,top*asp,-top,top,znear,zfar); }

/*void init_gl(GLFWwindow *win) {
  float ratio; int width, height; glfwGetFramebufferSize(win, &width, &height);
  ratio = width / (float) height; glViewport(0,0,width,height);
  glClear(GL_COLOR_BUFFER_BIT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity(); glOrtho(0,1,0,1,0,1); glMatrixMode(GL_MODELVIEW);
  glLoadIdentity(); }*/
void rsz(GLFWwindow *win, int w, int h) { glViewport(0,0,w,h);
  glMatrixMode(GL_PROJECTION); glLoadIdentity();
  perspective(45,w/(float)h,0.1,100); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
  glFlush(); }
void init_gl(GLFWwindow *win) { glShadeModel(GL_SMOOTH);
  glClearColor(0,0,0,0); glClearDepth(1); glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL); glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);
  glEnable(GL_LIGHTING); glEnable(GL_LIGHT0); int w, h; glfwGetFramebufferSize(win,&w,&h);
  rsz(win,w,h); }

static void error_callback(int error, const char* description) {
  fprintf(stderr, "Error: %s\n", description); }

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height); }

int any_eq(int a, int *b, int fro, int to) { int i; for(int i=fro;i<to&&b[i]!=a;i++);
  return i>=to?-1:i; }

void paint(GLFWwindow *win, CAut *c) { glLoadIdentity();
  glTranslatef(0,0,/*-1.5*/-5);

  glColor4f(1.0,0.0,0.0,1.0);
  glBegin(GL_QUADS); 
    //glVertex3f(-0.05,0,0.05); glVertex3f(0.05,0,0.05);
    //glVertex3f(0.05,0.1,0.05); glVertex3f(-0.05,0.1,0.05);
    d_conway(*c,win);
  glEnd(); }

int pressed(GLFWwindow *win, int k) { return glfwGetKey(win,k)==GLFW_PRESS; }

//void set_k(GLfloat a[RC], GLfloat b[RC]) { for(int i=0;i<RC;i++) { a[i] = b[i]; } }
// all key processing happens here.
/*KState getInput(GLFWwindow *win) { KState n;
  n.x = pressed(win,GLFW_KEY_D)-pressed(win,GLFW_KEY_A);
  n.z = pressed(win,GLFW_KEY_W)-pressed(win,GLFW_KEY_S);
  n.y = pressed(win,GLFW_KEY_R)-pressed(win,GLFW_KEY_F); 
  n.tht = pressed(win,GLFW_KEY_UP)-pressed(win,GLFW_KEY_DOWN);
  n.phi = pressed(win,GLFW_KEY_RIGHT)-pressed(win,GLFW_KEY_LEFT); return n; }*/

/*int *getKeys(int keys[KC], GLFWwindow *win) { int a[KC];
  for(int i=0;i<ksz;i++) { a[i] = glfwGetKey(win,keys[i]); } return a; }*/
// TODO: make into a vect3 the position.
/*void procInput(GState *g, GLFWwindow *win) { KState a =  getInput(win);
  GLfloat cx = sin(deg_rad(g->ca.cxz)); GLfloat cz = -cos(deg_rad(g->ca.cxz));
  
  // change acceleration and velocity
  g->pl.acc = v3(0.,g->gravity,0.); g->pl.vel = vec_add(g->pl.acc,g->pl.vel); 
  // change position
  g->pl.x += -a.z*0.01*cx-a.x*0.01*cz+g->pl.vel.x;
  g->pl.y += a.y*0.01+g->pl.vel.y; g->pl.z += -a.z*0.01*cz+a.x*0.01*cx+g->pl.vel.z;
  // change camera position
  g->ca.cxz += a.phi; g->ca.cyz += a.tht; g->lk = a; }*/

/* Initialize PortAudio; pass to PortAudio the GState; use function that takes the state and returns
     an output sample (for example, if the state function is to return a sine function, then
                       it will just return the sample of the sine at the GState's time).
     The function would usually have a (Predicate,SndState) array.  When sounds are to be added,
     the PortAudio stream stops and the new sound is added to the array with a given predicate.
     All predicates take a GState as their input. */
int main(void) {
    srand(time(NULL));
    CAut *c = malloc(sizeof(CAut)); CAut *bc = malloc(sizeof(CAut));
    
    Cell *a = malloc(SZ*SZ*sizeof(Cell));
    for(int i=0;i<SZ*SZ;i++) {
      a[i].state = rand()%2; }
    int *fra = malloc(2*sizeof(int)); fra[0] = fra[1] = SZ;
    *c = c_aut2D(a,fra,2,conway);

    Cell *b = calloc(SZ*SZ,sizeof(Cell));
    int *bfra = malloc(2*sizeof(int)); bfra[0] = bfra[1] = SZ;
    *bc = c_aut2D(b,bfra,2,conway);

    SwapCAut cc = (SwapCAut) { c, bc };

    //PaStreamParameters oP; PaStream *stream;
    GLFWwindow* window;

    /*Pa_Initialize();
    glfwSetErrorCallback(error_callback);

    oP.device = Pa_GetDefaultOutputDevice();
    oP.channelCount = 2;
    oP.sampleFormat = paFloat32;
    oP.suggestedLatency = Pa_GetDeviceInfo(oP.device)->defaultLowOutputLatency;
    oP.hostApiSpecificStreamInfo = NULL;

    Pa_OpenStream(&stream,NULL,&oP,SAMPLE_RATE,FPB,paClipOff,detCallback,&g);*/

    if (!glfwInit())
        exit(EXIT_FAILURE);

    window = glfwCreateWindow(400, 400, "hangmak_sound", NULL, NULL);
    if(!window) { glfwTerminate(); exit(EXIT_FAILURE); }

    glfwMakeContextCurrent(window);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glfwSwapInterval(1);

    GLuint prog = 0u; prog = mk_shader_program(ver_v, frag_v);
    glUseProgram(prog);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    init_gl(window);
    //glfwSetKeyCallback(window, key_callback);
    //psound_det(stream);
    // this is just a temporary time keeper; it is unreliable.
    int time = 0;
    while (!glfwWindowShouldClose(window)) {
      glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT); paint(window,cc.curr);
      //procInput(&g,window);
      if(!((time++)%3)) { next_state2D(cc.curr,cc.next); swap(&cc); }
      glfwSwapBuffers(window);
      glfwPollEvents(); }
    error:
    //Pa_StopStream(stream); Pa_CloseStream(stream); Pa_Terminate();

    glfwTerminate();
    exit(EXIT_SUCCESS);
}


