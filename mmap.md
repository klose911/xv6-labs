
# Table of Contents

-   [mmap](#org6dda300)
-   [munmap](#org779fa41)
-   [Job](#org34db8d7)
    -   [Hints](#org3c0b7e4)

The <span class="underline">mmap</span> and munmap system calls allow UNIX programs to exert detailed **control over** their <span class="underline">address spaces</span> . They can be used to

-   share memory among processes
-   to map files into process address spaces
-   as part of user-level page fault schemes such as the garbage-collection algorithms discussed in lecture

    In this lab you'll add mmap and munmap to xv6, focusing on memory-mapped files


<a id="org6dda300"></a>

# mmap

The manual page (run <span class="underline">man 2 mmap</span> ) shows this declaration for mmap:

    void *mmap(void *addr, size_t len, int prot, int flags,
               int fd, off_t offset);

mmap can be called in many ways, but this lab requires only a subset of its features relevant to **memory-mapping a file** . You can assume that

-   <span class="underline">addr</span> will **always be zero** , meaning that the kernel should decide the virtual address at which to map the file
-   mmap returns that address, or <span class="underline">0xffffffffffffffff</span> if it fails
-   len is <span class="underline">the number of bytes to map</span> ; it might not be the same as the file's length
-   prot indicates whether the memory should be mapped readable, writeable, and/or executable
    -   you can assume that prot is <span class="underline">PROT\_READ</span> or <span class="underline">PROT\_WRITE</span> or <span class="underline">both</span>
-   flags will be either
    -   <span class="underline">MAP\_SHARED</span>  meaning that modifications to the mapped memory should be written back to the file
    -   <span class="underline">MAP\_PRIVATE</span>  meaning that they should not
        
            You don't have to implement any other bits in flags
-   fd is the <span class="underline">open file descriptor</span> of the file to map
-   You can assume **offset is zero** (it's the starting point in the file at which to map)

Your implementation should <span class="underline">fill in the page table</span> **lazily** , in response to **page faults** . That is, mmap itself **should not allocate** physical memory or read the file. Instead, do that in <span class="underline">page fault handling</span> code in (or called by) usertrap, as in the copy-on-write lab

    The reason to be lazy is to ensure that mmap of a large file is fast
    
    and that mmap of a file larger than physical memory is possible

It's OK if processes that map the same MAP\_SHARED file do not share physical pages.


<a id="org779fa41"></a>

# munmap

The manual page (run <span class="underline">man 2 munmap</span> ) shows this declaration for munmap:

    int munmap(void *addr, size_t len);

munmap should **remove** <span class="underline">mmap mappings</span> in the indicated address range if any

-   If the process has modified the memory and has it mapped MAP\_SHARED, the modifications should first be written to the file
-   An munmap call might cover only <span class="underline">a portion</span> of an mmap-ed region, but you can assume that it will either unmap <span class="underline">at the start</span> , or  <span class="underline">at the end</span> , or <span class="underline">the whole region</span>
    
        but not punch a hole in the middle of a region
-   When a process <span class="underline">exits</span> , any modifictions it has made to MAP\_SHARED regions should be written to the relevant files, as if the process had called munmap


<a id="org34db8d7"></a>

# Job

    You should implement enough mmap and munmap functionality to make the mmaptest test program work
    
    If mmaptest doesn't use a mmap feature, you don't need to implement that feature
    
    You must also ensure that usertests -q continues to work

When you're done, you should see output similar to this:

    $ mmaptest
    test basic mmap
    test basic mmap: OK
    test mmap private
    test mmap private: OK
    test mmap read-only
    test mmap read-only: OK
    test mmap read/write
    test mmap read/write: OK
    test mmap dirty
    test mmap dirty: OK
    test not-mapped unmap
    test not-mapped unmap: OK
    test lazy access
    test lazy access: OK
    test mmap two files
    test mmap two files: OK
    test fork
    test fork: OK
    test munmap prevents access
    usertrap(): unexpected scause 0xd pid=7
    sepc=0x924 stval=0xc0001000
    usertrap(): unexpected scause 0xd pid=8
    sepc=0x9ac stval=0xc0000000
    test munmap prevents access: OK
    test writes to read-only mapped memory
    usertrap(): unexpected scause 0xf pid=9
    sepc=0xaf4 stval=0xc0000000
    test writes to read-only mapped memory: OK
    mmaptest: all tests succeeded
    $ usertests -q
    usertests starting
    ...
    ALL TESTS PASSED
    $ 


<a id="org3c0b7e4"></a>

## Hints

1.  Start by adding **\_mmaptest** to <span class="underline">UPROGS</span> , and <span class="underline">mmap</span> and <span class="underline">munmap</span> system calls, in order to get user/mmaptest.c to compile
    -   For now, just return errors from mmap and munmap
    -   We defined **PROT\_READ** etc for you in <span class="underline">kernel/fcntl.h</span>
    -   Run mmaptest, which will fail at the first mmap call
2.  Keep track of what mmap has mapped for each process
    -   **Define** a <span class="underline">structure</span> corresponding to the **VMA** <span class="underline">virtual memory area</span> described in the "virtual memory for applications" lecture. This should record
        -   the <span class="underline">address</span> , <span class="underline">length</span> ,  <span class="underline">permissions</span> ,  <span class="underline">file</span> , etc. for a virtual memory range created by mmap
        -   Since the xv6 kernel doesn't have a variable-size memory allocator in the kernel, it's OK to declare a <span class="underline">fixed-size array</span> of VMAs and allocate from that array as needed
            
                A size of 16 should be sufficient
3.  Implement mmap:
    1.  find an **unused region** in the process's address space in which to map the file
    2.  add a VMA to the process's table of mapped regions
        -   The VMA should contain a pointer to a struct file for the file being mapped
        -   mmap should increase the file's reference count so that the structure doesn't disappear when the file is closed (hint: see <span class="underline">filedup</span>)
    3.  Run mmaptest: the first mmap should succeed, but the first access to the mmap-ed memory will cause a page fault and kill mmaptest
4.  Add code to cause a page-fault in a mmap-ed region to
    -   **allocate** a page of physical memory
    -   read 4096 bytes of the relevant file into that page
        -   . Read the file with readi, which takes an offset argument at which to read in the file (but you will have to lock/unlock the inode passed to readi)
    -   map it into the user address space
    -   Don't forget to set the permissions correctly on the page
    -   Run mmaptest; it should get to the first munmap
5.  Implement munmap:
    1.  find the VMA for the address range and unmap the specified pages (hint: use <span class="underline">uvmunmap</span> )
    2.  If munmap removes all pages of a previous mmap, it should **decrement** the reference count of the corresponding struct file
    3.  If an unmapped page has been modified and the file is mapped MAP\_SHARED, **write** the page back to the file
        -   Look at <span class="underline">filewrite</span> for inspiration
6.  Ideally your implementation would only write back **MAP\_SHARED** pages that the program actually modified
    -   The <span class="underline">dirty bit</span> (D) in the <span class="underline">RISC-V PTE</span> indicates whether a page has been written
    -   However, mmaptest does not check that non-dirty pages are not written back
        
            thus you can get away with writing pages back without looking at D bits
7.  Modify <span class="underline">exit</span> to **unmap** the process's mapped regions as if munmap had been called
    -   Run mmaptest; all tests through test mmap two files should pass, but probably not test fork
8.  Modify <span class="underline">fork</span> to ensure that the child has the **same** <span class="underline">mapped regions</span> as the parent
    -   Don't forget to **increment** the reference count for a VMA's struct file
    -   In the page fault handler of the child, it is OK to allocate a new physical page instead of sharing a page with the parent
        
            The latter would be cooler, but it would require more implementation work
    -   Run mmaptest; it should pass all the tests

    Run usertests -q to make sure everything still works

