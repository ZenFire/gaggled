#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

void callback(int a) {}

int main() {
  for (int i = 0; environ[i] != NULL; i++)
    printf("env[%d] = %s\n", i, environ[i]);

	signal(SIGTERM, callback);
	signal(SIGINT, callback);

  while (1) {
    sleep(60);
  }
}
