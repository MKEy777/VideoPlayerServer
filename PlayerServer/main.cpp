#include "Process.h"

#include <cstdio>
// --- 业务代码 ---

int CreateLogServer(CProcess* proc) {
    printf("%s(%d):<%s> pid=%d\n", __FILE__,__LINE__, __FUNCTION__, getpid());
    return 0;
}

int CreateClientServer(CProcess* proc) {
    printf("%s(%d):<%s> pid=%d\n", __FILE__,__LINE__, __FUNCTION__, getpid());
    int fd = -1;
    int ret = proc->RecvFD(fd);
    printf("%s(%d):<%s> ret=%d\n", __FILE__,__LINE__, __FUNCTION__, ret);
    printf("%s(%d):<%s> fd=%d\n", __FILE__,__LINE__, __FUNCTION__, fd);
    sleep(1);
    char buf[10] = "";
    lseek(fd, 0, SEEK_SET);
    read(fd, buf, sizeof(buf));
    printf("%s(%d):<%s> buf=%s\n", __FILE__,__LINE__, __FUNCTION__, buf);
    close(fd);
    return 0;
}

int main() {
    //CProcess::SwitchDeamon();
    CProcess proclog, proccliets;
    printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
    proclog.SetEntryFunction(CreateLogServer, &proclog);
    int ret = proclog.CreateSubProcess();
    if(ret!=0) {
        //std::cout << -1 << std::endl;
        printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
        return -1;
	}
    printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
    proccliets.SetEntryFunction(CreateClientServer, &proccliets);
    ret= proccliets.CreateSubProcess();
    if (ret != 0) {
        //std::cout << -2 << std::endl;
        printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
        return -2;
    }
    printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
    //生产环境下这里通常需要 wait() 子进程，否则主进程退出后子进程会托管给 init
	usleep(100000);
    int fd = open("/home/zym/projects/PlayerServer/bin/x64/Debug/test.txt",
        O_RDWR | O_CREAT | O_APPEND, 0666);
    printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
    if (fd == -1) return -3;
    ret = proccliets.SendFD(fd);
    printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
    write(fd, "Hello", 5);
	close(fd);
    return 0;
}