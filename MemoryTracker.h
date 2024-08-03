
/** @file */
#ifndef MEMORYTRACKER_H_
#define MEMORYTRACKER_H_
#include <map>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>


#if defined(__clang__)
# include "malloc_allocator.h"
#elif defined(__GNUC__) && (__GNUC__ >= 3)
# include <ext/malloc_allocator.h>
using __gnu_cxx::malloc_allocator;
#else
# include "malloc_allocator.h"
#endif

// Macro to obtain program name
#if defined(__APPLE__) /* other BSD? */
#include <sys/syslimits.h>
#include <sys/param.h>
#include <mach-o/dyld.h>
# define  GET_SHORT_PROGRAM_NAME() getprogname()
# define  GET_FULL_PROGRAM_NAME()  getfullpath()
# define  MEM_MAX_LINE PATH_MAX
#elif defined(_WIN32)
# define  GET_SHORT_PROGRAM_NAME() __argv[0]
# define  GET_FULL_PROGRAM_NAME()  __argv[0]
# define  MEM_MAX_LINE MAX_PATH
#elif defined(__linux__)
#include <linux/limits.h>
extern char *__progname_full;
extern char *__progname;
# define  GET_SHORT_PROGRAM_NAME() __progname
# define  GET_FULL_PROGRAM_NAME()  __progname_full
# define  MEM_MAX_LINE PATH_MAX
#else
# error Unsupported platform! GET_[FULL|SHORT]_PROGRAM_NAME() macro.
// Solaris
// getexecname();
// AIX
// extern char **p_xargv; *p_xargv
// HPUX
// extern char *$ARGV; $ARGV
#endif

// Macro to define utility program to convert
// addresses into file name/line number & to
// de-mangle C++ names.
#if defined(__APPLE__)
#if defined(__arm64__)
#  define ATOS "/usr/bin/atos -arch arm64 -o "
#else
#  define ATOS "/usr/bin/atos -arch x86_64 -o "
#endif
#  define ADDR_LOOKUP ATOS
#else
#  define CPP_FILT  "c++filt -n "
#  define ADDR2LINE "addr2line -f -e "
#  define ADDR_LOOKUP ADDR2LINE
#endif

// Macros to handle invocation environment issues
#if defined(__CYGWIN__) /* In eclipse & cygwin - convert paths! */
#  include <sys/cygwin.h>
#  define CONVERT_TO_PATH(s,d) cygwin_conv_to_win32_path(s, d)
#  define PROG_EXT ".exe"
#elif defined(_WIN32)
#  define PROG_EXT ".exe"
#else
#  define CONVERT_TO_PATH(s,d) std::strcpy(d, s)
#  define PROG_EXT ""
#endif

//Macro to get stack pointer/return address
#if defined(__GNUC__)
#   define GET_STACKPOINTER(x) x = __builtin_frame_address(0)
#   define RETURN_ADDRESS(x)   x = __builtin_return_address(0)
#elif defined(__i386__)
	// Intel assm to get base pointer on i386
	// (needs -masm=intel with gcc)
	//#define GET_STACKPOINTER(x)	asm ("mov %0, %%ebp" :"=r"(x))
	// AT&T assm to get stack pointer on i386.
#	define GET_STACKPOINTER(x)	asm ("mov %%ebp, %0" :"=r"(x))
#	define RETURN_ADDRESS(x) (x[1])

#elif defined(__x86_64__)

	// AT&T assm to get stack pointer on x86_64.
#	define GET_STACKPOINTER(x)	asm ("mov %%rbp, %0" :"=r"(x))
#	define RETURN_ADDRESS(x) (x[1])

#elif defined(__ppc__) || defined(__ppc64__)

	// AT&T assm to get stack pointer on ppc.
#	define GET_STACKPOINTER(x)	asm ("mr %0, r0" :"=r"(x))
#	define RETURN_ADDRESS(x) (x)

#else
#	error Unsupported platform!
#endif


#if defined(ADDR2LINE) || defined(__APPLE__)
#  define REPORT_FUNC_NAME(buf) std::fprintf(stderr, "    In function: %s\n", buf)
#else
#  define REPORT_FUNC_NAME(buf) std::fprintf(stderr, "\n")
#endif



namespace cppt
{
#if defined(__APPLE__)
    const char *getfullpath()
    {
        static char resolved_path[PATH_MAX];
        uint32_t p(PATH_MAX);
        _NSGetExecutablePath(resolved_path, &p);
        return resolved_path;
    }
#endif

	class MemoryTracker
	{
	public:
		typedef void * Address;
		typedef enum MemoryFault_tag
		{
			PotentialMemoryLeak,
			MemoryLeak,
			DeleteOnArrayNew,
			ArrayDeleteOnNew,
			NonHeapDelete
		} MemoryFaultCode;
	
		class Allocation
		{
		public:
			Allocation(std::size_t s, Address a, bool ar, MemoryFaultCode fc = PotentialMemoryLeak)
			: size(s), addr(a), array(ar),
			  faultCode(fc) {}
			~Allocation() {}
	
