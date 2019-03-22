/*
** server.c -- a stream socket server demo
*/

#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define PORT "24444"  // the port users will be connecting to
#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 100 // max number of bytes we can get at once

int msg_buf;
int data[5] = { 0, 0, 0, 0, 0 };
char buf[MAXDATASIZE];
char msg[MAXDATASIZE];
char* message;
char data_msg[MAXDATASIZE];
char tijd[20];
char datum[20];
struct tm *sTm;
char c;

FILE * fp;
FILE * fptemp;

#define MAX_TIMINGS	85
#define DHT_PIN		7	/* GPIO-4 */

// Read Temperature data
void read_dht_data();

// SIGCHLD handler
void sigchld_handler(int s);

// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);

// Log to XML File
void doLogging(char hostname[INET6_ADDRSTRLEN], char message[MAXDATASIZE]);

int main(void){
	int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
	int yes=1;
	int rv;
	char s[INET6_ADDRSTRLEN];
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	struct sigaction sa;
	socklen_t sin_size;

	memset(&hints, 0, sizeof hints);
	
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// Loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next){
		if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("server: socket");
			continue;
		}

		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
			perror("setsockopt");
			exit(1);
		}

		if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}
	
	// All done with this structure
	freeaddrinfo(servinfo); 

	if(p == NULL){
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	if(listen(sockfd, BACKLOG) == -1){
		perror("listen");
		exit(1);
	}

	// Reap all dead processes
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	
	if(sigaction(SIGCHLD, &sa, NULL) == -1){
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	// Main accept() loop
	while(1){
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		
		if(new_fd == -1){
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
		printf("server: got connection from %s\n", s);

		while(1){
			// This is the child process
			if (!fork()){
				// Child doesn't need the listener
				close(sockfd);

				// Get msg
				if((msg_buf = recv(new_fd, buf, MAXDATASIZE-1, 0)) == -1){
					perror("recv");
					exit(1);
				}
				
				buf[msg_buf] = '\0';

				if(strcmp("close", buf) == 0){
					close(new_fd);
					exit(0);
				}
				
				if(strcmp("data", buf) == 0){
					if(wiringPiSetup() == -1)
						exit( 1 );
					else{
						read_dht_data();
					}
					strcpy(msg, data_msg);
					printf("%s\n", msg);
				}
				else{
					strcpy(msg, buf);
				}
				printf("de msg is : %s\n", msg);
				doLogging( s, buf );
				if(send(new_fd, msg, strlen(msg), 0) == -1){
					perror("send");
				}
			}
		}
		close(new_fd);  // parent doesn't need this
	}

	return 0;
}

// Read Temperature data
void read_dht_data(){
	uint8_t laststate	= HIGH;
	uint8_t counter		= 0;
	uint8_t j			= 0, i;

	data[0] = data[1] = data[2] = data[3] = data[4] = 0;

	// Pull pin down for 18 milliseconds
	pinMode( DHT_PIN, OUTPUT );
	digitalWrite( DHT_PIN, LOW );
	delay( 18 );

	// Prepare to read the pin
	pinMode( DHT_PIN, INPUT );

	// Detect change and read data
	for(i=0; i<MAX_TIMINGS; i++){
		counter = 0;
		
		while(digitalRead( DHT_PIN ) == laststate){
			counter++;
			delayMicroseconds( 1 );
			if(counter == 255){
				break;
			}
		}
		
		laststate = digitalRead( DHT_PIN );

		if(counter == 255){
			break;
		}

		// Ignore first 3 transitions
		if((i >= 4) && (i % 2 == 0)){
			// Shove each bit into the storage bytes
			data[j / 8] <<= 1;
			if ( counter > 50 ){
				data[j / 8] |= 1;
			}
			j++;
		}
	}

	//check we read 40 bits (8bit x 5 ) + verify checksum in the last byte print it out if data is good
	if((j >= 40) && (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF))){
		float h = (float)((data[0] << 8) + data[1]) / 10;
		if(h > 100){
			h = data[0];	// for DHT11
		}
		float c = (float)(((data[2] & 0x7F) << 8) + data[3]) / 10;
		if(c > 125){
			c = data[2];	// for DHT11
		}
		if(data[2] & 0x80 ){
			c = -c;
		}
		float f = c * 1.8f + 32;
		
		printf("Temperature = %.1f *C\tHumidity = %.1f %%\n", c, h);
		
		sprintf(data_msg, "Temperature = %.1f *C\tHumidity = %.1f %%", c, h);
	}
	else{
		printf("Data not good, skip\n");
	}
}

// SIGCHLD handler
void sigchld_handler(int s){
	(void)s; // quiet unused variable warning

	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

// Get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
	if(sa->sa_family == AF_INET){
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Log to XML File
void doLogging(char hostname[INET6_ADDRSTRLEN], char message[MAXDATASIZE]){
	FILE *fp;
	time_t current_time;
	char *c_time_string;

	// Open file
	fp = fopen("/var/www/html/mails.xml", "rw+");
	
	// Get Time
	current_time = time(NULL);
	c_time_string = ctime(&current_time);

	if(current_time == ((time_t) - 1)){
		fprintf(stderr, "Failure to obtain the current time.\n");
	}
	else if(c_time_string == NULL){
		fprintf(stderr, "Failure to convert the current time.\n");
	}
	
	// Delete newline
	c_time_string[strcspn(c_time_string, "\n")] = 0;
	
	// Move cursor
	fseek(fp, -8, SEEK_END);
	
	// Write XML data
	fprintf(fp, "\t<mail>\n");
	fprintf(fp, "\t\t<from>%s</from>\n", hostname);
	fprintf(fp, "\t\t<time>%s</time>\n", c_time_string);
	fprintf(fp, "\t\t<message>%s</message>\n", message);
	fprintf(fp, "\t</mail>\n");
	fprintf(fp, "</inbox>");

	printf("XML data logged !\n");
	
	fclose(fp);

	return;
}
