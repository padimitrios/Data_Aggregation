/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         Testing the broadcast layer in Rime
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "net/rime/rime.h"
#include "random.h"
#include "apps/powertrace/powertrace.h"
#include "energest.h"


#include "dev/button-sensor.h"

#include "dev/leds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/time.h>
#include <math.h>

static unsigned long rx_start_duration,cpu_start_duration,lpm_start_duration,normal_mote_consumption;

/*---------------------------------------------------------------------------*/
PROCESS(example_broadcast_process, "Broadcast example");
AUTOSTART_PROCESSES(&example_broadcast_process);
/*---------------------------------------------------------------------------*/
  

/*------------------------------------Tree-------------------------------------- */
struct node{
  char *tags;
  struct node *left;
  struct node *right;
};

/* create new node */
struct node* createNode(char *tags) {
  struct node* newNode = malloc(sizeof(struct node));
  newNode->tags = tags;
  newNode->left = NULL;
  newNode->right = NULL;

  return newNode;
}

/* Tree insertion functions for simple nodes */
struct node* insertLeft(struct node* root, char *tags) {
  root->left = createNode(tags);
  return root->left;
}

struct node* insertRight(struct node* root, char *tags) {
  root->right = createNode(tags);
  return root->right;
}

/* Functions to insert a whole subtree on the left or right of the root */
struct node* insertParentRight(struct node* root,struct node* subParent){
  root->right = subParent;
  return root->right;
}

struct node* insertParentLeft(struct node* root,struct node* subParent){
  root->left = subParent;
  return root->left;
}

int search(struct node* node, char *key){
    if (node == NULL)
        return 0;
 
    search(node->left,key);
  
    //found the key
    if (!strcmp(node->tags,key)){
      return 1;
    }
  
    search(node->right,key);

    return 0;
}

/* Used in level traversal */
int heightoftree(struct node *root){
    int max;
  
    if (root!=NULL)
    {
        /* Finding the height of left subtree. */
        int leftsubtree = heightoftree(root->left);
        /* Finding the height of right subtree. */
        int rightsubtree = heightoftree(root->right);
      
        if (leftsubtree > rightsubtree){
            max = leftsubtree + 1;
            return max;
        }
        else{
            max = rightsubtree + 1;
            return max;
        }
    }
}

int height_of_binary_tree(struct node *node){
  if(node == NULL)
    return 0;
  else {
    int left_side;
    int right_side;
    left_side = height_of_binary_tree(node -> left);
    right_side = height_of_binary_tree(node -> right);
    
    if(left_side > right_side){
      return left_side + 1;
    }else
      return right_side + 1;
  }
}
/* Function to print all the nodes left to right of the current level */
void currentlevel(struct node *root, int level){
    
  if (root != NULL) {
    if (level == 1){
        printf("%s |", root->tags);
    }
    else if (level > 1){ 
        currentlevel(root->left, level-1); 
        currentlevel(root->right, level-1);
    }     
  }
}

/*------------------------------------List-------------------------------------- */

struct list_node{
  int id;
  char *tags;
  struct list_node *next;
};

void insertAtBeginning(struct list_node** head_ref, int id, char *tags) {
  // Allocate memory to a node
  struct list_node* new_node = (struct list_node*)malloc(sizeof(struct list_node));

  // insert the data
  new_node->id = id;
  new_node->tags = strdup(tags);

  new_node->next = (*head_ref);

  // Move head to new node
  (*head_ref) = new_node;
}

void deleteNode(struct list_node** head_ref, int key) {
  struct list_node *temp = *head_ref, *prev;

  if (temp != NULL && temp->id == key) {
    *head_ref = temp->next;
    free(temp);
    return;
  }
  // Find the key to be deleted
  while (temp != NULL && temp->id != key) {
    prev = temp;
    temp = temp->next;
  }

  // If the key is not present
  if (temp == NULL) return;

  // Remove the node
  prev->next = temp->next;

  free(temp);
}

