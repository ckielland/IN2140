#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//Total bytes:388
struct ruter{
  unsigned int id;
  unsigned char flag;
  unsigned char length;
  unsigned char producer[250];
  struct ruter* connections[10]; //Array with router struct pointers
  int connection_index;
  int connectionsToMe[10]; //Contains ruterIDs and the connected ruter structs
  int connectionsToMe_index;
  int i;  //utility indexing number to keep track of ruter's indexing position in the ruter list
};

//Globally declared array of pointers of struct router pointers
struct ruter** ruters;

//Globally declared N (total routers)
int N;

// Finds and returns the router pointer from the pointer array according to the ID
// unsigned int ruterID: ID of router struct
// int n: total routers
struct ruter* findRouter(unsigned int ruterID, int n){
  for (int i = 0; i < n; i++){
    if (ruters[i] == NULL){
      printf("Error, no such router exists");
    }
    if (ruters[i]->id == ruterID){
      return ruters[i];
    }
  }
  return NULL;
}

//Prints the binary format of a given pointer
// void *n: a pointer
//int size: the size of the pointer
void printbits(void *n, int size){
  char *num = (char *)n;
  int i,j;

  for(i = size-1; i >= 0; i--){
    for (j = 7; j >= 0; j--){
      printf("%c", (num[i] & (1 << j)) ? '1' : '0');
    }
    printf(" ");
  }
  printf("\n");
}

//Utilitary function to call printbits function
//unsigned char: a char to be printed in binary form
void printchar(unsigned char c){
  printbits(&c, sizeof(char));
}

//based on a the value of an unsigned char it prints whether it is true or false
//char *description: a sentence to be printed together with the boolean value
//unsigned char c: if c is 1 true, else false
void printboolean(char *description, unsigned char c){
  if (c > 0) {
    printf("%s: true\n",description );
  }
  else {printf("%s: false\n", description);}
}

//increments the 7th-4th bits of an unsigned char
//struct ruter* r: a router struct whose change counter will be incremented
void incrementCounter(struct ruter* r){
  //Increment change counter
  unsigned char unaffected = r->flag << 4;
  unaffected = unaffected >> 4;
  unsigned char counter = r->flag >> 4;
  unsigned char lastDigit = counter & (1 << 0);
  if (lastDigit == 0){
    counter |= (1<<0);
  }
  else if (lastDigit == 1){
    counter = counter >> 1;
    counter |= (1<<0);
    counter = counter << 1;
  }
  //update the whole flag
  counter = counter << 4;
  r->flag = counter | unaffected;
}

