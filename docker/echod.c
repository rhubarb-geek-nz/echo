// Copyright (c) 2025 Roger Brown.
// Licensed under the MIT License.

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifdef _WIN32
#	include <winsock2.h>
#	pragma comment(lib,"ws2_32")
#else
#	include <sys/types.h>
#	include <sys/time.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <netdb.h>
#	include <errno.h>
#	include <unistd.h>
#	include <sys/ioctl.h>
#	include <fcntl.h>
#	ifndef INVALID_SOCKET
#		define INVALID_SOCKET		-1
		typedef int SOCKET;
#	endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#	include <strings.h>
#endif

#ifndef HAVE_SOCKLEN_T
#	define HAVE_SOCKLEN_T
		typedef int socklen_t;
#endif

struct conn
{
	struct conn *next;
	SOCKET fd;
	char buffer[4097];
	int head,tail;
};

static int next(struct conn *c,int x)
{
	x++;

	if (x >= sizeof(c->buffer)) x=0;

	return x;
}

static int soclose(SOCKET fd)
{
#ifdef _WIN32
	return closesocket(fd);
#else
	return close(fd);
#endif
}

static int sockopts(SOCKET fd)
{
#ifdef _WIN32
	BOOL ul=1;
	int i=ioctlsocket(fd,FIONBIO,(unsigned long *)&ul);
#else
#	ifdef HAVE_FCNTL_F_SETFL_O_NDELAY
	int i=fcntl(fd,F_SETFL,O_NDELAY);
	if (i < 0)
	{
		perror("F_SETFL,O_NDELAY");
	}
#	else
	int ul=1;
	int i=ioctl(fd,FIONBIO,(void *)&ul);
	if (i)
	{
		perror("FIONBIO");
	}
#	endif
#endif
	return i;
}

static void dump_data(const char *buf,int k)
{
	static int off=0;

	while (k--)
	{
		printf("%02X",(int)(unsigned char)*buf);
		buf++;

		off++;

		if (off & 0xf)
		{
			if (off & 3)
			{
				printf(" ");
			}
			else
			{
				printf("  ");
			}
		}
		else
		{
			printf("\n");
		}
	}

	fflush(stdout);
}

int main(int argc,char **argv)
{
#ifdef _WIN32
	WSADATA wsd;
#endif
	struct servent *sp=NULL;
	SOCKET fdListen=INVALID_SOCKET;
	struct sockaddr_in addr;
	struct conn *conn=NULL;
	char *port="echo";

#ifdef _WIN32
	if (WSAStartup(0x202,&wsd)) return 1;
#endif

	if (argc > 1)
	{
		port=argv[1];
	}

	memset(&addr,0,sizeof(addr));

	addr.sin_family=AF_INET;

	sp=getservbyname(port,"tcp");

	if (sp)
	{
		addr.sin_port=sp->s_port;
	}
	else
	{
		short s_port=(short)atol(port);
		addr.sin_port=htons(s_port);
	}

	fdListen=socket(AF_INET,SOCK_STREAM,0);

	if (fdListen==INVALID_SOCKET) 
	{
		perror("socket");

		return 1;
	}

	if (bind(fdListen,(struct sockaddr *)&addr,sizeof(addr)))
	{
		perror("bind");

		return 1;
	}

	if (listen(fdListen,5))
	{
		perror("listen");

		return 1;
	}

	sockopts(fdListen);

	while (fdListen!=INVALID_SOCKET)
	{
		fd_set fdr,fdw;
		SOCKET n=fdListen;
		int i;
		struct conn *c=conn;

		FD_ZERO(&fdr);
		FD_ZERO(&fdw);

		FD_SET(fdListen,&fdr);

		while (c)
		{
			if (next(c,c->head)!=c->tail)
			{
				FD_SET(c->fd,&fdr);

				if (c->fd > n) n=c->fd;
			}

			if (c->head != c->tail)
			{
				FD_SET(c->fd,&fdw);

				if (c->fd > n) n=c->fd;
			}

			c=c->next;
		}

		i=select((int)(n+1),&fdr,&fdw,NULL,NULL);

		if (i > 0)
		{
			if (fdListen!=INVALID_SOCKET)
			{
				if (FD_ISSET(fdListen,&fdr))
				{
					socklen_t j=sizeof(addr);
					SOCKET fd=accept(fdListen,(struct sockaddr *)&addr,&j);

					if (fd!=INVALID_SOCKET)
					{
						sockopts(fd);

						c=calloc(sizeof(*c),1);

						c->fd=fd;
						c->next=conn;
						conn=c;
					}
				}
			}

			c=conn;

			while (c)
			{
				if (FD_ISSET(c->fd,&fdw))
				{
					FD_CLR(c->fd,&fdw);

					if (c->head != c->tail)
					{
						int off=c->tail;
						int k=0;

						if (c->head > c->tail)
						{
							k=c->head - c->tail;
						}
						else
						{
							k=sizeof(c->buffer)-c->tail;
						}

#if defined(_WIN32) && defined(_M_IX86)
						if (!k) __asm int 3;
#endif

						k=send(c->fd,c->buffer+off,k,0);

						if (k > 0)
						{
							c->tail+=k;

							if (c->tail > sizeof(c->buffer))
							{
#if defined(_WIN32) && defined(_M_IX86)
								__asm int 3
#else
								void **pv=NULL;
								*pv=NULL;
#endif
							}

							if (c->tail==sizeof(c->buffer))
							{
								c->tail=0;
							}
						}
					}
				}

				if (FD_ISSET(c->fd,&fdr))
				{
					int k;
					int off=c->head;

					FD_CLR(c->fd,&fdr);

					if (c->head == c->tail)
					{
						off=0;
						k=sizeof(c->buffer)-1;
						c->head=0;
						c->tail=0;
					}
					else
					{
						if (c->head > c->tail)
						{
							k=sizeof(c->buffer)-c->head;

							if (!c->tail) k--;
						}
						else
						{
							k=c->tail-c->head-1;
						}
					}

#if defined(_M_IX86) && defined(_WIN32)
					if (k < 0) __asm int 3;
					if (!k) __asm int 3;
#endif

					k=recv(c->fd,c->buffer+off,k,0);

					if (k > 0)
					{
						dump_data(c->buffer+off,k);

						c->head+=k;

						if (c->head > sizeof(c->buffer))
						{
#if defined(_WIN32) && defined(_M_IX86)
								__asm int 3
#else
								void **pv=NULL;
								*pv=NULL;
#endif
						}

						if (c->head==sizeof(c->buffer))
						{
							c->head=0;
						}
					}
					else
					{
						if (k==-1)
						{
#ifdef _WIN32
							int e=WSAGetLastError();
#else
							int e=errno;
#endif

							switch (e)
							{
#ifdef _WIN32
							case WSAEWOULDBLOCK:
#else
							case EWOULDBLOCK:
#endif
								break;
							default:
								k=0;
								break;
							}
						}

						if (k==0)
						{
							soclose(c->fd);

							if (c==conn)
							{
								conn=c->next;
							}
							else
							{
								struct conn *p=conn;

								while (p->next != c) p=p->next;

								p->next=c->next;
							}

							free(c);

							c=conn;
						}
					}
				}

				if (c)
				{
					c=c->next;
				}
			}
		}
	}

#ifdef _WIN32
	WSACleanup();
#endif

	return 0;
}