void insertAtEnd(struct list_node** head_ref, int id, char tags[]) {
  struct list_node* new_node = (struct list_node*)malloc(sizeof(struct list_node));
  struct list_node* last = *head_ref; /* used in step 5*/

  new_node->id = id;
  new_node->tags = strdup(tags);
  
  new_node->next = NULL;

  if (*head_ref == NULL) {
  *head_ref = new_node;
  return;
  }

  while (last->next != NULL) last = last->next;

  last->next = new_node;
  return;
}

void printList(struct list_node* node) {
  while (node != NULL) {
    printf(" %d: %s\n", node->id,node->tags);
    node = node->next;
  }
}

/*------------------------------------END-------------------------------------- */

/*--------------------------------- DISTANCES ----------------------------------*/
/* Hamming Distance */
int hammingDistance(char str1[], char str2[]){
    int i = 0, count = 0;
  
    while(str1[i]!='\0'){
        if (str1[i] != str2[i])
            count++;
        i++;
    }
      count += abs((int)strlen(str1)-(int)strlen(str2));
    return count;
}

/* Levenstein Distance */
int levDistance (const char * word1,int len1,const char * word2,int len2){
    int matrix[len1 + 1][len2 + 1];
    int i;
    for (i = 0; i <= len1; i++) {
        matrix[i][0] = i;
    }
    for (i = 0; i <= len2; i++) {
        matrix[0][i] = i;
    }
    for (i = 1; i <= len1; i++) {
        int j;
        char c1;

        c1 = word1[i-1];
        for (j = 1; j <= len2; j++) {
            char c2;

            c2 = word2[j-1];
            if (c1 == c2) {
                matrix[i][j] = matrix[i-1][j-1];
            }
            else {
                int delete;
                int insert;
                int substitute;
                int minimum;

                delete = matrix[i-1][j] + 1;
                insert = matrix[i][j-1] + 1;
                substitute = matrix[i-1][j-1] + 1;
                minimum = delete;
                if (insert < minimum) {
                    minimum = insert;
                }
                if (substitute < minimum) {
                    minimum = substitute;
                }
                matrix[i][j] = minimum;
            }
        }
    }
    return matrix[len1][len2];
}
/*------------------------------------------------------------------------------*/

char *combination(char *str_1, char *str_2){
  char *final_str;
  final_str = (char*)malloc ((strlen(str_1)+strlen(str_2)+1) * sizeof (char));
  
  int i,j;
  for(i=0;i<strlen(str_1);i++){
    final_str[i] = str_1[i];
  }
  
  int index = i;
  int k=0;
  int prev=0;

  char word[2000];
  final_str[index] = '-';
  index++;
  //printf("%s\n",final_str);
  
  char *token = strtok(str_2,"-");

  while(token != NULL){
    //printf( "%s\n", token );
    int b = strstr(final_str,token) == NULL;
    if (b){
        /*str foung oh my god*/
      for (i=0;i<strlen(token);i++){
        final_str[index] = token[i];
        index++;
      }
      final_str[index] = '-';
      index++;
    }
    //printf( " %d\n", b );
    token = strtok(NULL, "-");
  }

  if(final_str[index-1] == '-'){
    final_str[index-1] = '\0';
    return final_str;
  }

  final_str[index] = '\0';
  //printf("\nfinal_str: %s",final_str);
  return final_str;
}

/* character triming */
char *trim(char *str,char ch){
  
  int len = strlen(str);
  int i,j;
        
    for(i = 0; i < len; i++){
      
      if(str[i] == ch){
        for(j = i; j < len; j++)
        {
          str[j] = str[j + 1];
        }
        len--;
        i--;  
      } 
    } 

  return str;
}

/*---------------------------------------------------------------------------*/
/*--------------------------tree gen function--------------------------------*/
/*---------------------------------------------------------------------------*/

struct node* tree_generation(int argc, char **argv) {
  
  struct list_node* head = NULL;
  struct node* tree_head = NULL;

  int maximum_tag_length = -1;
  /* Ta arguments einai oi etiketes */
  int i,j;
  for (i = 1; i < argc; ++i){
    /* Thelei typcast giati to strlen tou argv einai unsigned long int */
    if ((int)strlen(argv[i]) > maximum_tag_length){
      maximum_tag_length = strlen(argv[i]);
    }
  }

