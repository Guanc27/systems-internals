#include "smalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <math.h>
#include <inttypes.h>
#include <time.h>
#include <stdbool.h>


/* Misc */
#define MAXLINE     1024 /* max string size */
#define HDRLINES       4 /* number of header lines in a trace file */
#define LINENUM(i) (i+5) /* cnvt trace request nums to linenums (origin 1) */

/* Config */
#define ALIGNMENT 8

/* Returns true if p is ALIGNMENT-byte aligned */
#define IS_ALIGNED(p)  ((((uintptr_t)(p)) % ALIGNMENT) == 0)


/* Records the extent of each block's payload */
typedef struct range_t {
    char *lo;              /* low payload address */
    char *hi;              /* high payload address */
    struct range_t *next;  /* next list element */
} range_t;

/* Characterizes a single trace operation (allocator request) */
typedef struct {
    enum {ALLOC, FREE, REALLOC, GETHEAD, GETNEXT, GETPREV} type; /* type of request */
    int index;                        /* index for free() to use later */
    int size;                         /* byte size of alloc/realloc request */
} traceop_t;

/* Holds the information for one trace file*/
typedef struct {
    int sugg_heapsize;   /* suggested heap size */
    int num_ids;         /* number of alloc/realloc ids */
    int num_ops;         /* number of distinct requests */
    int weight;          /* weight for this trace (unused) */
    traceop_t *ops;      /* array of requests */
    char **blocks;       /* array of ptrs returned by malloc/realloc... */
    size_t *block_sizes; /* ... and a corresponding array of payload sizes */
    char **free_blocks;  /* array of ptrs freed by free */
} trace_t;

/* 
 * Holds the params to the xxx_speed functions, which are timed by fcyc. 
 * This struct is necessary because fcyc accepts only a pointer array
 * as input.
 */
typedef struct {
    trace_t *trace;  
    range_t *ranges;
} speed_t;

/* Summarizes the important stats for some malloc function on some trace */
typedef struct {
    /* defined for both libc malloc and student malloc package (mm.c) */
    double ops;      /* number of ops (malloc/free/realloc) in the trace */
    int valid;       /* was the trace processed correctly by the allocator? */
    double secs;     /* number of secs needed to run the trace */

    /* defined only for the student malloc package */
    double util;     /* overall space utilization for this trace (always 0 for libc) */

    double inst_util;     /* instanteous space utilization for this trace (always 0 for libc) */

    /* Note: secs and util are only defined if valid is true */
} stats_t; 

char msg[MAXLINE];      /* for whenever we need to compose an error message */


static int eval_valid(trace_t *trace, range_t **ranges, char* output_filepath);
static trace_t *read_trace(char *filename);
static void usage(void);
void free_trace(trace_t *trace);
void malloc_error(int opnum, char *msg);
void app_error(char *msg);
void unix_error(char *msg);
void write_status(Malloc_Status* status, FILE* outputfile);
void check_head_pointer_validity(Pointer_Status* status, FILE* outputfile, int offset);
void check_pointer_validity(FILE* outputfile, int offset);

/**************
 * Main routine
 **************/
int main(int argc, char **argv)
{
    int i;
    char c;
    char* tracefile = NULL;  /* trace file name */
    char* output_filepath = NULL; /* output file */
    trace_t *trace;     /* stores a single trace file in memory */
    range_t *ranges = NULL;    /* keeps track of block extents for one trace */
    //  stats_t *libc_stats = NULL;/* libc stats for trace */
    stats_t my_stats;  /* smalloc stats for trace */
    speed_t speed_params;      /* input parameters to the xx_speed routines */ 
    
    if (argc == 1) {
        usage();
        exit(1);
    }
    /* 
    * Read and interpret the command line arguments 
    */
    while ((c = getopt(argc, argv, "o:t:hvVgal")) != EOF) {
        switch (c) {
            case 't': /* Use one specific trace file only (relative to curr dir) */
                tracefile = strdup(optarg);
                printf("testing trace file: %s\n", tracefile);
                break;
            case 'o':
                output_filepath = strdup(optarg);
                printf("output will be written to: %s\n", output_filepath);
                break;
            case 'h': /* Print this message */
                usage();
                exit(0);
            default:
                usage();
                exit(1);
        }
    }

    if (!tracefile) {
        printf("ERROR: Please provide the tracefile!!!\n\n");
        usage();
        exit(1);
    }

    if (!output_filepath) {
        printf("ERROR: Please provide the output file path!!!\n\n");
        usage();
        exit(1);
    }

	trace = read_trace(tracefile);
	my_stats.ops = trace->num_ops;
 
	my_stats.valid = eval_valid(trace, &ranges, output_filepath);
	free_trace(trace);

    printf("output has been written to: %s\n", output_filepath);
    exit(0);
}


