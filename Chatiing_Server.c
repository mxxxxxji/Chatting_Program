//server
#define _CRT_SECURE_NO_WARNINGS
#define WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <process.h> 

#define BUF_SIZE 100
#define MAX_CLNT 256
#define NAMESIZE 100
#define FILE_SIZE 50000

unsigned WINAPI HandleClnt(void * arg);
void SendMsg(char * msg, int len);
void ErrorHandling(char * msg);
void SendWhisper(SOCKET hClntSock, char* s_name, char* name, char* msg); //
void SendList(SOCKET hClntSock);
void SendInAndOut(char * name, int io); //
void err_quit(const char *msg);
void err_display(const char *msg);
void sendFile(SOCKET sock);
void recvFile(SOCKET sock);
int recvn(SOCKET s, char *buf, int len, int flags);
int clntCnt = 0;
char fileinfo[BUF_SIZE];
typedef struct Client {
	SOCKET Socks;
	IN_ADDR Addr;
	char Name[BUF_SIZE];
}Client;
Client clnt[MAX_CLNT];
HANDLE hMutex;
char fileinfo[BUF_SIZE];

int main(int argc, char *argv[])
{
	WSADATA wsaData;
	SOCKET hServSock, hClntSock;
	SOCKADDR_IN servAdr, clntAdr;
	int clntAdrSz;
	HANDLE  hThread;
	int strLen = 0;

	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		ErrorHandling("WSAStartup() error!");

	hMutex = CreateMutex(NULL, FALSE, NULL);
	hServSock = socket(PF_INET, SOCK_STREAM, 0);

	memset(&servAdr, 0, sizeof(servAdr));
	servAdr.sin_family = AF_INET;
	servAdr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAdr.sin_port = htons(atoi(argv[1]));

	if (bind(hServSock, (SOCKADDR*)&servAdr, sizeof(servAdr)) == SOCKET_ERROR)
		ErrorHandling("bind() error");
	if (listen(hServSock, 5) == SOCKET_ERROR)
		ErrorHandling("listen() error");

	while (1) {
		clntAdrSz = sizeof(clntAdr);
		hClntSock = accept(hServSock, (SOCKADDR*)&clntAdr, &clntAdrSz);

		char *name = malloc(sizeof(char)*BUF_SIZE);

		WaitForSingleObject(hMutex, INFINITE);
		clntCnt++;
		clnt[clntCnt].Socks = hClntSock;
		clnt[clntCnt].Addr = clntAdr.sin_addr;

		ReleaseMutex(hMutex);

		hThread = (HANDLE)_beginthreadex(NULL, 0, HandleClnt,
			(void*)&hClntSock, 0, NULL);
		printf("Connected client IP: %s \n", inet_ntoa(clntAdr.sin_addr));
	}
	closesocket(hServSock);
	WSACleanup();
	return 0;
}
unsigned WINAPI HandleClnt(void * arg)
{
	SOCKET hClntSock = *((SOCKET*)arg);
	int strLen = 0, i;
	char msg[BUF_SIZE];

	while (1) {
		strLen = recv(hClntSock, msg, sizeof(msg), 0);
		if (strLen == 0 || strLen == -1)
			break;
		char *temp = malloc(sizeof(char)*BUF_SIZE);
		char temp2[BUF_SIZE];

		char *msg_tok[BUF_SIZE] = { NULL }; 
		int toknum = 0;
		int check = 0;

		msg[strLen] = '\0';
		strcpy(temp, msg);

		char *ptr = strtok(temp, " ");
		while (ptr != NULL) {
			msg_tok[toknum] = ptr;
			ptr = strtok(NULL, " ");
			toknum++;
		}
		msg_tok[toknum - 1] = strtok(msg_tok[toknum - 1], "\n");


		if (toknum == 1) {
			strcpy(clnt[clntCnt].Name, msg_tok[0]);
			SendInAndOut(msg_tok[0], 1);
		}
		else {
			if (!strcmp(msg_tok[1], "/list"))
				SendList(hClntSock);
			else if (!strcmp(msg_tok[1], "/to"))
				SendWhisper(hClntSock, msg_tok[0], msg_tok[2], msg_tok[3]);
			else if (!strcmp(msg_tok[1], "/help")) {
				WaitForSingleObject(hMutex, INFINITE);

				sprintf(temp2, "*�ӼӸ� '/to [�г���] �޽���' - �ش� �г��� ����ڿ��Ը� �޽��� ����\n"
					"*������Ȯ�� '/list' - Ŭ���̾�Ʈ�� ip�ּҿ� �г��� ���\n"
					"*�������� '/fileto [�г���] [���ϸ�]' - �ش� �г��� ����ڿ��� ���� ����\n\n");
				send(hClntSock, temp2, strlen(temp2), 0);

				ReleaseMutex(hMutex);
			}
			else if (!strcmp(msg_tok[1], "/fileto")) {
				strcpy(fileinfo, msg_tok[3]);
				check = 0;
				for (int i = 1; i <= clntCnt; i++)
				{
					if (!strcmp(clnt[i].Name, msg_tok[2])) {
						sprintf(temp2, "������ �۽��մϴ�");
						send(hClntSock, temp2, strlen(temp2), 0);
						recvFile(hClntSock);

						sprintf(temp2, "������ �����մϴ�");
						send(clnt[i].Socks, temp2, strlen(temp2), 0);
						sendFile(clnt[i].Socks);

						sprintf(temp2, "���ϼ۽��� �Ϸ��Ͽ����ϴ�\n");
						send(hClntSock, temp2, strlen(temp2), 0);

						check = 1;
					}

				}
				if (check == 0) {
					sprintf(temp2, "%s���� �������� �ƴմϴ�\n", msg_tok[2]);
					send(hClntSock, temp2, strlen(temp2), 0);
				}
			}
			else {
				SendMsg(msg, strLen);
			}
		}

	}

	WaitForSingleObject(hMutex, INFINITE);
	for (i = 1; i <= clntCnt; i++)   // remove disconnected client
	{
		if (hClntSock == clnt[i].Socks)
		{
			SendInAndOut(clnt[i].Name, 0);
			while (i < clntCnt) {
				clnt[i].Socks = clnt[i + 1].Socks;
				strcpy(clnt[i].Name, clnt[i + 1].Name);
				clnt[i].Addr = clnt[i + 1].Addr;
				i++;
			}
			break;
		}
	}
	clntCnt--;
	ReleaseMutex(hMutex);
	closesocket(hClntSock);
	return 0;
}

