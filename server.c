//===========Ex3-part B HTTP-server==========//
//===========Author: Oron Sason=============//
//===========I.D: 303038129================//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <ctype.h>
#include <errno.h>
#include <sys/socket.h>
#include <dirent.h>
#include "threadpool.h"

#define FAIL -1
#define SUCCESS 0
#define IS_FILE 10
#define NO_FILE 20
#define INDEX 30
#define BAD_REQ 400
#define NOT_EXIST 404
#define NOT_SUPP 501
#define MISSING 302
#define FORBID_RESP 403
#define OK 200
#define MAXBUF	4000
#define INTERNAL_ERROR 500
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

char *build_table(int,char*,struct stat); //build the table for folders
void handle_response(int ,char*,struct stat,int); //handle in response for the request
void handle_error(int,char*,int); //handle all error response
int check_if_digit(char * ,int);// check if all three args after app are numbers
int dispatch_function(void *arg);//dispatch (from threadpool)
int parse_line(int soket,char *receive, char* path, char* protocol,char *method);// handle the first line and check validation
void handle_request(char* path,int socket); //handle with valid request
char *get_mime_type(char *name); //get the type of file
void build_html(char *type,int length_html,char *message);


int main(int argc,char *argv[])
{

  	threadpool *thread_pool;
   	int port;
	int pool_size;
	int max_num_request;
	 //check if the argc is in the right size
	if(argc!=4)
	{
		printf("usage: server <port> <pool-size> <max-number-of-request>");
		exit(EXIT_FAILURE);
	}
	//check that all  values are numbers
	if((check_if_digit(argv[1],0))<0 || (check_if_digit(argv[2],0))<0 || (check_if_digit(argv[3],0))<0)
	{
		printf("\nusage: server <port> <pool-size> <max-number-of-request>\n");
		exit(EXIT_FAILURE);
	}
	
	//convert them to int after check 
	port=atoi(argv[1]);
	pool_size=atoi(argv[2]);
	max_num_request=atoi(argv[3]);
	thread_pool=create_threadpool(pool_size);	//create the pool!!
	
	if(!(thread_pool))	//if the creation fail
	{
		printf("create_threadpool is fail\n");
		exit(EXIT_FAILURE);
	}
	
	int listen_socket,client_socket;
	struct sockaddr_in server,client;
	
	//=============create server that listen to requests==============//
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = INADDR_ANY;

	socklen_t socklen= sizeof(struct sockaddr_in);
	if ( (listen_socket = socket(AF_INET, SOCK_STREAM,  IPPROTO_TCP)) < 0 )
	{
		perror("Socket");
		destroy_threadpool(thread_pool);
		exit(EXIT_FAILURE);
	}
	if (bind(listen_socket, (struct sockaddr *) &server,
            sizeof(struct sockaddr_in))<0)
    {
       	 perror("bind");
       	 destroy_threadpool(thread_pool);
       	 exit(EXIT_FAILURE);
	}
	if (listen(listen_socket,5) <0 )
	{
		perror("socket--listen");
		destroy_threadpool(thread_pool);
		exit(EXIT_FAILURE);
	}
	int counter=0;
	//=====the requests will came in loop======//
	while(1)
	{
		bzero(&client, sizeof(client));
		client_socket = accept(listen_socket,(struct sockaddr*)&client,&socklen);
		if(client_socket<0)
		{
			destroy_threadpool(thread_pool);
			perror("accept");
			exit(EXIT_FAILURE);
		}
		//Check to see if we reached the maximum number of requests
		if(max_num_request==counter)
		{
			destroy_threadpool(thread_pool);
			close(listen_socket);
			exit(EXIT_SUCCESS);
		}
		dispatch(thread_pool,dispatch_function,(void*)&client_socket);
		counter++;
		
	}
	close(listen_socket);
	destroy_threadpool(thread_pool);	
	return EXIT_SUCCESS;

}
//private method that check if all arguments in argc is integer
int check_if_digit(char *str ,int i)
{
	for(;i<strlen(str);i++)
	if((int)str[i]<48 || (int)str[i]>57)
		return FAIL;
	return SUCCESS;
}
//here we have the reading  request  and the way of treatment
int dispatch_function(void *arg)
{
	
	char temp[1000]={0};
	char receive[MAXBUF]={0};
	int socket = *(int*)arg; 	//the socket that we are communicate with him
	int n_bytes;
	char path[MAXBUF]={0}, protocol[8] = { 0 }, method[3]={ 0 };
	
	while(1)
	{
		n_bytes = read(socket,temp,1000);
		if(	n_bytes<0)
		{
			perror("read");
    		close(socket);	
    		return FAIL;
		}	
		else if (n_bytes > 0)	//here we are keep reading 
		{
			strcat(receive,temp);
			if(strstr(temp,"\r\n")!=NULL)	//we dont need to read anymore
			break;
		}
		else if(n_bytes == 0) 
			break;
		bzero(temp,sizeof(temp));	//Reset the temporary buffer

	}
	if(strlen(receive)==0)	//empty request
	{
		close(socket);	
		return FAIL;	
	}
	//cheak validation of request
	if(parse_line(socket,receive, path, protocol,method)<0)
	{
		close(socket);	
		return FAIL;
	}
	//here we handle in the request
	handle_request(path,socket);
	close(socket);
	
	return SUCCESS;
}
int parse_line(int socket, char *receive, char* path, char* protocol,char *method)
{

	int arg_num = sscanf(receive,"%s %s %s\r\n",method,path,protocol);
	if(arg_num!=3)//bad request
	{
	    handle_error(socket,path,BAD_REQ); 
	    return FAIL;
	}
	
	if(strcmp(method,"GET")!=0)//not supported
	{
    	handle_error(socket,path,NOT_SUPP); //501 not supported 	
    	return FAIL;
	}
	if(strcmp(protocol,"HTTP/1.0")!=0 && strcmp(protocol,"HTTP/1.1")!=0)
	{
    		handle_error(socket,path,BAD_REQ); 
    		return FAIL;
	}
	return SUCCESS;
}
void handle_request(char* path,int socket)
{

	struct stat sb;
	struct dirent *dire;
	int index_flag=0;
	char path_buff[MAXBUF]={0};
	
	//==my path root is the server directory(current)==//
	
	strcpy(path_buff,path);
	bzero(path,strlen(path));
	path[0]='.';
	strcat(path,path_buff);

	//===============================================//
	
 	if (lstat(path, &sb) <0)
    {
    	if(errno==ENOENT)
    		handle_error(socket,path,NOT_EXIST); //The path does not exist 
    	else if (errno==EACCES)
			handle_error(socket,path,FORBID_RESP);//Directories in the path do not have executing permissions.
    }
    //the path is directory
	else if(S_ISDIR(sb.st_mode))
	{	
		int i=strlen(path); 
		if(path[i-1]!='/')		
    		handle_error(socket,path,MISSING);	//Missing / to path
		//the path is valid and there is a directory
		else 
		{
		    DIR *dir;
		    dir=opendir(path);
		    if(dir!=NULL)
		    {
		    	while((dire=readdir(dir))) //Goes on any contents of the folder
		    	{
		    		if(strcmp(dire->d_name,"index.html") ==0)	//if exists index.html in this folder
		    			index_flag=1;		
		    	}
		    	if(index_flag==1)
		    		handle_response(socket,path,sb,INDEX);// return the file.
		    	else			
		    		handle_response(socket,path,sb,NO_FILE);	//return the contents of the directory in the format as in dir'
		    }
		    else
		    {
		     	if(errno==ENOENT)
    				handle_error(socket,path,NOT_EXIST); //The path does not exist 
    			else if (errno==EACCES)
					handle_error(socket,path,FORBID_RESP);//Directories in the path do not have executing permissions.	
		    }
		    closedir(dir);
		}
	}
	else if(S_ISREG(sb.st_mode))//file
	{
		handle_response(socket,path,sb,IS_FILE);	//return the file, format in file file.txt		
	}
	else//not regular file
   		 	handle_error(socket,path,FORBID_RESP);  //403 ferbiden
   		 	
	return;
	
}
void handle_response(int socket ,char* path,struct stat sb,int type)
{

	char file_data[MAXBUF]; //to read data from response
	char message[MAXBUF];	//write response to socket
	time_t now;
	char timebuf[128];
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
		
	if(type==INDEX)//index.html exists in the current folder
	{
		strcat(path,"index.html");   
		if(stat(path, &sb) <0)
		{
		    handle_error(socket,path,INTERNAL_ERROR);
		}
		//=============Builde the response for socket================//
					
		sprintf(message,"HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\n"
		"Date: %s\r\nContent-Type: text/html\r\nContent-Length: %d\r\n",timebuf,(int)sb.st_size);
		now=sb.st_mtime;
		strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
		sprintf(message+strlen(message),"Last-Modified: %s\r\nConnection: close\r\n\r\n",timebuf);
		write(socket, message, strlen(message));
		//====We open index.html and we read from him to socket====//
		
		int fp = open(path,O_RDONLY);
		if(fp<0)
		{
			handle_error(socket,path,FORBID_RESP);
			return;
		}
		else
		{
			int n_bytes=read(fp,file_data,sizeof(file_data));
			if(n_bytes<0)
			{
				close(fp);
				perror("read");
				return;
			}
			while(n_bytes!=0) 
			{		
    			write(socket,file_data,n_bytes);
       			bzero(file_data,sizeof(file_data));
    			n_bytes=read(fp,file_data,sizeof(file_data));
    			
    			if(n_bytes<0)
				{
					close(fp);
					perror("read");
					return;
				}
			}
			close(fp);
		}
	}
	//==========folder==========//
	else if(type==NO_FILE)
	{
		sprintf(message,"HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: text/html\r\n",timebuf);
		
		char *html;
		html=build_table(socket,path,sb);
		if(!html)
		{
		    handle_error(socket,path,INTERNAL_ERROR);
		    return;
		}
		int len=strlen(html);
		sprintf(message+strlen(message),"Content-Length: %d\r\n",len);
		if(stat(path, &sb) <0)
		{
			free(html);
		    handle_error(socket,path,INTERNAL_ERROR);
		    return;
		}
		now=sb.st_mtime;		
		strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
		sprintf(message+strlen(message),"Last-Modified: %s\r\nConnection: close\r\n\r\n",timebuf);
		strcat(message,html);
		free(html);
		write(socket, message, strlen(message));
	}
	//==In case we have file(No the specific file index.html)==//
	else if(type==IS_FILE)
	{
		char *type;
		char file_data[MAXBUF];
		type=get_mime_type(path);
		
		//====If we support this kind of file====//
		if(type)
			sprintf(message,"HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\nContent-Type: %s\r\n",timebuf,type);
		else if (!type)
			sprintf(message,"HTTP/1.0 200 OK\r\nServer: webserver/1.0\r\nDate: %s\r\n",timebuf);
			
		if (stat(path, &sb) <0)
		{
		    handle_error(socket,path,INTERNAL_ERROR);
		}
		now=sb.st_mtime;		
		strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
		sprintf(message+strlen(message),"Content-Length: %d\r\nLast-Modified: %s\r\nConnection: close\r\n\r\n",(int)sb.st_size,timebuf);
		write(socket,message,strlen(message));

		//==read the file==//
		int fp = open(path,O_RDONLY);
		if(fp<0)
		{
			handle_error(socket,path,FORBID_RESP);
			return;
		}
		else
		{
			int write_byte, was_write;
			int n_bytes=read(fp,file_data,sizeof(file_data));
			if(n_bytes<0)
			{
				close(fp);
				perror("read");
				return;
			}
			while(n_bytes != 0) 
			{		
				was_write = n_bytes;
				while ((write_byte = write(socket, file_data, was_write)) > 0)
				{
					was_write = n_bytes - write_byte;
					if(was_write==0)//no more left to write
						break;
    			}
    			if (write_byte <0)
    			{
    			    perror("write");
    				close(fp);
				   	return;	
    			}
       			bzero(file_data,MAXBUF);
    			n_bytes=read(fp,file_data, sizeof(file_data));
    			if(n_bytes<0)
				{
				  	close(fp);
					perror("read");
					return;
				}
			}
			close(fp);
		}
	}
	return;
}
void handle_error(int socket ,char* path,int flag)
{
	int flag_302=0;	//Flag for 302 error
	time_t now;
	char timebuf[128];
	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	char *type;
	int length_html;
	char message[MAXBUF];
	if(flag==NOT_EXIST){type="404 Not Found";length_html=112;}
	else if(flag==BAD_REQ){type="400 Bad Request";length_html=113;}
	else if(flag==NOT_SUPP){type="501 Not supported";length_html=129;}
	else if(flag==MISSING){type="302 Found"; flag_302=1;length_html=123;}
	else if(flag==FORBID_RESP){type="403 Forbidden";length_html=111;}
	else if(flag==INTERNAL_ERROR){type="500 Internal Server Error";length_html=144;}
	sprintf(message,"HTTP/1.0 %s\r\nServer: webserver/1.0\r\nDate: %s\r\n",type,timebuf);
	
	if(flag_302==1)
	{
		sprintf(message+strlen(message),"Location: %s/\r\n",path+1);
	}
	sprintf(message+strlen(message),"Content-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",length_html);

		//==Build the body error==//
	
	build_html(type,length_html,message);
	write(socket, message, strlen(message));
	return;	
}
char *build_table(int socket,char* path,struct stat sb)
{
		time_t now;
		char timebuf[128];
		int num_of_items=0;
		struct dirent *dire;
		DIR *dir;
		dir=opendir(path);
		if(dir!=NULL)
		{
		  while((dire=readdir(dir)))	//count the file number in directory
		  {
		   	num_of_items++;
		  }
		}
		else {
			return NULL;
		}
		closedir(dir);
		char *message=(char*)calloc(num_of_items*500,sizeof(char));
		sprintf(message,"<HTML>\r\n<HEAD><TITLE>Index of%s</TITLE></HEAD>\r\n<BODY>\r\n"
		"<H4>Index of %s</H4>\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n",path+1,path+1);
		dir=opendir(path);
		 if(dir!=NULL)
		 {
		  	while((dire=readdir(dir)))
		   	{
		   		char full_path[MAXBUF];
				sprintf(full_path,"%s%s",path,dire->d_name);
		   	 	if (stat(full_path, &sb) <0)
		   	 	{
					closedir(dir);
		   	 		free(message);
		   	 		return NULL;
		   	 	} 	
		   		now = sb.st_mtime;
		   		strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
				//is a file
		   		if(S_ISREG(sb.st_mode))
		   		{
		    		sprintf(message+strlen(message),"<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td><td>%d</td></tr>\r\n"
		    		,dire->d_name,dire->d_name,timebuf,(int)sb.st_size);
		    	}
				//is directory
		    	else if(S_ISDIR(sb.st_mode))
		    	{
		    		sprintf(message+strlen(message),"<tr><td><A HREF=\"%s\">%s</A></td><td>%s</td><td></td></tr>\r\n",dire->d_name,dire->d_name,timebuf);
		    	}
		   	}
		 }
		 else
		 {		 		
		 	free(message);
		 	return NULL;
		 }
		 closedir(dir);
		 sprintf(message+strlen(message),"</table>\r\n<HR>\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n"
		 "</BODY></HTML>\r\n");
		 return message;
		
}
void build_html(char *type,int length_html,char *message)
{
	int i=0;
	i=atoi(type+i);
	char *content;
	if(i==NOT_EXIST)content="File not found.";
	else if(i==BAD_REQ)content="Bad Request.";
	else if(i==NOT_SUPP)content="Method is not supported.";
	else if(i==MISSING)content="Directories must end with a slash.";
	else if(i==FORBID_RESP)content="Access denied.";
	else if(i==INTERNAL_ERROR)content="Some server side error.";

	sprintf(message+strlen(message),"<HTML><HEAD><TITLE>%s</TITLE></HEAD>\r\n<BODY><H4>%s</H4>\r\n%s\r\n</BODY></HTML>\r\n",type,type,content);

	return;
	
	
}	
char *get_mime_type(char *name)
{
	char *ext = strrchr(name, '.');
	if (!ext) return NULL;
	if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return 	"text/html";
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
	if (strcmp(ext, ".gif") == 0) return "image/gif";
	if (strcmp(ext, ".png") == 0) return "image/png";
	if (strcmp(ext, ".css") == 0) return "text/css";
	if (strcmp(ext, ".au") == 0) return "audio/basic";
	if (strcmp(ext, ".wav") == 0) return "audio/wav";
	if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
	if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
	if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
		return NULL;
}

