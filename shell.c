#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define LINE_SIZE 100
#define NO_OF_ARGS 100
#define NO_OF_PIPES 100
#define MAX_CHILDREN 100

struct child_processes{
	int running[MAX_CHILDREN];
	int stopped[MAX_CHILDREN];
	int bg[MAX_CHILDREN];
	int cur_running;
	int cur_stopped;
	int cur_bg;
}children; 

void print_jobs(){
	if(children.cur_stopped < 0){
		printf("No stopped jobs.\n");
	}
	int i;
	printf("\n");
	for(i=0; i<=children.cur_stopped;i++){
		printf("%d \n",children.stopped[i] );
	}
}

int get_process_index(int *arr, int proc, int n){
	int i;
	for(i = 0; i<=n ;i++){
		if(arr[i] == proc){
			return i;
		}
	}
	return -1;
}

void shift_left(int *arr, int n, int k){
	int i;
	for(i = k; i<n; i++){
		arr[i] = arr[i+1];
	}
	return;
}

// returns 1 if some job was brought in fg else 0
int handle_fg(char* pid){
	int i = 0;
	
	int p = atoi(pid);
	int index = get_process_index(&children.stopped, p, children.cur_stopped);
	if(index != -1){
		kill(p,SIGCONT);
		children.cur_running ++;
		children.running[children.cur_running] = children.stopped[index];
		shift_left(&children.stopped, children.cur_stopped, index);
		children.cur_stopped --;
		//printf("%d fg \n",children.cur_stopped );
		return 1;
	}
	else{
		printf("process %d is not stopped or doest exist.\n",p );
		return 0;
	}
	
}

int handle_bg(char* pid){
	int p = atoi(pid);
	int index = get_process_index(&children.stopped, p, children.cur_stopped);
	if(index != -1){
		kill(p,SIGCONT);
		children.cur_bg ++;
		children.bg[children.cur_bg] = children.stopped[index];
		shift_left(&children.stopped, children.cur_stopped, index);
		children.cur_stopped --;
		//printf("%d fg \n",children.cur_stopped );
		return 1;
	}
	else{
		printf("process %d is not stopped or doest exist.\n",p );
		return 0;
	}
}

void ctrl_c_handler(){
	int i, self_pid;
	if(children.cur_running >= 0){
		for(i = children.cur_running; i>=0 ; i--){
			kill(children.running[i], SIGINT);
		}
	}

	/*if(children.cur_running >= 0){
		printf("%d\n",children.running[children.cur_running] );
		kill(children.running[children.cur_running], SIGINT);
		children.cur_running --;	
		
	}*/	
}

// to do -  do exactly what ctrl + z does, here fuction simply exits the shell.
void ctrl_z_handler(){
	int i;
	for(i = children.cur_running; i>=0; i--){
		printf("%d\n",children.running[i] );
		kill(children.running[i], SIGSTOP);
		children.cur_stopped ++;
		children.stopped[children.cur_stopped] = children.running[i];
		
	}
	children.cur_running = -1;
	/*if(children.cur_running >= 0){
		printf("%d\n",children.running[children.cur_running] );
		kill(children.running[children.cur_running], SIGSTOP);
		children.cur_stopped ++;
		children.stopped[children.cur_stopped] = children.running[children.cur_running];
		children.cur_running --;	
	}*/
}

char * readline(){
	char *line = malloc(LINE_SIZE);
	char c;
	int ct = 0, maxsize = LINE_SIZE;
	while(1){
		c = getchar();
		if(c == EOF || c == '\n'){
			line[ct] = '\0';
			break;
			ct++;
		}
		else{
			line[ct] = c;
			ct++;
		}
		if(ct >= maxsize ){
			maxsize += LINE_SIZE;
			line = realloc(line, maxsize);
		}
	}
	return line;
}

char *remove_white_spaces(char* line){
	int n =  strlen(line);
	int p = 0,q = n-1;
	char *newline = NULL;
	while(1){
		if(p>=q){
			break;
		}
		if(line[p] != ' ' && line[q] != ' ') break;
		if(line[p] == ' ') p++;
		if(line[q] == ' ') q--;
	}
	//printf("%d %d\n",p,q );
	if(q-p >= 0){
		newline = malloc(q-p +2);
		strncpy(newline, &line[p], q-p+1);
	}
	else{
		newline = NULL;
	}
	//free(line);
	return newline;
}

