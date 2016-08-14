#ifndef IOCP
#define IOCP

#include <assert.h>
#include <time.h>  
#include <string.h>  
#include <process.h>
#include <Winsock2.h>
#include <Windows.h>
#include <mswsock.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <list>
#include <array>
#include <vector>
#include <hash_map>
#include <map>

#pragma comment(lib, "ws2_32.lib") 
#pragma comment(lib, "mswsock.lib") 

#ifndef Log
#define Log(format,...) {fprintf( stdout , (format) , __VA_ARGS__);}
#else
void log(char * str, ... ){}
#define Log(format,...) {log( NULL , __VA_ARGS__);}
#endif

#ifndef RELEASE
// �ͷ�ָ���
#define RELEASE(X) {if(X!=NULL){delete X;X=NULL;}}
#endif

#ifndef RELEASE_HANDLE
// �ͷž����
#define RELEASE_HANDLE(x){if(x != NULL && x!=INVALID_HANDLE_VALUE){ CloseHandle(x);x = NULL;}}
#endif

#ifndef RELEASE_SOCKET
// �ͷ�Socket��
#define RELEASE_SOCKET(x){if(x !=INVALID_SOCKET) { closesocket(x);x=INVALID_SOCKET;}}
#endif

#define BUFLEN 8192
#define DEFAULT_PORT 6001

enum OperationType {
	SIG_NULL,
	SIG_ACCEPT,
	SIG_RECV,
	SIG_SEND,
	SIG_EXIT
};

class IOCPContext{
public:
	OVERLAPPED overlapped;
	SOCKET sockAccept;
	WSABUF wsabuf;
	OperationType operation;

	IOCPContext(){
		ZeroMemory(&overlapped, sizeof(OVERLAPPED));
		sockAccept = INVALID_SOCKET;
		wsabuf.len = BUFLEN;
		wsabuf.buf = new char[BUFLEN];
		operation = SIG_NULL;
	}

	~IOCPContext(){
		delete[] wsabuf.buf;
		RELEASE_SOCKET(sockAccept);
	}

	void ResetBuffer(void){
		ZeroMemory(this->wsabuf.buf, BUFLEN);
	}
};

class SocketContext{
public:
	SOCKET socket;
	SOCKADDR_IN sockAddr;
	std::list<IOCPContext*> contextList;

	SocketContext(){
		socket = INVALID_SOCKET;
		ZeroMemory(&sockAddr, sizeof(SOCKADDR_IN));
	}

	~SocketContext(){
		std::list<IOCPContext*>::iterator end = contextList.end();
		
		for (std::list<IOCPContext*>::iterator it = contextList.begin(); it != end; it++){
			RELEASE(*it);
		}
		contextList.clear();

		RELEASE_SOCKET(socket);
	}

	IOCPContext * CreateIOCPContext(void){
		IOCPContext * ret = new IOCPContext;
		contextList.push_back(ret);
		return ret;
	}

	void RemoveContext(IOCPContext * ptr){
		std::list<IOCPContext*>::iterator end = contextList.end();

		for (std::list<IOCPContext*>::iterator it = contextList.begin(); it != end; it++){
			if (ptr == *it){
				RELEASE(ptr);
				contextList.erase(it);
				break;
			}
		}
	}
};

class IOCPServer{
private:
	// ��ɶ˿ڵľ��
	HANDLE hIOCP;
	// ���ڼ�����Socket��Context��Ϣ
	SocketContext * listenContext;
	std::list<HANDLE> threadList;

	//for accept
	CRITICAL_SECTION lock;
	std::list<SocketContext*> socketList;

	//AcceptEx �ĺ���ָ��
	LPFN_ACCEPTEX lpfnAcceptEx;
	//GetAcceptExSockaddrs �ĺ���ָ��
	LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockAddrs;

protected:
	//get address of acceptex and getacceptexsockaddrs
	bool GetFunctionAddress(void){
		GUID guid = WSAID_ACCEPTEX;        // GUID�������ʶ��AcceptEx���������
		DWORD dwBytes = 0;

		if (listenContext == NULL){
			Log("[server]:null ptr.\n");
			return false;
		}

		int iResult = WSAIoctl(listenContext->socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid,
			sizeof(guid), &lpfnAcceptEx, sizeof(lpfnAcceptEx), &dwBytes,
			NULL, NULL);
		if (iResult == INVALID_SOCKET){
			Log("[server]:init accept ex error.\n");
			return false;
		}
		else{
			Log("[server]:init accept ex success.\n");
		}

		guid = WSAID_GETACCEPTEXSOCKADDRS;
		iResult = WSAIoctl(listenContext->socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid,
			sizeof(guid), &lpfnGetAcceptExSockAddrs, sizeof(lpfnGetAcceptExSockAddrs), &dwBytes,
			NULL, NULL);
		if (iResult == INVALID_SOCKET){
			Log("[server]:init sockaddr ex error.\n");
			return false;
		}
		else{
			Log("[server]:init sockaddr ex success.\n");
		}
		return true;
	}

