/***********************************
 * Copyright (c) 2025 Roger Brown.
 * Licensed under the MIT License.
 ****/
#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

static wchar_t ServiceName[] = L"echo";
static HANDLE ghSvcStopEvent = NULL;
static SERVICE_STATUS gSvcStatus;
static SERVICE_STATUS_HANDLE gSvcStatusHandle;

static DWORD CALLBACK do_recv(LPVOID lp)
{
	SOCKET fd = (SOCKET)lp;

	__try
	{
		while (TRUE)
		{
			char buf[4096];
			int i = recv(fd, buf, sizeof(buf), 0);

			if (i > 0)
			{
				const char* p = buf;

				while (i)
				{
					int j = send(fd, p, i, 0);

					if (j > 0)
					{
						p += j;
						i -= j;
					}
					else
					{
						if (j < 1)
						{
							if (WSAEWOULDBLOCK == WSAGetLastError())
							{
								fd_set fdw;
								FD_ZERO(&fdw);
								FD_SET(fd, &fdw);
								select((int)(fd + 1), NULL, &fdw, NULL, NULL);
							}
							else
							{
								p = NULL;
								break;
							}
						}
						else
						{
							p = NULL;
							break;
						}
					}
				}

				if (!p)
				{
					break;
				}
			}
			else
			{
				if (i == 0)
				{
					break;
				}
				else
				{
					if (WSAEWOULDBLOCK == WSAGetLastError())
					{
						fd_set fdr;
						FD_ZERO(&fdr);
						FD_SET(fd, &fdr);
						select((int)(fd + 1), &fdr, NULL, NULL, NULL);
					}
					else
					{
						break;
					}
				}
			}
		}
	}
	__finally
	{
		closesocket(fd);
	}

	return 0;
}

static void do_recvfrom(SOCKET fd)
{
	while (TRUE)
	{
		struct
		{
			struct sockaddr addr;
			struct sockaddr_in addr4;
			struct sockaddr_in6 addr6;
		} addr;
		int addrLen = sizeof(addr);
		char buf[4096];
		int i = recvfrom(fd, buf, sizeof(buf), 0, &addr.addr, &addrLen);
		if (i > 0)
		{
			sendto(fd, buf, i, 0, &addr.addr, addrLen);
		}
		else
		{
			break;
		}
	}
}

static void do_accept(SOCKET fd)
{
	while (TRUE)
	{
		struct
		{
			struct sockaddr addr;
			struct sockaddr_in addr4;
			struct sockaddr_in6 addr6;
		} addr;
		int addrLen = sizeof(addr);
		SOCKET fd2 = accept(fd, &addr.addr, &addrLen);

		if (fd2 == INVALID_SOCKET)
		{
			break;
		}
		else
		{
			if (!WSAEventSelect(fd2, NULL, 0))
			{
				DWORD tid;
				HANDLE h = CreateThread(NULL, 0, do_recv, (LPVOID)fd2, 0, &tid);
				if (h)
				{
					fd2 = INVALID_SOCKET;
					CloseHandle(h);
				}
			}

			if (fd2 != INVALID_SOCKET)
			{
				closesocket(fd2);
			}
		}
	}
}

static VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 1;

	gSvcStatus.dwCurrentState = dwCurrentState;
	gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
	gSvcStatus.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
	{
		gSvcStatus.dwControlsAccepted = 0;
	}
	else
	{
		gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	}

	if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED))
	{
		gSvcStatus.dwCheckPoint = 0;
	}
	else
	{
		gSvcStatus.dwCheckPoint = dwCheckPoint++;
	}

	SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

static void WINAPI ServiceCtrlHandler(DWORD dwCtrl)
{
	HANDLE hEvent = ghSvcStopEvent;

	switch (dwCtrl)
	{
	case SERVICE_CONTROL_STOP:
		if (hEvent)
		{
			ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

			SetEvent(hEvent);
		}
		break;

	case SERVICE_CONTROL_INTERROGATE:
		break;

	default:
		break;
	}
}

struct echo
{
	int family, type, protocol;
	void (*handler)(SOCKET fd);
	SOCKET fd;
};