			std::size_t getSize() const { return size; }
			Address getAddress() const { return addr; }
			bool isArray() const { return array; }
			void setFault(MemoryFaultCode c) { faultCode = c; }
			MemoryFaultCode getFault() const { return faultCode; }
			bool isLeak() const
			{
				if (faultCode == NonHeapDelete)
					return false;
				return true;
			}
	
		private:
			std::size_t size;
			Address addr;
			bool array;
			MemoryFaultCode faultCode;
		};
	
        MemoryTracker()
        {
            // Now need this for running within Netbeans:
            unsetenv("LD_PRELOAD");
            unsetenv("DYLD_LIBRARY_PATH");
            unsetenv("LD_LIBRARY_PATH");

			// Set shutdown to run on exit & allocate data.
			atexit(MemoryTracker::shutdown);
			allocMap = new MemoryMap();
			cmd = new std::string(ADDR_LOOKUP);    // Address lookup command line

            // Needed this under NetBeans???
            char prog[MEM_MAX_LINE];
#if defined(__CYGWIN__) /* In eclipse & cygwin - convert paths! */
            char cprog[MEM_MAX_LINE];
            cygwin_conv_to_posix_path(prog, cprog);
            std::strcpy(prog, cprog);
#elif defined(__linux__) || defined(WIN32)
            std::strcpy(prog, GET_FULL_PROGRAM_NAME());
#elif defined(__APPLE__)
            std::strcpy(prog, GET_FULL_PROGRAM_NAME());
#endif

            *cmd += "\"";
			*cmd += prog;      // name of this program
			*cmd += PROG_EXT;  // program extension
            *cmd += "\"";
	
			// Must be last thing in CTOR (enable mem tracking).
			tracking = true;
		}
	
		~MemoryTracker()
		{
		}
		
		static void *track(std::size_t c, Address a, bool ar) noexcept(false)
		{
			void *p = std::malloc(c);
			if (p)
			{
				if (tracking)
				{
					if (logging) std::fprintf(stderr, "new %s %ld at %p (from %p)\n", ar ? "array" : "scalar", c, p, a);
					allocMap->insert(std::make_pair(reinterpret_cast<Address>(p), Allocation(c, a, ar)));
				}
			}
			else
				throw std::bad_alloc();
	
			return p;
		}
		
		static void untrack(void *p, Address a, bool ar)
		{
			if (p)
			{
				if (tracking)
				{
					if (logging) std::fprintf(stderr, "del %s at %p\n", ar ? "array" : "scalar", p);
					MemoryMap::iterator it = allocMap->find(reinterpret_cast<Address>(p));
					
					// Is memory to be deleted in the map?
					if (it != allocMap->end())
					{
						// Do we have mis-matched new/delete?
						// Check for delete of array obj
						if ((ar == false) && (it->second.isArray()))
						{
							it->second.setFault(MemoryTracker::DeleteOnArrayNew);
							// We've got a 'delete obj' for a 'obj = new type[xxx]'
							// Strictly, we should only release first element of array,
							// but we won't release anything!
						}
						// Check for array delete of new obj
						else if ((ar == true) && (it->second.isArray() == false))
						{
							it->second.setFault(MemoryTracker::ArrayDeleteOnNew);
							// We've got a 'delete [] obj' for 'obj = new type' 
						}
						else
						{
							// No, new/delete match - release memory
							allocMap->erase(reinterpret_cast<Address>(p));
							std::free(p);
						}
					}
					else
					{
						// Delete of unknown memory - track it.
						allocMap->insert(std::make_pair(reinterpret_cast<Address>(p),
								Allocation(0, reinterpret_cast<Address>(a), ar, NonHeapDelete)));
					}
	
					// Protection from undefined behaviour of freeing
					// non-heap memory addresses! If ptr is not null
					// and we're tracking, then we SHOULD be removing
					// something from the map! If we do (it != end())
					// free it, if we don't, return early to avoid
					// the default call to free.
					return;
				}
				else // Not tracking memory - release it.
					std::free(p);
			}
		}
		
		static void setLogging(bool b) { logging = b; }

