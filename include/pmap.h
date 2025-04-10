#ifndef _PMAP_H_
#define _PMAP_H_

#include <mmu.h>
#include <printk.h>
#include <queue.h>
#include <types.h>

extern Pde *cur_pgdir;

LIST_HEAD(Page_list, Page);
typedef LIST_ENTRY(Page) Page_LIST_entry_t;

struct Page {
	Page_LIST_entry_t pp_link; /* free list link, pp_link是表示链表前后节点的结构体*/

	// Ref is the count of pointers (usually in page table entries)
	// to this page.  This only holds for pages allocated using
	// page_alloc.  Pages allocated at boot time using pmap.c's "alloc"
	// do not have valid reference count fields.

	u_short pp_ref; // 这一页物理内存被引用的次数，等于有多少虚拟页映射到该物理页，反映页的使用情况
					// 为0时表示该页空闲，可以被分配出去
};

// 展开之后的结构：
// 	struct Page {
// 		struct {
// 			struct Page *le_next;
// 			struct Page **le_prev;
// 		};
// 		u_short pp_ref;
// 	};

extern struct Page *pages; // 页数组，管理所有的页控制块
extern struct Page_list page_free_list; // 称为空闲链表，储存空闲的物理页

// 通过指针减法获取对应的页控制块是第几个页
static inline u_long page2ppn(struct Page *pp) {
	return pp - pages;
}

// 通过页控制块pp获取该页起始位置的物理地址（可用于填充pte）
static inline u_long page2pa(struct Page *pp) {
	//相当于*4096，也就是一页的大小(4KB)，页数乘以一页的大小即可得到其物理地址
	// 与PPN(pa)作用相反
	return page2ppn(pp) << PGSHIFT; 
}

// 通过物理地址pa获取该地址对应的页控制块（读取pte后可进行转换）
static inline struct Page *pa2page(u_long pa) {
	if (PPN(pa) >= npage) {
		panic("pa2page called with invalid pa: %x", pa);
	}
	return &pages[PPN(pa)];
}

// 通过页控制块pp获取该页起始位置的虚拟地址
static inline u_long page2kva(struct Page *pp) {
	return KADDR(page2pa(pp));
}

// 查页表，将虚拟地址转换为物理地址（测试时常用）
static inline u_long va2pa(Pde *pgdir, u_long va) {
	Pte *p;

	pgdir = &pgdir[PDX(va)];
	if (!(*pgdir & PTE_V)) {
		return ~0;
	}
	p = (Pte *)KADDR(PTE_ADDR(*pgdir));
	if (!(p[PTX(va)] & PTE_V)) {
		return ~0;
	}
	return PTE_ADDR(p[PTX(va)]);
}

void mips_detect_memory(u_int _memsize);
void mips_vm_init(void);
void mips_init(u_int argc, char **argv, char **penv, u_int ram_low_size);
void page_init(void);
void *alloc(u_int n, u_int align, int clear);

int page_alloc(struct Page **pp);
void page_free(struct Page *pp);
void page_decref(struct Page *pp);
int page_insert(Pde *pgdir, u_int asid, struct Page *pp, u_long va, u_int perm);
struct Page *page_lookup(Pde *pgdir, u_long va, Pte **ppte);
void page_remove(Pde *pgdir, u_int asid, u_long va);

extern struct Page *pages;

u_int page_conditional_remove(Pde *pgdir, u_int asid, u_int perm_mask, u_long begin_va, u_long end_va);
void physical_memory_manage_check(void);
void page_check(void);

#endif /* _PMAP_H_ */
