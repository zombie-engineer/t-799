
void self_test_context_switch(void)
{
	volatile register long x0 asm ("x0") = 0;
	volatile register long x1 asm ("x1") = 0;
	volatile register long x2 asm ("x2") = 0;
	volatile register long x3 asm ("x3") = 0;
	volatile register long x4 asm ("x4") = 0;
	volatile register long x5 asm ("x5") = 0;
	volatile register long x6 asm ("x6") = 0;
	volatile register long x7 asm ("x7") = 0;
	volatile register long x8 asm ("x8") = 0;
	volatile register long x9 asm ("x9") = 0;
	volatile register long x10 asm ("x10") = 0;
	volatile register long x11 asm ("x11") = 0;
	volatile register long x12 asm ("x12") = 0;
	volatile register long x13 asm ("x13") = 0;
	volatile register long x14 asm ("x14") = 0;
	volatile register long x15 asm ("x15") = 0;
	volatile register long x16 asm ("x16") = 0;
	volatile register long x17 asm ("x17") = 0;
	volatile register long x18 asm ("x18") = 0;
	volatile register long x19 asm ("x19") = 0;
	volatile register long x20 asm ("x20") = 0;
	volatile register long x21 asm ("x21") = 0;
	volatile register long x22 asm ("x22") = 0;
	volatile register long x23 asm ("x23") = 0;
	volatile register long x24 asm ("x24") = 0;
	volatile register long x25 asm ("x25") = 0;
	volatile register long x26 asm ("x26") = 0;
	volatile register long x27 asm ("x27") = 0;
	volatile register long x28 asm ("x28") = 0;
	volatile register long x29 asm ("x29") = 0;
	int i;
	while(1) {
		i++;
		x0++;
		x1++;
		x2++;
		x3++;
		x4++;
		x5++;
		x6++;
		x7++;
		x8++;
		x9++;
		x10++;
		x11++;
		x12++;
		x13++;
		x14++;
		x15++;
		x16++;
		x17++;
		x18++;
		x19++;
		x20++;
		x21++;
		x22++;
		x23++;
		x24++;
		x25++;
		x26++;
		x27++;
		x28++;
		x29++;
		asm volatile ("svc 0");
	}
}

