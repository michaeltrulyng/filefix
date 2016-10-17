/*
* filefix.c
*	Program that reports and corrects invalid characters (I1760 errors in PPRO)
*
* Flow:
*    -specify data file (-d)
*    -specify record length (-l)
*    -specify record position (-p)
*	 -specify input file with record positions (-f)
*        +input file is assumed to have a single record position on each line
*		 +if there is an invalid input (NAN), then we will abort without proceeding
*		 +if a record position (-p) is provided in addition to a file, both will
*		 	be processed starting with the single position
*		 +if an invalid record position is provided, we will abort without continuing
*	 -set hex zero (0x00) full-detection mode (-x). Fill char is 0xFF.
*	 -set non-hex zero full-detection mode (-y). Fill char can be set and defaults to 0x20.
*
*	  se run in update mode.
*    If any of the above are missing, the program will abort.
*    DATAFILE should include the filepath.
*
*	Run notes:
*
*		- Hex zero and non-hex zero modes can be run in conjunction. However, neither
*			of these can be run with the normal position detection mode (single
*			postion or list via input file)
*		- Running in hex zero (0x00) full-detection mode requires that ITEST be run or
*			the data file reindexed.
*		- The record size should be one more than the size specified in XXXDEF files to
*			account for the record divider character (0xFA)
*
* Author: Michael Ly
*	version: 1.1
*
*	Version history
*		- 1.0 (2014.12.24):
*			- Initial revision
*			- Support converting characters that are returned by filechk
*
*		- 1.1 (2015.11.09):
*			- Added support for complete data file traversal
*			- Added support for rolling out ITEST
*			- Added support for 0x00 character detection (past a threshold) with
*				removal of the entire record
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "filelib.h"

/* Globals */
#define	ARRAY_SIZE 1024

size_t RECORD_SIZE = 0 ;
char DATAFILE[ARRAY_SIZE] = "" ;
char INPUTFILE[ARRAY_SIZE] = "" ;

unsigned int VALID_START = 32 ;
unsigned int VALID_END = 126 ;
unsigned char END_OF_RECORD = 250 ;
unsigned char END_OF_RECORD_CR = 10 ;
unsigned char NULL_VALUE = 0 ;

unsigned char FILL_VALUE = 32 ;
unsigned char NULL_FILL_VALUE = 255 ;
unsigned int DELETE_NULL_THRESHOLD = 10 ;

char* DBC_IKEYS = NULL ;
char* DBC_ICHRS = NULL ;

size_t RECORD_POSITION = 0 ;
size_t POSITION_SET = 0 ;

unsigned char VERBOSE_FLAG = 0 ;
unsigned char HELP_FLAG = 0 ;
unsigned char UPDATE_FLAG = 0 ;
unsigned char FULL_DETECTION_FLAG = 0 ;
unsigned char ZERO_DETECTION_FLAG	= 0 ;
unsigned char ITEST_FLAG = 0 ;

const char ITEST1_PATH[25] = "/ppro/src/cffp/ITEST.PRG\0" ; // ITEST assumed to exist at all customers
/* Temp changing these on Hera */
//const char ITEST2_PATH[28] = "/ppro/mtl/src/cf/ITEST2.PRG\0" ;
//const char ITEST3_PATH[28]  = "/ppro/mtl/src/cf/ITEST3.PRG\0" ;

const char ITEST2_PATH[24] = "/ppro/src/cf/ITEST2.PRG\0" ;
const char ITEST3_PATH[24]  = "/ppro/src/cf/ITEST3.PRG\0" ;

const char FILEPATH_SEPARATOR[2] = "/\0" ;

unsigned char ITEST_VERSION = 1 ;


/* Prototypes */
int process_file(void) ;
int set_position(const char *param) ;
int set_data_file(const char *param) ;
int set_size(const char *param) ;
int set_fill_val(const char *param) ;
int parse_cmd(const char *cmd, const char *param) ;
int set_input_file(const char *param) ;
int parse_input_file(void) ;
void get_env(void) ;
void help_msg(void) ;
void set_update(void) ;
void set_help(void) ;
void set_full_detection(void) ;
void set_zero_detection(void) ;
void set_itest(void) ;
void verbose(void) ;
void run_itest(void) ;

off_t ftello(FILE*);

