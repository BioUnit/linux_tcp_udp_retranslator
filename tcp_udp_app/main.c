#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <semaphore.h>


#define MAX_PACKET_LENGHT 128
#define MIN_PACKET_LENGHT 16
#define MESSAGE_QUEUE_SIZE 10


bool address_parse(uint8_t *out_address, uint16_t *out_port, char *string_addr);
void* udp_receive_thread(void*);
void* tcp_transmit_thread(void*);


/* UDP and TCP socket file-descriptors */
int udp_sockfd, tcp_sockfd, tcp_connfd;
struct sockaddr_in tcp_sock_addr, udp_sock_addr;
uint8_t queue_counter = 0;
uint8_t queue_current_write = 0;
uint8_t queue_current_read = 0;
char symbols[4];
bool tcp_conn_flag = 0;

typedef struct message_data{
   void *pointer;
   uint8_t len;
} message_data;

message_data message_queue[MESSAGE_QUEUE_SIZE];

FILE *log_file;
char *log_file_path;

pthread_mutex_t q_counter_lock;
sem_t message_counter_sem;

int main(int argc, char **argv){
   /* main() parameters parse handling group */
   uint8_t udp_address[4] = {0};
   uint16_t *p_udp_port, *p_tcp_port;
   uint16_t udp_port, tcp_port;
   uint8_t tcp_address[4] = {0};
   char address_str[20];
   p_udp_port = &udp_port;
   p_tcp_port = &tcp_port;
   char input_str[20];

   /* Thread handling group */
   pthread_t udp_thread_id, tcp_thread_id;

   /* Check parameters quantity */
   if(argc < 5){
      printf("ERROR: Not all parameters were specified.\r\n");
      printf("Please specify UDP and TCP addresses.\r\n");
      return -1;
   } else if (argc > 5) {
      printf("ERROR: Too many parameters.\r\n");
      printf("Please use only UDP and TCP addresses.\r\n");
      return -1;
   }

   /* Check UDP address correctness */
   if(address_parse(&udp_address, p_udp_port, (char*)argv[1]) != 1){
      printf("ERROR: Incorrect UDP address or port\r\n");
   } else {
      printf("UDP accepted: %d.%d.%d.%d:%d\r\n", udp_address[0], udp_address[1], udp_address[2], udp_address[3], udp_port);
   }

   /* Check TCP address correctness */
   if(address_parse(&tcp_address, p_tcp_port, (char*)argv[2]) != 1){
      printf("ERROR: Incorrect TCP address or port\r\n");
   } else {
      printf("TCP accepted: %d.%d.%d.%d:%d\r\n", tcp_address[0], tcp_address[1], tcp_address[2], tcp_address[3], tcp_port);
   }

   /* Check log file path correctness */
   log_file = fopen(argv[3], "w+");
   if(log_file == NULL){
      printf("ERROR: Log file could not be created\r\n");
      printf("Please choose one of two methods as follows:\r\n");
      printf("    Specify full path: /your/full/path/to/file.txt\r\n");
      printf("    Specify from current directory: ./from/current/dir/file.txt\r\n");
   } else {
      printf("Log path accepted: %s\r\n", argv[3]);
      fprintf(log_file, "Starting log at...\r\n");
      fclose(log_file);
      log_file_path = malloc(strlen(argv[3]));
      memcpy(log_file_path, argv[3], strlen(argv[3])+1);  //+1 for '\0'
      printf("%s\r\n", log_file_path);
   }

   /* Check special symbols quantity */
   if( strlen(argv[4]) != 4 ){
      printf("ERROR: wrong special symbols quantity\r\n");
      printf("Please input only 4 symbols\r\n");
   } else {
      memcpy(symbols, argv[4], 4);
      printf("Sybmols accepted: %s\r\n", symbols);
   }

   /* UDP socket */
   udp_sock_addr.sin_family = AF_INET;
   udp_sock_addr.sin_port = htons(udp_port);
   udp_sock_addr.sin_addr.s_addr = (((uint32_t)udp_address[3]<<24)+((uint32_t)udp_address[2]<<16)+((uint32_t)udp_address[1]<<8)+((uint32_t)udp_address[0]));

   udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
   if(udp_sockfd == -1){
      printf("ERROR: cannot create UDP socket\r\n");
      return -1;
   }
   if((bind(udp_sockfd, (struct sockaddr*)&udp_sock_addr, sizeof(udp_sock_addr))) != 0 ){
      printf("ERROR: UDP binding failed\r\n");
      return -1;
   }
   if( pthread_create(&udp_thread_id, NULL, udp_receive_thread, 0) != 0 ){
      printf("ERROR: Cannot create thread\r\n");
      return -1;
   } else {
      printf("Starting to listen UDP\r\n");
   }

   /* TCP socket */
   tcp_sock_addr.sin_family = AF_INET;
   tcp_sock_addr.sin_port = htons(tcp_port);
   tcp_sock_addr.sin_addr.s_addr = (((uint32_t)tcp_address[3]<<24)+((uint32_t)tcp_address[2]<<16)+((uint32_t)tcp_address[1]<<8)+((uint32_t)tcp_address[0]));

   tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if(tcp_sockfd == -1){
      printf("ERROR: cannot create TCP socket\r\n");
      return -1;
   }

   if(connect(tcp_sockfd, (struct sockaddr*)&tcp_sock_addr, sizeof(tcp_sock_addr))){
      printf("ERROR: Cannot create TCP connection\r\n");
      printf("Is server active?\r\n");
   } else {
      tcp_conn_flag = 1;
      if( pthread_mutex_init(&q_counter_lock, NULL) != 0 ) {
         printf("ERROR: Mutex init failed\r\n");
         return -1;
      }
      if( sem_init(&message_counter_sem, 0, 0) != 0 ){
         printf("ERROR: Cannot create semaphore\r\n");
         return -1;
      }
      if( pthread_create(&tcp_thread_id, NULL, tcp_transmit_thread, 0) != 0 ){
         printf("ERROR: Cannot create thread\r\n");
         return -1;
      }
   }

   while(1){
      printf("Type command: ");
      fgets(input_str, 10, stdin);
      printf("%s\r\n", input_str);
   }

   shutdown(udp_sockfd, 2);
   close(udp_sockfd);
   shutdown(tcp_sockfd, 2);
   close(tcp_sockfd);
   fclose(log_file);
   pthread_mutex_destroy(&q_counter_lock);
   return 0;
}


