#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void logger(const char* tag, const char* message) {
   time_t now;
   time(&now);
   if(strcmp(tag, "INFO") != 0) {
     printf("%s [%s]: %s\n", ctime(&now), tag, message);
   }
}