/*
* valid_cmd - returns whether a command is valid
* @cmd - input command
* @return Input command is a valid command
*/
int valid_cmd(char *cmd){
    return (strcmp(cmd,"d")==0 || strcmp(cmd,"f")==0 || strcmp(cmd,"h")==0 || strcmp(cmd,"i")==0 || 
    	strcmp(cmd,"l")==0 || strcmp(cmd,"p")==0 || strcmp(cmd,"t")==0 || strcmp(cmd,"u")==0 || 
    	strcmp(cmd,"v")==0 || strcmp(cmd,"x")==0 || strcmp(cmd,"y")==0) ;
} ;


/*
* parse_cmd - control for branching to valid commands
*
* valid commands:
*   -d: specify data file
*   -f: fill value (ASCII) - default: 32
*   -l: specify record length
*   -p: specify record position
*   -h: help (display syntax info)
*	-u: update mode
*   -v: verbose
*	-x: hex 00 detection mode
*
* @cmd command
* @param parameter associated with command
* @return Command executed successfully
*/
int parse_cmd(const char *cmd, const char *param){
	int ret_val = 0 ;

//	printf("command: %s; parameter: %s\n", cmd, param) ;

	if (strcmp(cmd,"d")==0)
		ret_val = set_data_file(param) ;
    else if (strcmp(cmd,"f")==0)
        ret_val = set_fill_val(param) ;
    else if (strcmp(cmd,"h")==0)
        set_help() ;
    else if (strcmp(cmd,"i")==0)
        set_input_file(param) ;
	else if (strcmp(cmd,"l")==0)
		ret_val = set_size(param);
	else if (strcmp(cmd,"p")==0)
		ret_val = set_position(param) ;
	else if (strcmp(cmd,"t")==0)
		set_itest() ;
	else if (strcmp(cmd,"u")==0)
		set_update() ;
    else if (strcmp(cmd,"v")==0)
        verbose() ;
	else if (strcmp(cmd,"x")==0)
		set_zero_detection() ;
	else if (strcmp(cmd,"y")==0)
		set_full_detection() ;
	return ret_val ;
} ;

/*
* set_size - sets size of record to parse
* @return Record size successfully set
*/
int set_size(const char *param){
	int ret_val = 0 ;
	if (is_number((char**)&param)){
		RECORD_SIZE = (size_t) strtoll(param,(char**)NULL,10) ;
		printf("Setting record size to: %zu\n",RECORD_SIZE) ;
	}else 
		ret_val = 1 ;
	return ret_val ;
} ;

/*
* set_data_file - set the data file
* @return Data file successfully set
*/
int set_data_file(const char *param){
	int ret_val = 0 ;
	int param_size = strlen(param) ;
	strncpy((char*)&DATAFILE,param,param_size*sizeof(char)) ;
	DATAFILE[param_size] = '\0' ;
	if ( strcmp(param,(char*)&DATAFILE)!= 0 )
		ret_val = 1 ;
	printf("DATAFILE: %s\n",DATAFILE) ;
	return ret_val ;
} ;

/*
* set_input_file - set the input record position file
* @return Input file successfully set
*/
int set_input_file(const char *param){
	int ret_val = 0 ;
	int param_size = strlen(param) ;
	strncpy((char*)&INPUTFILE,param,param_size*sizeof(char)) ;
	INPUTFILE[param_size] = '\0' ;
	if ( strcmp(param,(char*)&INPUTFILE)!= 0 )
		ret_val = 1 ;
	printf("Input file: %s\n",INPUTFILE) ;
	return ret_val ;
} ;

/*
* set_position - set the (single) record position for which to check for invalid characters
* @return Record position successfully set
*/
int set_position(const char *param){
	int ret_val = 0 ;
	if (is_number((char**)&param)){
		RECORD_POSITION = (size_t) strtoll(param,(char**)NULL,10) ;
		POSITION_SET = 1 ;
		printf("Setting record position to: %zu\n",RECORD_POSITION) ;
	}else
		ret_val = 1 ;
	return ret_val ;
} ;

