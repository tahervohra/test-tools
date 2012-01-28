#include <stdio.h>
#include <stdarg.h>

#include <Windows.h>
#include <vector>
#include <stdlib.h>

void log(const char * fmt, ...);
DWORD WINAPI iothread(LPVOID arg);

// Measure the time taken for an operation.
// chkpt_start will return a token, chkpt_end will give the delta
// for a particular token
struct chkpt {
        unsigned long long nsec;
};

chkpt * chkpt_start() {
        // TODO: Start measurement

        return NULL;
};

chkpt * chkpt_end(chkpt * start) {
        // TODO: Measure the time
        return NULL;
}

// When you perform an operation that can be continued, provide the 
// recommended next step when adding this continuation to the pending_table.
enum next_step {
        CLOSE,
        READ,
        WRITE,
        STAT,
        MAX
};

struct pending_entry {
        next_step next;
        FILE * fp;

        pending_entry(next_step next_, FILE * fp_) :
                next(next_),
                fp(fp_)
                { }
};

// The table of operations that can be continued, and a lock for 
// safe access
std::vector<pending_entry> pending_table;
HANDLE pending_table_lock;

// Open a file, measure the open time and add the opened file (with a recommended next step)
// to the pending table.
void op_open (void * arg)
{
        // TODO : Pick a random file from the srcdir
        //        open it, and register it in the open files table
        char * filename = "myfile";
        log ("[%d] opening %s\n", GetCurrentThreadId(), filename);
        chkpt * c = chkpt_start();
        FILE * fp;
        fopen_s(&fp, filename, "r");
        chkpt_end(c);

        next_step next = (next_step) (rand() % MAX);

        WaitForSingleObject(pending_table_lock, INFINITE);
        pending_table.push_back(pending_entry(pending_entry(next, fp)));
        ReleaseMutex(pending_table_lock);
};

// Randomly close a file, if an entry with that recommendation is found in the pending_table
// Random search has a limit, beyond which we will go FIFO, and if no recommendation is found
// just finish the op without error
void op_close (void * arg)
{        
        FILE * fd = NULL;
        unsigned random_retry = 5;
        WaitForSingleObject(pending_table_lock, INFINITE);

        if(pending_table.empty()) {
                log("[%x] Pending table is empty", GetCurrentThreadId());
                return;
        }
        // Random search
        while (random_retry--) {
                unsigned i = rand() % pending_table.size();
                if (pending_table[i].next == CLOSE) {
                        fd = pending_table[i].fp;
                        pending_table.erase(pending_table.begin() + i);
                        break;
                }

        }
 
        if(!fd) {
                // FIFO sweep
                for (auto i = pending_table.begin(); i != pending_table.end(); i++) {
                        if (i->next == CLOSE) {
                                fd = i->fp;
                                pending_table.erase(i);
                        }
                }
                if (!fd) {
                        // No pending close recommendation found. Return without doing anything
                        return;
                }
        }
        ReleaseMutex(pending_table_lock);

        log("[%x] Closing %d", fd);
        chkpt * c = chkpt_start();
        fclose(fd);
        chkpt_end(c);
};


struct op_entry {
        typedef void (*op_func_t)(void * arg);

        char * op_name;
        op_func_t op_func;

        op_entry(char *op_name_, op_func_t op_func_) :
                op_name(op_name_),
                op_func(op_func_)
                {}
};

std::vector<op_entry> ops;
        
// Operating parameters
struct params
{
	int threads;	// Number of threads
	int numops;		// Per thread # of ops
} g_params = 
{
	3, // num threads
	1 // num ops
};

volatile unsigned int live_threads = 0;

int
wmain( int argc, wchar_t *argv[ ], wchar_t *envp[ ] )
{
        pending_table_lock = CreateMutex(NULL, false, NULL);
        ops.push_back(*new op_entry("file open", op_open));
        ops.push_back(*new op_entry("file close", op_close));

        for (auto i = 0; i < g_params.threads; i++) {
		CreateThread(NULL, 0, iothread, NULL, 0, 0);
		InterlockedIncrement(&live_threads);
	}
	while(1) {
		Sleep(5000);
		if (!live_threads) break;
	}
	return 0;
}

void log(const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

DWORD WINAPI iothread(LPVOID arg)
{
	log("[%d] Thread entering\n", GetCurrentThreadId());
        unsigned my_ops = g_params.numops;
        while (my_ops--) {
                // Select an op from the op_table and call the op_handler
                unsigned i = rand() % ops.size();
                log("[%d], Running %s\n", GetCurrentThreadId(), ops[i].op_name);
                ops[i].op_func(NULL);
        }
	log("[%d] Thread exiting\n", GetCurrentThreadId());
        InterlockedDecrement(&live_threads);
	return 0;

}
