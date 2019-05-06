#include "asp.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/time.h>

#define MAX_FILE_SIZE 30
#define MAX_QUEUE 10

struct wave_header {
    char riff_id[4];
    uint32_t size;
    char wave_id[4];
    char format_id[4];
    uint32_t format_size;
    uint16_t w_format_tag;
    uint16_t n_channels;
    uint32_t n_samples_per_sec;
    uint32_t n_avg_bytes_per_sec;
    uint16_t n_block_align;
    uint16_t w_bits_per_sample;
};

/* wave file handle */
struct wave_file {
    struct wave_header *wh;
    int fd;

    void *data;
    uint32_t data_size;

    uint8_t *samples;
    uint32_t payload_size;
};

static struct wave_file wf = {0,};

/* open the wave file */
static int open_wave_file(struct wave_file *wf, const char *filename) {

	  uint8_t *p;
	  float playlength;
	  struct stat statbuf;
	  uint32_t *size;

	  /* Open the file */
	  if ((wf->fd = open(filename, O_RDONLY)) < 0)
	    errorMsg("opening the file\n");

	  /* Get the size of the file */
	  if (fstat(wf->fd, &statbuf) < 0) return -1;
	  wf->data_size = statbuf.st_size;

	  /* Map the file into memory */
	  wf->data = mmap(0x0, wf->data_size, PROT_READ, MAP_SHARED, wf->fd, 0);
	  if (wf->data == MAP_FAILED) {
	    fprintf(stderr, "mmap failed\n");
	    return -1;
	  }

	  wf->wh = wf->data;

	  /* Check whether the file is a wave file */
	  if (strncmp(wf->wh->riff_id, "RIFF", 4) || strncmp(wf->wh->wave_id, "WAVE", 4) || strncmp(wf->wh->format_id, "fmt", 3)) {
	    fprintf(stderr, "%s is not a valid wave file\n", filename);
	    return -1;
	  }

	  /* Skip to actual data fragment */
	  p = wf->data + wf->wh->format_size + 16 + 4;
	  size = (uint32_t *) (p + 4);

	  do {
	    if (strncmp((char *)p, "data", 4))
	      break;

	    wf->samples = p;
	    wf->payload_size = *size;
	    p += 8 + *size;
	  } while (strncmp((char*)p, "data", 4) && (uint32_t) (((void *) p) - wf->data) < statbuf.st_size);

	  if (wf->wh->w_bits_per_sample != 16) {
	    fprintf(stderr, "can't play sample with bitsize %d\n",
		    wf->wh->w_bits_per_sample);
	    return -1;
	  }

	  playlength = (float) *size / (wf->wh->n_channels * wf->wh->n_samples_per_sec * wf->wh->w_bits_per_sample / 8);

	  printf("file %s, mode %s, samplerate %d, time %.1f sec\n",
		 filename, wf->wh->n_channels == 2 ? "Stereo" : "Mono",
		 wf->wh->n_samples_per_sec, playlength);

	  wf->samples += 8;

  return 0;
}

/* close the wave file/clean up */
static void inline close_wave_file(struct wave_file *wf) {
  munmap(wf->data, wf->data_size);
  close(wf->fd);
}