static int eval_valid(trace_t *trace, range_t **ranges, char* output_filepath) 
{
    int i;
    int index;
    int size;
    int offset;
    char *newp;
    char *oldp;
    char *p;

    Malloc_Status status;
    Pointer_Status pstatus;
    FILE *outputfile;

    if ((outputfile = fopen(output_filepath, "w")) == NULL) {
        sprintf(msg, "Could not open %s for writting output", output_filepath);
        unix_error(msg);
    }


    /* Call the mm package's init function */
    if (my_init(trace->sugg_heapsize) < 0) {
        malloc_error(0, "mm_init failed.");
        return 0;
    }

    /* Interpret each operation in the trace in order */
    for (i = 0;  i < trace->num_ops;  i++) {
        index = trace->ops[i].index;
        size = trace->ops[i].size;

        switch (trace->ops[i].type) {
            case ALLOC: /* smalloc */
                /* Call the student's malloc */
                
                
                status.success = -2;
                status.payload_offset = -2;
                status.hops = -2;

                p = smalloc(size, &status);
                
                write_status(&status, outputfile);

                /* Remember region */
                trace->blocks[index] = p;
                trace->block_sizes[index] = size;
                break;

            case FREE: /* sfree */
                /* Remove region from list and call student's free function */
                p = trace->blocks[index];
                trace->free_blocks[index] = p;
                sfree(p);
                break;
            
            case GETHEAD: /* get head */
                offset = get_head_pointer(&pstatus);
                check_head_pointer_validity(&pstatus, outputfile, offset);
                break;
            
            case GETNEXT: /* get next */
                p = trace->free_blocks[index];
                offset = get_next_pointer(p);
                check_pointer_validity(outputfile, offset);
                break;

            case GETPREV: /* get prev */
                p = trace->free_blocks[index];
                offset = get_prev_pointer(p);
                check_pointer_validity(outputfile, offset);
                break;

        default:
            app_error("Nonexistent request type in eval_valid");
            return -1;
        }

    }

    fclose(outputfile);
    /* As far as we know, this is a valid malloc package */
    return 1;
}


/*
 * read_trace - read a trace file and store it in memory
 */