  /* filling the tags table and align them 
     maximum_tag_length + 1 because we will add '\0' to declare end of the string   
  */
  char tags_table[argc-1][(int)maximum_tag_length+1];  
  for ( i = 0; i < argc-1; ++i){
    int final_char = maximum_tag_length;
    for ( j =0; j <(int)strlen(argv[i+1]);++j){
      tags_table[i][j] = argv[i+1][j];
      final_char = j;
    }
    tags_table[i][final_char+1] = '\0';
    if ( i==0 ){
      insertAtBeginning(&head,i,tags_table[i]); 
    }else{
      insertAtEnd(&head,i,tags_table[i]);
    }
  }

  //printList(head);


  // for (int i =0; i < argc-1; i++){
  //   for(int j=0;j<(int)maximum_tag_length;++j){
  //     printf("%c",tags_table[i][j]);
  //   }
  //   printf("\n");
  //   printf("[%d]: %s\n",i+1,tags_table[i]);
  // }

  //initial dif array
  float differences[argc-1][argc-1];
  memset( differences, 0, sizeof(differences));

  for ( i =0; i < argc-1; i++){
    for( j=i+1;j<argc-1;++j){
      differences[i][j] = hammingDistance(tags_table[i],tags_table[j]);
      //differences[i][j] = levDistance(tags_table[i],strlen(tags_table[i]),tags_table[j],strlen(tags_table[j]));
    }
  }

  /* Fidning minimum value */
  int table_size = argc-1;

  /* Tree root */
  char* first_tag_string;
  char* second_tag_string;
  struct list_node* temp = NULL;

