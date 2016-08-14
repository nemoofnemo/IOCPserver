#include "IOCP.h"

bool IOCPServer::PostAccept(IOCPContext * pIC){
	//
	if ( pIC == NULL || listenContext->socket == INVALID_SOCKET){
		return false;
	}

	DWORD dwBytes = 0;
	pIC->operation = SIG_ACCEPT;

	// Ϊ�Ժ�������Ŀͻ�����׼����Socket( ������봫ͳaccept�������� )
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

	// 1. ����ȡ������ͻ��˵ĵ�ַ��Ϣ
	lpfnGetAcceptExSockAddrs(pIC->wsabuf.buf, pIC->wsabuf.len - ((sizeof(SOCKADDR_IN)+16) * 2),sizeof(SOCKADDR_IN)+16, sizeof(SOCKADDR_IN)+16, (LPSOCKADDR*)&localAddr, &localLen, (LPSOCKADDR*)&clientAddr, &remoteLen);

	Log(("[client]: %s:%d connect.\n"), inet_ntoa(clientAddr->sin_addr), ntohs(clientAddr->sin_port));
	Log("[client]: %s:%d\ncontent:%s.\n", inet_ntoa(clientAddr->sin_addr), ntohs(clientAddr->sin_port), pIC->wsabuf.buf);

	// 2. ������Ҫע�⣬���ﴫ��������ListenSocket�ϵ�SocketContext�����Context���ǻ���Ҫ���ڼ�����һ������
	SocketContext * pSocketContext = new SocketContext;
	pSocketContext->socket = pIC->sockAccept;
	memcpy(&(pSocketContext->sockAddr), clientAddr, sizeof(SOCKADDR_IN));
	// ����������ϣ������Socket����ɶ˿ڰ�(��Ҳ��һ���ؼ�����)
	if (CreateIoCompletionPort((HANDLE)pSocketContext->socket, hIOCP, (ULONG_PTR)pSocketContext, 0) == NULL){
		RemoveSocketContext(pSocketContext);
		Log("[server]:in doaccept,iocp failed\n");
		return false;
	}

	// 3. �������������µ�IoContext�����������Socket��Ͷ�ݵ�һ��Recv��������
	IOCPContext * pIOCPContext = pSocketContext->CreateIOCPContext();
	pIOCPContext->operation = SIG_RECV;
	pIOCPContext->sockAccept = pSocketContext->socket;
	// ���Buffer��Ҫ���������Լ�����һ�ݳ���
	if (PostRecv(pIOCPContext) == false){
		pSocketContext->RemoveContext(pIOCPContext);
		Log("[server]:in doaccept,post failed\n");
		return false;
	}
	else{
		// 4. ���Ͷ�ݳɹ�����ô�Ͱ������Ч�Ŀͻ�����Ϣ�����뵽ContextList��ȥ(��Ҫͳһ���������ͷ���Դ)
		AddSocketContext(pSocketContext);
	}

	// 5. ʹ�����֮�󣬰�Listen Socket���Ǹ�IoContext���ã�Ȼ��׼��Ͷ���µ�AcceptEx
	pIC->ResetBuffer();
	return PostAccept(pIC);
}

// Ͷ�ݽ�����������
bool IOCPServer::PostRecv(IOCPContext * pSC){
	// ��ʼ������
	DWORD dwFlags = 0;
	DWORD dwBytes = 0;
	WSABUF *pWSAbuf = &pSC->wsabuf;
	OVERLAPPED *pOl = &pSC->overlapped;

	pSC->ResetBuffer();
	pSC->operation = SIG_RECV;

	// ��ʼ����ɺ󣬣�Ͷ��WSARecv����
	int nBytesRecv = WSARecv(pSC->sockAccept, pWSAbuf, 1, &dwBytes, &dwFlags, pOl, NULL);

	// �������ֵ���󣬲��Ҵ���Ĵ��벢����Pending�Ļ����Ǿ�˵������ص�����ʧ����
	if ((SOCKET_ERROR == nBytesRecv) && (WSA_IO_PENDING != WSAGetLastError()))
	{
		Log("[server]:in postrecv , post failed\n");
		return false;
	}

	return true;
}

// ���н��յ����ݵ����ʱ�򣬽��д���
bool IOCPServer::DoRecv(SocketContext * pSC, IOCPContext * pIC){
	SOCKADDR_IN* ClientAddr = &pSC->sockAddr;
	Log("[client]:  %s:%d\ncontent:%s\n", inet_ntoa(ClientAddr->sin_addr), ntohs(ClientAddr->sin_port), pIC->wsabuf.buf);
	// Ȼ��ʼͶ����һ��WSARecv����
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

		// �ж��Ƿ�����˴���
		if (!flag){
			//todo
			//show error message
			continue;
		}
		else{
			// ��ȡ����Ĳ���
			IOCPContext * pIC = CONTAINING_RECORD(pOverlapped, IOCPContext, overlapped);
			Log("[server]: io request in thread %d\n", pArg->index);

			// �ж��Ƿ��пͻ��˶Ͽ���
			if ((dwBytesTransfered == 0) && pServer->IsValidOperaton(pIC->operation)){
				Log("[client]: %s:%d disconnect.\n", inet_ntoa(pSocketContext->sockAddr.sin_addr), ntohs(pSocketContext->sockAddr.sin_port));

				// �ͷŵ���Ӧ����Դ
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