static trace_t *read_trace(char *filename)
{
    FILE *tracefile;
    trace_t *trace;
    char type[MAXLINE];
    char path[512];
    unsigned index, size;
    unsigned max_index = 0;
    unsigned op_index;

    /* Allocate the trace record */
    if ((trace = (trace_t *) malloc(sizeof(trace_t))) == NULL)
        unix_error("malloc 1 failed in read_trance");

    /* Read the trace file header */
    strcpy(path, filename);
    if ((tracefile = fopen(path, "r")) == NULL) {
        sprintf(msg, "Could not open %s in read_trace", path);
        unix_error(msg);
    }
    fscanf(tracefile, "%d", &(trace->sugg_heapsize));
    fscanf(tracefile, "%d", &(trace->num_ids));     
    fscanf(tracefile, "%d", &(trace->num_ops));     
    fscanf(tracefile, "%d", &(trace->weight));        /* not used */

    /* We'll store each request line in the trace in this array */
    if ((trace->ops = 
        (traceop_t *)malloc(trace->num_ops * sizeof(traceop_t))) == NULL)
        unix_error("malloc 2 failed in read_trace");

    /* We'll keep an array of pointers to the allocated blocks here... */
    if ((trace->blocks = 
        (char **)malloc(trace->num_ids * sizeof(char *))) == NULL)
        unix_error("malloc 3 failed in read_trace");

    /* ... along with the corresponding byte sizes of each block */
    if ((trace->block_sizes = 
        (size_t *)malloc(trace->num_ids * sizeof(size_t))) == NULL)
        unix_error("malloc 4 failed in read_trace");

    /* Keep an array of pointers to the freed blocks here... */
    if ((trace->free_blocks = 
        (char **)malloc(trace->num_ids * sizeof(char *))) == NULL)
        unix_error("malloc 5 failed in read_trace");

    /* read every request line in the trace file */
    index = 0;
    op_index = 0;
    while (fscanf(tracefile, "%s", type) != EOF) {
        switch(type[0]) {
            case 'a':
                fscanf(tracefile, "%u %u", &index, &size);
                trace->ops[op_index].type = ALLOC;
                trace->ops[op_index].index = index;
                trace->ops[op_index].size = size;
                max_index = (index > max_index) ? index : max_index;
                break;
            case 'r':
                fscanf(tracefile, "%u %u", &index, &size);
                trace->ops[op_index].type = REALLOC;
                trace->ops[op_index].index = index;
                trace->ops[op_index].size = size;
                max_index = (index > max_index) ? index : max_index;
                break;
            case 'f':
                fscanf(tracefile, "%ud", &index);
                trace->ops[op_index].type = FREE;
                trace->ops[op_index].index = index;
                break;
            case 'h':
                trace->ops[op_index].type = GETHEAD;
                break;
            case 'n':
                fscanf(tracefile, "%ud", &index);
                trace->ops[op_index].type = GETNEXT;
                trace->ops[op_index].index = index;
                break;
            case 'p':
                fscanf(tracefile, "%ud", &index);
                trace->ops[op_index].type = GETPREV;
                trace->ops[op_index].index = index;
                break;
            default:
                printf("Bogus type character (%c) in tracefile %s\n", 
                type[0], path);
                exit(1);
        }
        op_index++;

    }
    fclose(tracefile);
    assert(max_index == trace->num_ids - 1);
    assert(trace->num_ops == op_index);

    return trace;
}


/*
 * free_trace - Free the trace record and the three arrays it points
 *              to, all of which were allocated in read_trace().
 */
void free_trace(trace_t *trace)
{
    free(trace->ops);         /* free the three arrays... */
    free(trace->blocks);      
    free(trace->free_blocks);      
    free(trace->block_sizes);
    free(trace);              /* and the trace record itself... */
}


void write_status(Malloc_Status* status, FILE* outputfile)
{
    char line[MAXLINE];
    sprintf(line, "%d\t%d\t%d\n", status->success, status->payload_offset, status->hops);
    // printf("%s", line);
    fprintf(outputfile, "%s", line);
}

void check_head_pointer_validity(Pointer_Status* status, FILE* outputfile, int offset)
{
    char line[MAXLINE];
    sprintf(line, "%d\t%d\t%d\n", status->success, offset, status->block_size);
    // printf("%s", line);
    fprintf(outputfile, "%s", line);
}

void check_pointer_validity(FILE* outputfile, int offset)
{
    char line[MAXLINE];
    sprintf(line, "%d\t%d\t%d\n", offset, offset, offset);
    // printf("%s", line);
    fprintf(outputfile, "%s", line);
}


/*
 * malloc_error - Report an error returned by the mm_malloc package
 */
void malloc_error(int opnum, char *msg)
{
    printf("ERROR [line %d]: %s\n", LINENUM(opnum), msg);
}


/* 
 * app_error - Report an arbitrary application error
 */
void app_error(char *msg) 
{
    printf("%s\n", msg);
    exit(1);
}

/* 
 * unix_error - Report a Unix-style error
 */
void unix_error(char *msg) 
{
    printf("%s: %s\n", msg, strerror(errno));
    exit(1);
}


/* 
 * usage - Explain the command line arguments
 */
static void usage(void) 
{
    fprintf(stderr, "Usage: ./test [-t <tracefile>] [-o <output_filepath>]\n");
    fprintf(stderr, "Options\n");
    fprintf(stderr, "\t-t <tracefile>  Use <tracefile> as the trace file path.\n");
    fprintf(stderr, "\t-o <output_filepath>  Use <output_filepath> as the output file path.\n");
}