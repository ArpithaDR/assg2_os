#include "defs.h"
void *child(void *arg)
{
  if (*(int*)arg > 2) pthread_exit((void*)1);
  return((void*)2);
}
int main(int argc, char *argv[])
{
  pthread_t thread;
  void *result=NULL;
  pthread_create(&thread, 0, child, &argc);
  pthread_join(thread, (void**)&result);
  switch ((int)(long)result) {
  case 1: printf("result is 1\n"); break;
  case 2: printf("result is 2\n"); break;
  }
  return 0;
}
