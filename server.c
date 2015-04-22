#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#define IP "127.0.0.1"
#define PORT 8080
#define BACKLOG 10
#define CLIENTS 10
#define BUFFSIZE 1024
#define ALIASLEN 32
#define OPTLEN 16
#define LENGTH 512
struct PACKET {
char option[OPTLEN];//instruction 
char alias[ALIASLEN];// alias du client 
char buff[BUFFSIZE]; //taille du buffer
};
struct THREADINFO {
pthread_t thread_ID;//thread ointer 
int sockfd;//soket file descriptor 
char alias[ALIASLEN]; 
};
struct LLNODE {
struct THREADINFO threadinfo;
struct LLNODE *next;// next le suivant
};
struct LLIST {
struct LLNODE *head, *tail;//head le premier element avec sa taille tail
int size;
};
int compare(struct THREADINFO *a, struct THREADINFO *b) {
return a->sockfd - b->sockfd;
}//comparaison si valeure de la difference null donc identique
void list_init(struct LLIST *ll) {
ll->head = ll->tail = NULL;
ll->size = 0;
}//initialisation de la list a des valeure null
int list_insert(struct LLIST *ll, struct THREADINFO *thr_info) {
if(ll->size == CLIENTS) return -1;
if(ll->head == NULL) {
ll->head = (struct LLNODE *)malloc(sizeof(struct LLNODE));
ll->head->threadinfo = *thr_info;
ll->head->next = NULL;
ll->tail = ll->head;
}
else {
ll->tail->next = (struct LLNODE *)malloc(sizeof(struct LLNODE));
ll->tail->next->threadinfo = *thr_info;
ll->tail->next->next = NULL;
ll->tail = ll->tail->next;
}//pour ne depasse pas le nbr de client disponible qui peut se connecte
//mais si la taille du list est null le premier element sera ce client qui va se connecter 
//et le suivant sera null donc avec malloc il ya reservation du case 
//et si la taille pas null lelement sajout au precedents donc la taille augment en plus 1 (size ++)
ll->size++;
return 0;
}//fin list_insert
int list_delete(struct LLIST *ll, struct THREADINFO *thr_info){ 
struct LLNODE *curr, *temp;
if(ll->head == NULL) return -1;//si la list vide pas de suppression
if(compare(thr_info, &ll->head->threadinfo) == 0) {
temp = ll->head;
ll->head = ll->head->next;
if(ll->head == NULL) ll->tail = ll->head;
free(temp);
ll->size--;
return 0;
}//compare ce queje veut le supprimer avec le premier element du list je le supprime eh le head sera le next
for(curr = ll->head; curr->next != NULL; curr = curr->next) {
if(compare(thr_info, &curr->next->threadinfo) == 0) {
temp = curr->next;
if(temp == ll->tail) ll->tail = curr;
curr->next = curr->next->next;
free(temp);
ll->size--;
return 0;
}//comparaison avec chaque element pour chercher je que je veut le supprimer,aund je le trouve je le supprime
//en mettant le derniere valeur du list au lieu ou je veux supprime ,eh la taille du list sera au moins 1
}
return -1;
}//fin du list_delet
  
void list_dump(struct LLIST *ll) {//affiche le list des clients connectees avec son nombre
struct LLNODE *curr;
struct THREADINFO *thr_info;
printf("Connection count: %d\n", ll->size);
for(curr = ll->head; curr != NULL; curr = curr->next) {
thr_info = &curr->threadinfo;
printf("[%d] %s\n", thr_info->sockfd, thr_info->alias);
}
}
int sockfd, newfd;
struct THREADINFO thread_info[CLIENTS];
struct LLIST client_list;
//initialize linked list

pthread_mutex_init(&clientlist_mutex, NULL);//initiate mutex

if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {//avoir le socket
err_ret = errno;
fprintf(stderr, "socket() failed...\n");
return err_ret;
}

serv_addr.sin_family = AF_INET;
serv_addr.sin_port = htons(PORT);
serv_addr.sin_addr.s_addr = inet_addr(IP);
memset(&(serv_addr.sin_zero), 0, 8);

if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1) {
err_ret = errno;
fprintf(stderr, "bind() failed...\n");
return err_ret;
}

if(listen(sockfd, BACKLOG) == -1) {
err_ret = errno;
fprintf(stderr, "listen() failed...\n");
return err_ret;
}

printf("Starting server...\n");
if(pthread_create(&interrupt, NULL, io_handler, NULL) != 0) {
err_ret = errno;
fprintf(stderr, "pthread_create() failed...\n");
return err_ret;
}

