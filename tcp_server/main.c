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
#include <pthread.h>

#define MAX_PACKET_LENGHT 132

int main(int argc, char **argv){
   int tcp_sockfd, tcp_connfd, client_size, message_len;
   struct sockaddr_in tcp_serv_addr, tcp_client_addr;
   char buffer[MAX_PACKET_LENGHT];

   /* TCP socket */
   tcp_serv_addr.sin_family = AF_INET;
   tcp_serv_addr.sin_port = htons(6000);
   tcp_serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

   tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if(tcp_sockfd == -1){
      printf("ERROR: cannot create TCP socket\r\n");
      return -1;
   }
   if((bind(tcp_sockfd, (struct sockaddr*)&tcp_serv_addr, sizeof(tcp_serv_addr))) != 0 ){
      printf("ERROR: TCP binding failed\r\n");
      return -1;
   }
   if(listen(tcp_sockfd, 1) != 0){
      printf("ERROR: Cannot start listenning\r\n");
      return -1;
   }
   tcp_connfd = accept(tcp_sockfd, (struct sockaddr*)&tcp_client_addr, &client_size);
   if(tcp_connfd < 0) {
      printf("ERROR: Cannot accept connection from client\r\n");
      return -1;
   } else {
      printf("Connection established\r\n");
   }
   while(1){
      message_len = recvfrom(tcp_connfd, buffer, MAX_PACKET_LENGHT, 0, (struct sockaddr*)&tcp_client_addr, &client_size);
      if(message_len > 0){
         for(uint8_t i=0; i<message_len; i++){
            if(buffer[i] != 0){
               printf("%c", buffer[i]);
            } else {
               printf("<0>");
            }
         }
         printf("\r\n");
      } else if(message_len == 0){
         printf("WARNING: connection has been dropped\r\n");

         tcp_connfd = accept(tcp_sockfd, (struct sockaddr*)&tcp_client_addr, &client_size);
         if(tcp_connfd < 0) {
            printf("ERROR: Cannot accept connection from client\r\n");
             return -1;
         } else {
            printf("Connection established\r\n");
         }
      }
   }

   return 0;
}