		static void reportFault(const Allocation &memAlloc, void *addr, char *path, char *funcname)
		{
			// Get memory fault code.
			MemoryTracker::MemoryFaultCode mfc = memAlloc.getFault();

			if (mfc == MemoryTracker::PotentialMemoryLeak)
			{
				std::fprintf(stderr, "%s: error: Potential memory leak!\n", path);
				std::fprintf(stderr, "    %ld bytes ", memAlloc.getSize());
				std::fprintf(stderr, "allocated at: %p ", addr);
				std::fprintf(stderr, "(from program address %p)\n",
						reinterpret_cast<void *>(memAlloc.getAddress()));
				REPORT_FUNC_NAME(funcname);
			}
			// Check if leak is due to 'new[]/delete' mis-match.
			else if (mfc == MemoryTracker::DeleteOnArrayNew)
			{
				std::fprintf(stderr, "%s: error: Non-array delete on array Object!\n", path);
				std::fprintf(stderr, "    Array of %ld bytes ", memAlloc.getSize());
				std::fprintf(stderr, "allocated at: %p ", addr);
				std::fprintf(stderr, "(from program address %p)\n",
						reinterpret_cast<void *>(memAlloc.getAddress()));
				REPORT_FUNC_NAME(funcname);
			}
			else if (mfc == MemoryTracker::ArrayDeleteOnNew)
			{
				std::fprintf(stderr, "%s: error: Array delete on non-array Object!\n", path);
				std::fprintf(stderr, "    %ld bytes ", memAlloc.getSize());
				std::fprintf(stderr, "allocated at: %p ", addr);
				std::fprintf(stderr, "(from program address %p)\n",
						reinterpret_cast<void *>(memAlloc.getAddress()));
				REPORT_FUNC_NAME(funcname);
			}
			else if (mfc == MemoryTracker::NonHeapDelete)
			{
				std::fprintf(stderr, "%s: error: Delete of non-heap Memory!\n", path);
				std::fprintf(stderr, "    Attempt to delete non-heap memory at: %p ", addr);
				std::fprintf(stderr, "(from program address %p)\n",
						reinterpret_cast<void *>(memAlloc.getAddress()));
				REPORT_FUNC_NAME(funcname);
			}
		}

		static void shutdown(void)
		{
			// Disable memory tracking, otherwise the code below may
			// alter 'allocMap'!
			tracking = false;
	
			if (allocMap->empty() == false)
			{
				unsigned long totalLeak(0L);
				unsigned long numLeaks(0L);
				MemoryMap::const_iterator it = allocMap->begin();
				
				// Scan allocMap to get total leak and build string
				// of address locations for each leak.
				std::stringstream ss;
				while (it != allocMap->end())
				{
					// If fault code represents a potential leak of some form...
					if (it->second.isLeak())
					{
						// ...total up the memory leaks.
						totalLeak += it->second.getSize();
						numLeaks++;
					}

					// Build string of addresses for lookup.
					Address il = it->second.getAddress();
					ss << " " << std::hex << il;
					it++;
				}
	
				std::fflush(stdout);
				std::fprintf(stderr, "\n\nProgram %s: %ld Memory leaks detected (with %lu bytes leaked):\n\n",
					GET_SHORT_PROGRAM_NAME(), numLeaks, totalLeak);
	
				std::string lcmd(*cmd + ss.str());
				if (logging) fprintf(stderr, "AddrLookup Cmd: %s\n", lcmd.c_str());

				// Start the address conversion program on list of leaks.
				FILE *fp_a = popen(lcmd.c_str(), "r");
				char path[MEM_MAX_LINE];
				char demangled[MEM_MAX_LINE];
	
				it = allocMap->begin();
				while (it != allocMap->end())
				{
#if defined(ADDR2LINE)
                    // If we are using 'addr2line' for address
                    // lookup, read the (mangled) function name.
                    char func[MEM_MAX_LINE];
                    std::fgets(func, MEM_MAX_LINE, fp_a);
#else
                    // Read filename/linenum
                    std::fgets(demangled, MEM_MAX_LINE, fp_a);
                    CONVERT_TO_PATH((char *)GET_FULL_PROGRAM_NAME(), path);
#endif
					
#if defined(CPP_FILT)
                    // If names need to be demangled,
                    // build command to demangle them.
                    std::string fcmd(CPP_FILT);
                    if (func[0] != '_') fcmd += "\"";
                    fcmd += func;
                    if (func[0] != '_') fcmd += "\"";
				    if (logging) fprintf(stderr, "C++Filt Cmd: %s\n", fcmd.c_str());
                    // Demangle the name.
                    FILE *fp_f = popen(fcmd.c_str(), "r");
                    std::fgets(demangled, MEM_MAX_LINE, fp_f);
                    pclose(fp_f);
#endif

					reportFault(it->second, reinterpret_cast<void *>(it->first), path, demangled);
					it++;
				}
				pclose(fp_a);
			}
			else
			{
				std::fprintf(stdout, "\n\n%s: Zero memory leaks detected!\n\n", GET_SHORT_PROGRAM_NAME());
				std::fflush(stdout);
			}
			
			delete allocMap;
			delete cmd;
		}
	
		typedef malloc_allocator<std::pair<const Address, Allocation> > MemAlloc;
		typedef std::less<Address> Predicate;
		typedef std::map<Address, Allocation, Predicate, MemAlloc> MemoryMap;
	
	private:
		static MemoryMap *allocMap;
		static bool tracking;
		static std::string *cmd;
		static bool logging;
	};
}

#ifndef NO_IMPL
#include "MemoryTrackerImpl.h"
#endif

#endif /*MEMORYTRACKER_H_*/
