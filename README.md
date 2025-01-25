# echo

TCP echo server on port 7

## Summary

Server for testing TCP/IP connectivity

## Docker

```
$ docker build -t echo .
$ docker run --rm --publish 7:7/tcp --name echo echo
```

# Win32

```
C:> cl.exe echod.c
```

# POSIX

```
$ gcc -I. echod.c -o echod -DHAVE_CONFIG_H -Wall -Werror
```

# Client

Socket clients for .NET, Java and PowerShell
