#include <stdio.h>
#include <string.h>
#include <unistd.h>

void testtest2(void)
{
        usleep(50000);
}

void testtest(void)
{
	usleep(500000);
}

int main()
{
   testtest2();
   while(1) {
      testtest();
      usleep(500);
   }
}

