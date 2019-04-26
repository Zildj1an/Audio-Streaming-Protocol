#include "asp.h"
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SAMPLE_RATE 44100
#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100
/* 1 Frame = Stereo 16 bit = 32 bit = 4kbit */
#define FRAME_SIZE 4

void play_music(struct package *the_package, int payload_size){

	 uint8_t *playbuffer;
	 int err, ret, i;

	 /* Open audio device */
	 snd_pcm_t *snd_handle;

	 if ((err = snd_pcm_open(&snd_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0)
	     errorMsg("Opening audio device: %s\n");

	 /* Configure parameters of PCM output */
	 err = snd_pcm_set_params(snd_handle, SND_PCM_FORMAT_S16_LE,SND_PCM_ACCESS_RW_INTERLEAVED,
	           	   	   	   	    NUM_CHANNELS,
	                            SAMPLE_RATE,
	                            0,          /* Allow software resampling */
	                            5);    /* nanoseconds latency */
	 if (err < 0)
	     errorMsg("configuring audio device");

	 /* Set up buffer */
	 playbuffer = malloc(payload_size);

	 for(i = 0; i < payload_size ; ++i)
			   playbuffer[i] = the_package->data[i];

	 /* Play */
	 printf(">> playing...\n");

	 /* write frames to ALSA */
	 snd_pcm_sframes_t frames = snd_pcm_writei(snd_handle, playbuffer, payload_size / FRAME_SIZE);

	 /* Check for errors */
	 ret = 0;
	 if (frames < 0)
	      ret = snd_pcm_recover(snd_handle, frames, 0);
	 if (ret < 0)
	       errorMsg("writing audio");

	 if (frames > 0 && frames < payload_size / FRAME_SIZE)
	       printf("Short write (expected %d, wrote %li)\n",
	    		   payload_size / FRAME_SIZE, frames);

	 /* clean up */
	 free(playbuffer);
	 snd_pcm_drain(snd_handle);
	 snd_pcm_hw_free(snd_handle);
	 snd_pcm_close(snd_handle);
}

int main(int argc, char **argv) {

		register int TCPclient = 0;
		int ret __attribute__((unused));
		int payload_size;
		struct sockaddr_in serv_addr;
		struct tm *local;
		time_t t;
		uint8_t aux;
		struct package the_package;

		init_package(&the_package);
		ascii_art(0);

		/* (1) Parse command-line options */

		if(argc > 1 && argc < 4){

				if(*argv[1] == 'h' && argc == 2){
					printf("----------------------------------------\n");
					printf("This is the ASP client.\nOptions: \nYou can specify the buffer size with 'b'\nYou can ask for help with 'h'\n");
					if(UNRELIABLE){
						printf("The simulation macro is currently ON.\n");
					    printf("Edit the macro at asp.h and re-compile for musi.c\n");
					}
					else{
						printf("The simulation macro is currently OFF");
						printf("Edit the macro at asp.h and re-compile for simulation\n");
					}
					printf("----------------------------------------\n");
					exit(1);
				}
				else if(*argv[1] == 'b' && argc == 3){
					if(*argv[2] < MAX_BUFFER)
						if(!(atoi(argv[2]) % 2)){
							the_package.buffer_size = atoi(argv[2]);
							printf("> New buffer size assigned = %d \n", the_package.buffer_size);
						}
						else
							errorMsg("buffer(should be a multiple of 2!");
					else
						errorMsg("buffer (way to big!)");
				}
				else errorMsg("input format");
			}

		/* (2) Set up network connection */

		if ((TCPclient = socket(AF_INET, PROTOCOL, 0)) < 0)
		       errorMsg("socket");

		 memset(&serv_addr, '0', sizeof(serv_addr));
		 memset(the_package.data, SENTINEL, MAX_BUFFER);
		 serv_addr.sin_family = AF_INET;
		 serv_addr.sin_port = htons(PORT);

		 if(inet_pton(AF_INET, LOCAL_HOST, &serv_addr.sin_addr)<=0)
		        errorMsg("\nInvalid address/ Address not supported \n");

		 /* (3) Communication  */

		 if (connect(TCPclient, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
			 	 perror("(!) Perror of connect : ");
			 	 exit(EXIT_FAILURE);
		 }

		 if(UNRELIABLE) goto unreliable;

		 while(!the_package.received_everything){

			 compress(the_package.data, the_package.quality, the_package.buffer_size);
			 the_package.checksum = compute_checksum(MAX_BUFFER, (short unsigned int *)the_package.data);
			 printf("> Compressing with quality level %d... \n", the_package.quality + 1);
			 update_quality(&the_package, localtime(&t));

			 while((ret = send(TCPclient , &the_package, sizeof(the_package), 0)) < 0) {}
			 t = time(NULL);
			 local = localtime(&t);
			 printf("> %s Package sent... \n", asctime(local));

			 while((ret = read(TCPclient , &the_package, sizeof(the_package))) < 0){}
			 payload_size = the_package.buffer_size;
			 if(the_package.quality < 2) payload_size -= (the_package.buffer_size) / 64;
			 if(the_package.quality > 1 && the_package.quality < 4) payload_size -= (the_package.buffer_size) / 128;

			 if(the_package.received_everything){
				 /* In case is the last package (smaller) we don't play it all */
				 payload_size = 0;
				 while((aux = the_package.data[payload_size++]) != SENTINEL){}
			 }

			 update_quality(&the_package, local);

			 t = time(NULL);
			 local = localtime(&t);
			 printf("> %s Package received... \n",asctime(local));

			 close(TCPclient);

			 if ((TCPclient = socket(AF_INET, PROTOCOL, 0)) < 0)
			 		       errorMsg("socket");

			 if (connect(TCPclient, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
			 			 	 perror("(!) Perror: ");
			 			 	 errorMsg("connection");
			 }

			 play_music(&the_package, payload_size);
			 the_package.num_packages++;
		 }


unreliable: /* This is the plain connection without music */

		while(UNRELIABLE)
		{
			sleep(1.5);

			compress(the_package.data, the_package.quality, the_package.buffer_size);
			 the_package.checksum = compute_checksum(MAX_BUFFER, (short unsigned int *)the_package.data);
			 printf("> Compressing with quality level %d... \n", the_package.quality + 1);
			 update_quality(&the_package, localtime(&t));

			 while((ret = send(TCPclient , &the_package, sizeof(the_package), 0)) < 0) {}
			 t = time(NULL);
			 local = localtime(&t);
			 printf("> %s Package sent... \n", asctime(local));

			 while((ret = read(TCPclient , &the_package, sizeof(the_package))) < 0){}
			 update_quality(&the_package, local);
			 t = time(NULL);
			 local = localtime(&t);
			 printf("> %s Package received... \n",asctime(local));

			 close(TCPclient);

			 if ((TCPclient = socket(AF_INET, PROTOCOL, 0)) < 0)
			 		       errorMsg("socket");

			 if (connect(TCPclient, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
			 			 	 perror("(!) Perror: ");
			 			 	 errorMsg("connection");
			 }
		}

  return 0;
}
