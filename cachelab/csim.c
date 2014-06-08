/*
 * csim.c - Cache simulator
 *
 * Sandra Ros Hrefnu Jonsdottir, sandrarj13@ru.is
 *
 * This cache simulator takes in a valgrind trace file and simulates a cache memory,
 * using the operations in the valgrind trace. It works for arbitrary cache sizes,
 * the cache size is determined by the values passed with the -s <s>, -E <E> and -b <b>
 * flags (set index bits, associativity and block bits, respectively).
 *
 * The output is the number of hits, misses and evictions. Optionally verbose output can
 * be enabled by using the -v flag, this causes the simulator to print out the trace,
 * indicating whether there was a hit, miss or eviction (or any combination of the three
 * possibilities).
 */

#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>

#define ADDRESS_BITS 64

// Function prototypes
FILE *parse_arguments(int argc, char **argv, int *s, int *E, int *b);
void check_cache(unsigned long long addr, void *the_sets, unsigned long long *masks, int *hits, int *misses, int *evictions, int assoc, int set_bits, int block_bits);
void print_usage();

// Structs to simulate the cache and sets and lines
typedef struct {
    int valid;
    int last_used;
    unsigned long long line;
} line;

typedef struct {
    line *set_lines;
} set;

static int verb = 0;

int main(int argc, char **argv)
{
    // Variables to track hits, misses and evictions
    int hit_count = 0, miss_count = 0, eviction_count = 0;
    
    // Variables for the command line arguments.
    //  assoc      = <E>
    //  set_bits   = <s>
    //  block_bits = <b>
    int assoc, set_bits, block_bits, tag_bits;
    
    // Number of sets (S = 2^<s>)
    int set_amount;
    
    // Bit masks to extract the tag, set index and block offset
    // (block offset might be unneeded).
    // addr_masks[0] = tag_mask
    // addr_masks[1] = set_mask
    unsigned long long addr_masks[2];

    // Cache
    set *sets;
    line *set_line;
    
    // The trace file to be replayed.
    FILE *trace_file;
    
    // Variables to be used when parsing a line of the trace file.
    // op: Operation character
    // addr: 64 bit hex address
    // size: number of bytes accessed by the operation
    char op;
    unsigned long long addr;
    int size, check;

    // Parse the arguments passed from the command line and open the trace file for reading.
    trace_file = parse_arguments(argc, argv, &set_bits, &assoc, &block_bits);

    // Calculate the number of tag bits, number of sets and the block size (in bytes).
    tag_bits = ADDRESS_BITS - (set_bits + block_bits);
    set_amount = (int)pow(2.0, (double)set_bits);

    // Create the bit masks for the tag, set index and block offset.
    addr_masks[0] = ((unsigned long long)pow(2.0, (double)tag_bits) - 1) << (set_bits + block_bits);
    addr_masks[1] = ((unsigned long long)pow(2.0, (double)set_bits) - 1) << block_bits;

    // Allocate memory for the cache.
    if((set_line = (line *)malloc(sizeof(line) * (set_amount * assoc))) == NULL)
    {
        printf("lines malloc error\n");
        exit(1);
    }
    if((sets = (set *)malloc(sizeof(set) * set_amount)) == NULL)
    {
        printf("sets malloc error\n");
        exit(1);
    }
    
    for(int i = 0; i < (set_amount * assoc); i++)
    {
        set_line[i].valid = 0;
        set_line[i].last_used = 0;
        set_line[i].line = 0;
    }
    // Set the set_lines pointer to the proper offset of lines. Starting at set_line
    for(int i = 0; i < set_amount; i++)
        sets[i].set_lines = &set_line[i * assoc];
    
    // Main while loop.
    //  Parse the trace file line by line.
    //  Check for hits, misses and evictions.
    while((check = fscanf(trace_file, " %c %llx,%d\n", &op, &addr, &size)) != EOF)
    {
        if(op == 'I')
            continue;
        if(check < 3)
        {
            printf("Error reading line, count of items read: %d\nop: %c --- addr: %llx --- size: %d\n", check, op, addr, size);
            exit(1);
        }
        if(verb)
            printf("%c %llx,%d ", op, addr, size);
        if(op == 'L' || op == 'S')
            check_cache(addr, (void *)sets, addr_masks, &hit_count, &miss_count, &eviction_count, assoc, set_bits, block_bits);
        else if(op == 'M')
        {
            // Load part
            check_cache(addr, (void *)sets, addr_masks, &hit_count, &miss_count, &eviction_count, assoc, set_bits, block_bits);
            // Followed by a Store part.
            check_cache(addr, (void *)sets, addr_masks, &hit_count, &miss_count, &eviction_count, assoc, set_bits, block_bits);
        }
        if(verb)
            printf("\n");
    }

    // Close the trace file.
    if(fclose(trace_file) != 0)
    {
        printf("Error closing file.\n");
        exit(1);
    }

    free(sets);
    free(set_line);

    // Print the summary.
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}

