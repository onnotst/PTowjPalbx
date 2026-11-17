# 1. 装编译工具
dnf install -y gcc make git

# 2. 拉源码
cd /tmp
git clone https://github.com/opsengine/cpulimit.git
cd cpulimit

# 3. 编译
make

# 4. 装到系统
cp src/cpulimit /usr/local/bin/cpulimit
chmod +x /usr/local/bin/cpulimit

# 5. 验证
cpulimit -h | head -3


//
或者下载cpulimit
或者下载cpulimit.tar.gz解压编译

[root@vm-tms2yw-16283-b67410 cpulimit]# make
cd src && make all
make[1]: 进入目录“/tmp/cpulimit/src”
cc -c list.c -Wall -g -D_GNU_SOURCE
cc -c process_iterator.c -Wall -g -D_GNU_SOURCE
cc -c process_group.c -Wall -g -D_GNU_SOURCE
cc -o cpulimit cpulimit.c list.o process_iterator.o process_group.o -Wall -g -D_GNU_SOURCE
cpulimit.c:46:18: 警告：#ifdef 指示的末尾有多余的标识符 [默认启用]
 #ifdef __APPLE__ || __FREEBSD__
                  ^
make[1]: 离开目录“/tmp/cpulimit/src”
cd tests && make all
make[1]: 进入目录“/tmp/cpulimit/tests”
cc -o busy busy.c -lpthread -Wall -g
cc -I../src -o process_iterator_test process_iterator_test.c ../src/list.o ../src/process_iterator.o ../src/process_group.o -lpthread -Wall -g
process_iterator_test.c:31:18: 警告：#ifdef 指示的末尾有多余的标识符 [默认启用]
 #ifdef __APPLE__ || __FREEBSD__
                  ^
make[1]: 离开目录“/tmp/cpulimit/tests”

我的centos编译成功
我的centos源过期自行处理valut