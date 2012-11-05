/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * mozilla.org
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
 *   Nicholas Nethercote <nnethercote@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAtomTable.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsMemoryReporterManager.h"
#include "nsArrayEnumerator.h"
#include "nsISimpleEnumerator.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;

static PRInt64 GetExplicit()
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nsnull)
        return (PRInt64)-1;

    PRInt64 n;
    nsresult rv = mgr->GetExplicit(&n);
    NS_ENSURE_SUCCESS(rv, rv);

    return n;
}

#if defined(MOZ_MEMORY)
#  define HAVE_JEMALLOC_STATS 1
#  include "jemalloc.h"
#endif  // MOZ_MEMORY

#if defined(XP_LINUX) || defined(XP_MACOSX) || defined(SOLARIS)

#include <sys/time.h>
#include <sys/resource.h>

static PRInt64 GetHardPageFaults()
{
  struct rusage usage;
  int err = getrusage(RUSAGE_SELF, &usage);
  if (err != 0) {
    return PRInt64(-1);
  }

  return usage.ru_majflt;
}

static PRInt64 GetSoftPageFaults()
{
  struct rusage usage;
  int err = getrusage(RUSAGE_SELF, &usage);
  if (err != 0) {
    return PRInt64(-1);
  }

  return usage.ru_minflt;
}

#endif

#if defined(XP_LINUX)

#include <unistd.h>
static PRInt64 GetProcSelfStatmField(int n)
{
    // There are more than two fields, but we're only interested in the first
    // two.
    static const int MAX_FIELD = 2;
    size_t fields[MAX_FIELD];
    NS_ASSERTION(n < MAX_FIELD, "bad field number");
    FILE *f = fopen("/proc/self/statm", "r");
    if (f) {
        int nread = fscanf(f, "%zu %zu", &fields[0], &fields[1]);
        fclose(f);
        return (PRInt64) ((nread == MAX_FIELD) ? fields[n]*getpagesize() : -1);
    }
    return (PRInt64) -1;
}

static PRInt64 GetVsize()
{
    return GetProcSelfStatmField(0);
}

static PRInt64 GetResident()
{
    return GetProcSelfStatmField(1);
}

#elif defined(SOLARIS)

#include <procfs.h>
#include <fcntl.h>
#include <unistd.h>

static void XMappingIter(PRInt64& Vsize, PRInt64& Resident)
{
    Vsize = -1;
    Resident = -1;
    int mapfd = open("/proc/self/xmap", O_RDONLY);
    struct stat st;
    prxmap_t *prmapp = NULL;
    if (mapfd >= 0) {
        if (!fstat(mapfd, &st)) {
            int nmap = st.st_size / sizeof(prxmap_t);
            while (1) {
                // stat(2) on /proc/<pid>/xmap returns an incorrect value,
                // prior to the release of Solaris 11.
                // Here is a workaround for it.
                nmap *= 2;
                prmapp = (prxmap_t*)malloc((nmap + 1) * sizeof(prxmap_t));
                if (!prmapp) {
                    // out of memory
                    break;
                }
                int n = pread(mapfd, prmapp, (nmap + 1) * sizeof(prxmap_t), 0);
                if (n < 0) {
                    break;
                }
                if (nmap >= n / sizeof (prxmap_t)) {
                    Vsize = 0;
                    Resident = 0;
                    for (int i = 0; i < n / sizeof (prxmap_t); i++) {
                        Vsize += prmapp[i].pr_size;
                        Resident += prmapp[i].pr_rss * prmapp[i].pr_pagesize;
                    }
                    break;
                }
                free(prmapp);
            }
            free(prmapp);
        }
        close(mapfd);
    }
}

static PRInt64 GetVsize()
{
    PRInt64 Vsize, Resident;
    XMappingIter(Vsize, Resident);
    return Vsize;
}

static PRInt64 GetResident()
{
    PRInt64 Vsize, Resident;
    XMappingIter(Vsize, Resident);
    return Resident;
}

#elif defined(XP_MACOSX)

#include <mach/mach_init.h>
#include <mach/task.h>

static bool GetTaskBasicInfo(struct task_basic_info *ti)
{
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    kern_return_t kr = task_info(mach_task_self(), TASK_BASIC_INFO,
                                 (task_info_t)ti, &count);
    return kr == KERN_SUCCESS;
}