//seperate redirections- given string conatining multiple inputs and outpu redirections 
//returns an array of 3 strings first is command with options, second is input redirection file and third is ouput redirection file
//to do - handle the case where there can be multiple filenames in in/out redirection string;
char** parse_redirections(char *line){
	char **cmd_and_rds = malloc(sizeof(char*) * 3);
	char *cmd = malloc(LINE_SIZE);
	char *out_r_file = NULL;
	char *in_r_file = NULL;

	// identify command
	int n = strlen(line), i, in = -1, in_d = -1, out = -1, out_d = -1, last_rd;  //in = starting index, in_d = distance, last rd = "what was last < ot >?"
	for(i = 0; i < n; i++){
		if(line[i] == '<'){
			cmd = malloc(i+2);
			strncpy(cmd, line, i);
			in = i; in_d = 0;
			last_rd = 0;
			break;
		}
		if(line[i] == '>'){
			cmd = malloc(i+2);
			strncpy(cmd, line, i);
			out = i; out_d = 0; 
			last_rd = 1;
			break;
		}

	}
	if(i >= n){
		strcpy(cmd, line);
		cmd_and_rds[0] = cmd;
		cmd_and_rds[1] = in_r_file;
		cmd_and_rds[2] = out_r_file;
		return cmd_and_rds;
	}
	// identify output and input redirection files.
	for(; i< n; i++){
		if(line[i] == '<'){
			in = i;
			in_d = 0;
			last_rd = 0;
		}
		if(line[i] == '>'){
			out = i;
			out_d = 0;
			last_rd = 1;
		}
		if(last_rd == 0){
			in_d ++;
		}
		if(last_rd == 1){
			out_d++;
		}
	}
	if(in > 0 && in_d > 0){
		in_r_file = malloc(in_d + 2);
		strncpy(in_r_file , &line[in+1], in_d - 1);	
		in_r_file = remove_white_spaces(in_r_file);
	}
	if(out > 0 && out_d > 0){
		out_r_file = malloc(out_d + 2);
		strncpy(out_r_file, &line[out+1], out_d -1);
		out_r_file = remove_white_spaces(out_r_file);	
	}
	/*printf("%s\n",cmd );
	if(in_r_file)
	printf("%s\n",in_r_file );
	if(out_r_file)
	printf("%s\n",out_r_file );*/
	cmd_and_rds[0] = cmd;
	cmd_and_rds[1] = in_r_file;
	cmd_and_rds[2] = out_r_file;
	return cmd_and_rds;
}

// seperate commands and argumentts
char** parse_command(char *line){
	//printf("%s\n",line );
	char **cmd = malloc(sizeof(char *) * NO_OF_ARGS);
	char *tmp;
	int i = 0 , maxargs = NO_OF_ARGS;
	
	tmp = strtok(line, " ");

	if(tmp != NULL){
		cmd[i] = malloc(strlen(tmp));
		strcpy(cmd[i], tmp);
		i++;
	}
	
	for( ; ;i++){
		tmp = strtok(NULL, " ");
		if(tmp == NULL)
			break;

		if(i >= NO_OF_ARGS){
			maxargs += NO_OF_ARGS;
			cmd = realloc(cmd, maxargs);
		}
		cmd[i] = malloc(strlen(tmp));
		strcpy(cmd[i], tmp);
		//printf("%s\n",cmd[i] );
	}

	if(i >= NO_OF_ARGS){
			maxargs += sizeof(char **);
			cmd = realloc(cmd, maxargs);
	}

	cmd[i] = NULL;
	return cmd;

}

char ** parse_pipes(char *line){
	char **cmd = malloc(sizeof(char *) * NO_OF_PIPES);
	char *tmp;
	int i = 0 , maxargs = NO_OF_PIPES;
	
	tmp = strtok(line, "|");

	if(tmp != NULL){
		cmd[i] = malloc(strlen(tmp));
		strcpy(cmd[i], tmp);
		i++;
	}
	
	for( ; ;i++){
		tmp = strtok(NULL, "|");
		if(tmp == NULL)
			break;

		if(i >= NO_OF_PIPES){
			maxargs += NO_OF_PIPES;
			cmd = realloc(cmd, maxargs);
		}
		cmd[i] = malloc(strlen(tmp));
		strcpy(cmd[i], tmp);
		//printf("%s\n",cmd[i] );
	}

	if(i >= NO_OF_PIPES){
		maxargs += sizeof(char **);
		cmd = realloc(cmd, maxargs);
	}

	cmd[i] = NULL;
	return cmd;
}

