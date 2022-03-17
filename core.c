#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

// low 7 bits of each memory cell is looked up and its code field executed
//
// white -> yellow -> red
// red words are executed (fade to black as their heat is radiated away)
// yellow words are compiled to red words
// white words are compiled to yellow words
//
// full contents of memory cell in xreg
// pointer to word header in wreg
//

#define MEM_SIZE        0x10000
#define BLOCK_SIZE      0x400

#define SYM_YELLOW      1
#define SYM_WHITE       2
#define SYM_CYAN        3
#define SYM_MAGENTA     4

#define HEADER_SIZE     2
#define HEADER_CFA      0
#define HEADER_DFA      1

#define DSTACK_SIZE     0x100
#define RSTACK_SIZE     0x100
#define TABLE_SIZE      0x100

#define TABLE_ADR       0
#define DSTACK_ADR      (TABLE_ADR+TABLE_SIZE)
#define RSTACK_ADR      (DSTACK_ADR+DSTACK_SIZE)
#define HERE_ADR        (RSTACK_ADR+RSTACK_SIZE)
#define HEAP_START      (HERE_ADR+1)


size_t mem[MEM_SIZE];
size_t dsp, rsp, ip, xreg, wreg;


void run(void) {
    void (*fun)(void);
    while(1) {
        xreg = mem[ip++];
        wreg = mem[(xreg&0x7f)*2 + 1];
        fun = (void*)mem[(xreg&0x7f)*2];
        fun();
    }
}

static void define_sym(char c, void (*fun)(void), size_t dfa) {
    mem[TABLE_ADR + 2*((int)c)] = (size_t)fun;
    mem[TABLE_ADR + 1 + 2*((int)c)] = dfa;
}

static void comma(size_t x) {
    mem[mem[HERE_ADR]++] = x;
}

static void code_enter(void) {
    mem[--rsp] = ip;
    ip = wreg;
}

static void code_exit(void) {
    ip = mem[rsp++];
}

static void code_lit(void) {
    mem[--dsp] = mem[ip++];
}

static void code_branch(void) {
    ip = mem[ip];
}

static void code_call(void) {
    mem[--rsp] = ip+1;
    ip = mem[ip];
}

static void code_cbranch(void) {
    size_t cond = mem[dsp++];
    size_t adr = mem[ip];
    if (! cond) ip = adr; else ip++;
}

static void code_emit(void) {
    putchar(mem[dsp++] & 0xff);
}

static void code_read(void) {
    mem[--dsp] = getchar();
}

static void code_bye(void) {
    exit(0);
}

static void code_here(void) {
    mem[--dsp] = HERE_ADR;
}

static void code_fetch(void) {
    mem[dsp] = mem[mem[dsp]];
}

static void code_rpop(void) {
    size_t x = mem[rsp++];
    mem[--dsp] = x;
}

static void code_rpush(void) {
    size_t x = mem[dsp++];
    mem[--rsp] = x;
}

static void code_inc(void) {
    mem[dsp]++;
}

static void code_dup(void) {
    mem[dsp-1] = mem[dsp];
    dsp--;
}

static void code_drop(void) {
    dsp++;
}

static void code_sub(void) {
    mem[dsp+1] -= mem[dsp];
    dsp++;
}

static void code_divmod(void) {
    size_t tos = mem[dsp], nos = mem[dsp+1];
    mem[dsp] = nos / tos;
    mem[dsp+1] = nos % tos;
}

static void code_add(void) {
    mem[dsp+1] += mem[dsp];
    dsp++;
}

static void code_mul(void) {
    mem[dsp+1] *= mem[dsp];
    dsp++;
}

static void code_and(void) {
    mem[dsp+1] &= mem[dsp];
    dsp++;
}

static void code_swap(void) {
    const size_t t = mem[dsp];
    mem[dsp] = mem[dsp+1];
    mem[dsp+1] = t;
}

static void code_double(void) {
    mem[dsp] *= 2;
}

static void code_store(void) {
    const size_t a = mem[dsp++];
    const size_t x = mem[dsp++];
    mem[a] = x;
}

static void code_unimplemented(void) {
    fprintf(stderr, "Unimplemented operation: '%c' at 0x%Zx\n",
            (int)xreg, ip-1);
    exit(1);
}

static void code_yellow(void) {
    comma((xreg >> 8) & 0x7f);
}

static void code_white(void) {
    comma((xreg & 0xff00) | SYM_YELLOW);
}

static void code_nop(void) {
}

static void code_magenta(void) {
    define_sym((xreg >> 8) & 0x7f, code_enter, mem[HERE_ADR]);
}

static void initialize(void) {
    // initialize heap
    mem[HERE_ADR] = HEAP_START;
    // allocate data stack
    dsp = DSTACK_ADR + DSTACK_SIZE;
    // allocate return stack
    rsp = RSTACK_ADR + RSTACK_SIZE;

    // initialize all dictionary entries to code_unimplemented so we can catch
    // invalid opcodes/undefined symbols
    for(int c=0; c<0x80; c++)
        define_sym(c, code_unimplemented, 0);

    // special symbols, if printed bits 8--14 contain their ASCII value
    define_sym(SYM_YELLOW, code_yellow, 0);
    define_sym(SYM_WHITE, code_white, 0);
    define_sym(SYM_CYAN, code_nop, 0);
    define_sym(SYM_MAGENTA, code_magenta, 0);

    define_sym('B', code_bye, 0);
    define_sym('D', code_dup, 0);
    define_sym('E', code_emit, 0);
    define_sym('F', code_call, 0);
    define_sym('H', code_here, 0);
    define_sym('I', code_inc, 0);
    define_sym('J', code_branch, 0);
    define_sym('L', code_lit, 0);
    define_sym('O', code_double, 0);
    define_sym('P', code_rpush, 0);
    define_sym('Q', code_rpop, 0);
    define_sym('R', code_read, 0);
    define_sym('S', code_swap, 0);
    define_sym('V', code_drop, 0);
    define_sym(';', code_exit, 0);
    define_sym('@', code_fetch, 0);
    define_sym('!', code_store, 0);
    define_sym('+', code_add, 0);
    define_sym('*', code_mul, 0);
    define_sym('&', code_and, 0);
    define_sym('%', code_divmod, 0);
    define_sym('-', code_sub, 0);
    define_sym('?', code_cbranch, 0);
}

void load_block(const char *filename, size_t offset, size_t n_words) {
    uint16_t data[n_words];
    size_t n_read;
    FILE *file;
    if((file = fopen(filename, "rb")) == NULL) {
        perror("Unable to open file");
        exit(1);
    }
    if((n_read = fread(data, 2, n_words, file)) != n_words) {
        fprintf(stderr, "Read %Zd words from block file but expected %Zd\n",
                n_read, n_words);
        exit(1);
    }
    fclose(file);
    for(size_t i=0; i<n_words; i++)
        mem[offset+i] = data[i];
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "./%s blockfile\n", argv[0]);
        exit(1);
    }
    initialize();
    ip = MEM_SIZE-BLOCK_SIZE;
    load_block(argv[1], ip, BLOCK_SIZE);
    run();
}