// The VSIZE figure on Mac includes huge amounts of shared memory and is always
// absurdly high, eg. 2GB+ even at start-up.  But both 'top' and 'ps' report
// it, so we might as well too.
static PRInt64 GetVsize()
{
    task_basic_info ti;
    return (PRInt64) (GetTaskBasicInfo(&ti) ? ti.virtual_size : -1);
}

static PRInt64 GetResident()
{
#ifdef HAVE_JEMALLOC_STATS
    // If we're using jemalloc on Mac, we need to instruct jemalloc to purge
    // the pages it has madvise(MADV_FREE)'d before we read our RSS.  The OS
    // will take away MADV_FREE'd pages when there's memory pressure, so they
    // shouldn't count against our RSS.
    //
    // Purging these pages shouldn't take more than 10ms or so, but we want to
    // keep an eye on it since GetResident() is called on each Telemetry ping.
    {
      Telemetry::AutoTimer<Telemetry::MEMORY_FREE_PURGED_PAGES_MS> timer;
      jemalloc_purge_freed_pages();
    }
#endif

    task_basic_info ti;
    return (PRInt64) (GetTaskBasicInfo(&ti) ? ti.resident_size : -1);
}

#elif defined(XP_WIN)

#include <windows.h>
#include <psapi.h>

static PRInt64 GetVsize()
{
  MEMORYSTATUSEX s;
  s.dwLength = sizeof(s);

  bool success = GlobalMemoryStatusEx(&s);
  if (!success)
    return -1;

  return s.ullTotalVirtual - s.ullAvailVirtual;
}

static PRInt64 GetPrivate()
{
    PROCESS_MEMORY_COUNTERS_EX pmcex;
    pmcex.cb = sizeof(PROCESS_MEMORY_COUNTERS_EX);

    if (!GetProcessMemoryInfo(GetCurrentProcess(),
                              (PPROCESS_MEMORY_COUNTERS) &pmcex, sizeof(pmcex)))
    return (PRInt64) -1;

    return pmcex.PrivateUsage;
}

NS_MEMORY_REPORTER_IMPLEMENT(Private,
    "private",
    KIND_OTHER,
    UNITS_BYTES,
    GetPrivate,
    "Memory that cannot be shared with other processes, including memory that "
    "is committed and marked MEM_PRIVATE, data that is not mapped, and "
    "executable pages that have been written to.")

static PRInt64 GetResident()
{
  PROCESS_MEMORY_COUNTERS pmc;
  pmc.cb = sizeof(PROCESS_MEMORY_COUNTERS);

  if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
      return (PRInt64) -1;

  return pmc.WorkingSetSize;
}

#else

static PRInt64 GetResident()
{
    return (PRInt64) -1;
}

#endif

#if defined(XP_LINUX) || defined(XP_MACOSX) || defined(XP_WIN) || defined(SOLARIS)
NS_MEMORY_REPORTER_IMPLEMENT(Vsize,
    "vsize",
    KIND_OTHER,
    UNITS_BYTES,
    GetVsize,
    "Memory mapped by the process, including code and data segments, the "
    "heap, thread stacks, memory explicitly mapped by the process via mmap "
    "and similar operations, and memory shared with other processes. "
    "This is the vsize figure as reported by 'top' and 'ps'.  This figure is of "
    "limited use on Mac, where processes share huge amounts of memory with one "
    "another.  But even on other operating systems, 'resident' is a much better "
    "measure of the memory resources used by the process.")
#endif

#if defined(XP_LINUX) || defined(XP_MACOSX) || defined(SOLARIS)
NS_MEMORY_REPORTER_IMPLEMENT(PageFaultsSoft,
    "page-faults-soft",
    KIND_OTHER,
    UNITS_COUNT_CUMULATIVE,
    GetSoftPageFaults,
    "The number of soft page faults (also known as \"minor page faults\") that "
    "have occurred since the process started.  A soft page fault occurs when the "
    "process tries to access a page which is present in physical memory but is "
    "not mapped into the process's address space.  For instance, a process might "
    "observe soft page faults when it loads a shared library which is already "
    "present in physical memory. A process may experience many thousands of soft "
    "page faults even when the machine has plenty of available physical memory, "
    "and because the OS services a soft page fault without accessing the disk, "
    "they impact performance much less than hard page faults.")

