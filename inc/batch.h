#ifndef JOS_INC_BATCH_H
#define JOS_INC_BATCH_H

#define MAXBATCH 20

struct batch {
	int sys_no;
	int p1;
	int p2;
	int p3;
	int p4;
	int p5;
};

void setup_batch(struct batch *b, int sys_no, int p1, int p2,
		 int p3, int p4, int p5);

#endif
