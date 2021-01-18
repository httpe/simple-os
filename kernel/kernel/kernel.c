#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/arch_init.h>
#include <stdint.h>
#include <kernel/heap.h>
#include <kernel/multiboot.h>

void kernel_main(uint32_t mbt_physical_addr) {

	// Architecture specific initialization
	initialize_architecture(mbt_physical_addr);

	terminal_clear_screen();

	printf("Welcome to Simple-OS!\n");

	uint32_t a = (uint32_t) kmalloc(8);
	printf("Heap alloc a[8]: %d\n", a);
	uint32_t b = (uint32_t) kmalloc(8);
	printf("Heap alloc b[8]: %d\n", b);
	uint32_t c = (uint32_t) kmalloc(8);
	printf("Heap alloc c[8]: %d\n", c);
	printf("free(a):\n", c);
	kfree(a);
	printf("free(c):\n", c);
	kfree(c);
	printf("free(b):\n", c);
	kfree(b);
	uint32_t d = (uint32_t) kmalloc(4096*3);
	printf("Heap alloc d[4096*3]: %d\n", d);
	uint32_t e = (uint32_t) kmalloc(8);
	printf("Heap alloc e[8]: %d\n", e);
	printf("free(d)\n");
	kfree(d);
	printf("free(e)\n");
	kfree(e);

	// Manually triggering a page fault
	// Use volatile to avoid it get optimized away
	printf("Manually triggering a page fault at 0xA0000000...\n");
    uint32_t *ptr = (uint32_t*)0xA0000000;
    uint32_t volatile do_page_fault = *ptr;

	// Hang by infinite loop
	while(1);
}

