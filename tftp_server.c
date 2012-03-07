/* TFTP Server - Harihara Krishnan Narayanan */


/* Required header files */
#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<sys/types.h>
#include <stdlib.h>
#include<sys/select.h>
#include<time.h> 


/* #defines are mentioned here */

#define FILE_NAME_MAXIMUM 64
#define MAX_FILE_LENGTH 512
#define WINDOW 4
#define PORT_DEFAULT 8000 
#define TIMEOUT 1



/* Start of main function */
int main(int argc, char *argv[]) 
{

   struct sockaddr_in server, client; //define the client and server sockets.

   char opcode; //Opcode in the packet.
   char block_no;
   char *temp;

   static last_window,recent_window; //Window variables. Maintain the last ACKed packet and the most recent packet sent in the window. 

   // Buffer that holds the transmitted data for each packet in the window.
   char transmitted_data[ WINDOW ][512];

   //List of important character arrays used
   char client_data [516]; // Useful Data received from the client.
   char file_data[ MAX_FILE_LENGTH ]; //File contents
   char file_name[ FILE_NAME_MAXIMUM ]; //File name.
   char stream_data[516]; //data to be sent to client
   int i,j,k, num, num_of_bytes, port, client_len, sock_id;
   int data_size = 516, begin = 1, char_count;

   static block_number, last_ack_packet, completed;

   char c;

   FILE *fp;  // File Pointer

   fd_set readset; //readset buffer. 

   static current, previous;

   int select_result; //result of select. 
   
   time_t t1;


   if (argc == 2) //If user has entered the port
       port = atoi(argv[1]);
   else if (argc == 1) //If user has not entered the port
       port = PORT_DEFAULT ;
   else    //Any other ...     
       {
          fprintf(stderr, "Format expected is ./a.out <port_number>\n");
          exit (1);
       }

   bzero((char *)&server, sizeof(struct sockaddr_in));
   server.sin_family = AF_INET;  //set the family
   server.sin_port = htons(port); //set the port
   server.sin_addr.s_addr = htonl(INADDR_ANY); //set the ip address.


   if((sock_id= socket(AF_INET, SOCK_DGRAM, 0))== -1) //Socket Function & catching error.
   {
     fprintf(stderr, "Socket cannot be created \n");
     exit(1);
   }

  
   if(bind(sock_id, (struct sockaddr *)&server, sizeof(server))== -1) //bind function & catching error.....
   {
     fprintf(stderr, "cannot bind name to the socket \n");
     exit(1);
   }

   printf("Server ready to receive request from Clients \n"); // Yay!! server set up.....

   int byte_index; // duh! gives the byte index.... :)
   int error_occured; //error has occured...

   struct timeval tv;
   tv.tv_sec = 1;
   tv.tv_usec = 0; //total time of 1 sec....

   //Window packet paramters stored here.
   int packet_block_number [ WINDOW ]; // Stores the block numbers of packets in the window. 
   int packet_time_sent [ WINDOW ]; // Stores the time when these packets were sent. 

   // Main loop

   while(1)
   {
     int timedout = 0; client_len = sizeof (client);

     for (k=0; k < WINDOW; k++) //loop through the window to check for timeout. 
     {  
        (void) time (&t1);
        if ( ((int)t1 - packet_time_sent [k]) > TIMEOUT && begin ==0)   //timedout condition. Check that its not the beginning. 
        { 
            timedout = 1; printf("TIMEOUT\n");
            stream_data[0] = 0; stream_data[1] = 3; 
            stream_data[2] = 0; stream_data[3] = packet_block_number [k];
            packet_time_sent[k] = (int) t1;
            if(sendto(sock_id, stream_data, data_size, 0, (struct sockaddr *)&client, client_len) == -1) //send to client.
            {
                fprintf(stderr, "Sending error...\n");
                exit(1);
            }
            printf(" DATA Packet Block_no = %d is sent\n", stream_data[3]);
           
            break;  
                      

        }    
     }

     if (timedout == 1) //if timedout, go back to beginning of loop
        continue;

     FD_ZERO (&readset);
     FD_SET (sock_id, &readset);

     select_result = select (sock_id + 1, &readset, 0, 0, &tv);

     if (select_result <= 0) //The select function call has timedout....go back to loop beginning. 
     {
        continue;
     }
     
     
 
     if( (num = recvfrom(sock_id, client_data, 516,0, (struct sockaddr *)&client, (socklen_t*)&client_len))< 0)
     {
        fprintf(stderr, "Cannot receive connection \n");
        exit(1);
     }
       
     
     //fprintf(stderr,"hello");

     if (completed == 1 && *(client_data+1) == 4) //Already all the required packets have been sent....only ACKs remain....unless of course timeouts happen :) :)
        {   
            block_no = *(client_data +3);
            printf(" ACK received for block # %d\n", block_no);
            continue; //move on....
        }
    
     temp = client_data; temp++; //getting opcode from the packet.
     opcode = *temp; temp++;
 
     switch(opcode)
     {
     case 1:
            strcpy(file_name, &client_data[2]);
            printf(" Received a read request for file %s \n", file_name);
    
            /* Reading the content from the file requested and sending it to the client*/
            fp=fopen(file_name, "r");
            if(fp == NULL) //File not found.
            { 
              error_occured = 1; 
              stream_data[0] = 0; stream_data[1] = 5; //setting up the error packet. Its opcode is 5. 
              stream_data[2] = 0; stream_data[3] = 1;
              printf("Oops!! The file is not found in this directory!!!! Tough luck!\n");
              strcpy(&stream_data[4],"File not found");
              data_size = sizeof(stream_data);
              if(sendto(sock_id, stream_data, data_size, 0, (struct sockaddr *)&client, client_len) == -1)
              {
                 fprintf(stderr, "Send error....\n");
                 exit(1);
              }
              
              break;
            }

           else           
           { 
              completed = 0; error_occured = 0; begin = 0;
              block_number = 1; //block numbering starting from 1.
              printf("File opened successfully \n");

                for(i =0; i<4; i++)
                {
                  
                  num_of_bytes = fread(file_data, 1, 512, fp);       
                  //printf("\n*****\n%s\n*****\n", file_data);
                  if (num_of_bytes < 512)
                  {    completed = 1; begin = 1; //Set to completed and begin again. 
                  }
                   
                 stream_data[0]= 0; stream_data[1]= 3;
                 stream_data[2]= 0; stream_data[3]= block_number;
                  
        
                 strcpy(&stream_data[4], file_data);
                 strcpy(transmitted_data[i], &stream_data[4]);
                 packet_block_number [i] = block_number; //save the block number. 
                 (void) time (&t1); //get time. 
                 packet_time_sent [i] = (int) t1; //save the time.                 

                 current = 0; previous = 3; //save the current and previous windows.
                 printf("DATA packet Block_num = %d is sent\n",stream_data[3]); //print the packet to be sent.
       
                 data_size = num_of_bytes + 4; //file size + four additional bytes. 
                 if(sendto(sock_id, stream_data, data_size, 0, (struct sockaddr *)&client, client_len) == -1) //send to client.
                 {
                      fprintf(stderr, "Sending error...\n");
                      exit(1);
                 }
                 if (completed == 1)
                 {
                      recent_window = block_no;break; 
                 }
                 block_number ++; //increment the block number.  
               }
               recent_window = 4; 
            }
            break; //break from the switch statement.

   case 4:  if (completed == 1)
            {           
               begin = 1; //Set to begin again.                
               break; //Done....All packets already sent.
            }
            temp++;//increment the pointer to point to the block_number.
            block_no = *temp; //get the block number.
            last_window = block_no; //block number received is set to last.
            if(last_ack_packet != block_no) //If ACK received is not duplicate. 
            {
                if(last_window ==1)
                   recent_window = last_window + 4;
                else
                   recent_window = recent_window + 1;
            

                printf(" ACK received for block # %d\n", block_no);
                if (error_occured == 1)
                {
                   error_occured = 0;
                   break;
                }
                num_of_bytes = fread(file_data,1,512,fp);

                if (num_of_bytes < 512)
                {    completed = 1; begin = 1; //Completed and begin again. 
                }
             
                stream_data[0] = 0; stream_data[1] = 3;
                stream_data[2] = 0; stream_data[3] = recent_window;

                strcpy(&stream_data[4], file_data);
                if(current != 3)
                {
                      previous = current;
                      current++;
                }
                else
                {
                     previous = current;
                     current = 0;
                }
                strcpy(transmitted_data[previous], &stream_data[4]);
                packet_block_number [previous] = block_number; //save the block number. 
                (void) time (&t1);
                packet_time_sent [previous] = (int) t1; //save the time. 
               
                printf("DATA packet Block_num = %d packet is sent\n",stream_data[3]);

                data_size = num_of_bytes + 4;
                if(sendto(sock_id, stream_data, data_size, 0, (struct sockaddr *)&client, client_len) == -1)
                {
                    fprintf(stderr, "Cannot send datagram 3\n");
                     exit(1);
                }
                   
                last_ack_packet = block_no;
           }
           else
           {
              printf("Duplicate ACK # %d\n", block_no); //Duplicate ACK received. 
              break;                  
           }
          last_ack_packet = 5;
       
           break;
  
   default:
             printf("NOT A VALID OPERATION\n");
             break;
  }
}

fclose(fp);
close(sock_id);

return 0;        

}