/*
* set_fill_val - set the fill value (decimal) to replace invalid characters
* @return Fill value successfully set
*/
int set_fill_val(const char *param){
	int ret_val = 0, param_val ;
	if ( is_number((char**)&param)){
		param_val = (size_t) strtoll(param,(char**)NULL,10) ;
		if (param_val>=VALID_START&&param_val<=VALID_END){
			FILL_VALUE = param_val ;
			printf("Setting fill value to: %u (%c)\n",FILL_VALUE,(char) FILL_VALUE) ;
		}
		else{
			printf("Invalid fill value specified.\n") ;
			ret_val = 1 ;
		}
	}else{
		printf("Input is not a number.\n") ;
		ret_val = 1 ;
	}
	return ret_val ;
} ;

/*
* set_help - set help message print flag
* + when the help message is printed, no other command is run
*/
void set_help( void ){
	HELP_FLAG = 1 ;
	return ;
} ;

/*
 * set_full_detection - set full detection mode flag
 * + set flag to process entire data file instead of specific positions
 */
void set_full_detection( void ){
	FULL_DETECTION_FLAG = 1 ;
	printf("Full-detection mode set.\n");
	return ;
} ;

/*
 * set_full_detection - set full detection mode flag
 * + set flag to process entire data file instead of specific positions
 */
void set_zero_detection( void ){
	ZERO_DETECTION_FLAG = 1 ;
	printf("Zero-detection mode set.\n");
	return ;
} ;

/*
* help_msg - display help message
*/
void help_msg( void ){
	printf("Usage: filefix [-d data_file] [-f fill] [-h] [-l length] [-p position] [-u] [-v]\n") ;
	printf("Update unsupported characters in files.\n\n") ;
	printf("Mandatory arguments:\n") ;
	printf("\t-d data file      Input data file including file path and extension\n") ;
	printf("\t                  (e.g. /ppro/data/SOH0001.TXT\n") ;
	printf("\t-l length         Record size (file definition size +1 for record separator).\n") ;
	printf("\t                  (i.e. XXX.DEF)\n") ;
	printf("One of:\n") ;
	printf("\t-i input file 	Input file including file path an extension containing\n") ;
	printf("\t 						record positions for records with invalid characters.\n") ;
	printf("\t-p position       Record position as returned from filechk.\n") ;
	printf("\t-y				Run in full-detection mode. Uses same fill\n") ;
	printf("\t						character as invalid character detection mode.\n") ;
	printf("\nOptional arguments:\n") ;
	printf("\t-f fill           Set ASCII fill value. Default is 32 (space).\n") ;
	printf("\t-t ITEST			Run ITEST (after other operations). Highly recommended\n") ;
	printf("\t 						to run after hex-zero full-detection mode.\n") ;
	printf("\t-h help 			Display help messages.\n") ;
	printf("\t-u update mode    Run program in update mode. Default is report only.\n") ;
	printf("\t-x 				Run in hex zero full-detection mode. Uses 0xFF as\n") ;
	printf("\t						the fill character.\n") ;
	printf("\t-v verbose		Run in verbose mode.\n") ;
	
	return ;
} ;

/*
* verbose - set verbose mode
*/
void verbose( void ){
	VERBOSE_FLAG = 1 ;
	return ;
} ;

/*
* set_update - set update mode
*/
void set_update( void ){
	printf("Update mode set.\n") ;
	UPDATE_FLAG = 1 ;
	return ;
} ;

/*
* get_env - get environment variables that are set for EXTDREP
*/
void get_env( void ){
	DBC_IKEYS = getenv("DBC_IKEYS") ;
	DBC_ICHRS = getenv("DBC_ICHRS") ;

//	printf("DBC_IKEYS = %s\n",DBC_IKEYS) ;
//	printf("DBC_ICHRS = %s\n",DBC_ICHRS) ;
	return ;
} ;

/*
* set_itest - set itest flag
*/
void set_itest( void ){
	ITEST_FLAG = 1 ;
	return ;
} ;