/*
 * parse_arguments - Parse the command line arguments.
 *
 * Start by checking if there are too few or too many arguments, and if the help flag is set.
 * Loop through the argument array and look for the arguments.
 * Print usage information if required.
 *
 * Returns the FILE argument of the trace file being opened.
 */
FILE *parse_arguments(int argc, char **argv, int *s, int *E, int *b)
{
    // Check if there are enough or too many arguments.
    if(argc < 9 || argc > 11)
    {
        for(int i = 0; i < argc; i++)
        {
            // See if the help flag is set, if it is we preint out the help message.
            if(strcmp(argv[i], "-h") == 0)
            {
                printf("-h: Prints usage info.\n");
                printf("-v: Enable verbose output that displays trace info.\n");
                printf("-s <s>: Number of set index bits (S = 2^s is the number of sets).\n");
                printf("-E <E>: Associativity (number of lines per set).\n");
                printf("-b <b>: Number of block bits (B = 2^b is the block size).\n");
                printf("-t <tracefile>: Name of the valgrind trace to replay.\n");
                exit(1);
            }
        }
        // Otherwise we print out the usage info.
        print_usage();
    }

    char *trace;
    FILE *trace_file;
    int flags = 0;
    
    // Parse command line
    for(int i = 0; i < argc; i++)
    {
        if(strcmp(argv[i], "-v") == 0)
            verb = 1;
        if(strcmp(argv[i], "-s") == 0)
        {
            *s = atoi(argv[i + 1]);
            flags++;
        }
        if(strcmp(argv[i], "-E") == 0)
        {
            *E = atoi(argv[i + 1]);
            flags++;
        }
        if(strcmp(argv[i], "-b") == 0)
        {
            *b = atoi(argv[i + 1]);
            flags++;
        }
        if(strcmp(argv[i], "-t") == 0)
        {
            trace = argv[i + 1];
            if((trace_file = fopen(trace, "r")) == NULL)
            {
                printf("Failed to open %s, exiting.\n", trace);
                exit(1);
            }
            flags++;
        }
    }
    
    // Check if all required flags were set.
    if(flags != 4)
    {
        // print out usage info and exit.
        print_usage();
    }
    
    return trace_file;
}

/*
 * check_cache - Checks the cache for a hit, miss or eviction.
 *
 * Start by scanning the cache to see if there's a hit and keep track of any free lines. If there is, raise the hit counter.
 * If no hit, raise the miss counter. If there was a free line fill the first free line, if every line is full, evict the
 * least recently used line by finding the oldest line.
 */
void check_cache(unsigned long long addr, void *the_sets, unsigned long long *masks, int *hits, int *misses, int *evictions, int assoc, int set_bits, int block_bits)
{
    set *sets = (set *)the_sets;
    int set_index = (addr & (int)masks[1]) >> block_bits;
    unsigned long long tag = (addr & masks[0]) >> (block_bits + set_bits);
    int free = assoc, hit = 0, max = 0, index = 0;
    
    // Check for hits.
    for(int i = 0; i < assoc; i++)
    {
        // If the valid bit is set.
        if(sets[set_index].set_lines[i].valid == 1)
        {
            // Check if the tags match.
            if(tag == ((sets[set_index].set_lines[i].line & masks[0]) >> (block_bits + set_bits)))
            {
                if(verb)
                    printf("hit ");
                // Line is being used, so set last_used to 0
                sets[set_index].set_lines[i].last_used = 0;
                // Raise hit count and set the hit flag.
                (*hits)++;
                hit = 1;
                break;
            }
            // Not a hit, so reduce the free line counter.
            free--;
            // Raise the last_used of the line by one.
            sets[set_index].set_lines[i].last_used++;
        }
    }
    // Wasn't a hit, so process a miss.
    if(!hit)
    {
        if(verb)
            printf("miss ");
        // Raise miss count.
        (*misses)++;
        // If at least one line was free, we don't need to evict.
        if(free > 0)
        {
            // Go through all the lines and use the first one that's not being used.
            for(int i = 0; i < assoc; i++)
            {
                if(sets[set_index].set_lines[i].valid == 0)
                {
                    sets[set_index].set_lines[i].valid = 1;
                    sets[set_index].set_lines[i].last_used = 0;
                    sets[set_index].set_lines[i].line = addr;
                    break;
                }
            }
        }
        // We need to evict the least-recently used line.
        else
        {
            if(verb)
                printf("eviction ");
            // Find the oldest line
            // Replace it
            // Increment evicition counter
            for(int i = 0; i < assoc; i++)
            {
                if(sets[set_index].set_lines[i].last_used >= max)
                {
                    max = sets[set_index].set_lines[i].last_used;
                    index = i;
                }
            }
            // Can most likely skip setting the valid bit, as it would already be set,
            // but better be safe than sorry.
            sets[set_index].set_lines[index].valid = 1;
            sets[set_index].set_lines[index].last_used = 0;
            sets[set_index].set_lines[index].line = addr;
            (*evictions)++;
        }
    }
}

/*
 * print_usage - Prints out the usage info for the simulator if the -h flag is set.
 */
void print_usage()
{
    printf("Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
    printf("Use the -h flag to print out a help message.\n");
    exit(1);
}