	bool InitServer(void){
		listenContext = new SocketContext();
		listenContext->socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
		listenContext->sockAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
		listenContext->sockAddr.sin_family = AF_INET;
		listenContext->sockAddr.sin_port = htons(6001);
		
		hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
		if (hIOCP == NULL){
			Log("[IOCP]:cannot create iocp.\n");
			return false;
		}
		else{
			Log("[server]:create iocp success.\n");
		}

		if (CreateIoCompletionPort((HANDLE)listenContext->socket, hIOCP, (ULONG_PTR)&listenContext, 0) == NULL){
			Log("[server]:bind iocp error.\n");
			return false;
		}
		else{
			Log("[server]:bind iocp success.\n");
		}

		if (bind(listenContext->socket, (SOCKADDR*)&listenContext->sockAddr, sizeof(SOCKADDR)) == SOCKET_ERROR){
			Log("[server]:bind error.\n");
			return false;
		}
		else{
			Log("[server]:bind success.\n");
		}

		if (listen(listenContext->socket, SOMAXCONN) == SOCKET_ERROR){
			Log("[server]:listen error.\n");
			return false;
		}
		else{
			Log("[server]:listen success.\n");
		}

		if (InitializeCriticalSectionAndSpinCount(&lock, 4000) == FALSE){
			Log("[server]:init cs error.\n");
			return false;
		}
		else{
			Log("[server]:init cs success.\n");
		}

		if (GetFunctionAddress() == false){
			Log("[server]:get function address failed.\n");
			return false;
		}
		else{
			Log("[server]:get function address success.\n");
		}

		return true;
	}

	struct _WT_PARAM{
		HANDLE handle;
		IOCPServer * pServer;
		int index;
	};

	int CreateWorkThreads(void){
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		int numberOfProcessors = si.dwNumberOfProcessors;

		for (int temp_i = 0; temp_i < numberOfProcessors * 2 ; ++temp_i){
			_WT_PARAM * ptr = new _WT_PARAM;
			ptr->handle = hIOCP;
			ptr->index = temp_i;
			ptr->pServer = this;
			threadList.push_back((HANDLE)_beginthreadex(NULL, 0, WorkThread, ptr, 0, NULL));
		}

		return numberOfProcessors;
	}

	bool PostAccept(IOCPContext * pSC);

	bool DoAccept(SocketContext * pSC, IOCPContext * pIC);

	bool PostRecv(IOCPContext * pSC);

	bool DoRecv(SocketContext * pSC, IOCPContext * pIC);

	bool PostSend();//todo

	bool DoSend();//todo

	bool IsValidOperaton(OperationType t){
		if (t >= 0 && t <= 4){
			return true;
		}
		else{
			return false;
		}
	}

	// �жϿͻ���Socket�Ƿ��Ѿ��Ͽ���������һ����Ч��Socket��Ͷ��WSARecv����������쳣
	// ʹ�õķ����ǳ��������socket�������ݣ��ж����socket���õķ���ֵ
	// ��Ϊ����ͻ��������쳣�Ͽ�(����ͻ��˱������߰ε����ߵ�)��ʱ�򣬷����������޷��յ��ͻ��˶Ͽ���֪ͨ��
	bool IsSocketAlive(SOCKET s){
		int nByteSent = send(s, "", 0, 0);
		if (nByteSent == -1) 
			return false;
		return true;
	}

	void AddSocketContext(SocketContext * pSC){
		EnterCriticalSection(&lock);
		socketList.push_back(pSC);
		LeaveCriticalSection(&lock);
	}

	void RemoveSocketContext(SocketContext * pSC){
		EnterCriticalSection(&lock);
		socketList.remove(pSC);
		RELEASE(pSC);
		LeaveCriticalSection(&lock);
	}

	// �������̣߳�  ΪIOCP�������Ĺ������߳�
	// Ҳ����ÿ����ɶ˿��ϳ�����������ݰ����ͽ�֮ȡ�������д�����߳�
	static unsigned int __stdcall WorkThread(void * ptr);

public:
	IOCPServer(){
		listenContext = NULL;
		hIOCP = INVALID_HANDLE_VALUE;
	}

	~IOCPServer(){

	}

	// ����������
	int Start(void){
		if (InitServer() == false){
			Log("[server]:cannot start.\n");
			return false;
		}
		else{
			Log("[server]:server init success.\n");
		}

		int num = CreateWorkThreads();
		//post accept
		for (int i = 0; i < num ; ++i){
			IOCPContext * pIC = listenContext->CreateIOCPContext();
			if (PostAccept(pIC)){
				
			}
			else{
				Log("[server]:post error in thread %d\n", i);
				listenContext->RemoveContext(pIC);
			}
		}

		while (true){
			Sleep(100);
		}

		return 0;
	}

	//	ֹͣ������
	void Stop(void){

		std::list<HANDLE>::iterator it = threadList.begin();
		for (int i = 0; i < threadList.size(); ++i){
			IOCPContext * ptr = new IOCPContext;
			ptr->operation = SIG_EXIT;
			PostQueuedCompletionStatus(hIOCP, 1, NULL, (LPOVERLAPPED)ptr);
			CloseHandle(*it);
			++it;
		}

		Sleep(100);

		EnterCriticalSection(&lock);
		std::list<IOCPContext*>::iterator it2 = listenContext->contextList.begin();
		while (it2 != listenContext->contextList.end()){
			RELEASE(*it);
			it2++;
		}
		listenContext->contextList.clear();
		LeaveCriticalSection(&lock);

		RELEASE_SOCKET(listenContext->socket);
		RELEASE(listenContext);
		threadList.clear();
		CloseHandle(hIOCP);
		DeleteCriticalSection(&lock);
		Log("[server]:server exit success");
	}

	// ����Socket��
	bool LoadSocketLib(void){
		{
			WSADATA wsaData;
			if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
				Log("[IOCP]:cannot start wsa .\n");
				return false;
			}

			if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2){
				WSACleanup();
				Log("[IOCP]:fuck.\n");
				return false;
			}
			return true;
		}
	}

	// ж��Socket�⣬��������
	void UnloadSocketLib(void){
		WSACleanup();
	}

	
};

#endif // !IOCP
