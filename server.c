#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>


int main(int argc, char **argv)
{
    
    struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
    char buf[128] = { 0 };
    char finished[5]={0};
    int s, client, bytes_read;
    socklen_t opt = sizeof(rem_addr);

    // allocate socket
    s = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

    // bind socket to port 1 of the first available 
    // local bluetooth adapter
    loc_addr.rc_family = AF_BLUETOOTH;
    loc_addr.rc_bdaddr = *BDADDR_ANY;
    loc_addr.rc_channel = (uint8_t) 20;
    bind(s, (struct sockaddr *)&loc_addr, sizeof(loc_addr));
    int pFile, oflags;
    int first=0;
    // Opens to device file
    pFile = open("/dev/mycar", O_RDWR);

    if (pFile < 0) {
	fprintf (stderr, "alef module isn't loaded\n");
        return 1;
    }
    while(1){
    	// put socket into listening mode
    	listen(s, 20);

    	// accept one connection
    	client = accept(s, (struct sockaddr *)&rem_addr, &opt);

    	ba2str( &rem_addr.rc_bdaddr, buf );
    	fprintf(stderr, "accepted connection from %s\n", buf);
    	memset(buf, 0, sizeof(buf));

    	// read data from the client
    	bytes_read = read(client, buf, sizeof(buf));
	//bytes_read=5;
	
	//memcpy(buf,"aleffm",sizeof("aleffm"));
    	if( bytes_read > 0 ) {
        	printf("received [%s]\n", buf);
		read(pFile, finished,sizeof(finished));
		printf("%s\n", finished);
		if(first==0){
			if(strncmp (buf, "alef",4)==0){
				char direction[3];
				memcpy(direction, &buf[4],2 );
				direction[3] = '\0';
				printf("%s\n", direction);
			 	write(pFile, direction, 128);
			        first==1;
			}
			else{
				printf("Are you talking to Alef");
			}

		}
		else if (strcmp(finished, "done")==0){
			if(strncmp (buf, "alef",4)==0){
				char direction[3];
				memcpy(direction, &buf[4],2 );
				direction[3] = '\0';
				printf("%s\n", direction);
			 	write(pFile, direction, 128);
			        first==1;
			}
			else{
				printf("Are you talking to Alef");
			}

		}
		else{
			printf("cannot process now. please repeat %s", buf);
		}
		
    		}
	}
    // close connection
    close(client);
    memset(buf,0,sizeof(buf));
    close(s);
    return 0;
}
