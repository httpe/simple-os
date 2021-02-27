#include <stdint.h>

////////////////////////////////
// Segmentation (GDT/TSS)
////////////////////////////////

// From: xv6/mmu.h

// various segment selectors.
#define SEG_NULL 0   // null segment
#define SEG_KCODE 1  // kernel code
#define SEG_KDATA 2  // kernel data+stack
#define SEG_UCODE 3  // user code
#define SEG_UDATA 4  // user data+stack
#define SEG_TSS   5  // this process's task state

// segment selector for CS/DS/ES etc.
// See https://wiki.osdev.org/Selector (RPL=Requested Privilege Level)
#define SEG_SELECTOR(seg,rpl) (((seg) << 3) + rpl)

// global var gdt[NSEGS] holds the above segments.
#define NSEGS     6

// Segment Descriptor
typedef struct segdesc {
  uint32_t lim_15_0 : 16;  // Low bits of segment limit
  uint32_t base_15_0 : 16; // Low bits of segment base address
  uint32_t base_23_16 : 8; // Middle bits of segment base address
  uint32_t type : 4;       // Segment type (see STS_ constants)
  uint32_t s : 1;          // 0 = system, 1 = application
  uint32_t dpl : 2;        // Descriptor Privilege Level
  uint32_t p : 1;          // Present
  uint32_t lim_19_16 : 4;  // High bits of segment limit
  uint32_t avl : 1;        // Unused (available for software use)
  uint32_t rsv1 : 1;       // Reserved
  uint32_t db : 1;         // 0 = 16-bit segment, 1 = 32-bit segment
  uint32_t g : 1;          // Granularity: limit scaled by 4K when set
  uint32_t base_31_24 : 8; // High bits of segment base address
} segdesc;

// For normal segments (4K granularity, so lim >> 12, lim >> 28)
#define SEG(type, base, lim, dpl) (struct segdesc)    \
{ ((lim) >> 12) & 0xffff, (uint32_t)(base) & 0xffff,      \
  ((uint32_t)(base) >> 16) & 0xff, type, 1, dpl, 1,       \
  (uint32_t)(lim) >> 28, 0, 0, 1, 1, (uint32_t)(base) >> 24 }
// For TSS segment (1B granularity)
#define TSS_SEG(type, base, lim, dpl) (struct segdesc)  \
{ (uint32_t)(lim) & 0xffff, (uint32_t)(base) & 0xffff,              \
  ((uint32_t)(base) >> 16) & 0xff, type, 0, dpl, 1,       \
  (uint32_t)(lim) >> 16, 0, 0, 1, 0, (uint32_t)(base) >> 24 }

#define DPL_KERNEL  0x0     // Kernel DPL
#define DPL_USER    0x3     // User DPL

// Application segment type bits
#define STA_X       0x8     // Executable segment
#define STA_W       0x2     // Writeable (non-executable segments)
#define STA_R       0x2     // Readable (executable segments)

// System segment type bits
#define STS_T32A    0x9     // Available 32-bit TSS
#define STS_IG32    0xE     // 32-bit Interrupt Gate
#define STS_TG32    0xF     // 32-bit Trap Gate

// Task state segment format
typedef struct task_state {
  uint16_t link;         // Old ts selector
  uint16_t padding0;
  uint32_t esp0;        // Stack pointers after an increase in privilege level 
  uint16_t ss0;         // Segment selectors after an increase in privilege level
  uint16_t padding1;
  uint32_t *esp1;
  uint16_t ss1;
  uint16_t padding2;
  uint32_t *esp2;
  uint16_t ss2;
  uint16_t padding3;
  uint32_t cr3;         // Page directory base
  uint32_t *eip;        // Saved state from last task switch
  uint32_t eflags;
  uint32_t eax;         // More saved state (registers)
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t *esp;
  uint32_t *ebp;
  uint32_t esi;
  uint32_t edi;
  uint16_t es;          // Even more saved state (segment selectors)
  uint16_t padding4;
  uint16_t cs;
  uint16_t padding5;
  uint16_t ss;
  uint16_t padding6;
  uint16_t ds;
  uint16_t padding7;
  uint16_t fs;
  uint16_t padding8;
  uint16_t gs;
  uint16_t padding9;
  uint16_t ldtr;
  uint16_t padding10;
  uint16_t t;           // Trap on task switch
  uint16_t iomb;        // I/O map base address
} task_state;

