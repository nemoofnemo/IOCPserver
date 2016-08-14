#include "IOCP.h"

bool IOCPServer::PostAccept(IOCPContext * pIC){
	//
	if ( pIC == NULL || listenContext->socket == INVALID_SOCKET){
		return false;
	}

	DWORD dwBytes = 0;
	pIC->operation = SIG_ACCEPT;

	// 为以后新连入的客户端先准备好Socket( 这个是与传统accept最大的区别 )
	pIC->sockAccept = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (pIC->sockAccept == INVALID_SOCKET){
		Log("[server]:in postaccept,invalid socket\n");
		return false;
	}

	if (lpfnAcceptEx(listenContext->socket, pIC->sockAccept, pIC->wsabuf.buf, pIC->wsabuf.len - ((sizeof(SOCKADDR_IN)+16) * 2) - 1, sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, &dwBytes, &pIC->overlapped) == FALSE){
			if (WSA_IO_PENDING != WSAGetLastError()){
				Log("[server]:in postaccept,acceptex error code=%d.\n",WSAGetLastError());
				RELEASE_SOCKET(pIC->sockAccept);
				return false;
			}

			//if WSAGetLastError == WSA_IO_PENDING
			//	io is still working normally.
			// dont return false!!!!
	}

	return true;
}

bool IOCPServer::DoAccept(SocketContext * pSC, IOCPContext * pIC){
	SOCKADDR_IN* clientAddr = NULL;
	SOCKADDR_IN* localAddr = NULL;
	int remoteLen = sizeof(SOCKADDR_IN), localLen = sizeof(SOCKADDR_IN);

	// 1. 首先取得连入客户端的地址信息
	lpfnGetAcceptExSockAddrs(pIC->wsabuf.buf, pIC->wsabuf.len - ((sizeof(SOCKADDR_IN)+16) * 2),sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPSOCKADDR*)&localAddr, &localLen, (LPSOCKADDR*)&clientAddr, &remoteLen);

	Log(("[client]: %s:%d connect.\n"), inet_ntoa(clientAddr->sin_addr), ntohs(clientAddr->sin_port));
	Log("[client]: %s:%d\ncontent:%s.\n", inet_ntoa(clientAddr->sin_addr), ntohs(clientAddr->sin_port), pIC->wsabuf.buf);

	// 2. 这里需要注意，这里传入的这个是ListenSocket上的SocketContext，这个Context我们还需要用于监听下一个连接
	SocketContext * pSocketContext = new SocketContext;
	pSocketContext->socket = pIC->sockAccept;
	memcpy(&(pSocketContext->sockAddr), clientAddr, sizeof(SOCKADDR_IN));
	// 参数设置完毕，将这个Socket和完成端口绑定(这也是一个关键步骤)
	if (CreateIoCompletionPort((HANDLE)pSocketContext->socket, hIOCP, (ULONG_PTR)pSocketContext, 0) == NULL){
		RemoveSocketContext(pSocketContext);
		Log("[server]:in doaccept,iocp failed\n");
		return false;
	}

	// 3. 继续，建立其下的IoContext，用于在这个Socket上投递第一个Recv数据请求
	IOCPContext * pIOCPContext = pSocketContext->CreateIOCPContext();
	pIOCPContext->operation = SIG_RECV;
	pIOCPContext->sockAccept = pSocketContext->socket;
	// 如果Buffer需要保留，就自己拷贝一份出来
	if (PostRecv(pIOCPContext) == false){
		pSocketContext->RemoveContext(pIOCPContext);
		Log("[server]:in doaccept,post failed\n");
		return false;
	}
	else{
		// 4. 如果投递成功，那么就把这个有效的客户端信息，加入到ContextList中去(需要统一管理，方便释放资源)
		AddSocketContext(pSocketContext);
	}

	// 5. 使用完毕之后，把Listen Socket的那个IoContext重置，然后准备投递新的AcceptEx
	pIC->ResetBuffer();
	return PostAccept(pIC);
}

// 投递接收数据请求
bool IOCPServer::PostRecv(IOCPContext * pSC){
	// 初始化变量
	DWORD dwFlags = 0;
	DWORD dwBytes = 0;
	WSABUF *pWSAbuf = &pSC->wsabuf;
	OVERLAPPED *pOl = &pSC->overlapped;

	pSC->ResetBuffer();
	pSC->operation = SIG_RECV;

	// 初始化完成后，，投递WSARecv请求
	int nBytesRecv = WSARecv(pSC->sockAccept, pWSAbuf, 1, &dwBytes, &dwFlags, pOl, NULL);

	// 如果返回值错误，并且错误的代码并非是Pending的话，那就说明这个重叠请求失败了
	if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		Log("[server]:in postrecv , post failed\n");
		return false;
	}

	return true;
}

// 在有接收的数据到达的时候，进行处理
bool IOCPServer::DoRecv(SocketContext * pSC, IOCPContext * pIC){
	SOCKADDR_IN* ClientAddr = &pSC->sockAddr;
	Log("[client]:  %s:%d\ncontent:%s\n", inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port), pIC->wsabuf.buf);
	// 然后开始投递下一个WSARecv请求
	return PostRecv(pIC);
}

unsigned int __stdcall IOCPServer::WorkThread(void * ptr){
	_WT_PARAM * pArg = (_WT_PARAM *)ptr;
	IOCPServer * pServer = pArg->pServer;

	OVERLAPPED * pOverlapped = NULL;
	SocketContext * pSocketContext = NULL;
	DWORD dwBytesTransfered = 0;
	Log("[server]:thread %d start success\n", pArg->index);

	while (true){
		BOOL flag = GetQueuedCompletionStatus(pArg->handle, &dwBytesTransfered, (PULONG_PTR)&pSocketContext, &pOverlapped, INFINITE);

		// 判断是否出现了错误
		if (!flag){
			//todo
			//show error message
			continue;
		}
		else{
			// 读取传入的参数
			IOCPContext * pIC = CONTAINING_RECORD(pOverlapped, IOCPContext, overlapped);
			Log("[server]: io request in thread %d\n", pArg->index);

			// 判断是否有客户端断开了
			if ((dwBytesTransfered == 0) && pServer->IsValidOperaton(pIC->operation)){
				Log("[client]: %s:%d disconnect.\n", inet_ntoa(pSocketContext->sockAddr.sin_addr), ntohs(pSocketContext->sockAddr.sin_port));

				// 释放掉对应的资源
				pServer->RemoveSocketContext(pSocketContext);
				continue;
			}
			else{
				pIC->wsabuf.buf[dwBytesTransfered] = '\0';
				
				switch (pIC->operation){
				case SIG_ACCEPT:
					pServer->DoAccept(pSocketContext, pIC);
					break;
				case SIG_RECV:
					pServer->DoRecv(pSocketContext, pIC);
					break;
				case SIG_SEND:

					break;
				case SIG_NULL:

					break;
				case SIG_EXIT:
					Log("[server]:exit signal received.\n");
					RELEASE(pArg);
					RELEASE(pIC);
					return 0;
				default:
					Log("[server]:invalid signal received.thread exit unnormally\n");
					return 0;
				}
			}
		}
	}
	
	return 0;
}

int main(void){
	IOCPServer * pServer = new IOCPServer;
	pServer->LoadSocketLib();
	pServer->Start();
	pServer->Stop();
	pServer->UnloadSocketLib();
	return 0;
}