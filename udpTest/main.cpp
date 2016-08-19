#include <stdlib.h>
#include <iostream>
#include <cstdio>
#include <Winsock2.h>
#include <Windows.h>
#include <process.h>
#include <time.h>
#include <list>

#pragma comment(lib, "ws2_32.lib") 

int main(void) {
	WORD socketVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(socketVersion, &wsaData) != 0)
	{
		return 0;
	}
	SOCKET sclient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(6001);
	sin.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	int len = sizeof(sin);

	char ch;
	puts("continue?y/n:");
	scanf("%c", &ch);
	while (getchar() != '\n') {
		continue;
	}

	while (ch != 'N' && ch != 'n') {
		char * sendData = "hello server!\n";
		sendto(sclient, sendData, strlen(sendData), 0, (sockaddr *)&sin, len);
		puts("continue?y/n:");

		scanf("%c", &ch);
		while (getchar() != '\n') {
			continue;
		}
	}

	//char recvData[255];
	//int ret = recvfrom(sclient, recvData, 255, 0, (sockaddr *)&sin, &len);
	//if (ret > 0)
	//{
	//	recvData[ret] = 0x00;
	//	printf(recvData);
	//}

	closesocket(sclient);
	WSACleanup();
	return 0;
}