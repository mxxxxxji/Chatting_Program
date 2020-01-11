//client
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>    // client side program
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h> 

#define BUF_SIZE 100
#define NAME_SIZE 20
#define FILE_SIZE 500000
unsigned WINAPI SendMsg(void * arg);
unsigned WINAPI RecvMsg(void * arg);
void ErrorHandling(char * msg);
void err_quit(const char *msg);
void err_display(const char *msg);
void sendFile(SOCKET sock);
void recvFile(SOCKET sock);
int recvn(SOCKET s, char *buf, int len, int flags);
char name[NAME_SIZE] = "[DEFAULT]";
char msg[BUF_SIZE];
char fileinfo[BUF_SIZE];
int main(int argc, char *argv[])
{
	WSADATA wsaData;
	SOCKET hSock;
	SOCKADDR_IN servAdr;
	HANDLE hSndThread, hRcvThread;
	if (argc != 4) {
		printf("Usage : %s <IP> <port> <name>\n", argv[0]);
		exit(1);
	}
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");

	sprintf(name, "[%s]", argv[3]);
	hSock = socket(PF_INET, SOCK_STREAM, 0);
	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = inet_addr(argv[1]);
	servAdr.sin_port = htons(atoi(argv[2]));

	if (connect(hSock, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
		ErrorHandling("connect() error");

	hSndThread =
		(HANDLE)_beginthreadex(NULL, 0, SendMsg, (void*)&hSock, 0, NULL);
	hRcvThread =
		(HANDLE)_beginthreadex(NULL, 0, RecvMsg, (void*)&hSock, 0, NULL);

	WaitForSingleObject(hSndThread, INFINITE);
	WaitForSingleObject(hRcvThread, INFINITE);
	closesocket(hSock);
	WSACleanup();

	return 0;
}

unsigned WINAPI SendMsg(void * arg)   // send thread main
{
	SOCKET hSock = *((SOCKET*)arg);
	char nameMsg[NAME_SIZE + BUF_SIZE];

	send(hSock, name, strlen(name), 0); //접속 성공 시 이름보냄
	char *temp = malloc(sizeof(char)*BUF_SIZE);

	char *buff[BUF_SIZE] = { NULL, };
	int toknum = 0;
	char *ptr;

	while (1) {
		toknum = 0;

		fgets(msg, BUF_SIZE, stdin);
		strcpy(temp, msg);


		ptr = strtok(temp, " ");
		while (ptr != NULL) {
			buff[toknum] = ptr;
			ptr = strtok(NULL, " ");
			toknum++;
		}
		buff[toknum - 1] = strtok(buff[toknum - 1], "\n");

		if (!strcmp(msg, "q\n") || !strcmp(msg, "Q\n")) {
			closesocket(hSock);
			exit(0);
		}
		if (!strcmp(buff[0], "/fileto")) {
			strcpy(fileinfo, buff[2]);
		}

		sprintf(nameMsg, "%s %s", name, msg);
		send(hSock, nameMsg, strlen(nameMsg), 0);

	}
	return 0;
}

unsigned WINAPI RecvMsg(void * arg)   // read thread main
{
	int hSock = *((SOCKET*)arg);
	char nameMsg[NAME_SIZE + BUF_SIZE];
	int strLen;
	while (1) {
		strLen = recv(hSock, nameMsg, NAME_SIZE + BUF_SIZE - 1, 0);
		if (strLen == 0)
			break;
		nameMsg[strLen] = '\0';
		if (!strcmp(nameMsg, "파일을 송신합니다")) {
			sendFile(hSock);
		}
		else if (!strcmp(nameMsg, "파일을 수신합니다")) {

			recvFile(hSock);
		}

		else
			fputs(nameMsg, stdout);
	}
	return 0;
}

void sendFile(SOCKET sock) {
	int retval;

	FILE *fp = fopen(fileinfo, "rb");

	printf("<<<<<%s>>>", fileinfo);
	if (fp == NULL) {
		perror("파일을 찾을 수 없습니다.");
		return;
	}

	// 파일 이름 보내기
	retval = send(sock, fileinfo, 256, 0);
	if (retval == SOCKET_ERROR) err_quit("send()");

	// 파일 크기 얻기
	fseek(fp, 0, SEEK_END);      // go to the end of file
	int totalbytes = ftell(fp);  // get the current position

						  // 파일 크기 보내기
	retval = send(sock, (char *)&totalbytes, sizeof(totalbytes), 0);
	if (retval == SOCKET_ERROR) err_quit("send()");

	// 파일 데이터 전송에 사용할 변수
	char buf[FILE_SIZE];
	memset(buf, 0, sizeof(buf));
	int numread;
	int numtotal = 0;

	// 파일 데이터 보내기
	rewind(fp);
	while (1) {
		numread = fread(buf, 1, FILE_SIZE, fp);
		if (numread > 0) {
			retval = send(sock, buf, numread, 0);
			if (retval == SOCKET_ERROR) {
				err_display("send()");
				break;
			}
			numtotal += numread;
		}
		else if (numread == 0 && numtotal == totalbytes) {
			printf("Complete to Send to server.(%d byte)\n", numtotal);
			break;
		}
		else {
			perror("파일 입출력 오류");
			break;
		}
	}

	fclose(fp);

	return;
}
void recvFile(SOCKET sock) {
	int retval;

	char buf[FILE_SIZE];
	memset(buf, 0, sizeof(buf));

	char filename[256];
	memset(filename, 0, sizeof(filename));


	retval = recvn(sock, filename, 256, 0);

	if (retval == SOCKET_ERROR) {
		err_display("파일 이름 recv()");
		closesocket(sock);
	}
	printf("file name: %s\n", filename);

	// 파일 크기 받기
	int totalbytes;
	retval = recvn(sock, (char *)&totalbytes, sizeof(totalbytes), 0);
	if (retval == SOCKET_ERROR) {
		err_display("파일 사이즈 recv()");
		closesocket(sock);
	}
	printf("file size: %d\n", totalbytes);

	FILE *fp = fopen(filename, "wb");
	if (fp == NULL) {
		perror("파일 입출력 오류");
		closesocket(sock);
	}

	// 파일 데이터 받기
	int numtotal = 0;
	while (1) {
		retval = recvn(sock, buf, totalbytes, 0);

		if (retval == SOCKET_ERROR) {
			err_display("데이터 recv()");
			break;
		}
		else if (retval == 0)
			break;
		else {
			fwrite(buf, 1, retval, fp);
			if (ferror(fp)) {
				perror("파일 입출력 오류");
				break;
			}
			numtotal += retval;
			break;
		}
	}
	fclose(fp);

	// 전송 결과 출력
	if (numtotal == totalbytes)
		printf("Complete to Receive File!(%d byte)\n", totalbytes);
	else
		printf("Failed to Receve File!\n");
}
// 사용자 정의 데이터 수신 함수
int recvn(SOCKET s, char *buf, int len, int flags)
{
	int received;
	char *ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}
	return (len - left);
}
void ErrorHandling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
// 소켓 함수 오류 출력 후 종료
void err_quit(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(-1);
}
// 소켓 함수 오류 출력
void err_display(const char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (LPCTSTR)lpMsgBuf);
	LocalFree(lpMsgBuf);
}