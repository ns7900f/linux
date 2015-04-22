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

#define SERVERIP "127.0.0.1"
#define SERVERPORT 8080

#define BUFFSIZE 1024
#define ALIASLEN 32
#define OPTLEN 16
#define LINEBUFF 2048
#define LENGTH 512
struct PACKET {
char option[OPTLEN]; // instruction
char alias[ALIASLEN]; // client's username
char buff[BUFFSIZE]; // message size

};
struct USER {
int sockfd; // user's socket file descriptor: objets génériques avec des méthodes génériques (open(), close(), read(), write(), ...).
char alias[ALIASLEN]; // user's name
};
struct THREADINFO {
pthread_t thread_ID; // thread's pointer:récupérer l'ID d'un thread
int sockfd; // socket file descriptor
};
int isconnected, sockfd;
char option[LINEBUFF];
struct USER me;

int connect_with_server();
void setalias(struct USER *me);//set username
void logout(struct USER *me);//desconnecter de serveur
void login(struct USER *me);//login
void *receiver(void *param);//receive message
void sendtoall(struct USER *me, char *msg); //broadcast
void sendtoclient(struct USER *me, char * target, char *msg);//send to specific user
int main(int argc, char **argv) {
int sockfd, aliaslen;
memset(&me, 0, sizeof(struct USER));
while(gets(option)) {
if(!strncmp(option, "exit", 4)) {
logout(&me);
break;
}
else if(!strncmp(option, "login", 5)) {
char *ptr = strtok(option, " ");
ptr = strtok(0, " ");//set ptr as username
memset(me.alias, 0, sizeof(char) * ALIASLEN);
if(ptr != NULL) {
aliaslen = strlen(ptr);//strlen:calcule la longeur du chaine de caractere
if(aliaslen > ALIASLEN) ptr[ALIASLEN] = 0;
strcpy(me.alias, ptr);//copy ptr in me.alias
}
else {

strcpy(me.alias, "Anonymous");//copy Anonymous to me.alias



}
login(&me);
}
else if(!strncmp(option, "change", 6)) {
char *ptr = strtok(option, " ");
ptr = strtok(0, " ");
memset(me.alias, 0, sizeof(char) * ALIASLEN);
if(ptr != NULL) {
aliaslen = strlen(ptr);
if(aliaslen > ALIASLEN) ptr[ALIASLEN] = 0;
strcpy(me.alias, ptr);
setalias(&me);
}
}
else if(!strncmp(option, "specf", 5)) {
char *ptr = strtok(option, " ");
char temp[ALIASLEN];
ptr = strtok(0, " ");
memset(temp, 0, sizeof(char) * ALIASLEN);
if(ptr != NULL) {
aliaslen = strlen(ptr);
if(aliaslen > ALIASLEN) ptr[ALIASLEN] = 0;
strcpy(temp, ptr);
while(*ptr) ptr++; ptr++;
while(*ptr <= ' ') ptr++;
sendtoclient(&me, temp, ptr);
}
}

else if(!strncmp(option, "send", 4)) {
sendtoall(&me, &option[5]);
}
else if(!strncmp(option, "logout", 6)) {
logout(&me);
}
else fprintf(stderr, "Unknown option...\n");
}
return 0;
}

void login(struct USER *me) {
int recvd;
if(isconnected) {
fprintf(stderr, "You are already connected to server at %s:%d\n", SERVERIP, SERVERPORT);
return;
}
sockfd = connect_with_server();
if(sockfd >= 0) {
isconnected = 1;
me->sockfd = sockfd;
if(strcmp(me->alias, "Anonymous")) setalias(me);
printf("Logged in as %s\n", me->alias);
printf("Receiver started [%d]...\n", sockfd);
struct THREADINFO threadinfo;
pthread_create(&threadinfo.thread_ID, NULL, receiver, (void *)&threadinfo);

}
else {
fprintf(stderr, "Connection rejected...\n");
}
}

int connect_with_server() {
int newfd, err_ret;
struct sockaddr_in serv_addr;//pour communiquer via internet
struct hostent *to;//for host

/* generate address */
if((to = gethostbyname(SERVERIP))==NULL) {// gethostbyname: recieve info for host name
err_ret = errno;
fprintf(stderr, "gethostbyname() error...\n");
return err_ret;
}

/* open a socket */
if((newfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
err_ret = errno;
fprintf(stderr, "socket() error...\n");
return err_ret;
}

/* set initial values */
serv_addr.sin_family = AF_INET;
serv_addr.sin_port = htons(SERVERPORT);
serv_addr.sin_addr = *((struct in_addr *)to->h_addr);
memset(&(serv_addr.sin_zero), 0, 8);

/* try to connect with server */
if(connect(newfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1) {
err_ret = errno;
fprintf(stderr, "connect() error...\n");
return err_ret;
}
else {
printf("Connected to server at %s:%d\n", SERVERIP, SERVERPORT);
return newfd;
}
}

void logout(struct USER *me) {
int sent;
struct PACKET packet;
if(!isconnected) {
fprintf(stderr, "You are not connected...\n");
return;
}
memset(&packet, 0, sizeof(struct PACKET));
strcpy(packet.option, "exit");
strcpy(packet.alias, me->alias);
/* send request to close this connetion */
sent = send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
isconnected = 0;
}

void setalias(struct USER *me) {
int sent;
struct PACKET packet;
if(!isconnected) {
fprintf(stderr, "You are not connected...\n");
return;
}
memset(&packet, 0, sizeof(struct PACKET));
strcpy(packet.option, "change");
strcpy(packet.alias, me->alias);
/* send request to close this connetion */
sent = send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);//send packet
}

void *receiver(void *param) {
int recvd;
struct PACKET packet;
printf("Waiting here [%d]...\n", sockfd);
while(isconnected) {
recvd = recv(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
if(!recvd) {
fprintf(stderr, "Connection lost from server...\n");
isconnected = 0;
close(sockfd);
break;
}
if(recvd > 0) {
printf("[%s]: %s\n", packet.alias, packet.buff);
}
memset(&packet, 0, sizeof(struct PACKET));
}
return NULL;
}
char revbuf[LENGTH];

void sendtoall(struct USER *me, char *msg) {
int sent;
struct PACKET packet;
if(!isconnected) {
fprintf(stderr, "You are not connected...\n");
return;
}
msg[BUFFSIZE] = 0;
memset(&packet, 0, sizeof(struct PACKET));
strcpy(packet.option, "send");
strcpy(packet.alias, me->alias);
strcpy(packet.buff, msg);
/* send request to close this connetion */
sent = send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
}

void sendtoclient(struct USER *me, char *target, char *msg) {
int sent, targetlen;
struct PACKET packet;
if(target == NULL) {
return;
}
if(msg == NULL) {
return;
}
if(!isconnected) {
fprintf(stderr, "You are not connected...\n");
return;
}
msg[BUFFSIZE] = 0;
targetlen = strlen(target);
memset(&packet, 0, sizeof(struct PACKET));
strcpy(packet.option, "specf");
strcpy(packet.alias, me->alias);
strcpy(packet.buff, target);
strcpy(&packet.buff[targetlen], " ");
strcpy(&packet.buff[targetlen+1], msg);
/* send request to close this connetion */
sent = send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
}
