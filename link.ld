SECTIONS
{
	.bootload 0x0 (NOLOAD) : { start.o(.bootload) }

	. = 0x80000;
	.text : {
		*.o(.text)
	}
}