  /* To table size antiprosopeuei ta stoixeia ths listas otan ftasoume sto 1 tha exoyme to teliko super_set twn stoixeiwn (legetai table_size giati abtistoixei kai sto megethos tou diff table) */
  while(table_size != 1){

    temp = head;
    float minVal = 99999;
    int minRow = -1;
    int minCol = -1;

    //find min -> nearest couple
    for ( i =0; i < table_size; i++){
      for( j=i+1;j<table_size;++j){
        if (differences[i][j] < minVal){
          minVal = differences[i][j];
          minRow = i;
          minCol = j;
        }
      }
    }

    //printf("minclo/row val:%d %d %.f\n",minCol,minRow,minVal);

    //copy nearest couple tags
    while(temp != NULL){
      if(temp->id == minRow){
        first_tag_string = strdup(temp->tags);
      }
      if(temp->id == minCol){
        second_tag_string = strdup(temp->tags);
      }
      temp = temp->next;
    }

                      /* helper prints */
    // printf("i am the first string %s\n",first_tag_string);
    // printf("i am the second string %s\n",second_tag_string);

    //keep a copy of the second string because it's getting deformed by the combination function
    char *temp_second_tag_string = strdup(second_tag_string);
    temp_second_tag_string = trim(temp_second_tag_string,'_');

    //sundiasmos twn min couple tags
    char *aggregated_tags = combination(trim(first_tag_string,'_'),trim(second_tag_string,'_'));
    // printf("i am the second string after comb %s\n",temp_second_string);
    // printf("i am the first string afeter comb %s\n",first_tag_string);

    //to node poy tha einai o pateras tou zeugariou
    struct node* parent_of_childs = createNode(aggregated_tags);
    int first_tag_exists = search(tree_head,first_tag_string);
    int second_tag_exists = search(tree_head,temp_second_tag_string);

                      /*Tree formation*/
    if(tree_head == NULL){
        insertRight(parent_of_childs,first_tag_string);
        //printf("%s |",parent_of_childs->right->tags);
        insertLeft(parent_of_childs,temp_second_tag_string);
        //printf("%s \n",parent_of_childs->left->tags);
        tree_head = parent_of_childs;
    }else if(first_tag_exists && second_tag_exists){
      /*do nothing if both strings already exist */
      
    }else if(first_tag_exists){
      /* An einai to first string anagkastika den einai to deytero opote to dhmiourgoume kai to bazoume aristera enw vazoume to upoloipo dentro dexia */
      
      insertLeft(parent_of_childs,temp_second_tag_string);
      //printf("%s | ",parent_of_childs->left->tags);
      insertParentRight(parent_of_childs,tree_head);
      //printf("%s \n",parent_of_childs->right->tags);
      tree_head = parent_of_childs;

    }else if(second_tag_exists){
      
      insertLeft(parent_of_childs,first_tag_string);
      //printf("%s | ",parent_of_childs->left->tags);
      insertParentRight(parent_of_childs,tree_head);
      //printf("%s \n",parent_of_childs->right->tags);
      tree_head = parent_of_childs;
    }else{
      
      /* An den einai kanena apo ta 2 strings eidh sto dentro tote dimourgoume 2 kainourgia nodes ta bazoume se ena ypodentro kai to prosartoume sto arxiko mas dentro*/
      insertLeft(parent_of_childs,temp_second_tag_string);
      //printf("%s |",parent_of_childs->left->tags);
      insertRight(parent_of_childs,first_tag_string);
      //printf("%s \n\n",parent_of_childs->right->tags);
      
      
      struct node* super_parent_of_childs;
      //ftiaxnw to copy epeidh xalaei me to combination kai thelei epanaxrikopoihsh
      char *temp_head_tag_string = strdup(tree_head->tags);
      temp_head_tag_string = trim(temp_head_tag_string,'_');
      
      char *super_aggregated_tags = combination(trim(parent_of_childs->tags,'_'),trim(tree_head->tags,'_'));

      //epanaorizoume to pleon xalasmeno string me auto poy htan sthn arxh
      tree_head->tags = temp_head_tag_string;
      
      super_parent_of_childs = createNode(super_aggregated_tags);
      //printf("\nsuper %s\n",super_parent_of_childs->tags);

      insertParentLeft(super_parent_of_childs,parent_of_childs);
      //printf("%s |",super_parent_of_childs->left->tags);
      insertParentRight(super_parent_of_childs,tree_head);
      //printf("%s \n",super_parent_of_childs->right->tags);
      tree_head = super_parent_of_childs;
    }

    /* Sbinoume ta min nodes apo th lista kai ths prosthetoume to neo node poy einai sundiasmos twn allwn */
    deleteNode(&head, minCol);
    deleteNode(&head, minRow);
    //to 4 einai akyrh timh gia to id poy tha allaxei sto reset ids
    insertAtBeginning(&head,4,aggregated_tags);
    //printf("\n agg: %s\n",head->tags);

    temp = head;
    int index = 0;

    //reset ids
    while(temp != NULL){
      temp->id = index;
      index++;
      temp = temp->next;
    }

                      /* helper print */
    // printf("\n list\n");
    // printList(head);
    // printf("\n");

    //epanarxikopoihsh se 0 gia na broume tis kainourgiew diafores
    memset( differences, 0, sizeof(differences));

    //prospelash ths listas gia na ypologisoume tis apostaseis olwn twn stoixeiwn ths metaxy tous
    temp = head;
    struct list_node* temp_2 = head;
    int i,j;
    i = 0;
    j = 0;

    //printf("\nUpdating distance table iteration %d:\n",table_size);
    while(temp != NULL){
      //printf("\nid:%d tags:%s:\n",temp->id,temp->tags);
      temp_2= temp->next;
      j = i+1;
      while(temp_2 != NULL){
        //printf("\ncompared to -->id:%d tags:%s:\n",temp_2->id,temp_2->tags);
        differences[i][j] = hammingDistance(temp->tags,temp_2->tags); 
        //levDistance(temp->tags,strlen(temp->tags),temp_2->tags,strlen(temp_2->tags));
        //printf("%s   %s\n",temp->tags,temp_2->tags);

        temp_2 = temp_2->next;
        j++;
      }
      
      i++;
      temp = temp->next;
    }


                /* Print distance table */
    // for (i=0;i<table_size-1;i++){
    //   for(j=i;j<table_size-1;j++){
    //     printf("%.2f ",differences[i][j]);
    //   }
    //   printf("\n");
    // }
    
    table_size--;
  }
  
  // //printInorder(tree_head);
  int height = height_of_binary_tree(tree_head);