NS_MEMORY_REPORTER_IMPLEMENT(PageFaultsHard,
    "page-faults-hard",
    KIND_OTHER,
    UNITS_COUNT_CUMULATIVE,
    GetHardPageFaults,
    "The number of hard page faults (also known as \"major page faults\") that "
    "have occurred since the process started.  A hard page fault occurs when a "
    "process tries to access a page which is not present in physical memory. "
    "The operating system must access the disk in order to fulfill a hard page "
    "fault. When memory is plentiful, you should see very few hard page faults. "
    "But if the process tries to use more memory than your machine has "
    "available, you may see many thousands of hard page faults. Because "
    "accessing the disk is up to a million times slower than accessing RAM, "
    "the program may run very slowly when it is experiencing more than 100 or "
    "so hard page faults a second.")
#endif

NS_MEMORY_REPORTER_IMPLEMENT(Explicit,
    "explicit",
    KIND_OTHER,
    UNITS_BYTES,
    GetExplicit,
    "This is the same measurement as the root of the 'explicit' tree.  "
    "However, it is measured at a different time and so gives slightly "
    "different results.")

NS_MEMORY_REPORTER_IMPLEMENT(Resident,
    "resident",
    KIND_OTHER,
    UNITS_BYTES,
    GetResident,
    "Memory mapped by the process that is present in physical memory, "
    "also known as the resident set size (RSS).  This is the best single "
    "figure to use when considering the memory resources used by the process, "
    "but it depends both on other processes being run and details of the OS "
    "kernel and so is best used for comparing the memory usage of a single "
    "process at different points in time.")

/**
 ** memory reporter implementation for jemalloc and OSX malloc,
 ** to obtain info on total memory in use (that we know about,
 ** at least -- on OSX, there are sometimes other zones in use).
 **/

#if HAVE_JEMALLOC_STATS

static PRInt64 GetHeapUnallocated()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) stats.mapped - stats.allocated;
}

static PRInt64 GetHeapAllocated()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) stats.allocated;
}

static PRInt64 GetHeapCommitted()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) stats.committed;
}

static PRInt64 GetHeapCommittedFragmentation()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) 10000 * (1 - stats.allocated / (double)stats.committed);
}

static PRInt64 GetHeapDirty()
{
    jemalloc_stats_t stats;
    jemalloc_stats(&stats);
    return (PRInt64) stats.dirty;
}

NS_MEMORY_REPORTER_IMPLEMENT(HeapCommitted,
    "heap-committed",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapCommitted,
    "Memory mapped by the heap allocator that is committed, i.e. in physical "
    "memory or paged to disk.  When heap-committed is larger than "
    "heap-allocated, the difference between the two values is likely due to "
    "external fragmentation; that is, the allocator allocated a large block of "
    "memory and is unable to decommit it because a small part of that block is "
    "currently in use.")

NS_MEMORY_REPORTER_IMPLEMENT(HeapCommittedFragmentation,
    "heap-committed-fragmentation",
    KIND_OTHER,
    UNITS_PERCENTAGE,
    GetHeapCommittedFragmentation,
    "Fraction of committed bytes which do not correspond to an active "
    "allocation; i.e., 1 - (heap-allocated / heap-committed).  Although the "
    "allocator will waste some space under any circumstances, a large value here "
    "may indicate that the heap is highly fragmented.")

NS_MEMORY_REPORTER_IMPLEMENT(HeapDirty,
    "heap-dirty",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapDirty,
    "Memory which the allocator could return to the operating system, but "
    "hasn't.  The allocator keeps this memory around as an optimization, so it "
    "doesn't have to ask the OS the next time it needs to fulfill a request. "
    "This value is typically not larger than a few megabytes.")

#elif defined(XP_MACOSX) && !defined(MOZ_MEMORY)
#include <malloc/malloc.h>

static PRInt64 GetHeapUnallocated()
{
    struct mstats stats = mstats();
    return (PRInt64) (stats.bytes_total - stats.bytes_used);
}

static PRInt64 GetHeapAllocated()
{
    struct mstats stats = mstats();
    return (PRInt64) stats.bytes_used;
}

