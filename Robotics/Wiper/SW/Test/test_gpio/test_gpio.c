
#include <stdint.h> // uint16_t and family
#include <stdio.h> // printf and family
#include <unistd.h> // file ops
#include <fcntl.h> // open() flags
#include <string.h> // strerror()
#include <errno.h> // errno

#define DEV_STREAM_FN "/dev/gpio_stream"

#define DEBUG 0

void usage(FILE* f){
	fprintf(f,
"\nUsage: "\
"\n	test_gpio -h|--help"\
"\n		print this help i.e."\
"\n	test_gpio <gpio_no> w <wr_wal>"\
"\n		set GPIO to output and write it 0 or 1"\
"\n	test_gpio <gpio_no> r"\
"\n		set GPIO to input and read value"\
"\n	gpio_no = [2, 26]"\
"\n wr_val = 0 or 1"\
"\n"\
);

}

static inline int c_str_eq(const char* a, const char* b) {
	return !strcmp(a, b);
}

int parse_args(
	int argc,
	char** argv,
	int* p_gpio_no,
	char* p_op,
	int* p_wr_val
) {
	if(argc == 2){
		if(c_str_eq(argv[1], "-h") || c_str_eq(argv[1], "--help")){
			// Print help.
			usage(stdout);
			return 0;
		}else if(
			c_str_eq(argv[1], "n") ||
			c_str_eq(argv[1], "u") ||
			c_str_eq(argv[1], "d")
		){
			*p_op = argv[1][0];
		}else{
			// Error.
			fprintf(stderr, "ERROR: Wrong op \"%s\"!\n", argv[1]);
			usage(stderr);
			return 1;
		}
	}else if(argc == 3){
		if(!c_str_eq(argv[1], "r")){
			fprintf(stderr, "ERROR: Wrong op \"%s\"!\n", argv[1]);
			usage(stderr);
			return 2;
		}
		*p_op = 'r';
		int n;
		n = sscanf(argv[2], "%d", p_gpio_no);
		if(n != 1){
			fprintf(stderr, "ERROR: Invalid number \"%s\"!\n", argv[2]);
			return 3;
		}
	}else if(argc == 4){
		if(!c_str_eq(argv[1], "w")){
			fprintf(stderr, "ERROR: Wrong op \"%s\"!\n", argv[1]);
			usage(stderr);
			return 2;
		}
		*p_op = 'w';
		int n;
		n = sscanf(argv[2], "%d", p_gpio_no);
		if(n != 1){
			fprintf(stderr, "ERROR: Invalid number \"%s\"!\n", argv[2]);
			return 3;
		}

		n = sscanf(argv[3], "%d", p_wr_val);
		if(n != 1){
			fprintf(stderr, "ERROR: Invalid number \"%s\"!\n", argv[3]);
			return 3;
		}
	}else{
		// Error.
		fprintf(stderr, "ERROR: Wrong number of arguments!\n");
		usage(stderr);
		return 1;
	}
	//TODO limits
	return 0;
}


int main(int argc, char** argv){
	int gpio_no;
	char op;
	int wr_val;
	int r = parse_args(argc, argv, &gpio_no, &op, &wr_val);
	if(r){
		return r;
	}

#if DEBUG
	printf("gpio_no = %d\n", gpio_no);
	printf("op = %c\n", op);
	printf("wr_val = %d\n", wr_val);
#endif


	//TODO Check gpio_num, op and wr_val for correct values.
	if(op != 'w' && op != 'r' && op != 'n' && op != 'u' && op != 'd'){
		printf("ERROR: op not w nor r\n");
		return 5;
	}

	if(op == 'w'){
	    if(wr_val != 0 && wr_val != 1){
			printf("ERROR: wr_val must be 0 or 1\n");
			return 6;
		}
	}



	int fd;
	fd = open(DEV_STREAM_FN, O_RDWR);
	if(fd < 0){
		fprintf(stderr, "ERROR: \"%s\" not opened!\n", DEV_STREAM_FN);
		fprintf(stderr, "fd = %d %s\n", fd, strerror(-fd));
		return 4;
	}


	if(op == 'w'){
		uint8_t pkg[3];
		pkg[0] = 'w';
		pkg[1] = gpio_no;
		pkg[2] = wr_val;

		printf("write %d to gpio%d\n", wr_val, gpio_no);

		r = write(fd, pkg, sizeof(pkg));
		if(r != sizeof(pkg)){
			fprintf(stderr, "ERROR: write went wrong!\n");
			return 4;
		}
	}else if(op == 'r'){
		uint8_t pkg[2];
		pkg[0] = 'r';
        pkg[1] = gpio_no;

		r = write(fd, pkg, sizeof(pkg));
		if(r != sizeof(pkg)){
			fprintf(stderr, "ERROR: write went wrong!\n");
			return 4;
		}

		uint8_t rd_val;
		r = read(fd, (char*)&rd_val, sizeof(rd_val));
        if(r != sizeof(rd_val)){
			fprintf(stderr, "ERROR: read went wrong!\n");
			return 5;
		}
#if DEBUG
		printf("rd_val = %d\n", rd_val);
#endif

		printf("read %d from gpio%d\n", rd_val, gpio_no);
	}else if(op == 'n' || op == 'u' || op == 'd'){
		uint8_t pkg[1];
		pkg[0] = op;
 
		r = write(fd, pkg, sizeof(pkg));
		if(r != sizeof(pkg)){
			fprintf(stderr, "ERROR: write went wrong!\n");
			return 4;
		}
	}
	
	close(fd);

	printf("End.\n");

	return 0;
}
