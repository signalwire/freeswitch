#include <stdio.h>

int main(void)
{
	printf("#ifndef RTPTYPES_H\n\n");
	printf("#define RTPTYPES_H\n\n");
	printf("#include <sys/types.h>\n\n");
	
	if (sizeof(char) == 1)
	{
		printf("typedef char int8_t;\n");
		printf("typedef unsigned char uint8_t;\n");
	}
	else
		return -1;

	if (sizeof(short) == 2)
	{
		printf("typedef short int16_t;\n");
		printf("typedef unsigned short uint16_t;\n");
	}
	else
		return -1;

	if (sizeof(int) == 4)
	{
		printf("typedef int int32_t;\n");
		printf("typedef unsigned int uint32_t;\n\n");
		printf("#endif // RTPTYPES_H\n");
		return 0;
	}
	if (sizeof(long) == 4)
	{
		printf("typedef long int32_t;\n");
		printf("typedef unsigned long uint32_t;\n\n");
		printf("#endif // RTPTYPES_H\n");
		return 0;
	}
	return -1;
}

