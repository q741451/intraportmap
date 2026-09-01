# intraportmap
将内网端口映射到公网，支持跨平台，支持域名，支持掉线重连，支持IPv6，支持UDP

### 快速开始

服务端，域名为test.3322.org，端口为22120：
```
$ ./intraportmap -s [::]:22120
```
客户端，将本机3389端口映射到服务端test.3322.org的13389端口上：
```
> intraportmap.exe -c -s test.3322.org:22120 -t [::]:13389 -f 127.0.0.1:3389
```

### UDP

默认只映射TCP。加`-u`则同一个端口上同时映射TCP和UDP，加`-U`则只映射UDP。
服务端和客户端两边都要加同样的参数。服务端不需要额外端口，UDP复用`-s`里的同一个端口号。

把本机3389端口的TCP和UDP一起映射出去：
```
$ ./intraportmap -s [::]:22120 -u
> intraportmap.exe -c -s test.3322.org:22120 -t [::]:13389 -f 127.0.0.1:3389 -u
```

只映射UDP，例如把内网DNS映射出去：
```
$ ./intraportmap -s [::]:22120 -U
> intraportmap.exe -c -s test.3322.org:22120 -t [::]:15353 -f 127.0.0.1:53 -U
```

UDP没有连接可言，会话靠空闲超时回收，默认180秒，用`-T`调整：
```
> intraportmap.exe -c -s test.3322.org:22120 -t [::]:15353 -f 127.0.0.1:53 -U -T 30
```

超过MTU的数据报不会被丢弃，交给IP分片处理。

### 参数

```
-c                 以客户端运行，不加则为服务端
-s server:port     服务端地址。服务端用于监听，客户端用于连接
-t to_server:port  要在服务端上开放的地址和端口（仅客户端）
-f from_server:port被映射的内网地址和端口（仅客户端）
-k key             通信密钥，两端需一致
-w seconds         客户端断线重连间隔，默认15
-b bytes           每条TCP隧道的缓冲上限，默认1048576
-u                 在TCP之外同时映射UDP
-U                 只映射UDP
-T seconds         UDP会话空闲超时，默认180
```
