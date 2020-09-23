#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<errno.h>
#include<unistd.h>

#define thread_num 200

void* thread1(void*);

/*
* when program is started, a single thread is created, called the initial thread or main thread.
* Additional threads are created by pthread_create.
* So we just need to create two thread in main().
*/
int main(int argc, char** argv)
{
printf("enter main\n");
pthread_t tid1[thread_num];
int rc1=0;
int i=0;
while(i<thread_num)
{
rc1 = pthread_create(&tid1[i], NULL, thread1, NULL);
if(rc1 != 0)
{
printf("%s: %d\n",__func__, strerror(rc1));
printf("leave main\n");
exit(0); 
}
else
{
i++;
}
}
printf("fork %d thread succ\r\n",thread_num);
while(1)
{
sleep(10);
}
}

int func()
{
unsigned long seed = 0;
int fd=-1;

fd = open("/dev/urandom", O_RDONLY);
if (fd >= 0) {
while(1)
{
read(fd, &seed, sizeof(seed));
usleep(10);
}
close(fd);
}
}
/*
* thread1() will be execute by thread1, after pthread_create()
* it will set g_Flag = 1;
*/
void* thread1(void* arg)
{
sleep(10);//execute together with other thread to read random
func();
}

