# KCP_over_UDP
Demo for kcp over udp. 
用于学习理解KCP协议，提供`libev`和`while`循环处理两种思路。


## Complie

### Client
#### While 循环处理
`gcc -g client.cpp ikcp.c -o client`

#### Libev 处理

libev 源码：https://github.com/enki/libev
先编译按装libev：
```
# 编译和使用方法可参考：https://zhuanlan.zhihu.com/p/626826013

git clone https://github.com/enki/libev
cd libev
./configure # 可指定位置
make
make install
# 编译好后，libev被放在了/usr/local/lib这个文件夹里。
# 设置系统 动态库 链接路径
echo "/usr/local/lib" >> /etc/ld.so.conf
ldconfig
```
之后执行：
`gcc -g client_libev.cpp ikcp.c -lev -o client_libev`


### Server
```
go mod init
go mod tidy
# 之后
go run server.go

# 或者
go build -o server server.go
./server
```

## Doc
https://mp.weixin.qq.com/s/8dn4eZptGeTipOFDyYInFA