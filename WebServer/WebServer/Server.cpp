/*
	server.cpp
	Main entrance of the WebServer.
	For Internet & World Wide Web course
	1430003013 ¡ıÿπΩ° / 1430003045 ÷”æ˚»Â
*/

#include <winsock2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "http_session.h"
#include "get_time.h"

#define DEFAULT_PORT 80
#define MAX_CONN 32


int main(void) {

	printf("Initializing variables\n");
	char recv_buf[4096] = { '\0' };
	unsigned char send_buf[100000] = { '\0' };
	unsigned char *file_buf = new unsigned char[FILE_MAX_SIZE + 1];
	char uri_buf[URI_SIZE + 1] = { '\0' };
	char *mime_type;

	int msg_len;
	int addr_len;
	int uri_status;
	int send_bytes = 0;
	int file_size = 0;
	struct sockaddr_in local, client_addr;

	SOCKET sock, msg_sock;
	WSADATA wsaData;
	
	printf("WSAStartup\n");
	if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR) {
		// stderr: standard error are printed to the screen.
		fprintf(stderr, "WSAStartup failed with error %d\n", WSAGetLastError());
		//WSACleanup function terminates use of the Windows Sockets DLL. 
		WSACleanup();
		return -1;
	}
	// Fill in the address structure
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = INADDR_ANY;
	local.sin_port = htons(DEFAULT_PORT);

	sock = socket(AF_INET, SOCK_STREAM, 0);	//TCp socket


	if (sock == INVALID_SOCKET) {
		fprintf(stderr, "socket() failed with error %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	if (bind(sock, (struct sockaddr *)&local, sizeof(local)) == SOCKET_ERROR) {
		fprintf(stderr, "bind() failed with error %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	//waiting for the connections
	if (listen(sock, 5) == SOCKET_ERROR) {
		fprintf(stderr, "listen() failed with error %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}


	printf("Waiting for the connections ........\n");

	addr_len = sizeof(client_addr);

	// Accepting multiple connections.
	int connCount = 0;
	while (connCount <= MAX_CONN) {
		connCount++;
		msg_sock = accept(sock, (struct sockaddr*)&client_addr, &addr_len);
		if (msg_sock == INVALID_SOCKET) {
			fprintf(stderr, "accept() failed with error %d\n", WSAGetLastError());
			WSACleanup();
			connCount--;
			return -1;
		}

		printf("accepted connection from %s, port %d\n",
			inet_ntoa(client_addr.sin_addr),
			htons(client_addr.sin_port));

		msg_len = recv(msg_sock, recv_buf, sizeof(recv_buf), 0);

		if (msg_len == SOCKET_ERROR) {
			fprintf(stderr, "recv() failed with error %d\n", WSAGetLastError());
			WSACleanup();
			connCount--;
			return -1;
		}

		if (msg_len == 0) {
			printf("Client closed connection, recieving no information\n");
			closesocket(msg_sock);
			connCount--;
			//return -1;
		}

		printf("Bytes Received: %d, message: %s from %s\n", msg_len, recv_buf, inet_ntoa(client_addr.sin_addr));

		memset(uri_buf, '\0', sizeof(uri_buf));
		// Get URI.
		if (get_uri(recv_buf, uri_buf) == NULL)
		{
			uri_status = URI_TOO_LONG;
			return -1;
		}
		else
		{
			printf("URI is %s\n", uri_buf);
			uri_status = get_uri_status(uri_buf);
			// Find file.
			switch (uri_status)
			{
			case FILE_OK:
				printf("File OK!\n");
				mime_type = get_mime_type(uri_buf);
				printf("Mime type is %s.\n", mime_type);
				file_size = get_file_disk(uri_buf, file_buf, mime_type);
				send_bytes = reply_normal_information(send_buf, file_buf, file_size, mime_type);
				break;
			case FILE_NOT_FOUND:
				printf("In switch on case FILE_NOT_FOUND\n");
				send_bytes = set_error_information(send_buf, FILE_NOT_FOUND);
				break;
			case FILE_FORBIDEN:
				break;
			case URI_TOO_LONG:
				break;
			default:
				break;
			}
		}

		msg_len = send(msg_sock, (char*)send_buf, sizeof(send_buf), 0);
		if (msg_len == 0) {
			printf("Client closed connection, sending no information\n");
			closesocket(msg_sock);
			connCount--;
		}
		closesocket(msg_sock);
		connCount--;
	}
	WSACleanup();
}

char *get_uri(char *req_header, char *uri_buf)
{
	int index = 0;
	while ((req_header[index] != '/') && (req_header[index] != '\0'))
	{
		index++;
	}
	int base = index;
	while (((index - base) < URI_SIZE) && (req_header[index] != ' ') && (req_header[index] != '\0'))
	{
		index++;
	}
	if ((index - base) >= URI_SIZE)
	{
		fprintf(stderr, "error: too long of uri request.\n");
		return NULL;
	}
	if ((req_header[index - 1] == '/') && (req_header[index] == ' '))
	{
		strcpy(uri_buf, "index.html");
		return uri_buf;
	}
	strncpy(uri_buf, req_header + base + 1, index - base - 1);
	printf(uri_buf);
	return uri_buf;

}

// TODO: Check file availiblity.
int get_uri_status(char *uri)
{
	// Test existance.
	if (access(uri, 0) == -1)
	{
		fprintf(stderr, "File: %s not found.\n", uri);
		return FILE_NOT_FOUND;
	}
	// Test privilege.
	if (access(uri, 2) == -1)
	{
		fprintf(stderr, "File: %s can not read.\n", uri);
		return FILE_FORBIDEN;
	}
	return FILE_OK;
}


char *get_mime_type(char *uri)
{
	int len = strlen(uri);
	int dot = len - 1;
	while (dot >= 0 && uri[dot] != '.')
	{
		dot--;
	}
	if (dot == 0)        /* if the uri begain with a dot and the dot is the last one, then it is a bad uri request,so return NULL */
	{
		return NULL;
	}
	if (dot < 0)            /* the uri is '/',so default type text/html returns */
	{
		return "text/html";
	}
	dot++;
	int type_len = len - dot;
	char *type_off = uri + dot;
	switch (type_len)
	{
	case 4:
		if (!strcmp(type_off, "html") || !strcmp(type_off, "HTML"))
		{
			return "text/html";
		}
		if (!strcmp(type_off, "jpeg") || !strcmp(type_off, "JPEG"))
		{
			return "image/jpeg";
		}
		break;
	case 3:
		if (!strcmp(type_off, "htm") || !strcmp(type_off, "HTM"))
		{
			return "text/html";
		}
		if (!strcmp(type_off, "css") || !strcmp(type_off, "CSS"))
		{
			return "text/css";
		}
		if (!strcmp(type_off, "png") || !strcmp(type_off, "PNG"))
		{
			return "image/png";
		}
		if (!strcmp(type_off, "jpg") || !strcmp(type_off, "JPG"))
		{
			return "image/jpeg";
		}
		if (!strcmp(type_off, "gif") || !strcmp(type_off, "GIF"))
		{
			return "image/gif";
		}
		if (!strcmp(type_off, "txt") || !strcmp(type_off, "TXT"))
		{
			return "text/plain";
		}
		break;
	case 2:
		if (!strcmp(type_off, "js") || !strcmp(type_off, "JS"))
		{
			return "text/javascript";
		}
		break;
	default:        /* unknown mime type or server do not support type now*/
		return "NULL";
		break;
	}

	return NULL;
}

// TODO: Open file.
// Function returns file size. File goes to file_buf.
int get_file_disk(char *uri, unsigned char *file_buf, char *type)
{
	int read_count = 0;
	FILE* fp = NULL;

	// Read text files.
	if (strstr(type, "text") != NULL)
	{
		fp = fopen(uri, "r+");
		if (fp == NULL)
			perror("File open failed.\n");
		else
			read_count = fread(file_buf, sizeof(char), (FILE_MAX_SIZE / sizeof(char) + 1), fp);
	}
	// Read binary files.
	else
	{
		fp = fopen(uri, "rb");
		if (fp == NULL)
			perror("File open failed.\n");
		read_count = fread(file_buf, 1, FILE_MAX_SIZE, fp);
	}
	if (read_count == -1)
	{
		perror("read() in get_file_disk http_session.c");
		return -1;
	}
	fclose(fp);
	return read_count;
}


int set_error_information(unsigned char *send_buf, int errorno)
{
	register int index = 0;
	register int len = 0;
	char *str = NULL;
	int htmllen = 0;
	switch (errorno)
	{

	case FILE_NOT_FOUND:
		printf("In set_error_information FILE_NOT_FOUND case\n");
		str = "HTTP/1.1 404 File Not Found\r\n";
		len = strlen(str);
		memcpy(send_buf + index, str, len);
		index += len;

		len = strlen(SERVER);
		memcpy(send_buf + index, SERVER, len);
		index += len;

		memcpy(send_buf + index, "\r\nDate:", 7);
		index += 7;

		char time_buf[TIME_BUFFER_SIZE];
		memset(time_buf, '\0', sizeof(time_buf));
		get_time_str(time_buf);
		len = strlen(time_buf);
		memcpy(send_buf + index, time_buf, len);
		index += len;

		str = "\r\nContent-Type:text/html\r\nContent-Length:";
		len = strlen(str);
		memcpy(send_buf + index, str, len);
		index += len;

		str = "\r\n\r\n404 File not found. Please check your url, and try it again!";
		len = strlen(str);
		htmllen = len;
		char num_len[5];
		memset(num_len, '\0', sizeof(num_len));
		sprintf(num_len, "%d", len);

		len = strlen(num_len);
		memcpy(send_buf + index, num_len, len);
		index += len;

		memcpy(send_buf + index, str, htmllen);
		index += htmllen;
		break;


	default:
		break;

	}
	return index;
}


int reply_normal_information(unsigned char *send_buf, unsigned char *file_buf, int file_size, char *mime_type)
{
	char *str = "HTTP/1.1 200 OK\r\nServer:Windows NT 6.2\r\nDate:";
	register int index = strlen(str);
	memcpy(send_buf, str, index);

	char time_buf[TIME_BUFFER_SIZE];
	memset(time_buf, '\0', sizeof(time_buf));
	str = get_time_str(time_buf);
	int len = strlen(time_buf);
	memcpy(send_buf + index, time_buf, len);
	index += len;

	len = strlen(ALLOW);
	memcpy(send_buf + index, ALLOW, len);
	index += len;

	memcpy(send_buf + index, "\r\nContent-Type:", 15);
	index += 15;
	len = strlen(mime_type);
	memcpy(send_buf + index, mime_type, len);
	index += strlen(mime_type);

	memcpy(send_buf + index, "\r\nContent-Length:", 17);
	index += 17;
	char num_len[8];
	memset(num_len, '\0', sizeof(num_len));
	sprintf(num_len, "%d", file_size);
	len = strlen(num_len);
	memcpy(send_buf + index, num_len, len);
	index += len;

	memcpy(send_buf + index, "\r\n\r\n", 4);
	index += 4;


	memcpy(send_buf + index, file_buf, file_size);
	index += file_size;
	return index;

}