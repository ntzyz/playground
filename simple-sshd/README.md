simple sshd
===========

A simple Secure Shell Server built with libssh.
```
ntzyz@ntzyz-solaris ~ % ssh localhost -p 2022
ntzyz@localhost's password: 
PTY allocation request failed on channel 0
[NSH]$ 123
ECHO: 123
[NSH]$ ping
Pong!
[NSH]$ exit
exiting...
Connection to localhost closed.
```

#### Dependence
libssh-dev

#### Build and run
```bash
$ sudo apt install 
$ make
$ ./server
```