/*
* run_itest - attempts to run the latest version of ITEST. Can
* be forced to run a specific version (if exists)
*/
void run_itest ( void ){
	struct stat_struct* stat_buf = malloc(sizeof(struct stat)) ;
	char *datafile_name = parse_filename(DATAFILE, (const char*) FILEPATH_SEPARATOR) ;
	char rollout_cmd[ARRAY_SIZE] ;
	memset(rollout_cmd,0,ARRAY_SIZE*sizeof(char)) ;

//	printf("ITEST3_PATH: %s\n",ITEST3_PATH) ;
//	printf("ITEST2_PATH: %s\n",ITEST2_PATH) ;

	if (stat(ITEST3_PATH, (struct stat*) stat_buf) == 0){
		ITEST_VERSION = 3 ;
	}else if (stat(ITEST2_PATH, (struct stat*) stat_buf) == 0){
		ITEST_VERSION = 2 ;
	}

	switch (ITEST_VERSION){
		case 3:
			strcpy(rollout_cmd, "DBC ITEST3 ") ;
			strcat(rollout_cmd, datafile_name) ;
			strcat(rollout_cmd, " ALL DUP") ;
			break ;
		case 2:
			strcpy(rollout_cmd, "DBC ITEST2 ") ;
			strcat(rollout_cmd, datafile_name) ;
			strcat(rollout_cmd, " ALL DUP") ;
			break ;
		case 1:
		default:
			strcpy(rollout_cmd, "DBC ITEST") ;
			break ;
	}
	printf("Rolling out the current ITEST command: %s\n", rollout_cmd) ;
	system(rollout_cmd) ;
	return;
} ;