int main() {
	if(signal(SIGTSTP,ctrl_z_handler) == SIG_ERR)
		printf("cant catch signal\n");
	if(signal(SIGINT,ctrl_c_handler) == SIG_ERR)
		printf("cant catch signal z\n");
	int pid,i,j,p,q, wstatus;
	children.cur_running = -1;
	children.cur_stopped = -1;
	children.cur_bg = -1;
	char *cmd = NULL;
	char ** cmd_array = NULL;
	char ** rd_array = NULL;
	char ** pipe_array;
	int pipe_cnt = 0;
	int **pipes;
	while(1) {
		printf("nachiket's shell $");
		cmd = readline();
		if(strcmp(cmd,"") == 0){
			continue;
		}
		pipe_array = parse_pipes(cmd);
		i = 1;
		while(pipe_array[i]){
			//printf("%s\n",pipe_array[i] );
			i++;
		}
		pipe_cnt = i-1;
		//printf("%d\n",pipe_cnt );
		if(pipe_cnt > 0){
			//initialise 2d array of size pipe_cnt
			pipes = malloc(sizeof(int*) * pipe_cnt);
			for(i = 0; i< pipe_cnt; i++){
				pipes[i] = malloc(sizeof(int) *2);
			}

			for(i = 0; i < pipe_cnt+1; i++){
				pipe(pipes[i]);
				cmd_array = parse_command(pipe_array[i]);
				p = fork();
				if(p == 0){
					if(i == 0){
						close(1);
						dup(pipes[i][1]);
						close(pipes[i][0]);
						if(execvp(cmd_array[0], cmd_array) == -1){
							perror("one");
							exit(0);
						}
					}
					else if(i == pipe_cnt){
						close(0);
						dup(pipes[i-1][0]);
						close(pipes[i-1][1]);
						if(execvp(cmd_array[0], cmd_array) == -1){
							perror("two");
							exit(0);
						}
					}
					else{
						close(0);
						dup(pipes[i-1][0]);
						close(pipes[i-1][1]);
						close(1);
						dup(pipes[i][1]);
						close(pipes[i][0]);
						if(execvp(cmd_array[0], cmd_array) == -1){
							perror("three");
							exit(0);
						}
					}
				}
				else{
					children.cur_running += 1;
					children.running[children.cur_running] = p;
				}
			}
			waitpid(0,&wstatus,WUNTRACED);
			children.cur_running = -1;
			for(i = 0; i< pipe_cnt; i++){
				free(pipes[i]);
			}
			free(pipes);
		}
		else{
			//printf("here\n");
			rd_array = parse_redirections(cmd);
			cmd_array = parse_command(rd_array[0]);
			if(strcmp(cmd_array[0],"fg") == 0){
				printf("\n");
				if(handle_fg(cmd_array[1]))
					waitpid(0,&wstatus,WUNTRACED);	
				continue;
			}
			if(strcmp(cmd_array[0],"bg") == 0){
				if(handle_bg(cmd_array[1]))
					waitpid(0,&wstatus,WCONTINUED);	
				continue;
			}
			if(strcmp(cmd_array[0],"jobs") == 0){
				print_jobs();
				continue;
			}
			if(strcmp(cmd_array[0],"exit") == 0){
				exit(0);
			}
			p = fork();
			if(p == 0) {
				//printf("%s\n", rd_array[0]);
				if(rd_array[1]){
					//printf("inr\n");
					//printf("%s\n",rd_array[1]);
					close(0);
					int fd = open(rd_array[1], O_RDONLY);
					if(fd == -1){
						printf("%d\n",errno);
						exit(1);
					}
				}
				if(rd_array[2]){
					//printf("out_r\n");
					//printf("%s\n",rd_array[2]);
					close(1);
					int fd = open(rd_array[2], O_WRONLY | O_CREAT, 0777);
					//printf("write %d\n", fd);
					if(fd == -1){
						printf("%d\n",errno);
						exit(1);
					}

				}

				if(execvp(cmd_array[0], cmd_array) == -1){
					printf("undefined command %d\n",errno);
					exit(0);
				}
			} 
			else {
				children.cur_running += 1;
				children.running[children.cur_running] = p;
				waitpid(0,&wstatus,WUNTRACED);
				children.cur_running = -1;
			}
		}
	}
	return 0;
}
