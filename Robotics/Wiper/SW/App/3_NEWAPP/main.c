
#include <stdint.h> // uint16_t and family
#include <stdio.h> // printf and family
#include <unistd.h> // file ops
#include <fcntl.h> // open() flags
#include <string.h> // strerror()
#include <errno.h> // errno


#define DEV_STREAM_FN "/dev/gpio_stream"

int main()
{
    int fd,r;
	fd = open(DEV_STREAM_FN, O_RDWR);
	if(fd < 0){
		fprintf(stderr, "ERROR: \"%s\" not opened!\n", DEV_STREAM_FN);
		fprintf(stderr, "fd = %d %s\n", fd, strerror(-fd));
		return 4;
	}

    uint8_t first_command[3] = {'w', 4, 1}; 
    uint8_t second_command[3] = {'w', 3, 0}; 
    uint8_t third_command[3] = {'w', 2, 1};
    uint8_t fourth_command[2] = {'d', 22};
    uint8_t fifth_command[3] = {'w', 2, 0};


	r = write(fd, first_command, sizeof(first_command));
	if(r != sizeof(first_command)){
		fprintf(stderr, "ERROR: write went wrong!\n");
		return 4;
	}
    usleep(100000);

    printf("Stigli smo do ovde : 1\n");

    r = write(fd, second_command, sizeof(second_command));
	if(r != sizeof(second_command)){
		fprintf(stderr, "ERROR: write went wrong!\n");
		return 4;
	}
    usleep(100000);

    printf("Stigli smo do ovde: 2\n");

    r = write(fd, third_command, sizeof(third_command));
	if(r != sizeof(third_command)){
		fprintf(stderr, "ERROR: write went wrong!\n");
		return 4;
	}
    usleep(100000); 


    printf("Stigli smo do ovde : 3\n");

    uint8_t rd_val=0;

    while(1)
    {
        r = write(fd, fourth_command, sizeof(fourth_command));
	    if(r != sizeof(fourth_command)){
		    fprintf(stderr, "ERROR: write went wrong!\n");
		    return 4;
	    }
        usleep(100); 

		r = read(fd, (char*)&rd_val, sizeof(rd_val));
        if(r != sizeof(rd_val)){
			fprintf(stderr, "ERROR: read went wrong!\n");
			return 5;
		}


        if(rd_val)
        {
            printf("Doslo je do stanja 1\n");
            break;
        }

        printf("Trenutna vrednost je nula\n");

    }

    r = write(fd, fifth_command, sizeof(fifth_command));
	if(r != sizeof(fifth_command)){
	    fprintf(stderr, "ERROR: write went wrong!\n");
	    return 4;
    }


    printf("Zavrsili smo sa rotiranjem\n");
    close(fd);

    return 0;
}