static PRInt64 GetHeapZone0Committed()
{
#ifdef MOZ_DMD
    // malloc_zone_statistics() crashes when run under DMD because Valgrind
    // doesn't intercept it.  This measurement isn't important for DMD, so
    // don't even try.
    return (PRInt64) -1;
#else
    malloc_statistics_t stats;
    malloc_zone_statistics(malloc_default_zone(), &stats);
    return stats.size_in_use;
#endif
}

static PRInt64 GetHeapZone0Used()
{
#ifdef MOZ_DMD
    // See comment in GetHeapZone0Committed above.
    return (PRInt64) -1;
#else
    malloc_statistics_t stats;
    malloc_zone_statistics(malloc_default_zone(), &stats);
    return stats.size_allocated;
#endif
}

NS_MEMORY_REPORTER_IMPLEMENT(HeapZone0Committed,
    "heap-zone0-committed",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapZone0Committed,
    "Memory mapped by the heap allocator that is committed in the default "
    "zone.")

NS_MEMORY_REPORTER_IMPLEMENT(HeapZone0Used,
    "heap-zone0-used",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapZone0Used,
    "Memory mapped by the heap allocator in the default zone that is "
    "allocated to the application.")

#else

static PRInt64 GetHeapAllocated()
{
    return (PRInt64) -1;
}

static PRInt64 GetHeapUnallocated()
{
  return (PRInt64) -1;
}

#endif

NS_MEMORY_REPORTER_IMPLEMENT(HeapUnallocated,
    "heap-unallocated",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapUnallocated,
    "Memory mapped by the heap allocator that is not part of an active "
    "allocation. Much of this memory may be uncommitted -- that is, it does not "
    "take up space in physical memory or in the swap file.")

NS_MEMORY_REPORTER_IMPLEMENT(HeapAllocated,
    "heap-allocated",
    KIND_OTHER,
    UNITS_BYTES,
    GetHeapAllocated,
    "Memory mapped by the heap allocator that is currently allocated to the "
    "application.  This may exceed the amount of memory requested by the "
    "application because the allocator regularly rounds up request sizes. (The "
    "exact amount requested is not recorded.)")

NS_MEMORY_REPORTER_MALLOC_SIZEOF_FUN(AtomTableMallocSizeOf, "atom-table")

static PRInt64 GetAtomTableSize() {
  return NS_SizeOfAtomTableIncludingThis(AtomTableMallocSizeOf);
}

// Why is this here?  At first glance, you'd think it could be defined and
// registered with nsMemoryReporterManager entirely within nsAtomTable.cpp.
// However, the obvious time to register it is when the table is initialized,
// and that happens before XPCOM components are initialized, which means the
// NS_RegisterMemoryReporter call fails.  So instead we do it here.
NS_MEMORY_REPORTER_IMPLEMENT(AtomTable,
    "explicit/atom-table",
    KIND_HEAP,
    UNITS_BYTES,
    GetAtomTableSize,
    "Memory used by the atoms table.")

/**
 ** nsMemoryReporterManager implementation
 **/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsMemoryReporterManager, nsIMemoryReporterManager)

NS_IMETHODIMP
nsMemoryReporterManager::Init()
{
#if HAVE_JEMALLOC_STATS && defined(XP_LINUX)
    if (!jemalloc_stats)
        return NS_ERROR_FAILURE;
#endif

#define REGISTER(_x)  RegisterReporter(new NS_MEMORY_REPORTER_NAME(_x))

    REGISTER(HeapAllocated);
    REGISTER(HeapUnallocated);
    REGISTER(Explicit);
    REGISTER(Resident);

#if defined(XP_LINUX) || defined(XP_MACOSX) || defined(XP_WIN) || defined(SOLARIS)
    REGISTER(Vsize);
#endif

#if defined(XP_LINUX) || defined(XP_MACOSX) || defined(SOLARIS)
    REGISTER(PageFaultsSoft);
    REGISTER(PageFaultsHard);
#endif

#if defined(XP_WIN)
    REGISTER(Private);
#endif

#if defined(HAVE_JEMALLOC_STATS)
    REGISTER(HeapCommitted);
    REGISTER(HeapCommittedFragmentation);
    REGISTER(HeapDirty);
#elif defined(XP_MACOSX) && !defined(MOZ_MEMORY)
    REGISTER(HeapZone0Committed);
    REGISTER(HeapZone0Used);
#endif

    REGISTER(AtomTable);

    return NS_OK;
}