printf("accepting connections...\n");
while(1) {
sin_size = sizeof(struct sockaddr_in);
if((newfd = accept(sockfd, (struct sockaddr *)&client_addr, (socklen_t*)&sin_size)) == -1) {
err_ret = errno;
fprintf(stderr, "accept() failed...\n");
return err_ret;
}
else {
if(client_list.size == CLIENTS) {
fprintf(stderr, "Connection full, request rejected...\n");
continue;
}
printf("Connection requested received...\n");
struct THREADINFO threadinfo;
threadinfo.sockfd = newfd;
strcpy(threadinfo.alias, "Anonymous");
pthread_mutex_lock(&clientlist_mutex);
list_insert(&client_list, &threadinfo);
pthread_mutex_unlock(&clientlist_mutex);
pthread_create(&threadinfo.thread_ID, NULL, client_handler, (void *)&threadinfo);
}
}
return 0;
}
void *io_handler(void *param) {
char option[OPTLEN];
while(scanf("%s", option)==1) {
if(!strcmp(option, "exit")) {

printf("Terminating server...\n");
pthread_mutex_destroy(&clientlist_mutex);
close(sockfd);
exit(0);
}
else if(!strcmp(option, "list")) {
pthread_mutex_lock(&clientlist_mutex);
list_dump(&client_list);
pthread_mutex_unlock(&clientlist_mutex);
}
else {
fprintf(stderr, "Unknown command: %s...\n", option);
}
}
return NULL;
}
void *client_handler(void *fd) {
struct THREADINFO threadinfo = *(struct THREADINFO *)fd;
struct PACKET packet;
struct LLNODE *curr;
int bytes, sent;
while(1) {
bytes = recv(threadinfo.sockfd, (void *)&packet, sizeof(struct PACKET), 0);
if(!bytes) {
fprintf(stderr, "Connection lost from [%d] %s...\n", threadinfo.sockfd, threadinfo.alias);
pthread_mutex_lock(&clientlist_mutex);
list_delete(&client_list, &threadinfo);
pthread_mutex_unlock(&clientlist_mutex);
break;
}
printf("[%d] %s %s %s\n", threadinfo.sockfd, packet.option, packet.alias, packet.buff);
if(!strcmp(packet.option, "change")) {
printf("Set alias to %s\n", packet.alias);
pthread_mutex_lock(&clientlist_mutex);
for(curr = client_list.head; curr != NULL; curr = curr->next) {
if(compare(&curr->threadinfo, &threadinfo) == 0) {
strcpy(curr->threadinfo.alias, packet.alias);
strcpy(threadinfo.alias, packet.alias);
break;
}
}
pthread_mutex_unlock(&clientlist_mutex);
}
else if(!strcmp(packet.option, "specf")) {
int i;
char target[ALIASLEN];
for(i = 0; packet.buff[i] != ' '; i++); packet.buff[i++] = 0;
strcpy(target, packet.buff);
pthread_mutex_lock(&clientlist_mutex);
for(curr = client_list.head; curr != NULL; curr = curr->next) {
if(strcmp(target, curr->threadinfo.alias) == 0) {
struct PACKET spacket;
memset(&spacket, 0, sizeof(struct PACKET));
if(!compare(&curr->threadinfo, &threadinfo)) continue;
strcpy(spacket.option, "msg");
strcpy(spacket.alias, packet.alias);
strcpy(spacket.buff, &packet.buff[i]);
sent = send(curr->threadinfo.sockfd, (void *)&spacket, sizeof(struct PACKET), 0);
}
}
pthread_mutex_unlock(&clientlist_mutex);
}

else if(!strcmp(packet.option, "send")) {
pthread_mutex_lock(&clientlist_mutex);
for(curr = client_list.head; curr != NULL; curr = curr->next) {
struct PACKET spacket;
memset(&spacket, 0, sizeof(struct PACKET));
if(!compare(&curr->threadinfo, &threadinfo)) continue;
strcpy(spacket.option, "msg");
strcpy(spacket.alias, packet.alias);
strcpy(spacket.buff, packet.buff);
sent = send(curr->threadinfo.sockfd, (void *)&spacket, sizeof(struct PACKET), 0);
}
pthread_mutex_unlock(&clientlist_mutex);
}
else if(!strcmp(packet.option, "exit")) {
printf("[%d] %s has disconnected...\n", threadinfo.sockfd, threadinfo.alias);
pthread_mutex_lock(&clientlist_mutex);
list_delete(&client_list, &threadinfo);
pthread_mutex_unlock(&clientlist_mutex);
break;
}
else {
fprintf(stderr, "wrong data send from [%d] %s...\n", threadinfo.sockfd, threadinfo.alias);
}
}

close(threadinfo.sockfd);
return NULL;
}


