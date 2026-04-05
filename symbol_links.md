# Table of Contents

1.  [Symbol Links](#orgd8c39b1)
2.  [Job](#orga27c198)
3.  [Hints](#org26cf8c6)


<a id="orgd8c39b1"></a>

# Symbol Links

    In this exercise you will add symbolic links to xv6

Symbolic links (or soft links) refer to a linked file or directory by pathname

-   when a symbolic link is opened, the kernel looks up the linked-to name

    Symbolic links resemble hard links
    
    but hard links are restricted to pointing to files on the same disk, cannot refer to directories
    
    are tied to a specific target i-node rather than (as with symbolic links) referring to whatever happens at the moment to be at the target name, if anything

Implementing this system call is a good exercise to understand how pathname lookup works.

    You do not have to handle symbolic links to directories for this lab

the only system call that needs to know how to follow symbolic links is **open()**


<a id="orga27c198"></a>

# Job

You will implement the <span class="underline">symlink(char \*target, char \*path)</span> system call, which **creates** a new symbolic link at path that refers to file named by target

    For further information, see the man page symlink

To test, add <span class="underline">symlinktest</span> to the Makefile and run it. Your solution is complete when the tests produce the following output (including usertests succeeding).

    $ symlinktest
    Start: test symlinks
    test symlinks: ok
    Start: test concurrent symlinks
    test concurrent symlinks: ok
    $ usertests -q
    ...
    ALL TESTS PASSED
    $ 


<a id="org26cf8c6"></a>

# Hints

1.  First, create a <span class="underline">new system call number</span> for symlink, add an entry to <span class="underline">user/usys.pl</span> ,  <span class="underline">user/user.h</span> , and implement an empty <span class="underline">sys<sub>symlink</sub></span> in <span class="underline">kernel/sysfile.c</span>
2.  Add a new file type **T<sub>SYMLINK</sub>** to <span class="underline">kernel/stat.h</span> to represent a symbolic link
3.  Add a new flag to <span class="underline">kernel/fcntl.h</span> , **O<sub>NOFOLLOW</sub>**, that can be used with the <span class="underline">open system call</span>. Note
    -   that flags passed to open are combined using a <span class="underline">bitwise OR</span> operator, so your new flag should **not overlap with any existing flags**
        
            This will let you compile user/symlinktest.c once you add it to the Makefile
4.  Implement the <span class="underline">symlink(target, path)</span> system call to create a new symbolic link at path that refers to target
    -   Note **that target does not need to exist** for the system call to succeed
    -   You will need to choose somewhere to store the target path of a symbolic link
        
            for example, in the inode's data blocks
    -   symlink should return an integer representing <span class="underline">success (0)</span> or <span class="underline">failure (-1)</span> similar to link and unlink
5.  Modify the <span class="underline">open</span> system call to handle the case where the path refers to a symbolic link
    -   If the file does not exist, open must fail
    -   When a process specifies O<sub>NOFOLLOW</sub> in the flags to open, open should open the symlink (and not follow the symbolic link)
6.  If the linked file is also a symbolic link, you must **recursively** follow it until a non-link file is reached
    -   If the links form a cycle, you must return an error code
        
            You may approximate this by returning an error code if the depth of links reaches some threshold (e.g., 10)
7.  Other system calls (e.g., link and unlink) **must not follow** symbolic links
    
        these system calls operate on the symbolic link itself