//prints information about router structs
//unsigned int ruterID: ID of the router
//int n: total ruters
void printRouter(unsigned int ruterID, int n){
    if (findRouter(ruterID,n)==NULL){
      printf("Error, no such router exists");
      return;
    }
     struct ruter* router = findRouter(ruterID, n);
     printf("\n***** Router info: *****\n");
     printf("Router id: %d\n", router->id);
     printf("Flag: ");
     printchar(router->flag);
     printboolean("The router is aktiv", router->flag & (1 << 0));
     printboolean("The router is wireless", router->flag & (1 << 1));
     printboolean("The router supports 5GHz", router->flag & (1 << 2));
     printboolean("The router is unused", router->flag & (1<<3));
     printf("Producer: %s\n", router-> producer);
     printf("Changes counter: %d\n", router->flag >> 4);
     printf("\nConnections: Router IDs connected from this one: \n");
     for(int i = 0; i < router->connection_index; i++) {
          printf("RouterID: %d\n", router->connections[i]->id);
     }
    printf("\nConnections: Router IDs connected to this one: \n");
    for (int i = 0; i < router->connectionsToMe_index; i++){
      printf("RouterID: %d\n", router->connectionsToMe[i]);
    }
    printf("***** End of info *****\n");
  }

  //modifies a char by changing the value in the given bit position to 1
  //unsigned char *flag: pointer to unsigned char whose value will be modified
  //int pos: bit position to be modified
  void setTrue(unsigned char *flag, int pos){
    *flag |= (1 << pos);
  }

  //modifies a char by changing the value in the given bit position to 0
  //unsigned char *flag: pointer to unsigned char whose value will be modified
  //int pos: bit position to be modified
  void setFalse(unsigned char *flag, int pos){
    *flag &= ~(1 << (pos));
  }

  //modifies the flag of a ruter to the given value
  //unsigned int ruterID: ID of the ruter
  //unsigned char pos: which bit of the unsigned char (i.e. the flag) will be modified
  //unsigned char value: the new value
  //int n: total routers
  void setFlag(unsigned int ruterID, unsigned char pos, unsigned char value, int n){
    if (findRouter(ruterID,n)==NULL){
      printf("Error, no such router exists");
      return;
    }

    if ((pos == 3) || (pos <= 2 && value > 1) || (pos == 4 && value > 15)){
      printf("Invalid input");
      return;
    }

    struct ruter* ruter = findRouter(ruterID,n);

    if (pos != 4) {
      if (value == 0){
      setFalse(&ruter->flag, pos);
      }
      else{
        setTrue(&ruter->flag, pos);
      }
    }
      else if (pos == 4) {
       if (value == 0) {
           ruter->flag = ruter->flag << 4;
           ruter->flag = ruter->flag >> 4;
       } else {
           ruter->flag |= (value << pos);
       }
     }
  }

  //updates the model info of a router
  //unsigned int ruterID: ID of the router
  //unsigned char *model: new model value
  //int n: total routers
  void setModel(unsigned int ruterID, unsigned char *model, int n){
    if (findRouter(ruterID,n)==NULL){
      printf("Error, no such router exists");
      return;
    }
    struct ruter* ruter = findRouter(ruterID,n);
    strcpy(ruter->producer, model);
    ruter->length = strlen(model) + 1;
  }

  //Adds a struct router to the array of routers of a router struct
  //unsigned int ruterID1: ID of the router whose array will be updated
  //unsigned int ruterID2: ID of the router to be added
  //int n: total routers
  void addConnection(unsigned int ruterID1, unsigned int ruterID2, int n){
    if ((findRouter(ruterID1,n)==NULL) || (findRouter(ruterID2,n)==NULL)){
      printf("Error, at least one of the two routers provided does not exist");
      return;
    }


    struct ruter* ruter1 = findRouter(ruterID1,n);
    struct ruter* ruter2 = findRouter(ruterID2,n);

    for (int i = 0; i < ruter1->connection_index; i ++){
      if (ruter1->connections[i]->id == ruterID2){
        printf("These routers are already connected to each other in this direction, from %d to %d", ruterID1, ruterID2);
        return;
      }
    }

    ruter1->connections[ruter1->connection_index] = ruter2;
    ruter1->connection_index++;
    ruter2->connectionsToMe[ruter2->connectionsToMe_index] = ruterID1;
    ruter2->connectionsToMe_index++;

    incrementCounter(ruter1);
  }

  //deletes a struct router from the network
  //unsigned int ruterID: ID of the router
  //int n: total routers
  void deleteRouter(unsigned int ruterID, int *n){
    if (findRouter(ruterID,*n)==NULL){
      printf("Error, no such router exists");
      return;
    }

    struct ruter* ruter = findRouter(ruterID,*n);

    //Pointer to incremen the change counter of the affected routers
    char *counter;

    //Remove incoming pointers, modify change counter
    for (int i = 0; i < ruter->connectionsToMe_index; i++){
      struct ruter* affectedRuter = findRouter(ruter->connectionsToMe[i], *n);
      for (int j = 0; j < affectedRuter->connection_index; j++){
        if (affectedRuter->connections[j]->id == ruter->id){
          //Remove pointer
          affectedRuter->connections[j] = NULL;
          affectedRuter->connection_index--;
          //Increment change counter
          incrementCounter(affectedRuter);
        }
      }
    }

    //Notify my connected ruters for the change -- increment their change counter
    for (int i = 0; i < ruter->connection_index; i++){
      struct ruter* affectedRuter = findRouter(ruter->connections[i]->id, *n);
      for (int j = 0; j < affectedRuter->connectionsToMe_index; j++){
        if (affectedRuter->connectionsToMe[j] == ruter->id){
          affectedRuter->connectionsToMe[j] = -1;
          affectedRuter->connectionsToMe_index--;
          incrementCounter(affectedRuter);
        }
      }
    }

    //Delete the ruter from the list
    for (int i = 0; i < *n; i++){
      if (ruters[i]->id == ruterID){
        ruters[i] == NULL;
      }
    }
    *n--;
  }

  //Utility function to perfrom DFS. Recursive function
  //int i: index position in the array of routers of the source node
  //int j: index position in the array of routers of the destination node
  //unsigned int *visited: pointer to an array to keep track of which nodes were visited
  int dfsUtility(int i, int j, unsigned int *visited) {
    if (i == j) {
         return 1;
    }

    if (visited[i] == 1) {
         return 0;
    }
    visited[i] = 1;



    for (int l = 0; l < ruters[i]->connection_index; l++) {
        if (dfsUtility(ruters[i]->connections[l]->i, j, visited) == 1) {
             return 1;
        }
    }
    return 0;
  }

  //Performs Depth First Seach of a graph structure
  //unsigned int start: ID of source node
  //unsigned int end : ID of destination node
  //int n: total routers (nodes)
  int dfs(unsigned int start, unsigned int end, int n) {
    if ((findRouter(start,n)==NULL) || (findRouter(end,n)==NULL)){
      printf("Error, at least one of the two routers provided does not exist");
      return 0;
    }

    int i; //to facilitate indexing in the visited[] list
    int j;

    for (int k = 0; k < n; k++){
      if (ruters[k]->id == start){
        i = ruters[k]->i;
      }
      if (ruters[k]->id == end){
        j = ruters[k]->i;
      }
    }

    // Depth First Search
    unsigned int visited[n];
    for(int i = 0; i < n; i++) {
        visited[i] = 0;
    }
    return dfsUtility(i, j, visited);
  }

