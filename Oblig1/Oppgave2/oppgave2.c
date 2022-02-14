#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int stringsum(char* s){
  int sum = 0;
  char* p = &s[0];
  while (*p != '\0'){
    int ascii = (int) *p;
    if(ascii < 65 || ascii > 122){
      return -1;
    }
    if (ascii > 90 && ascii < 97){
      return -1;
    }
    if(ascii >= 97 && ascii <= 122){
        sum = sum + ascii -96;
    }
    else if(ascii >= 65 && ascii <= 90){
      sum = sum + ascii - 64;
    }
    p++;
  }
  return sum;
}

int distance_between(char* s, char c){
  int counter = 0;
  int distance = -1;
  char* p = &s[0];
  while (*p != '\0' && counter != 2){
    if (*p == c){
      counter ++;
      }
      if (counter > 0){
        distance++;
      }
      p++;
    }
    if (counter == 1) {distance = -1;}
    return distance;
  }

char* string_between(char *s, char c){
  int counter = 0;
  int distance = -1;
  int i = 0;
  int start_index = 0;
  char* p = &s[0];

  while (*p != '\0' && counter != 2){
    if (*p == c){
      counter ++;
      if (counter == 1) {start_index=i+1;}
      }
      if (counter > 0){
        distance++;
      }
      i++;
      p++;
    }

    if (counter == 1) {distance = -1;}

    if (distance == -1){return NULL;}

    char *word = (char *) malloc(distance);

    if (word == NULL){
      printf("Malloc did not manage to allocate memory");
      exit(1);
    }

    int characters = distance - 1;

    strncpy(word, s+start_index, characters);
    word[distance-1] = '\0';

    return word;
  }


  void stringsum2(char* s, int* res){
    *res = stringsum(s);
    printf("The value is: %d\n", *res);
  }