void* tcp_transmit_thread(void*){
   uint8_t len;
   char buffer[MAX_PACKET_LENGHT + 4];
   // Event invocation ?
   while(1){
      sem_wait(&message_counter_sem);
      if(queue_counter > 0){
         if(message_queue[queue_current_read].pointer == NULL){
            printf("ERROR: Cannnot send message with NULL pointer\r\n");
            pthread_mutex_lock(&q_counter_lock);
            log_file = fopen(log_file_path, "a");
            fprintf(log_file, "ERROR: Null pointer in the queue\r\n");
            fclose(log_file);
            pthread_mutex_unlock(&q_counter_lock);
         } else {
            if( send(tcp_sockfd, message_queue[queue_current_read].pointer, message_queue[queue_current_read].len, MSG_NOSIGNAL) == -1 ){
               printf("ERROR: Cannot send message\r\n");
               perror("Error message");
               /* Log only once per seccesfull connection */
               if(tcp_conn_flag == 1){
                   pthread_mutex_lock(&q_counter_lock);
                   log_file = fopen(log_file_path, "a");
                   fprintf(log_file, "Connection is broken\r\n");
                   fclose(log_file);
                   pthread_mutex_unlock(&q_counter_lock);
               }
               tcp_conn_flag = 0;
               printf("Reconnecting...\r\n");
               close(tcp_sockfd);
               tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
               if(tcp_sockfd == -1){
                  printf("ERROR: cannot create TCP socket\r\n");
               } else {
                  if(connect(tcp_sockfd, (struct sockaddr*)&tcp_sock_addr, sizeof(tcp_sock_addr))){
                     printf("ERROR: Cannot create TCP connection\r\n");
                     printf("Is server active?\r\n");
                     sleep(3);
                     sem_post(&message_counter_sem);  // posting sem to be able to return back here
                  } else {
                     printf("Connection reestablished\r\n");
                     pthread_mutex_lock(&q_counter_lock);
                     log_file = fopen(log_file_path, "a");
                     fprintf(log_file, "Connection reestablished\r\n");
                     fclose(log_file);
                     pthread_mutex_unlock(&q_counter_lock);
                     tcp_conn_flag = 1;
                  }
               }
            } else {
               free(message_queue[queue_current_read].pointer);
               message_queue[queue_current_read].len = 0;
               queue_current_read++;
               if(queue_current_read == MESSAGE_QUEUE_SIZE){
                  queue_current_read = 0;
               }
               pthread_mutex_lock(&q_counter_lock);
               queue_counter--;
               pthread_mutex_unlock(&q_counter_lock);
            }
         }
      }
   }
}