//Reads the topology.dat file
//const char *filename = filename,provided in the command line
int readTopology(const char *filename){

  FILE *topologyFile = fopen(filename, "rb");

  //Checking if the topologyFile is valid
  if (topologyFile == NULL) {
     perror("fopen");
     return EXIT_FAILURE;
   }

  int n = fread(&N, sizeof(int), 1, topologyFile);

  //Dynamically allocate memory from the heap
  //N * 8 bytes
  ruters = malloc(sizeof(struct ruter*) * N);

  for (int i = 0; i < N; i++){

    //Read router info
    //Allocates 388 bytes
    ruters[i] = malloc(sizeof(struct ruter));
    int rc = fread(&ruters[i]->id, sizeof(unsigned int), 1, topologyFile);
    rc = fread(&ruters[i]->flag, sizeof(unsigned char), 1, topologyFile);
    rc = fread(&ruters[i]->length, sizeof(unsigned char), 1, topologyFile);
    //OBS! Reading the producer info is not working. I have tried to fix it, but cannot find the bug. Any input is highly appreciated.
    rc = fread(&ruters[i]->producer, sizeof(unsigned char), (size_t) ruters[i]->length, topologyFile);
    rc = fread(&ruters[i]->producer, 1, 1, topologyFile); //To read the null byte
    ruters[i]->connection_index = 0;
    ruters[i]->connectionsToMe_index = 0;
    ruters[i]->i = i;
    }

    //Read info about numConnections
    unsigned int bufferID[2];
    struct ruter* ruter1;
    struct ruter* ruter2;
    while (fread(bufferID, sizeof(unsigned int), 2, topologyFile) == 2) {
      ruter1 = findRouter(bufferID[0], N);
      ruter2 = findRouter(bufferID[1], N);
      ruter1->connections[ruter1->connection_index] = ruter2;
      ruter1->connection_index++;
      ruter2->connectionsToMe[ruter2->connectionsToMe_index] = bufferID[0];
      ruter2->connectionsToMe_index++;
    }

   fclose(topologyFile);
}