static void WINAPI ServiceMain(DWORD argc, LPWSTR* argv)
{
	ghSvcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	gSvcStatus.dwServiceSpecificExitCode = 0;

	gSvcStatusHandle = RegisterServiceCtrlHandlerW(ServiceName, ServiceCtrlHandler);

	__try
	{
		WSADATA wsadata;

		if (!WSAStartup(0x202, &wsadata))
		{
			__try
			{
				struct echo echoes[] = {
					{ 0,0,0,NULL,INVALID_SOCKET },
					{ PF_INET,SOCK_STREAM,IPPROTO_TCP,do_accept,INVALID_SOCKET },
					{ PF_INET,SOCK_DGRAM,IPPROTO_UDP,do_recvfrom,INVALID_SOCKET },
					{ PF_INET6,SOCK_STREAM,IPPROTO_TCP,do_accept,INVALID_SOCKET },
					{ PF_INET6,SOCK_DGRAM,IPPROTO_UDP,do_recvfrom,INVALID_SOCKET }
				};
				HANDLE hEvents[sizeof(echoes) / sizeof(echoes[0])];
				struct echo* e = echoes + 1;
				int n = 0;
				struct servent* spU = getservbyname("echo", "udp");
				struct servent* spS = getservbyname("echo", "tcp");
				BOOL failed = FALSE;

				hEvents[n++] = ghSvcStopEvent;

				while (n < sizeof(echoes) / sizeof(echoes[0]))
				{
					unsigned short port = e->type == SOCK_DGRAM ? spU->s_port : spS->s_port;
					HANDLE h = WSACreateEvent();
					hEvents[n++] = h;

					e->fd = socket(e->family, e->type, e->protocol);

					if (e->fd == INVALID_SOCKET)
					{
						failed = TRUE;
					}
					else
					{
						if (e->family == PF_INET)
						{
							struct sockaddr_in addr;
							memset(&addr, 0, sizeof(addr));
							addr.sin_family = AF_INET;
							addr.sin_port = port;
							if (bind(e->fd, (struct sockaddr*)&addr, sizeof(addr))) failed = TRUE;
						}
						else
						{
							struct sockaddr_in6 addr;
							memset(&addr, 0, sizeof(addr));
							addr.sin6_family = AF_INET6;
							addr.sin6_port = port;
							if (bind(e->fd, (struct sockaddr*)&addr, sizeof(addr))) failed = TRUE;
						}

						if (e->type == SOCK_DGRAM)
						{
							WSAEventSelect(e->fd, h, FD_READ);
						}
						else
						{
							if (listen(e->fd, 5)) failed = TRUE;
							WSAEventSelect(e->fd, h, FD_ACCEPT);
						}
					}

					e++;
				}

				if (!failed)
				{
					ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

					while (TRUE)
					{
						DWORD dw = WaitForMultipleObjects(n, hEvents, FALSE, INFINITE);

						if (dw >= WAIT_OBJECT_0 && dw < WAIT_OBJECT_0 + n)
						{
							dw -= WAIT_OBJECT_0;

							e = echoes + dw;

							if (e->handler)
							{
								WSAResetEvent(hEvents[dw]);

								e->handler(e->fd);
							}
							else
							{
								break;
							}
						}
						else
						{
							break;
						}
					}
				}

				e = echoes + 1;

				while (n > 1)
				{
					n--;

					if (e->fd != INVALID_SOCKET)
					{
						WSAEventSelect(e->fd, NULL, 0);
						closesocket(e->fd);
					}

					if (hEvents[n])
					{
						WSACloseEvent(hEvents[n]);
					}

					e++;
				}
			}
			__finally
			{
				WSACleanup();
			}
		}
	}
	__finally
	{
		HANDLE hEvent = ghSvcStopEvent;
		ghSvcStopEvent = NULL;
		ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
		if (hEvent)
		{
			CloseHandle(hEvent);
		}
	}
}

static SERVICE_TABLE_ENTRYW DispatchTable[] =
{
	{ ServiceName, ServiceMain },
	{ NULL, NULL }
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	return StartServiceCtrlDispatcherW(DispatchTable) ? 0 : GetLastError();
}
