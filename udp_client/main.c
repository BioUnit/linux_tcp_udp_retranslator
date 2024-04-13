#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#define MESSAGE_LENGHT 16

int main(int argc, char **argv){
   int sockfd, connfd, len;
   char buffer[MESSAGE_LENGHT] = {"Start"};
   struct sockaddr_in server_addr={
      .sin_family = AF_INET,
      .sin_port = htons(5000),
      .sin_addr.s_addr = inet_addr("127.0.0.1")
   };
   memset(buffer, 0, MESSAGE_LENGHT);
   buffer[0] = 's'; buffer[1] = 't'; buffer[2] = 'a'; buffer[3] = 'r'; buffer[4] = 't';
   buffer[MESSAGE_LENGHT-4] = 's'; buffer[MESSAGE_LENGHT-3] = 't'; buffer[MESSAGE_LENGHT-2] = 'o'; buffer[MESSAGE_LENGHT-1] = 'p';
   sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   if(sockfd == -1){
      printf("Socket creation failed\r\n");
   }

   if(sendto(sockfd, buffer, MESSAGE_LENGHT, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
      printf("ERROR: cannot send message\r\n");
   }

   shutdown(sockfd, 2);
   close(sockfd);

   return 0;
}
