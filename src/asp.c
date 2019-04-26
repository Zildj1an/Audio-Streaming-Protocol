#include "asp.h"

void inline __attribute__((always_inline)) init_package(struct package *the_package){
	the_package->num_correct_checks = 0;
	the_package->num_incorrect_checks = 0;
	the_package->quality = HIGH;
	the_package->buffer_size = BLOCK_SIZE;
	the_package->received_everything = 0;
	the_package->num_packages = 0;
}

void inline __attribute__((always_inline)) init_addr(struct sockaddr_in *local_addr){
	  local_addr->sin_family = AF_INET;
	  local_addr->sin_addr.s_addr = INADDR_ANY;
	  local_addr->sin_port = htons(PORT);
}

void ascii_art(int server){

	FILE *file;
	register int c;
	file = fopen("ascii.txt", "r");

	if(server)
		printf(ANSI_COLOR_RED "");
	else
		printf(ANSI_COLOR_GREEN "");

	if (file) {
		while ((c = getc(file)) != EOF)
			        putchar(c);
		fclose(file);
	}
	else
		perror("ASCII Art file (ascii.txt) couldn't be opened. Maybe because of the permissions?");

	printf(ANSI_COLOR_RESET "");
	printf("\n");
}

/* Computes checksum for "count" byte beginning at location "addr".*/
long compute_checksum(int count, unsigned short *addr){

    register long sum = 0;

    while(count > 1)  {
        sum += * (unsigned short*) addr++;
        count -= 2;
    }

    /*  Add left-over byte, if any */
    if(count)
        sum += * (unsigned short *) addr;

    /*  Fold 32-bit sum to 16 bits */
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;
}

void errorMsg(char *msg){
    fprintf(stderr,"Problems with the %s...\n",msg);
    exit(EXIT_FAILURE);
}

void inline compress(uint8_t *data, enum quality_level quality, int size){

	switch(quality){

      case EXTREME_HIGH:
		    downsamp_reduct(data,COMPRESS_LOW,0, size);
	      break;
	  default:
	    	downsamp_reduct(data,(quality == EXTREME_LOW || quality == MEDIUM)? COMPRESS_LOW : COMPRESS_HIGH,0, size);
	    	downsamp_reduct(data,(quality == EXTREME_LOW || quality == LOW)? COMPRESS_LOW : COMPRESS_HIGH,1, size);
        break;
	}
}

void downsamp_reduct(uint8_t *data, int freq, int downsap, int size){

	   uint8_t aux[MAX_BUFFER];
	   int i = 0, m = 0;

	   assert(freq == COMPRESS_HIGH || freq == COMPRESS_LOW);
	   memset(aux, SENTINEL , sizeof(aux));

	   while(m < size){

			  if(data[m] == SENTINEL) break;

			  if(downsap){
				  if(m % freq)
				     aux[i++] = data[m];
			  }
			  else {
				  if(!(m % freq))
					  aux[m] = data[m]/2;
				  else
					  aux[m] = data[m];
				data[m] = aux[m];
			  }
		m++;
	   }

	   if(downsap){
		   size = i;
		   for(i = 0; i < size; ++i)
			   data[i] = aux[i];
	   }
}

void update_quality(struct package *the_package , struct tm *current_time){

  if(compute_checksum(MAX_BUFFER, (short unsigned int *)the_package->data) == the_package->checksum){
        printf("> Checksum successful, updating quality accordingly...\n");
        the_package->num_correct_checks++;
        the_package->last_success = *current_time;

        if (the_package->num_correct_checks > the_package->num_incorrect_checks && the_package->quality < 4)
          the_package->quality++;
  }
  else {
      printf("> Checksum fail, updating quality accordingly...\n");
      the_package->num_incorrect_checks++;
      the_package->last_fail = *current_time;

      if (the_package->num_correct_checks <= 2 * the_package->num_incorrect_checks && the_package->quality)
          the_package->quality--;
  }
}

