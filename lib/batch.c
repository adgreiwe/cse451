#include <inc/batch.h>

void setup_batch(struct batch *b, int sys_no, int p1, int p2,
		 int p3, int p4, int p5)
{
	b->sys_no = sys_no;
	b->p1 = p1;
	b->p2 = p2;
	b->p3 = p3;
	b->p4 = p4;
	b->p5 = p5;
}