nsMemoryReporterManager::nsMemoryReporterManager()
  : mMutex("nsMemoryReporterManager::mMutex")
{
}

nsMemoryReporterManager::~nsMemoryReporterManager()
{
}

NS_IMETHODIMP
nsMemoryReporterManager::EnumerateReporters(nsISimpleEnumerator **result)
{
    nsresult rv;
    mozilla::MutexAutoLock autoLock(mMutex);
    rv = NS_NewArrayEnumerator(result, mReporters);
    return rv;
}

NS_IMETHODIMP
nsMemoryReporterManager::EnumerateMultiReporters(nsISimpleEnumerator **result)
{
    nsresult rv;
    mozilla::MutexAutoLock autoLock(mMutex);
    rv = NS_NewArrayEnumerator(result, mMultiReporters);
    return rv;
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterReporter(nsIMemoryReporter *reporter)
{
    mozilla::MutexAutoLock autoLock(mMutex);
    if (mReporters.IndexOf(reporter) != -1)
        return NS_ERROR_FAILURE;

    mReporters.AppendObject(reporter);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::RegisterMultiReporter(nsIMemoryMultiReporter *reporter)
{
    mozilla::MutexAutoLock autoLock(mMutex);
    if (mMultiReporters.IndexOf(reporter) != -1)
        return NS_ERROR_FAILURE;

    mMultiReporters.AppendObject(reporter);
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::UnregisterReporter(nsIMemoryReporter *reporter)
{
    mozilla::MutexAutoLock autoLock(mMutex);
    if (!mReporters.RemoveObject(reporter))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::UnregisterMultiReporter(nsIMemoryMultiReporter *reporter)
{
    mozilla::MutexAutoLock autoLock(mMutex);
    if (!mMultiReporters.RemoveObject(reporter))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::GetResident(PRInt64 *aResident)
{
    *aResident = ::GetResident();
    return NS_OK;
}

struct MemoryReport {
    MemoryReport(const nsACString &path, PRInt64 amount) 
    : path(path), amount(amount)
    {
        MOZ_COUNT_CTOR(MemoryReport);
    }
    MemoryReport(const MemoryReport& rhs)
    : path(rhs.path), amount(rhs.amount)
    {
        MOZ_COUNT_CTOR(MemoryReport);
    }
    ~MemoryReport() 
    {
        MOZ_COUNT_DTOR(MemoryReport);
    }
    const nsCString path;
    PRInt64 amount;
};

#ifdef DEBUG
// This is just a wrapper for PRInt64 that implements nsISupports, so it can be
// passed to nsIMemoryMultiReporter::CollectReports.
class PRInt64Wrapper : public nsISupports {
public:
    NS_DECL_ISUPPORTS
    PRInt64Wrapper() : mValue(0) { }
    PRInt64 mValue;
};
NS_IMPL_ISUPPORTS0(PRInt64Wrapper)

class ExplicitNonHeapCountingCallback : public nsIMemoryMultiReporterCallback
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Callback(const nsACString &aProcess, const nsACString &aPath,
                        PRInt32 aKind, PRInt32 aUnits, PRInt64 aAmount,
                        const nsACString &aDescription,
                        nsISupports *aWrappedExplicitNonHeap)
    {
        if (aKind == nsIMemoryReporter::KIND_NONHEAP &&
            PromiseFlatCString(aPath).Find("explicit") == 0 &&
            aAmount != PRInt64(-1))
        {
            PRInt64Wrapper *wrappedPRInt64 =
                static_cast<PRInt64Wrapper *>(aWrappedExplicitNonHeap);
            wrappedPRInt64->mValue += aAmount;
        }
        return NS_OK;
    }
};
NS_IMPL_ISUPPORTS1(
  ExplicitNonHeapCountingCallback
, nsIMemoryMultiReporterCallback
)
#endif

NS_IMETHODIMP
nsMemoryReporterManager::GetExplicit(PRInt64 *aExplicit)
{
    NS_ENSURE_ARG_POINTER(aExplicit);
    *aExplicit = 0;

    nsresult rv;
    bool more;

    // Get "heap-allocated" and all the KIND_NONHEAP measurements from normal
    // (i.e. non-multi) "explicit" reporters.
    PRInt64 heapAllocated = PRInt64(-1);
    PRInt64 explicitNonHeapNormalSize = 0;
    nsCOMPtr<nsISimpleEnumerator> e;
    EnumerateReporters(getter_AddRefs(e));
    while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsIMemoryReporter> r;
        e->GetNext(getter_AddRefs(r));

        PRInt32 kind;
        rv = r->GetKind(&kind);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCString path;
        rv = r->GetPath(path);
        NS_ENSURE_SUCCESS(rv, rv);

        // We're only interested in NONHEAP explicit reporters and
        // the 'heap-allocated' reporter.
        if (kind == nsIMemoryReporter::KIND_NONHEAP &&
            path.Find("explicit") == 0)
        {
            PRInt64 amount;
            rv = r->GetAmount(&amount);
            NS_ENSURE_SUCCESS(rv, rv);

            // Just skip any NONHEAP reporters that fail, because
            // "heap-allocated" is the most important one.
            if (amount != PRInt64(-1)) {
                explicitNonHeapNormalSize += amount;
            }
        } else if (path.Equals("heap-allocated")) {
            rv = r->GetAmount(&heapAllocated);
            NS_ENSURE_SUCCESS(rv, rv);

            // If we don't have "heap-allocated", give up, because the result would be
            // horribly inaccurate.
            if (heapAllocated == PRInt64(-1)) {
                *aExplicit = PRInt64(-1);
                return NS_OK;
            }
        }
    }

    // For each multi-reporter we could call CollectReports and filter out the
    // non-explicit, non-NONHEAP measurements.  But that's lots of wasted work,
    // so we instead use GetExplicitNonHeap() which exists purely for this
    // purpose.
    //
    // (Actually, in debug builds we also do it the slow way and compare the
    // result to the result obtained from GetExplicitNonHeap().  This
    // guarantees the two measurement paths are equivalent.  This is wise
    // because it's easy for memory reporters to have bugs.)

    PRInt64 explicitNonHeapMultiSize = 0;
    nsCOMPtr<nsISimpleEnumerator> e2;
    EnumerateMultiReporters(getter_AddRefs(e2));
    while (NS_SUCCEEDED(e2->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsIMemoryMultiReporter> r;
      e2->GetNext(getter_AddRefs(r));
      PRInt64 n;
      rv = r->GetExplicitNonHeap(&n);
      NS_ENSURE_SUCCESS(rv, rv);
      explicitNonHeapMultiSize += n;
    }

#ifdef DEBUG
    nsRefPtr<ExplicitNonHeapCountingCallback> cb =
      new ExplicitNonHeapCountingCallback();
    nsRefPtr<PRInt64Wrapper> wrappedExplicitNonHeapMultiSize2 =
      new PRInt64Wrapper();
    nsCOMPtr<nsISimpleEnumerator> e3;
    EnumerateMultiReporters(getter_AddRefs(e3));
    while (NS_SUCCEEDED(e3->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsIMemoryMultiReporter> r;
      e3->GetNext(getter_AddRefs(r));
      r->CollectReports(cb, wrappedExplicitNonHeapMultiSize2);
    }
    PRInt64 explicitNonHeapMultiSize2 = wrappedExplicitNonHeapMultiSize2->mValue;

    // Check the two measurements give the same result.  This was an
    // NS_ASSERTION but they occasionally don't match due to races (bug
    // 728990).
    if (explicitNonHeapMultiSize != explicitNonHeapMultiSize2) {
        char *msg = PR_smprintf("The two measurements of 'explicit' memory "
                                "usage don't match (%lld vs %lld)",
                                explicitNonHeapMultiSize,
                                explicitNonHeapMultiSize2);
        NS_WARNING(msg);
        PR_smprintf_free(msg);
    }
#endif

    *aExplicit = heapAllocated + explicitNonHeapNormalSize + explicitNonHeapMultiSize;
    return NS_OK;
}

NS_IMETHODIMP
nsMemoryReporterManager::GetHasMozMallocUsableSize(bool *aHas)
{
    void *p = malloc(16);
    if (!p) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    size_t usable = moz_malloc_usable_size(p);
    free(p);
    *aHas = !!(usable > 0);
    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsMemoryReporter, nsIMemoryReporter)

nsMemoryReporter::nsMemoryReporter(nsACString& process,
                                   nsACString& path,
                                   PRInt32 kind,
                                   PRInt32 units,
                                   PRInt64 amount,
                                   nsACString& desc)
: mProcess(process)
, mPath(path)
, mKind(kind)
, mUnits(units)
, mAmount(amount)
, mDesc(desc)
{
}

nsMemoryReporter::~nsMemoryReporter()
{
}

NS_IMETHODIMP nsMemoryReporter::GetProcess(nsACString &aProcess)
{
    aProcess.Assign(mProcess);
    return NS_OK;
}

NS_IMETHODIMP nsMemoryReporter::GetPath(nsACString &aPath)
{
    aPath.Assign(mPath);
    return NS_OK;
}

NS_IMETHODIMP nsMemoryReporter::GetKind(PRInt32 *aKind)
{
    *aKind = mKind;
    return NS_OK;
}

NS_IMETHODIMP nsMemoryReporter::GetUnits(PRInt32 *aUnits)
{
  *aUnits = mUnits;
  return NS_OK;
}

NS_IMETHODIMP nsMemoryReporter::GetAmount(PRInt64 *aAmount)
{
    *aAmount = mAmount;
    return NS_OK;
}

NS_IMETHODIMP nsMemoryReporter::GetDescription(nsACString &aDescription)
{
    aDescription.Assign(mDesc);
    return NS_OK;
}

nsresult
NS_RegisterMemoryReporter (nsIMemoryReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nsnull)
        return NS_ERROR_FAILURE;
    return mgr->RegisterReporter(reporter);
}

nsresult
NS_RegisterMemoryMultiReporter (nsIMemoryMultiReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nsnull)
        return NS_ERROR_FAILURE;
    return mgr->RegisterMultiReporter(reporter);
}

nsresult
NS_UnregisterMemoryReporter (nsIMemoryReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nsnull)
        return NS_ERROR_FAILURE;
    return mgr->UnregisterReporter(reporter);
}

nsresult
NS_UnregisterMemoryMultiReporter (nsIMemoryMultiReporter *reporter)
{
    nsCOMPtr<nsIMemoryReporterManager> mgr = do_GetService("@mozilla.org/memory-reporter-manager;1");
    if (mgr == nsnull)
        return NS_ERROR_FAILURE;
    return mgr->UnregisterMultiReporter(reporter);
}

namespace mozilla {

#ifdef MOZ_DMD

class NullMultiReporterCallback : public nsIMemoryMultiReporterCallback
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Callback(const nsACString &aProcess, const nsACString &aPath,
                        PRInt32 aKind, PRInt32 aUnits, PRInt64 aAmount,
                        const nsACString &aDescription,
                        nsISupports *aData)
    {
        // Do nothing;  the reporter has already reported to DMD.
        return NS_OK;
    }
};
NS_IMPL_ISUPPORTS1(
  NullMultiReporterCallback
, nsIMemoryMultiReporterCallback
)

void
DMDCheckAndDump()
{
    nsCOMPtr<nsIMemoryReporterManager> mgr =
        do_GetService("@mozilla.org/memory-reporter-manager;1");

    // Do vanilla reporters.
    nsCOMPtr<nsISimpleEnumerator> e;
    mgr->EnumerateReporters(getter_AddRefs(e));
    bool more;
    while (NS_SUCCEEDED(e->HasMoreElements(&more)) && more) {
        nsCOMPtr<nsIMemoryReporter> r;
        e->GetNext(getter_AddRefs(r));

        // Just getting the amount is enough for the reporter to report to DMD.
        PRInt64 amount;
        (void)r->GetAmount(&amount);
    }

    // Do multi-reporters.
    nsCOMPtr<nsISimpleEnumerator> e2;
    mgr->EnumerateMultiReporters(getter_AddRefs(e2));
    nsRefPtr<NullMultiReporterCallback> cb = new NullMultiReporterCallback();
    while (NS_SUCCEEDED(e2->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsIMemoryMultiReporter> r;
      e2->GetNext(getter_AddRefs(r));
      r->CollectReports(cb, nsnull);
    }

    VALGRIND_DMD_CHECK_REPORTING;
}

#endif  /* defined(MOZ_DMD) */

}