  //ektypwnoume to teliko dentro
  for( i = 1; i <= height; i++){
        printf("%d->",i);
        currentlevel(tree_head,i);
        printf("\n");
  }

  return tree_head;
}

/*-----------------------------statistical calculation------------------------------------*/

struct statistical_list_node{
  char *tags;
  int q;
  int sum;
  struct list_node *next;
};

void insertStatistical(struct statistical_list_node** head_ref, int val, char *tags) {
  // Allocate memory to a node
  struct statistical_list_node* new_node = (struct statistical_list_node*)malloc(sizeof(struct statistical_list_node));

  // insert the data
  new_node->sum = val;
  new_node->q = 1;
  new_node->tags = strdup(tags);

  new_node->next = (*head_ref);

  // Move head to new node
  (*head_ref) = new_node;
}

int search_for_tag(struct statistical_list_node** head_ref,int val, char *tags){
  struct statistical_list_node *temp = *head_ref;

  while(temp != NULL){

    if(!strcmp(temp->tags,tags)){
      temp->q++;
      temp->sum += val;
      return 1;
    }
    
    temp = temp->next;
  }

  return 0;
}

void printStatisticalMean(struct statistical_list_node** head_ref) {
  struct statistical_list_node *temp = *head_ref;

  while (temp != NULL) {
    //printf(" Total value: %d Tag: %s Quantity: %d\n", temp->sum,temp->tags,temp->q);
    int mean = temp->sum / temp->q;
    printf(" The mean of values for the tag %s is: %u\n",temp->tags,mean);
    temp = temp->next;
  }
}

struct statistical_list_node* tags_head = NULL;


int one_iteration = 0;
struct node* root;