/*
* process_file
* @return Error encountered while processing file
*/
int process_file(void){
	FILE *data_file = NULL, *input_file = NULL ;
	char buffer[RECORD_SIZE], writebuf[RECORD_SIZE], *input_pos = NULL ;
	int i, ret_val = 0, check_position = POSITION_SET ;
	int read_size = 0, write_size = 0, valid_input = 0, first_pos, null_count ;
	size_t file_pos = 0, current_pos = 0, invalid_count = 0 ;
	unsigned char get_char ;

	if ( (data_file = fopen(DATAFILE, "r+b"))==NULL ){
		perror("ERROR") ;
		ret_val = 1 ;
	}

	if (strlen(INPUTFILE)>0){
		if ((input_file = fopen(INPUTFILE, "rb"))==NULL){
			perror("ERROR") ;
			ret_val = 1 ;
		}else{
			valid_input = 1 ;
			input_pos = malloc(sizeof(char)*ARRAY_SIZE) ;
		}
	}
	
	/* Process until any issue is encountered. If any issue is encountered, abort
	and write out the changes already made to the file */
	if (ret_val==0){

	/* Perform hex-zero detection */
	if (FULL_DETECTION_FLAG==1 || ZERO_DETECTION_FLAG==1){
		if (fseek(data_file,0,SEEK_SET)==0){
			file_pos = ftello(data_file) ;
			while (fread(&buffer,sizeof(char),RECORD_SIZE,data_file)==RECORD_SIZE){
				first_pos = 1 , null_count = 0 ;

				/* strncpy CANNOT be used because it only copies until a null-terminating
				character is encountered ('\0', 0x00) */
				//strncpy(&writebuf,&buffer,RECORD_SIZE*sizeof(char));
/*
				for (i=0;i<RECORD_SIZE;i++)
					writebuf[i] = buffer[i] ;
*/
				memcpy(writebuf,buffer,RECORD_SIZE*sizeof(char)) ;
/*
				for (i=0;i<RECORD_SIZE;i++){
					printf("%x",buffer[i]) ;
				}
				printf("\n") ;
*/
				for (i=0;i<RECORD_SIZE;i++){
					get_char = buffer[i] ;
/* Removing this line of code since we will run ITEST after hex-zero detection, which would delete 0xFF filled records..
					if ((get_char<VALID_START || get_char>VALID_END)&&(get_char != END_OF_RECORD && get_char != NULL_FILL_VALUE)){
*/
					if ((get_char<VALID_START || get_char>VALID_END) && ((get_char != END_OF_RECORD && get_char != END_OF_RECORD_CR)||((get_char == END_OF_RECORD || get_char == END_OF_RECORD_CR) && i != (RECORD_SIZE - 1)))){
//						printf("Invalid byte %d: %c (hex: %x; dec: %u)\n",i,get_char,get_char&0xff,get_char) ;
						if (get_char == NULL_VALUE && ZERO_DETECTION_FLAG == 1){
							if (first_pos == 1){
								first_pos = 0 ;
								if (file_pos == 0)
									printf("Record: 0; Position: %zu\n",file_pos) ;
								else
									printf("Record: %zd; Position: %zu\n",file_pos/RECORD_SIZE,file_pos) ;
							}
							writebuf[i] = (char) NULL_FILL_VALUE ;
							null_count++ ;
							invalid_count++ ;
							printf("Changing '%c' (hex: %x; dec: %d) to '%c' (hex: %x; dec: %d). Offset: %d\n",buffer[i],buffer[i],buffer[i]&0xff,writebuf[i],writebuf[i]&0xff,writebuf[i],i) ;
						}

						if (get_char != NULL_VALUE && FULL_DETECTION_FLAG == 1){
							if (first_pos == 1){
								first_pos = 0 ;
								if (file_pos == 0)
									printf("Record: 0; Position: %zu\n",file_pos) ;
								else
									printf("Record: %zu; Position: %zu\n",(file_pos/RECORD_SIZE),file_pos) ;
							}
							writebuf[i] = (char) FILL_VALUE ;
							invalid_count++ ;
							printf("Changing '%c' (hex: %x; dec: %d) to '%c' (hex: %x; dec: %d). Offset: %d\n",buffer[i],buffer[i],buffer[i]&0xff,writebuf[i],writebuf[i]&0xff,writebuf[i],i) ;
						}
					}
				}

				if (ZERO_DETECTION_FLAG == 1){
					if (null_count > 0)
						printf("%d occurrences of 0x00 characters found.\n", null_count) ;

					if (null_count > DELETE_NULL_THRESHOLD)
						memset(writebuf, NULL_FILL_VALUE, RECORD_SIZE*sizeof(char)) ;
				}
/*
				for (i=0;i<RECORD_SIZE;i++){
					printf("%x",writebuf[i]) ;
				}
				printf("\n") ;
*/
//				if (UPDATE_FLAG && strncmp(&writebuf,&buffer,RECORD_SIZE*sizeof(char))!=0){
				if (UPDATE_FLAG && memcmp(&writebuf,&buffer,RECORD_SIZE*sizeof(char))!=0){
					if (fseek(data_file,file_pos*sizeof(char),SEEK_SET)==0){
						if ((write_size = fwrite(&writebuf,sizeof(char),RECORD_SIZE,data_file))!=RECORD_SIZE)
							fprintf(stderr, "ERROR: %d bytes of %zu written.\n",read_size,RECORD_SIZE) ;
						else
							printf("File updated.\n") ;
						if (fseek(data_file,(file_pos + RECORD_SIZE)*sizeof(char),SEEK_SET)!=0)
							perror("FILE REPOSITIONING ERROR") ;
					}else
						perror("ERROR") ;
				}
				file_pos = ftello(data_file) ;
			}
		}
	}else{
			while (check_position || valid_input){
				if (check_position == 1){
					current_pos = RECORD_POSITION ;
					check_position = 0 ;
				}else if (input_file!=NULL){

					if (fgets(input_pos,ARRAY_SIZE,input_file)!=NULL){
						strtok(input_pos, "\n") ;
						if (!is_number((char**)&input_pos)){
							printf("Input file position (%s) is invalid.\n",input_pos) ;
							ret_val = 1 ;
						}else{
							current_pos = (size_t) strtoll(input_pos,(char**)NULL,10) ;
						}
					}else{
						fclose(input_file) ;
						valid_input = 0 ;
						free(input_pos) ;
						input_pos = NULL ;
						continue ;
					}
				}// Set position of record to check. RECORD_POSITION takes precedence
				printf("Record position: %zu\n", current_pos);
				if (fseek(data_file,current_pos*sizeof(char),SEEK_SET)==0){
					file_pos = ftello(data_file) ;
					if ((read_size = fread(&buffer,sizeof(char),RECORD_SIZE,data_file))!=RECORD_SIZE){
						fprintf(stderr, "ERROR: %d bytes of %zu read.\n",read_size,RECORD_SIZE) ;
						if (feof(data_file)){
							fprintf(stderr, "Hit end of file (EOF)!\n");
						}else{
							fprintf(stderr, "An unknown error interrupted read!\n");
						}
					}else{
						//strncpy(&writebuf,&buffer,RECORD_SIZE*sizeof(char)) ;
/*
						for (i=0;i<RECORD_SIZE;i++)
							writebuf[i] = buffer[i] ;
*/
						memcpy(writebuf,buffer,RECORD_SIZE*sizeof(char)) ;

						for(i=0;i<RECORD_SIZE;i++){
							get_char = buffer[i] ;
//							if ((get_char<VALID_START || get_char>VALID_END)&&(get_char != END_OF_RECORD && get_char != NULL_FILL_VALUE)){
							if ((get_char<VALID_START || get_char>VALID_END) && ((get_char != END_OF_RECORD && get_char != END_OF_RECORD_CR && get_char != END_OF_RECORD_CR)||((get_char == END_OF_RECORD || get_char == END_OF_RECORD_CR) && i != (RECORD_SIZE - 1)))){
								printf("Invalid byte %d: %c (hex: %x; dec: %u)\n",i,get_char,get_char&0xff,get_char) ;
								invalid_count++ ;
								writebuf[i] = (char) FILL_VALUE ;
							}
						}

//						if (UPDATE_FLAG && strncmp(&writebuf,&buffer,RECORD_SIZE*sizeof(char))!=0){
						if (UPDATE_FLAG && memcmp(&writebuf,&buffer,RECORD_SIZE*sizeof(char))!=0){
							if (fseek(data_file,file_pos*sizeof(char),SEEK_SET)==0){
								if ((write_size = fwrite(&writebuf,sizeof(char),RECORD_SIZE,data_file))!=RECORD_SIZE)
									fprintf(stderr, "ERROR: %d bytes of %zu written.\n",read_size,RECORD_SIZE) ;
								else
									printf("File updated.\n") ;
							}else 
								perror("ERROR") ;
						}
					}
				}
			}// ret_val if-statement
		}
	}

	printf("Number of invalid characters processed: %zu\n",invalid_count) ;

	fclose(data_file) ;
	return ret_val ;
} ;