void SendList(SOCKET hClntSock) {
	char buff[BUF_SIZE];
	WaitForSingleObject(hMutex, INFINITE);
	for (int i = 1; i <= clntCnt; i++) {
		sprintf(buff, "name : %s, IP : %s\n", clnt[i].Name, inet_ntoa(clnt[i].Addr));
		send(hClntSock, buff, strlen(buff), 0);
	}
	ReleaseMutex(hMutex);
}
void SendMsg(char * msg, int len)   // send to all
{
	int i;
	WaitForSingleObject(hMutex, INFINITE);

	for (i = 1; i <= clntCnt; i++) {
		send(clnt[i].Socks, msg, len, 0);
	}

	ReleaseMutex(hMutex);
}

void SendInAndOut(char * name, int io) {
	int i;
	char msg[BUF_SIZE];
	if (io == 1)
		sprintf(msg, "\n%s���� �����ϼ̽��ϴ�.\n", name);
	if (io == 0)
		sprintf(msg, "\n%s���� �����ϼ̽��ϴ�.\n", name);
	WaitForSingleObject(hMutex, INFINITE);
	for (i = 1; i <= clntCnt; i++)
		send(clnt[i].Socks, msg, strlen(msg), 0);
	ReleaseMutex(hMutex);
}

void SendWhisper(SOCKET hClntSock, char* s_name, char* name, char* msg)
{
	int check = 0;
	char temp2[BUF_SIZE] = { '\0' };
	WaitForSingleObject(hMutex, INFINITE);
	for (int i = 1; i <= clntCnt; i++) {
		if (!strcmp(clnt[i].Name, name)) {
			sprintf(temp2, "%s���� �ӼӸ� : %s\n", s_name, msg);
			send(clnt[i].Socks, temp2, strlen(temp2), 0);
			check = 1;
		}
	}
	if (check == 0) {
		sprintf(temp2, "%s���� �������� �ƴմϴ�\n", name);
		send(hClntSock, temp2, strlen(temp2), 0);
	}
	ReleaseMutex(hMutex);
}
void sendFile(SOCKET sock) {
	int retval;

	FILE *fp = fopen(fileinfo, "rb");
	if (fp == NULL) {
		perror("������ ã�� �� �����ϴ�.");
		return;
	}

	// ���� �̸� ������
	retval = send(sock, fileinfo, 256, 0);
	if (retval == SOCKET_ERROR) err_quit("send()");

	// ���� ũ�� ���
	fseek(fp, 0, SEEK_END);      // go to the end of file
	int totalbytes = ftell(fp);  // get the current position

						  // ���� ũ�� ������
	retval = send(sock, (char *)&totalbytes, sizeof(totalbytes), 0);
	if (retval == SOCKET_ERROR) err_quit("send()");

	char buf[FILE_SIZE];
	memset(buf, 0, sizeof(buf));
	int numread;
	int numtotal = 0;

	// ���� ������ ������
	rewind(fp);
	while (1) {
		numread = fread(buf, 1, FILE_SIZE, fp);
		if (numread > 0) {//�о� ���� ������ ������ ����
			retval = send(sock, buf, numread, 0);
			if (retval == SOCKET_ERROR) {
				err_display("send()");
				break;
			}
			numtotal += numread;
		}
		else if (numread == 0 && numtotal == totalbytes) {
			printf("Complete to Send to client.(%d byte)\n", numtotal);
			break;
		}
		else {
			perror("���� ����� ����");
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

	;
	retval = recvn(sock, filename, 256, 0);
	if (retval == SOCKET_ERROR) {
		err_display("���� �̸� recv()");
		closesocket(sock);
	}
	printf("file name: %s\n", filename);

	// ���� ũ�� �ޱ�
	int totalbytes;
	retval = recvn(sock, (char *)&totalbytes, sizeof(totalbytes), 0);
	if (retval == SOCKET_ERROR) {
		err_display("���� ������ recv()");
		closesocket(sock);
	}
	printf("file size: %d\n", totalbytes);

	FILE *fp = fopen(filename, "wb");
	if (fp == NULL) {
		perror("���� ����� ����");
		closesocket(sock);
	}

	// ���� ������ �ޱ�
	int numtotal = 0;
	while (1) {
		retval = recvn(sock, buf, totalbytes, 0);  //�޴� ���� �����ŭ recvn

		if (retval == SOCKET_ERROR) {
			err_display("������ recv()");
			break;
		}
		else if (retval == 0)
			break;
		else {
			fwrite(buf, 1, retval, fp); //���� �����͸� ���Ͽ� �ۼ�
			if (ferror(fp)) {
				perror("���� ����� ����");
				break;
			}
			numtotal += retval;  //���� recvn �� ����
			break;
		}
	}
	fclose(fp);

	// ���� ��� ���
	if (numtotal == totalbytes)   //���� recvn ���� ���� ���� ������ ũ�� ��
		printf("Complete to Receive File!(%d byte)\n", totalbytes);
	else
		printf("Failed to Receve File!\n");
}


// ����� ���� ������ ���� �Լ�
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
// ���� �Լ� ���� ��� �� ����
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
// ���� �Լ� ���� ���
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