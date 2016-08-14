//#include <time.h>  
//#include <string.h>  
//#include <process.h>
//#include <Winsock2.h>
//#include <Windows.h>
//#include <mswsock.h>
//#include <cstdio>
//#include <cstdlib>
//#include <iostream>
//#include <string>
//#include <utility>
//#include <list>
//#include <array>
//#include <vector>
//#include <hash_map>
//#include <map>
//
//#pragma comment(lib, "ws2_32.lib") 
//#pragma comment(lib, "mswsock.lib") 
//
//#ifndef Log
//#define Log(format,...) fprintf( stdout , (format) , __VA_ARGS__)
//#endif
//
//#define DEBUG
//
//using std::list;
//
//struct IOCP_CONTEXT {
//	OVERLAPPED  m_Overlapped;          // ÿһ���ص�I/O���������Ҫ��һ��                
//	SOCKET		m_sockAccept;          // ���I/O������ʹ�õ�Socket��ÿ�����ӵĶ���һ����  
//	WSABUF		m_wsaBuf;              // �洢���ݵĻ��������������ص��������ݲ����ģ�����WSABUF���滹�ὲ  
//	char		    m_szBuffer[4096];		// ��ӦWSABUF��Ļ�����  
//	int				m_OpType;               // ��־����ص�I/O��������ʲô�ģ�����Accept/Recv��  
//};
//
//struct SOCKET_CONTEXT{
//	SOCKET				m_Socket;              // ÿһ���ͻ������ӵ�Socket  
//	SOCKADDR_IN			m_ClientAddr;          // ����ͻ��˵ĵ�ַ  
//	list<IOCP_CONTEXT *> iocpContextList;
//};
//
////initialize socket library
//bool initSocket(void){
//	WSADATA wsaData;
//	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
//		Log("[IOCP]:cannot start wsa .\n");
//		return false;
//	}
//
//	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2){
//		WSACleanup();
//		Log("[IOCP]:fuck.\n");
//		return false;
//	}
//	return true;
//}
//
//bool PostAccept(IOCP_CONTEXT * pAcceptIoContext,SOCKET s)
//{
//
//	// ׼������
//	DWORD dwBytes = 0;
//	pAcceptIoContext->m_OpType = 0;
//	WSABUF *p_wbuf = &pAcceptIoContext->m_wsaBuf;
//	OVERLAPPED *p_ol = &pAcceptIoContext->m_Overlapped;
//
//	// Ϊ�Ժ�������Ŀͻ�����׼����Socket( ������봫ͳaccept�������� ) 
//	pAcceptIoContext->m_sockAccept = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
//	if (INVALID_SOCKET == pAcceptIoContext->m_sockAccept)
//	{
//		puts("shit");
//		return false;
//	}
//
//	// Ͷ��AcceptEx
//	if (FALSE == AcceptEx(s, pAcceptIoContext->m_sockAccept, p_wbuf->buf, p_wbuf->len - ((sizeof(SOCKADDR_IN)+16) * 2),
//		sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, &dwBytes, p_ol))
//	{
//		if (WSA_IO_PENDING != WSAGetLastError())
//		{
//			puts("shit");
//			return false;
//		}
//	}
//
//	return true;
//}
//
//unsigned int __stdcall WorkThread(void * ptr){
//	void * pContext = NULL;
//	OVERLAPPED * pOverLapped = NULL;
//	DWORD dwLength = 0;
//	Log("[server]:thread start.\n");
//
//	while (true){
//		int ret = GetQueuedCompletionStatus((*(HANDLE *)ptr), &dwLength, (PULONG_PTR)&pContext, &pOverLapped, INFINITE);
//		if (ret == 0 || pOverLapped == NULL){
//			//todo
//			//show errors
//			continue;
//		}
//		else{
//			IOCP_CONTEXT * pIOCPContext = CONTAINING_RECORD(pOverLapped, IOCP_CONTEXT, m_Overlapped);
//			if (dwLength == 0 || pIOCPContext->m_OpType < 0){
//				puts("end");
//			}
//			else{
//				if (pIOCPContext->m_OpType == 0){
//					SOCKADDR_IN a,b;
//					SOCKADDR_IN* ClientAddr  = &a;
//					SOCKADDR_IN* LocalAddr = &b;
//					int remoteLen = sizeof(SOCKADDR_IN), localLen = sizeof(SOCKADDR_IN);
//
//					GetAcceptExSockaddrs(pIOCPContext->m_szBuffer, pIOCPContext->m_wsaBuf.len - ((sizeof(SOCKADDR_IN)+16) * 2),
//						sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPSOCKADDR*)&LocalAddr, &localLen, (LPSOCKADDR*)&ClientAddr, &remoteLen);
//
//					Log(("�ͻ��� %s:%d ����.\n"), inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port));
//					Log("�ͻ��� %s:%d ��Ϣ��%s.\n", inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port), pIOCPContext->m_wsaBuf.buf );
//
//					SOCKET_CONTEXT * newSC = new SOCKET_CONTEXT;
//					newSC->m_Socket = pIOCPContext->m_sockAccept;
//					memcpy(&(newSC->m_ClientAddr), ClientAddr, sizeof(SOCKADDR_IN));
//					CreateIoCompletionPort((HANDLE)newSC->m_Socket, *(HANDLE*)ptr, (ULONG_PTR)newSC, 0);
//
//					//post first recv on this socket
//
//					//context list
//
//					//post accept
//					PostAccept(pIOCPContext, ((SOCKET_CONTEXT*)pContext)->m_Socket);
//				}
//
//			}
//		}
//	}
//
//	return 0;
//}
//
//bool StartServer(void){
//	//init iocp
//	HANDLE IOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
//
//	if (IOCPHandle == NULL){
//		Log("[IOCP]:cannot create iocp.\n");
//		return false;
//	}
//	else{
//		Log("[server]:create iocp success.\n");
//	}
//
//	//init socket
//	SOCKET sockSrv;
//	SOCKADDR_IN addrSrv;
//
//	if (initSocket() == false){
//		return false;
//	}
//
//	sockSrv = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
//	ZeroMemory((char*)&addrSrv, sizeof(SOCKADDR_IN));
//	addrSrv.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
//	addrSrv.sin_family = AF_INET;
//	addrSrv.sin_port = htons(6001);
//
//	//bind iocp
//	SOCKET_CONTEXT socketContext; //todo
//	socketContext.m_Socket = sockSrv;
//	socketContext.m_ClientAddr = addrSrv;
//
//	if (CreateIoCompletionPort((HANDLE)sockSrv, IOCPHandle, (ULONG_PTR)&socketContext, 0) == NULL){
//		Log("[server]:bind iocp error.\n");
//		return false;
//	}
//	else{
//		Log("[server]:bind iocp success.\n");
//	}
//
//	if (bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR)) == SOCKET_ERROR){
//		Log("[server]:bind error.\n");
//		return false;
//	}
//	else{
//		Log("[server]:bind success.\n");
//	}
//
//	if (listen(sockSrv, SOMAXCONN) == SOCKET_ERROR){
//		Log("[server]:listen error.\n");
//		return false;
//	}
//	else{
//		Log("[server]:listen success.\n");
//	}
//
//	//init socket pool
//	//LPFN_ACCEPTEX     lpfnAcceptEx;         // AcceptEx����ָ��  
//	//GUID GuidAcceptEx = WSAID_ACCEPTEX;        // GUID�������ʶ��AcceptEx���������  
//	//DWORD dwBytes = 0;
//
//	//int iResult = WSAIoctl(sockSrv, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx,
//	//	sizeof(GuidAcceptEx), &lpfnAcceptEx, sizeof(lpfnAcceptEx), &dwBytes,
//	//	NULL, NULL);
//	//if (iResult == INVALID_SOCKET){
//	//	Log("[server]:init accept ex error.\n");
//	//}
//	//else{
//	//	Log("[server]:init accept ex success.\n");
//	//}
//
//	//threads
//	list<HANDLE> threadList;
//	SYSTEM_INFO si;
//	GetSystemInfo(&si);
//	int numberOfProcessors = si.dwNumberOfProcessors;
//
//	for (int temp_i = 0; temp_i < numberOfProcessors * 2; ++temp_i){
//		threadList.push_back((HANDLE)_beginthreadex(NULL, 0, WorkThread, &IOCPHandle, 0, NULL));
//	}
//
//	//start main loop
//	Log("[server]:main loop start.\n");
//#ifndef DEBUG	
//	for (int i = 0; i < numberOfProcessors * 2 ; ++i){
//		IOCP_CONTEXT * pContext = new IOCP_CONTEXT;
//		ZeroMemory((void *)&(pContext->m_Overlapped), sizeof(OVERLAPPED));
//		pContext->m_OpType = 0;
//		pContext->m_sockAccept = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
//		pContext->m_wsaBuf.len = 4096;
//		pContext->m_wsaBuf.buf = pContext->m_szBuffer;
//		DWORD flag = 0;
//
//		int ret = lpfnAcceptEx(sockSrv, pContext->m_sockAccept, pContext->m_szBuffer, pContext->m_wsaBuf.len - ((sizeof (sockaddr_in)+16) * 2),
//			sizeof (sockaddr_in)+16, sizeof (sockaddr_in)+16, &flag, &pContext->m_Overlapped);
//	}
//
//	while (true){
//		Sleep(50);
//	}
//#else
//	for (int i = 0; i < numberOfProcessors * 2 ; ++i){
//		IOCP_CONTEXT * pContext = new IOCP_CONTEXT;
//		ZeroMemory((void *)&(pContext->m_Overlapped), sizeof(OVERLAPPED));
//		pContext->m_OpType = 0;
//		pContext->m_sockAccept = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
//		pContext->m_wsaBuf.len = 4096;
//		pContext->m_wsaBuf.buf = pContext->m_szBuffer;
//		DWORD flag = 0;
//
//		int ret = AcceptEx(sockSrv, pContext->m_sockAccept, pContext->m_szBuffer, pContext->m_wsaBuf.len - ((sizeof (SOCKADDR_IN)+16) * 2),
//			sizeof (SOCKADDR_IN)+16, sizeof (SOCKADDR_IN)+16, &flag, &pContext->m_Overlapped);
//	}
//
//	while (true){
//		Sleep(50);
//	}
//#endif
//
//	return true;
//}
//
//
//
//int mmmmm(void){
//	StartServer();
//	return 0;
//}