int main(int argc, char **argv){
	char *arg, current_cmd[256] = "", first_char = '-', *param;
	int i , keep_alive = 1, new_size, parse_pass = 0 ;

	/* 
	*	Initial control loop to validate all commands
	*	We pass through all because we want to abort on invalid command as well as have HELP execute
	*	only the help message.
	*/
	get_env() ;

	/* Two passes for parsing program arguments. */
	for (parse_pass=0;parse_pass<2;parse_pass++){
		i=0 ;

		while (keep_alive && i<argc&&!HELP_FLAG){
			arg = argv[i] ;

/*	Copy string minus "-" and process it */
			if (first_char_is(arg,&first_char)&&strlen(arg)>1){
				new_size = strlen(arg)-1 ;
				strncpy(current_cmd,arg + sizeof(char),new_size) ;
				current_cmd[new_size] = '\0' ;
				if (!valid_cmd(current_cmd)){
					printf("\"%s\" is not a valid command.\n",current_cmd) ;
					keep_alive = 0 ;
				}else{
	                if (strcmp(current_cmd,"h")==0 || strcmp(current_cmd,"t")==0 || 
	                	strcmp(current_cmd,"u")==0 || strcmp(current_cmd,"v")==0 || 
	                	strcmp(current_cmd,"x")==0 || strcmp(current_cmd,"y")==0){
						if (parse_cmd(current_cmd,(char*)NULL)){
							keep_alive = 0 ;
							printf("Invalid command: %s\n",current_cmd);
						}
					}
					else if (i+1<argc){
						param = argv[i+1] ;
						i++ ;
						if (parse_pass){
							if (parse_cmd(current_cmd,param)){
								keep_alive = 0 ;
								printf("Invalid command: %s\n",current_cmd);
							}
						}
					}else{						
						printf("No parameters provided for command %s.\n",current_cmd) ;
						keep_alive = 0 ;
					}
				}
			}
			i++ ;
		}/* Control loop END */
	}

	if (HELP_FLAG)
		help_msg() ;
	else if (!keep_alive)
		printf("Error detected. Program shutting down.\n") ;
	else if (strlen(DATAFILE)==0 || RECORD_SIZE==0 || ((POSITION_SET==0 && INPUTFILE==NULL) && FULL_DETECTION_FLAG==0 &&
		ZERO_DETECTION_FLAG == 0)){
		printf("Not all parameters provided. Exiting program.\n") ;
		printf("Data file: %s; RECORD_SIZE: %zu; POSITION_SET = %zu\n",DATAFILE,RECORD_SIZE,POSITION_SET) ;
	}
	else{
		printf("Using fill character: '%c' (hex: %x; dec: %d).\n",FILL_VALUE,FILL_VALUE&0xff,FILL_VALUE);
		process_file() ;
		if (ITEST_FLAG == 1)
			run_itest() ;
	}

	return 0 ;
} ;

