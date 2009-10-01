#include <unistd.h>

void parse_options( int* argc, char*** argv){

  int ch;
  extern char* optarg;
  extern int optind;
  extern int opterr;

  while ((ch = getopt(*argc, *argv, "ab:")) != -1){
    switch (ch){
    case 'a':
      printf("Option a is selected.\n");
      break;
    case 'b':
      printf("Option b is selected.\n");
      printf("Value = %d\n",atoi(optarg));
      break;
    }
  }
  *argc -= optind;
  *argv += optind;
  
}
