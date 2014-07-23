#include <iostream>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <errno.h>
#include <assert.h>
#include <cctype>
#include <algorithm>
#include <vector>
#include "mysock.h"
#include "mysock_vns.h"
#ifdef LINUX
#include <unistd.h> /*getopt*/
#endif


using namespace std;

/* Struct http_request
 * -------------------
 * Data type for a client sending a request
 * to a server
 */
 

 
struct http_request {
	string method;
	string host;
	string path;
	string version;
	unsigned short int port;
	string headers;
};


/* ------- Function Prototypes ------- */
int strcasecmp(const char *s1, const char *s2);
void init_server(int &listen_sd, char *port_num, bool is_unreliable);
bool init_client(int &client_sd, http_request req);
void process_request(int conn_sd, http_request req);
bool parse_request(string line, http_request &request, int sd);
bool parse_reqline(string message, http_request &request, int sd);
bool is_valid_method(char* method, http_request &request, int sd);
bool parse_url(char* temp_url, http_request &request, int sd);
bool parse_http_version(char* temp_ver, http_request &request, int sd);
bool parse_port(http_request &request, int sd);
bool parse_entity(string &message, int content_length, http_request request, int sd);
bool parse_headers(vector<string> header_lines, int &content_length, int sd);
bool parse_status(string status);
const char* get_ip_addr(string host);
void send_status_code(string status_code, string reason , int sd);
string read_until(int sd, bool double_crlf, bool use_stcp);
string touppercase(string str);
string tolowercase(string str);
void clear_request(http_request &req);
//Wrappers for socket functions
bool accept_e(int listen_sd, struct sockaddr_in &client_addr, int &conn_sd);
bool myaccept_e(int listen_sd, struct sockaddr_in &client_addr, int &conn_sd);
bool bind_e(int  sd,  const  struct  sockaddr *my_addr, socklen_t addrlen);
bool mybind_e(int  sd,  struct  sockaddr *my_addr, socklen_t addrlen);
bool connect_e(int sd, const struct sockaddr *serv_addr, socklen_t addrlen);
bool listen_e(int listen_sd);
bool mylisten_e(int listen_sd);
void close_e(int sd);
void myclose_e(int sd);
bool socket_e(int domain, int type, int protocol, int& sd);
bool mysocket_e(int &sd, bool is_reliable);
void writen(int sd, const void *ptr, size_t n, bool use_stcp);


#define BUFF_SIZE 100 //Buffer length for network reads


/* Function: main
 * ----------------
 * Initiates the proxy server
 * Listens and accepts connections
 * Reads HTTP requests, then process
 * them if they are valid. Closes all
 * connections when its done.
 */
int main(int argc, char* argv[])
{
	int conn_sd=-10;
	int listen_sd;
	struct sockaddr_in client_addr;
	http_request req;
	
	/* NOTE: I took the following getopt code from echo_server_main.c and pasted it here */
	bool_t is_reliable = TRUE, error_flag = FALSE;
    int opt;
    char *vns_host = NULL, *vns_server = NULL, *vns_rtable = NULL;
    char *vns_logfile = NULL;
    uint16_t vns_port = (uint16_t)-1, vns_topo = (uint16_t)-1;    
    while ((opt = getopt(argc, argv, "t:v:s:p:r:l:U")) != EOF)
    {
        switch (opt)
        {
        /* STCP options */
        case 'U':
            is_reliable = FALSE;
            break;

        /* VNS options */
        case 't': vns_topo    = atoi(optarg); break;
        case 'v': vns_host    = optarg; break;
        case 's': vns_server  = optarg; break;
        case 'p': vns_port    = atoi(optarg); break;
        case 'r': vns_rtable  = optarg; break;
        case 'l': vns_logfile = optarg; break;

        case '?':
            error_flag = TRUE;
            break;
        }
    }
    
    if (error_flag || optind != argc - 1)
    {
        fprintf(stderr, "usage: %s [-U] [VNS options] proxy_port\n", argv[0]);
        fprintf(stderr, "  where VNS options are zero or more of:\n");
        fprintf(stderr, "  -t topo id\n");
        fprintf(stderr, "  -v host\n");
        fprintf(stderr, "  -s VNS server\n");
        fprintf(stderr, "  -p VNS port\n");
        fprintf(stderr, "  -r routing table\n");
        fprintf(stderr, "  -l logfile\n");
        exit(EXIT_FAILURE);
    }    
   
   char *proxy_port = NULL;
   proxy_port = argv[optind];
    /* initialise VNS (raw IP) subsystem */
    if (sr_api_init(vns_server, vns_host, vns_rtable, vns_logfile,
                    vns_port, vns_topo) < 0)
    {
        fprintf(stderr, "can't initialise VNS subsystem\n");
        exit(EXIT_FAILURE);
    }
	/* Ends here */	
	
	/* Back to my code */
	init_server(listen_sd, proxy_port, is_reliable);
	while(true){
		bool success =  myaccept_e(listen_sd, client_addr, conn_sd);
		if (success){	
			clear_request(req);
			string message = read_until(conn_sd, true, true);
			if (parse_request(message, req, conn_sd)){
				process_request(conn_sd, req);
			}
		}
		myclose_e(conn_sd);
	}
	myclose_e(listen_sd);	
}