//Reads the commands.txt file
//const char *filename = filename,provided in the command line
int readCommands(const char *filename){
  FILE *commandsFile = fopen(filename, "r");

  //Checking if the commandosFile is Valid
  if (commandsFile == NULL){
    perror("fopen");
    return EXIT_FAILURE;
  }

  char *command;
  char buffer[500];

  //Read file
  while (fgets(buffer, 500, commandsFile)) {
        //split the string into tokens
          command = strtok(buffer, " ");

         if (strcmp(command, "print") == 0) {
            //Move in the next token and convert the str to int. The second argument is the last of
            //this sentence, i.e. the deliminator should be \n
              command = strtok(NULL, "\n");
              printRouter((unsigned int) atoi(command), N);
            }

          else if(strcmp(command, "sett_flagg")==0) {
            //Here the new token is in the same line,i.e. use " " as deliminator
            command = strtok(NULL, " ");
            unsigned int id = (unsigned int) atoi(command);
            command = strtok(NULL, " ");
            unsigned char pos = (unsigned char) atoi(command);
            command = strtok(NULL, "\n");
            unsigned char value = (unsigned char) atoi(command);
            setFlag(id, pos, value, N);
          }

          else if(strcmp(command, "sett_modell") ==0){
            command = strtok(NULL, " ");
            unsigned int id = (unsigned int) atoi(command);
            command = strtok(NULL, "\n");
            setModel(id,command, N);
          }

          else if(strcmp(command, "legg_til_kobling") == 0){
            command = strtok(NULL, " ");
            unsigned int id1 = (unsigned int) atoi(command);
            command = strtok(NULL, "\n");
            unsigned int id2 = (unsigned int) atoi(command);
            addConnection(id1, id2, N);
          }

          else if(strcmp(command, "slett_ruter") == 0){
            command = strtok(NULL, "\n");
            unsigned int id = (unsigned int) atoi(command);
            deleteRouter(id, &N);
          }

          else if(strcmp(command, "finnes_rute") == 0){
            command = strtok(NULL, " ");
            unsigned int id1 = (unsigned int) atoi(command);
            command = strtok(NULL, "\n");
            unsigned int id2 = (unsigned int) atoi(command);
            if (dfs(id1, id2, N) == 1){
              printf("\nRoute found from ruter with id %d to ruter with id %d\n", id1, id2);
            }
            else{printf("\nNo such route exists (i.e. from ruter with id %d to ruter with id %d)\n", id1, id2);}
          }
          else{printf("\nWrong command\n");}
    }


  fclose(commandsFile);
}

//Writes all data to an output file
int writeToFile(){
  FILE *outputFile = fopen("new-topology.dat", "wb");

  if (outputFile == NULL){
    perror("fopen");
    return EXIT_FAILURE;
  }

  fwrite(&N, sizeof(int), 1, outputFile);

  for (int i = 0; i < N; i++){
    if (ruters[i]!= NULL){
      fwrite(&ruters[i]->id, sizeof(unsigned int), 1, outputFile);
      fwrite(&ruters[i]->flag, sizeof(unsigned char), 1, outputFile);
      fwrite(&ruters[i]->length, sizeof(unsigned char), 1, outputFile);
      fwrite(&ruters[i]->producer, sizeof(unsigned char), (size_t) ruters[i]->length, outputFile);
      fwrite(&ruters[i]->producer, 1, 1, outputFile);
    }
  }

  for (int i = 0; i < N; i++) {
       if (ruters[i] != NULL) {
           for (int j = 0; j < ruters[i]->connection_index; j++) {
               fwrite(&ruters[i]->id, sizeof(unsigned int), 1, outputFile);
               fwrite(&ruters[i]->connections[j]->id, sizeof(unsigned int), 1, outputFile);
          }
       }
     }

  fclose(outputFile);
}

//Free allocated memory
//First free the memory allocated for each router (388 bytes)
//Then free the memory allocated for the whole router array (N*8 bytes)
int freeAll(){
  for (int i = 0; i < N; i++){
    free(ruters[i]);
  }
  free(ruters);
}

int main(int argc, char const *argv[]) {

  //Check that the number of arguments is correct
  if (argc < 3) {
      printf("Usage: %s [topologyFile] [commandsFile]\n", argv[0]);
      return EXIT_FAILURE;
    }

  readTopology(argv[1]);

  readCommands(argv[2]);

  writeToFile();

  freeAll();

  return EXIT_SUCCESS;
}