void* udp_receive_thread(void*){
   uint8_t len;
   char buffer[MAX_PACKET_LENGHT];
   // event invocation ?
   while(1){
      /* receive UDP packets from anyone */
      len = recvfrom(udp_sockfd, buffer, MAX_PACKET_LENGHT, 0, 0, 0);
      //printf("len = %d\r\n", len);  //debug message
      if(len < MIN_PACKET_LENGHT){
         printf("WARNING: Received packet lenght is less than 16\r\n");
         printf("Message was revocked\r\n");
         pthread_mutex_lock(&q_counter_lock);
         log_file = fopen(log_file_path, "a");
         fprintf(log_file, "WARNING: Not supported inbound message\r\n");
         fclose(log_file);
         pthread_mutex_unlock(&q_counter_lock);
      } else if(tcp_conn_flag == 0){
         printf("WARNING: TCP connection is not active\r\n");
         printf("Message was revocked\r\n");
      } else {
         /* Adding message to the buffer */
         if(queue_counter < MESSAGE_QUEUE_SIZE){
            message_queue[queue_current_write].pointer = malloc(len + 4);  //we need 4 bytes to add
            if(message_queue[queue_current_write].pointer == NULL){
               printf("ERROR: Cannot allocate memory for queue\r\n");
               pthread_mutex_lock(&q_counter_lock);
               log_file = fopen(log_file_path, "a");
               fprintf(log_file, "ERROR: No free space for message queue\r\n");
               fclose(log_file);
               pthread_mutex_unlock(&q_counter_lock);
            } else {
                message_queue[queue_current_write].len = len + 4;
                memcpy(message_queue[queue_current_write].pointer + 4, buffer, len);
                memcpy(message_queue[queue_current_write].pointer, symbols, 4);
                //printf("\r\nThe message will be: %.20s\r\n", (char*)message_queue[queue_current_write]);  //for debug
                /*
                printf("\r\nThe message will be: ");
                for(int i = 0; i<len+4; i++){
                   printf( "%c", *((char*)message_queue[queue_current_write].pointer + i) );  //casting array to char pointer, adding address shift and dereferencing the pointer
                }
                printf("\r\n");
                */
                queue_current_write++;
                if(queue_current_write == MESSAGE_QUEUE_SIZE){
                   queue_current_write = 0;
                }
                pthread_mutex_lock(&q_counter_lock);
                queue_counter++;
                pthread_mutex_unlock(&q_counter_lock);
                sem_post(&message_counter_sem);
            }
         } else {
            printf("WARNING: Queue buffer is full\r\n");
            printf("Message was revoked\n\r");
            pthread_mutex_lock(&q_counter_lock);
            log_file = fopen(log_file_path, "a");
            fprintf(log_file, "WARNING: Message queue is full\r\n");
            fclose(log_file);
            pthread_mutex_unlock(&q_counter_lock);
         }
      }
   }
}


bool address_parse(uint8_t *out_address, uint16_t *out_port, char *string_addr){
   char addr_number[3]={0};
   char port_number[5]={0};
   bool port = 0;

   for(uint8_t symb=0, adr_num=0, port_num=0, word=0; ((symb<20)||(adr_num<3)||(port_num<5)||(word<5)); symb++){
      if(isdigit(string_addr[symb]) != 0){
          if( (adr_num>2)||(port_num>4) ){
             return 0;
          }
          if(port == 0){
             addr_number[adr_num] = string_addr[symb];
             adr_num++;
          } else {
             port_number[port_num] = string_addr[symb];
             port_num++;
          }
      } else if(string_addr[symb]=='.'){
         /* if dot is placed at the beggining or one after another */
         if( (symb==0) || (isdigit(string_addr[symb+1])==0) ){
            return 0;
         }
         if( atoi(addr_number) > 255 ){
            return 0;
         }
         out_address[word] = atoi(addr_number);
         word++;
         adr_num = 0;
         memset(addr_number, 0, 3);
      } else if(string_addr[symb]==':'){
         if(port == 0){
            out_address[word] = atoi(addr_number);
            word++;
            adr_num = 0;
            port = 1;
         } else {
            return 0;
         }
      } else if( (string_addr[symb]==10) || (string_addr[symb]==13) || (string_addr[symb]=='\0') ){
         /* if it's the end of the string */
         if(word==4){
            if( atoi(port_number) > 65535 ){
               return 0;
            }
            *out_port = atoi(port_number);
            word++;
            return 1;
         }
      }  else {
          /* if it's not a digit, '.' or ':' symbol */
          return 0;
      }
   }
   return 1;
}