/* Function: init_server
 * -----------------
 * Initiates the proxy's server side using the standard
 * socket algorithm for setting up a server.
 */
void init_server(int &listen_sd, char *port_num, bool is_reliable)
{
	struct sockaddr_in server_addr;
	if (!mysocket_e(listen_sd, is_reliable)){
		exit(1);	
	}
	memset(&server_addr, sizeof(server_addr), 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (port_num==NULL){
		cout<<"Please enter a port number next time"<<endl;
		exit(1);	
	}
	int port = atoi(port_num);
	if (port==0){
		cout<<"Invalid Listening port"<<endl;
		exit(1);
	}
	if (port<1024) {
		cout<<"Warning: root access required for this port"<<endl;
	}
	server_addr.sin_port = htons((unsigned short int)atoi(port_num));
	if (!mybind_e(listen_sd,(struct sockaddr *) &server_addr, sizeof(server_addr))){
		exit(1);	
	}
	if (!mylisten_e(listen_sd)) {
		exit(1);
	}
}

/* Function: send_status_code
 * -----------------
 * This function is called whenever something bad happens. The error code
 * and appropriate message is sent to the client.
 */
void send_status_code(string status_code, string reason , int sd)
{
	string status = "HTTP/1.0 " + 	status_code + " " + reason + "\r\n";
	writen(sd, status.c_str(), status.length(), true);
}

/* Function: clear_request
 * -----------------
 * Cleans an http transaction message so it can be used again.
 */
void clear_request(http_request &req)
{
	req.method = "";
	req.host = "";
	req.path ="";
	req.version ="";
	req.port = 0;
	req.headers="";
}

/* Function: read_until
 * -----------------
 * Reads the network buffer until the delimiter
 * is found and places the result in a string.
 */
string read_until(int sd, bool double_crlf, bool use_stcp)
{
	char buf[BUFF_SIZE];
	string result = "";
	ssize_t n;
	while(true) {
		if ( (double_crlf && result.find("\r\n\r\n")!=string::npos) || 
			 (!double_crlf && result.find("\r\n")!=string::npos)) {
			break;	
		}
		if (use_stcp){
			n = myread(sd, buf, BUFF_SIZE);
		}
		else {
			n = read(sd,buf,BUFF_SIZE);
		}
		for (int i=0;i<n;i++){ 
			result+=buf[i];	
		}
	}
	return result;
}

/* Function: parse_request
 * -----------------
 * Top-level function that calls many helper functions to take
 * care of the fun task of parsing an http request, determining
 * if it's valid, and making it pretty before sending it to the 
 * final server.
 */
bool parse_request(string message, http_request &request, int sd)
{
	int content_length = -1; //-1 means we did not find a content-length
	vector<string> header_lines;

	if (!parse_reqline(message, request, sd)){
		 return false;
	}
	//Break up the headers by line (CRLFs) so we can parse
	//each line individually
	string headers = message.substr(message.find("\r\n")+2);
	char *header_line = strtok((char *)headers.c_str(),"\r\n");
	while (header_line!=NULL){
		header_lines.push_back(string(header_line));	
		header_line = strtok(NULL,"\r\n");
	}
	//Parse all header lines now that they are broken up
	if (!parse_headers(header_lines, content_length, sd)){
		 return false;
	}
	//Check for an entity and parse it
	if (!parse_entity(message, content_length, request, sd)){
		 return false;
	}
	//Everything went ok. The request seems valid
	request.headers = message;
    return true;
}



/* Function: parse_headers
 * -----------------
 * Iterates through each header line and ensures certain conditions are met. 
 * For example, there should always be a colon somewhere in the line.
 * Looks out for content-length and stores it for later use.
 */
bool parse_headers(vector<string> header_lines, int &content_length, int sd)
{
	for (unsigned int i=0;i<header_lines.size();i++){
		unsigned int colon_pos = header_lines[i].find(":");
		if (colon_pos==string::npos) {
			send_status_code("400", "Could not find colon in Header Field", sd);
			return false;	
		}
		char *header_token = strtok((char *)(header_lines[i]+" ").c_str(),": \t\v\f");
		if (header_token==NULL){
			send_status_code("400", "Invalid header format", sd);
			return false;	
		}
		bool extract_length = false;
		if (strcasecmp(header_token,"content-length")==0){
			extract_length = true;
		}
		header_token = strtok(NULL,": \t\v\f");
		if (header_token==NULL){
			send_status_code("400", "Missing value for header field", sd);
			return false;
		}	
		if (extract_length) {
			if (sscanf(header_token, "%d", &content_length)!=1 ||
				content_length<0){
				send_status_code("400", "Invalid content length", sd);
		 		return false;
			}
		}
	}
	return true;
}


/* Function: parse_entity
 * -----------------
 * Looks for a body/entity.  If there is one, there is a good chance we
 * have not finished reading all of it from the network buffer.  We finish
 * reading the number of bytes as specified in the content-length.
 */
bool parse_entity(string &message, int content_length, 
				  http_request request, int sd)
{
	message = message.substr(message.find("\r\n")+2);
	if (request.method!="GET"){
		if (content_length==-1 && request.method=="POST") {
			send_status_code("400", "Missing content length for POST", sd);
			return false;	
		}
		if (content_length!=-1){
			unsigned int clrf_pos = message.find("\r\n\r\n");
			unsigned int bytes_after_crlf = message.substr(clrf_pos+4).length();
			int bytes_left_to_read = content_length - bytes_after_crlf;
			//We read too much the first time around. Chop the entity.
			if (bytes_left_to_read<0){
				message = message.substr(0, message.length()+bytes_left_to_read);
			}
			char buf[BUFF_SIZE];
			while (bytes_left_to_read>0) {
			    //We did not finish reading the entity. Continue until done.
			    ssize_t n=read(sd, buf, BUFF_SIZE);
	  		 	for (int i=0;i<n;i++){ 
	  		 		message+=buf[i];
	  		 		bytes_left_to_read--;
	  		 		if (bytes_left_to_read==0) break;		
				}
			}
		}
	}
	return true;	
}


/* Function: parse_reqline
 * -----------------
 * High level function that calls helper functions for parsing out
 * all the sections of an HTTP request line (first line):
 * Method, Host, Port(optional), Path, HTTP version 
 */
bool parse_reqline(string message, http_request &request, int sd)
{
	//Extract the request line (first line) from the entire message
	unsigned int crlf_pos = message.find("\r\n");
	if (crlf_pos==string::npos || crlf_pos<1){
		 send_status_code("400", "Bad Request", sd);
		 return false;
	}
	string reqline = message.substr(0,crlf_pos);

	//Parse the Method call
	char *method = strtok ((char *)reqline.c_str()," \t");
	if (!is_valid_method(method, request, sd)){
		 return false;
	}	
	//Parse URL, (host and path)
	char *url = strtok(NULL, " \t");	
	if (!parse_url(url, request, sd)){
		 return false;
	}
	
	//Parse Port
	if (!parse_port(request, sd)){
		 return false;
	}
	
	//Parse HTTP version
	char* temp_ver = strtok(NULL," \t");
	if (!parse_http_version(temp_ver, request, sd)){
		 return false;
	}
       
	return true;
}

/* Function: is_valid_method
 * -----------------
 * Ensures that the header contains a valid method as allowed
 * by HTTP/1.0: GET, HEAD, or POST.  Corrects for lower case
 * values before sending to server.
 */
bool is_valid_method(char* method, http_request &request, int sd)
{
	if (method==NULL){
		 send_status_code("400", "Bad Request", sd);
		 return false;
	}
	if (strcasecmp(method,"GET")==0){	
		request.method = "GET";
	}
	else if (strcasecmp(method,"HEAD")==0){
		request.method = "HEAD";	
	}
	else if (strcasecmp(method,"POST")==0){
		request.method = "POST";	
	}
	else {
		 send_status_code("400", "Not Implemented", sd);
		 return false;
	}
	return true;
}


/* Function: parse_url
 * -----------------
 *  Determines whether we have a valid HTTP URL or not. As defined by the RFC,
 * there has to be an "http://" at the start of the URL. Makes the URL
 * pretty by doing things like making the host lowercase. (Yes, I saw
 * the WWW.microsoft.com in the grading script)
 */
bool parse_url(char* temp_url, http_request &request, int sd)
{
	if (temp_url==NULL){
		 send_status_code("400", "Bad URL Format", sd);
		 return false;
	}
	string url = string(temp_url);
	unsigned int pos1 = url.find("//");
	if (pos1==string::npos || (strcasecmp("http:", url.substr(0,pos1).c_str()))){
		 send_status_code("400", "Bad URL Format", sd);
		 return false;
	}
	unsigned int pos2 = url.find("/",pos1+2);
	if (pos2==string::npos){ //We didn't find a path.  Enter the default path: /
		 request.host = tolowercase(url.substr(pos1+2)); 
		 request.path = "/";
	}
	else{ //we found a path, parse it out
		 request.host = tolowercase(url.substr(pos1+2,pos2-pos1-2)); 
		 request.path = url.substr(pos2);
	}
	return true;	
	
}

/* Function: parse_port
 * -----------------
 *  The client has an option of talking to a server through a different
 *  port other than port 80 (though port 80 is used for HTTP). If the 
 *  client forces a port number, this function ensures it is properly 
 *  formated.
 */
bool parse_port(http_request &request, int sd)
{
	unsigned int port_pos = request.host.find(":");
	if (port_pos==string::npos) {
		request.port = 80; //default port
	}
	else {
		const char *port_num = request.host.substr(port_pos+1).c_str();
		int temp_port;
		if (sscanf(port_num, "%d", &temp_port)!=1){
			 send_status_code("400", "Invalid port format", sd);
			 return false;
		}
		else {
			request.port = (unsigned short)temp_port;	
		}
		request.host = request.host.substr(0,port_pos);	
	}
	return true;
}

/* Function: parse_http_version
 * -----------------
 *  This function ensures the client entered a properly formatted
 *  http version
 */
bool parse_http_version(char* temp_ver, http_request &request, int sd)
{
	if (temp_ver==NULL || (strcasecmp(temp_ver,"HTTP/1.0")!=0 && 
		strcasecmp(temp_ver,"HTTP/1.1")!=0)){
			  send_status_code("400", "Bad HTTP Version", sd);
			  return false;
		}
	request.version = touppercase(string(temp_ver)); 
	return true; 
}

/* Function: touppercase
 * -----------------
 *  Converts a C++ string to uppercase
 */
string touppercase(string str)
{
	string result="";
	for (unsigned int i=0;i<str.length();i++){
		result+= toupper(str[i]);
	}	
	return result;
}

/* Function: tolowercase
 * -----------------
 *  Converts a C++ string to lowercase
 */
string tolowercase(string str)
{
	string result="";
	for (unsigned int i=0;i<str.length();i++){
		result+= tolower(str[i]);
	}	
	return result;
}

/* Function: process_request
 * -----------------
 *  If we got here it means we've got a valid http request
 *  ready to be processed.  This function establishes the connection
 *  with the "actual" server, sends the request, and forwards the
 *  data to the client.
 */
void process_request(int conn_sd, http_request req)
{
	int client_sd;
	if (!init_client(client_sd, req)){
		send_status_code("400", "Could not find page", conn_sd);
		return;	
	}	
	string request = req.method + " " + req.path + " " + req.version + "\r\n";
	writen(client_sd, request.c_str(), request.length(), false);
	writen(client_sd, req.headers.c_str(), req.headers.length(), false);
	ssize_t n;
	char buf[BUFF_SIZE];
	string response = read_until(client_sd,false, false);
	string status = response.substr(0,response.find("\r\n"));
	if (!parse_status(status)){
		send_status_code("502", "Bad Gateway", conn_sd);	
	}
	else {
		writen(conn_sd, response.c_str(), response.length(), true);
		while ((n=read(client_sd, buf, BUFF_SIZE))>0){
			 writen(conn_sd,buf,n, true);
		}
	}
}

/* Function: parse_status
 * -----------------
 *  This function determines whether the status line
 *  received from the server is properly formatted.
 */
bool parse_status(string status)
{
	string status_copy = status + " ";
	char* status_token = strtok((char *)status_copy.c_str(), " \t");
	if (status_token==NULL){
		return false;	
	}
	if (strcasecmp(status_token, "HTTP/1.0")!=0 && 
		strcasecmp(status_token, "HTTP/1.1")!=0) {
		return false;		
	}
	status_token = strtok(NULL, " \t");
	if (status_token==NULL){
		return false;	
	}
    int code;
    if (sscanf(status_token, "%d", &code)!=1){
    	return false;	
    }
    status_token = strtok(NULL, " \t");
	if (status_token==NULL){
		return false;	
	}
	return true;	
}

/* Function: write_n
 * -----------------
 *  Wrapper function to ensure n bytes
 *  are written to the network buffer
 */
void writen(int sd, const void *ptr, size_t n, bool use_stcp)
{
	size_t bytes_left = n;
	int bytes_written;
	while (true){
		if (use_stcp){
			bytes_written = mywrite(sd,ptr,bytes_left);
		} else {
			bytes_written = write(sd,ptr,bytes_left);
		}
		if (bytes_written<0){
			perror("Error during write");	
		}
		else {
			bytes_left-=bytes_written;
			if (bytes_left<=0) break;
			ptr = (char *)ptr + bytes_written;
		}
	}
}




/* Function: init_client
 * -----------------
 *  This function establishes the connection with the real
 *  server (making the proxy a client). It uses the information
 *  we have received from the user's http request.
 */
bool init_client(int &client_sd, http_request req)
{
	struct sockaddr_in server_addr;
	if (!socket_e(AF_INET, SOCK_STREAM, 0, client_sd)){
		return false;	
	}
	memset(&server_addr, sizeof(server_addr), 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(req.port);
	const char *addr = get_ip_addr(req.host);	
	if (addr==NULL){
		return false;
	}	
	inet_pton(AF_INET, addr, &server_addr.sin_addr);
	if (!connect_e(client_sd, (struct sockaddr *)&server_addr, 
		sizeof(server_addr))) {
		return false;
	}
	return true;
}


/* Function: get_ip_addr
 * -----------------
 *  Determines whether the user passed an IP address or a host name meant
 *  for DNS look up.  Returns the IP address in either case.
 */
const char* get_ip_addr(string host)
{
	int ips[4];
	if (sscanf(host.c_str(),"%d.%d.%d.%d",&ips[0],&ips[1],&ips[2],&ips[3])!=4){
		struct hostent *he = gethostbyname(host.c_str());
		if (he==NULL) {
   			/* Changed this from herror to allow compilation in elaine */
   			perror("Error during DNS lookup");
   			return NULL;
		}
		return inet_ntoa(*(struct in_addr *)he->h_addr);	
	}
	else{
	    return host.c_str();
	}
}

/* Function: accept_e
 * -----------------
 * Wrapper function for socket's accept
 * Terminates unless error is EINTR as suggested by the Unix
 * Network Programming book
 */
bool accept_e(int listen_sd, struct sockaddr_in &client_addr, int &conn_sd)
{
	socklen_t client_length = sizeof(client_addr);
	if ( (conn_sd = accept(listen_sd, (struct sockaddr *) &client_addr, 
		  &client_length)) < 0) {
		if (errno != EINTR){
			perror("Fatal error while accepting connection");			
		}			
		return false;
	}
	return true;
} 

/* Function: listen_e
 * -----------------
 * Error wrapper for listen
 */
bool listen_e(int listen_sd)
{
	if (listen(listen_sd,SOMAXCONN)<0) {
		perror("Fatal error while listening for connection");
		return false;	
	}
	return true;
}

/* Function: bind_e
 * -----------------
 * Error wrapper for bind
 */
bool  bind_e(int  sd,  const  struct  sockaddr *my_addr, socklen_t addrlen)
{
	if (bind(sd,my_addr,addrlen)<0){
		perror("Fatal error while binding");
		return false;
	}	
	return true;
}

/* Function: connect_e
 * -----------------
 * Error wrapper for connect
 */
bool connect_e(int sd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
	if (connect(sd,serv_addr,addrlen)<0){
		perror("Fatal error during connect");
		return false;
	}
	return true;	
}

/* Function: close_e
 * -----------------
 * Error wrapper for close
 */
void close_e(int sd)
{
	if (close(sd)<0) {
		perror("Error while closing socket connection");
	}
}

/* Function: socket_e
 * -----------------
 * Error wrapper for socket
 */
bool socket_e(int domain, int type, int protocol, int &sd)
{
	if ((sd=socket(domain,type,protocol))<0){
		perror("Error establishing socket");
		return false;
	}
	return true;
}


/* Function: accept_e
 * -----------------
 * Wrapper function for socket's accept
 * Terminates unless error is EINTR as suggested by the Unix
 * Network Programming book
 */
bool myaccept_e(int listen_sd, struct sockaddr_in &client_addr, int &conn_sd)
{
	int client_length = sizeof(client_addr);
	if ( (conn_sd = myaccept(listen_sd, (struct sockaddr *) &client_addr, 
		 &client_length)) < 0) {
		if (errno != EINTR){
			perror("Fatal error while accepting connection");			
		}			
		return false;
	}
	return true;
} 

/* Function: listen_e
 * -----------------
 * Error wrapper for listen
 */
bool mylisten_e(int listen_sd)
{
	if (mylisten(listen_sd,SOMAXCONN)<0) {
		perror("Fatal error while listening for connection");
		return false;	
	}
	return true;
}

/* Function: bind_e
 * -----------------
 * Error wrapper for bind
 */
bool  mybind_e(int  sd,  struct  sockaddr *my_addr, socklen_t addrlen)
{
	if (mybind(sd,my_addr,addrlen)<0){
		perror("Fatal error while binding");
		return false;
	}	
	return true;
}

/* Function: connect_e
 * -----------------
 * Error wrapper for connect
 */
bool myconnect_e(int sd, const struct sockaddr *serv_addr, socklen_t addrlen)
{
	if (connect(sd,serv_addr,addrlen)<0){
		perror("Fatal error during connect");
		return false;
	}
	return true;	
}

/* Function: close_e
 * -----------------
 * Error wrapper for close
 */
void myclose_e(int sd)
{
	if (myclose(sd)<0) {
		perror("Error while closing socket connection");
	}
}

/* Function: socket_e
 * -----------------
 * Error wrapper for socket
 */
bool mysocket_e(int &sd, bool is_reliable)
{
	if ((sd=mysocket(is_reliable))<0){
		perror("Error establishing socket");
		return false;
	}
	return true;
}


/* Function: strcasecmp
 * -------------------------
 * This function is normally defined in string.h library
 * but CYGWIN would not find it so I have it here.
 */
int strcasecmp(const char *s1, const char *s2)
{
    while (toupper(*s1) == toupper(*s2++))
	if (*s1++ == '\0')
	    return(0);
    return(toupper(*s1) - toupper(*--s2));
}

