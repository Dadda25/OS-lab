#ifndef __ELF_H__
#define __ELF_H__

#include "common.h"

#define ELF_MAGIC 0x464C457FU

// ELF File header
typedef struct elf_header {
    uint32 magic; // must be ELF_MAGIC
    uint8 elf[12];
    uint16 type;
    uint16 machine;
    uint32 version;
    uint64 entry;
    uint64 phoff;
    uint64 shoff;
    uint32 flags;
    uint16 ehsize;
    uint16 phentsize;
    uint16 phnum;
    uint16 shentsize;
    uint16 shnum;
    uint16 shstrndx;
} elf_header_t;

// program section header
typedef struct program_header { 
    uint32 type;
    uint32 flags;
    uint64 off;
    uint64 vaddr;
    uint64 paddr;
    uint64 filesz;
    uint64 memsz;
    uint64 align;
} program_header_t;

// Values for Proghdr type
#define ELF_PROG_LOAD           1
#define ELF_PROG_INTERP         3
#define ELF_PROG_PHDR           6

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4

#define AT_NULL		0		/* End of vector */
#define AT_IGNORE	1		/* Entry should be ignored */
#define AT_EXECFD	2		/* File descriptor of program */
#define AT_PHDR		3		/* Program headers for program */
#define AT_PHENT	4		/* Size of program header entry */
#define AT_PHNUM	5		/* Number of program headers */
#define AT_PAGESZ	6		/* System page size */
#define AT_BASE		7		/* Base address of interpreter */
#define AT_FLAGS	8		/* Flags */
#define AT_ENTRY	9		/* Entry point of program */
#define AT_NOTELF	10		/* Program is not ELF */
#define AT_UID		11		/* Real uid */
#define AT_EUID		12		/* Effective uid */
#define AT_GID		13		/* Real gid */
#define AT_EGID		14		/* Effective gid */
#define AT_CLKTCK	17		/* Frequency of times() */

/* Some more special a_type values describing the hardware.  */
#define AT_PLATFORM	15		/* String identifying platform.  */
#define AT_HWCAP	16		/* Machine-dependent hints about processor capabilities.  */

/* This entry gives some information about the FPU initialization performed by the kernel.  */
#define AT_FPUCW	18		/* Used FPU control word.  */

/* Cache block sizes.  */
#define AT_DCACHEBSIZE	19		/* Data cache block size.  */
#define AT_ICACHEBSIZE	20		/* Instruction cache block size.  */
#define AT_UCACHEBSIZE	21		/* Unified cache block size.  */

/* A special ignored value for PPC, used by the kernel to control the interpretation of the AUXV. Must be > 16.  */
#define AT_IGNOREPPC	22		/* Entry should be ignored.  */
#define	AT_SECURE	23		/* Boolean, was exec setuid-like?  */
#define AT_BASE_PLATFORM 24		/* String identifying real platforms.*/
#define AT_RANDOM	25		/* Address of 16 random bytes.  */
#define AT_HWCAP2	26		/* More machine-dependent hints about processor capabilities.  */
#define AT_EXECFN	31		/* Filename of executable.  */

/* Pointer to the global system page used for system calls and other nice things.  */
#define AT_SYSINFO	32
#define AT_SYSINFO_EHDR	33

#define MAX_AT 33

#endif