int main(int argc, char **argv) {

	  char *filename;
	  uint8_t *c;
	  register int TCPsocket = 0, TCPclient = 0;
	  struct tm *local;
	  int start,end;
	  time_t t;
      struct package the_package;
      struct sockaddr_in local_addr;
      socklen_t len;
      int enable = 1, i =0;

      ascii_art(1); // Server ASCII art is green and client is red

	  /* (1) Parse command line options */

	  if(argc == 2){
			if(strlen(argv[1]) < MAX_FILE_SIZE){
				filename = malloc(strlen(argv[1]));
				strcpy(filename, argv[1]);
			}
			else
				errorMsg("file size");
		}
	  else if(!UNRELIABLE)
					errorMsg("format! Provide a file name!");

	  /* (2) Open the WAVE file */
	  if (!UNRELIABLE && open_wave_file(&wf, filename) < 0)
	  	  errorMsg("Opening the wave file");

	  /* (3) Read and send audio data via UDP #################################################################  */

	  /* Create the socket file descriptor
	   * AF_INET = IPv4, PROTOCOL = Can be SOCK_STREAM (TCP) or SOCK_DGRAM (UDP)
	   */
	  if (!(TCPsocket = socket(AF_INET, PROTOCOL, 0)))
	         errorMsg("socket failed");

	  // Avoid errors as 'address already in use'
	  if (setsockopt(TCPsocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &enable, sizeof(enable)))
		 errorMsg("setsockopt");

	  init_addr(&local_addr);
	  len = sizeof(local_addr);

	  // Attaching socket to the port
	  if (bind(TCPsocket, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
	         errorMsg("bind");

	  /* Passive Mode */
	  if (listen(TCPsocket, MAX_QUEUE) < 0)
	         errorMsg("listen");

	  if(!UNRELIABLE)
		  printf("> Song size = %d Bytes \n", wf.payload_size);
	  else
		  printf(" >> Welcome to the simulation mode. Open the client.\n");

	  if(UNRELIABLE) goto unreliable;

	  while(1){

		  /* Accept connection */
		  if ((TCPclient = accept(TCPsocket, (struct sockaddr *)&local_addr, (socklen_t*) &len)) < 0)
				 errorMsg("accept");

		  while((enable = read(TCPclient, &the_package, sizeof(the_package)) < 0)){}
		  fflush(fdopen(TCPclient, "w"));
		  t = time(NULL);
		  local = localtime(&t);
		  printf("> %s Fetched client, buffer size %d, quality %d\n", asctime(local),the_package.buffer_size, the_package.quality + 1);
		  update_quality(&the_package,local);

		  /* Check what part of the song is required */
		  start = the_package.buffer_size * the_package.num_packages;
      
		  if(start + the_package.buffer_size <  wf.payload_size)
			  end = start + the_package.buffer_size;
		  else { /* Last package of data */
			  end =  wf.payload_size;
			  the_package.received_everything = 1;
		  }

		  for(c = wf.samples + start; c < wf.samples + end; c++)
			  the_package.data[i++] = *c;

		  i = 0;
		  compress(the_package.data, the_package.quality,  the_package.buffer_size);
		  printf("> Compressing with quality level %d... \n", the_package.quality + 1);
		  the_package.checksum = compute_checksum(MAX_BUFFER, (short unsigned int *) the_package.data);

		  while((enable = send(TCPclient, &the_package, sizeof(the_package), 0) < 0)){}
		  t = time(NULL);
		  local = localtime(&t);
		  printf("> %s Package sent... \n", asctime(local));
		  close(TCPclient);
	  }

	  /* Clean up */
	  close_wave_file(&wf);

unreliable:

	while(UNRELIABLE){

		  /* Accept connection */
		  if ((TCPclient = accept(TCPsocket, (struct sockaddr *)&local_addr, (socklen_t*) &len)) < 0)
				 errorMsg("accept");

		  while((enable = read(TCPclient, &the_package, sizeof(the_package)) < 0)){}
		  t = time(NULL);
		  local = localtime(&t);
		  printf("> %s Fetched client, buffer size %d, quality %d\n", asctime(local),the_package.buffer_size, the_package.quality + 1);
		  update_quality(&the_package,local);
		  compress(the_package.data, the_package.quality,  the_package.buffer_size);
		  printf("> Compressing with quality level %d... \n", the_package.quality + 1);
		  the_package.checksum = compute_checksum(MAX_BUFFER, (short unsigned int *) the_package.data);

		  while((enable = send(TCPclient, &the_package, sizeof(the_package), 0) < 0)){}
		  t = time(NULL);
		  local = localtime(&t);
		  printf("> %s Package sent... \n", asctime(local));
		  close(TCPclient);

		  sleep(1.5);
	}


	   close(TCPsocket);

  return 0;
}
