#include <stdio.h>

int bytte(char setning[], char bok1, char bok2){
  int i = 0;
    while(setning[i]!= '\0'){
      if(setning[i] == bok1){
        setning[i] = bok2;
      }
      i++;
    }
    printf("%s\n", setning);
    return 0;
  }

int main(int argc, char *argv[]){

  bytte(argv[1], argv[2][0], argv[3][0]);
  return 0;
}