/*anti gia rimeaddr_t bazoume to linkaddr_t einai to idio*/
static void
broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from)
{
  //printf("broadcast message received from %d.%d: '%d'\n",from->u8[0], from->u8[1], random_rand()%55);
  // unsigned long total_energy = energest_type_time(ENERGEST_TYPE_LISTEN) + energest_type_time(ENERGEST_TYPE_TRANSMIT);
  // printf("\n Starting nergy: %u \n",total_energy);

  int temp_val;
  int hum_val;
  int co2_val;
  int energy_val;
  int light_val;
  int mold_val;

  normal_mote_consumption = energest_type_time(ENERGEST_TYPE_CPU);

  /*Tha psaxnw an ta tags einai sth lista an nai auxanw plhthos kata 1 kai prosthetw th timh toy sto sum*/
  /*na ta kanw ena tsek sto replit*/

  switch(from->u8[0]){
        case 1:
            temp_val = random_rand()%55;
            hum_val = random_rand()%55;
            if(!search_for_tag(&tags_head,temp_val,"TEMP")){
              insertStatistical(&tags_head,temp_val,"TEMP");
            }
            if(!search_for_tag(&tags_head,hum_val,"HUM")){
              insertStatistical(&tags_head,hum_val,"HUM");
            }

            printf("broadcast message received from %d.%d with tags TEMP-HUM: '%d' - '%d' \n",from->u8[0], from->u8[1], temp_val,hum_val);
            break;

        case 2:
            temp_val = random_rand()%55;
            hum_val = random_rand()%55;
            co2_val = random_rand()%55;
            if(!search_for_tag(&tags_head,temp_val,"TEMP")){
              insertStatistical(&tags_head,temp_val,"TEMP");
            }
            if(!search_for_tag(&tags_head,hum_val,"HUM")){
              insertStatistical(&tags_head,hum_val,"HUM");
            }
            if(!search_for_tag(&tags_head,co2_val,"CO2")){
              insertStatistical(&tags_head,co2_val,"CO2");
            }

            printf("broadcast message received from %d.%d with tags TEMP-HUM-CO2: '%d'-'%d'-'%d' \n",from->u8[0], from->u8[1], temp_val,hum_val,co2_val);
            break;

        case 3:
            energy_val = random_rand()%55;
            light_val = random_rand()%55;
            if(!search_for_tag(&tags_head,energy_val,"ENERGY")){
              insertStatistical(&tags_head,energy_val,"ENERGY");
            }
            if(!search_for_tag(&tags_head,light_val,"LIGHT")){
              insertStatistical(&tags_head,light_val,"LIGHT");
            }
            printf("broadcast message received from %d.%d with tags ENERGY-LIGHT: '%d'-'%d' \n",from->u8[0], from->u8[1], energy_val,light_val);
            break;

        case 4:
            hum_val = random_rand()%55;
            temp_val = random_rand()%55;
            mold_val = random_rand()%55;
            if(!search_for_tag(&tags_head,temp_val,"TEMP")){
              insertStatistical(&tags_head,temp_val,"TEMP");
            }
            if(!search_for_tag(&tags_head,hum_val,"HUM")){
              insertStatistical(&tags_head,hum_val,"HUM");
            }
            if(!search_for_tag(&tags_head,mold_val,"MOLD")){
              insertStatistical(&tags_head,mold_val,"MOLD");
            }
            printf("broadcast message received from %d.%d with tags HUM-MOLD-TEMP: '%d'-'%d'-'%d' \n",from->u8[0], from->u8[1], hum_val,mold_val,temp_val);
            break;

        default:
            printf("UKNWOWN SIGNAL");
    }

    printStatisticalMean(&tags_head);

    char *strings[] = {
        "iamgarbagevalue","TEMP-HUM","TEMP-HUM-CO2","ENERGY-LIGHT","HUM-MOLD-TEMP"
      };

    // rx_start_duration = energest_type_time(ENERGEST_TYPE_LISTEN);
    cpu_start_duration = energest_type_time(ENERGEST_TYPE_CPU);
    lpm_start_duration = energest_type_time(ENERGEST_TYPE_LPM);
    //deep_lpm_start_duration = energest_type_time(ENERGEST_TYPE_DEEP_LPM);

    if(one_iteration == 0){

      rtimer_clock_t start;
      start = RTIMER_NOW();
      root = tree_generation(14,strings);
      printf("The guide tree took %u ticks to generate (rtimer tick is 1/32768 sec) \n", (RTIMER_NOW() - start));
      one_iteration++;
    }

    /*To krataw gia duty cycle*/
    printf("The energy consumed tree gen: %lu \n", ((energest_type_time(ENERGEST_TYPE_CPU) - cpu_start_duration)/*+(energest_type_time(ENERGEST_TYPE_CPU) - lpm_start_duration)*/));
    printf("The energy consumed normal: %lu \n", ((energest_type_time(ENERGEST_TYPE_CPU) - normal_mote_consumption)));
    // printf("energy rx cpu lpm: %lu %lu %lu and total power cons: %lu\n",
    // energest_type_time(ENERGEST_TYPE_LISTEN) - rx_start_duration,
    // energest_type_time(ENERGEST_TYPE_CPU) - cpu_start_duration,
    // energest_type_time(ENERGEST_TYPE_LPM) - lpm_start_duration),
    // (energest_type_time(ENERGEST_TYPE_LISTEN) - rx_start_duration)/((energest_type_time(ENERGEST_TYPE_CPU) - cpu_start_duration)+(energest_type_time(ENERGEST_TYPE_LPM) - lpm_start_duration))*0.02*3;


    // total_energy -= (unsigned long)(energest_type_time(ENERGEST_TYPE_LISTEN) + energest_type_time(ENERGEST_TYPE_TRANSMIT));
    // printf("\n %u \n",total_energy);
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
//static unsigned long after_rx_start_duration,after_cpu_start_duration,after_lpm_start_duration,energy_consumed;

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_broadcast_process, ev, data)
{
  static struct etimer et;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();

  broadcast_open(&broadcast, 129, &broadcast_call);
  // rx_start_duration = energest_type_time(ENERGEST_TYPE_LISTEN);
  // cpu_start_duration = energest_type_time(ENERGEST_TYPE_CPU);
  // lpm_start_duration = energest_type_time(ENERGEST_TYPE_LPM);


  while(1) {

    /* Delay 2-4 seconds */
    etimer_set(&et, CLOCK_SECOND * 4 + random_rand() % (CLOCK_SECOND * 4));

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
