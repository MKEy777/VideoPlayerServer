#include "CPlayerServer.h"

int CreateLogServer(CProcess* proc)
{
	//printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	CLoggerServer server;
	int ret = server.Start();
	if (ret != 0) {
		printf("%s(%d):<%s> pid=%d errno:%d msg:%s ret:%d\n",
			__FILE__, __LINE__, __FUNCTION__, getpid(), errno, strerror(errno), ret);
	}
	int fd = 0;
	while (true) {
		ret = proc->RecvFD(fd);
		printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
		if (fd <= 0)break;
		//close(fd);
	}
	ret = server.Close();
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	return 0;
}

int CreateClientServer(CProcess* proc)
{
	//printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	int fd = -1;
	proc->RecvFD(fd);
	//printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	//printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
	sleep(1);
	char buf[10] = "";
	lseek(fd, 0, SEEK_SET);
	read(fd, buf, sizeof(buf));
	//printf("%s(%d):<%s> buf=%s\n", __FILE__, __LINE__, __FUNCTION__, buf);
	close(fd);
	return 0;
}

int LogTest()
{
	char buffer[] = "LogTest";
	usleep(1000 * 100);
	TRACEI("here is log %d %c %f %g %s 哈哈", 111111, 'A', 1.0f, 1.0, buffer);
	DUMPD((void*)buffer, (size_t)sizeof(buffer));
	LOGE << 333 << " " << 'S' << " " << 0.33333f << " " << 3.3333333 << " " << buffer << " 33编程";
	return 0;
}

int old_test()
{
	//CProcess::SwitchDeamon();
	CProcess proclog, procclients;
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	proclog.SetEntryFunction(CreateLogServer, &proclog);
	int ret = proclog.CreateSubProcess();
	if (ret != 0) {
		printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
		return -1;
	}
	LogTest();
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	CThread thread(LogTest);
	thread.Start();
	procclients.SetEntryFunction(CreateClientServer, &procclients);
	ret = procclients.CreateSubProcess();
	if (ret != 0) {
		printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
		return -2;
	}
	printf("%s(%d):<%s> pid=%d\n", __FILE__, __LINE__, __FUNCTION__, getpid());
	usleep(100 * 000);
	int fd = open("./test.txt", O_RDWR | O_CREAT | O_APPEND);
	printf("%s(%d):<%s> fd=%d\n", __FILE__, __LINE__, __FUNCTION__, fd);
	if (fd == -1)return -3;
	ret = procclients.SendFD(fd);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	write(fd, "edoyun", 6);
	close(fd);
	CThreadPool pool;
	ret = pool.Start(4);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	ret = pool.AddTask(LogTest);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	ret = pool.AddTask(LogTest);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	ret = pool.AddTask(LogTest);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	ret = pool.AddTask(LogTest);
	printf("%s(%d):<%s> ret=%d\n", __FILE__, __LINE__, __FUNCTION__, ret);
	if (ret != 0)printf("errno:%d msg:%s\n", errno, strerror(errno));
	(void)getchar();
	pool.Close();
	proclog.SendFD(-1);
	(void)getchar();
	return 0;
}


int Main()
{
	int ret = 0;
	CProcess proclog;
	ret = proclog.SetEntryFunction(CreateLogServer, &proclog);
	ERR_RETURN(ret, -1);
	ret = proclog.CreateSubProcess();
	ERR_RETURN(ret, -2);
	CPlayerServer business(2);
	CServer server;
	ret = server.Init(&business);
	ERR_RETURN(ret, -3);
	ret = server.Run();
	ERR_RETURN(ret, -4);
	return 0;
}

#include "HttpParser.h"

int http_test()
{
	Buffer str = "GET /favicon.ico HTTP/1.1\r\n"
		"Host: 0.0.0.0=5000\r\n"
		"User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0\r\n"
		"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*; q = 0.8\r\n"
		"Accept-Language: en-us,en;q=0.5\r\n"
		"Accept-Encoding: gzip,deflate\r\n"
		"Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n"
		"Keep-Alive: 300\r\n"
		"Connection: keep-alive\r\n"
		"\r\n";
	CHttpParser parser;
	size_t size = parser.Parser(str);
	if (parser.Errno() != 0) {
		printf("errno %d\n", parser.Errno());
		return -1;
	}
	if (size != 368) {
		printf("size error:%lld  %lld\n", size, str.size());
		return -2;
	}
	printf("method %d url %s\n", parser.Method(), (const char*)parser.Url());
	str = "GET /favicon.ico HTTP/1.1\r\n"
		"Host: 0.0.0.0=5000\r\n"
		"User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9) Gecko/2008061015 Firefox/3.0\r\n"
		"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
	size = parser.Parser(str);
	printf("errno %d size %lld\n", parser.Errno(), size);
	if (parser.Errno() != 0x7F) {
		return -3;
	}
	if (size != 0) {
		return -4;
	}
	UrlParser url1("https://www.baidu.com/s?ie=utf8&oe=utf8&wd=httplib&tn=98010089_dg&ch=3");

	int ret = url1.Parser();
	if (ret != 0) {
		printf("urlparser1 failed:%d\n", ret);
		return -5;
	}
	printf("ie = %s except:utf8\n", (char*)url1["ie"]);
	printf("oe = %s except:utf8\n", (char*)url1["oe"]);
	printf("wd = %s except:httplib\n", (char*)url1["wd"]);
	printf("tn = %s except:98010089_dg\n", (char*)url1["tn"]);
	printf("ch = %s except:3\n", (char*)url1["ch"]);
	UrlParser url2("http://127.0.0.1:19811/?time=144000&salt=9527&user=test&sign=1234567890abcdef");
	ret = url2.Parser();
	if (ret != 0) {
		printf("urlparser2 failed:%d\n", ret);
		return -6;
	}
	printf("time = %s except:144000\n", (char*)url2["time"]);
	printf("salt = %s except:9527\n", (char*)url2["salt"]);
	printf("user = %s except:test\n", (char*)url2["user"]);
	printf("sign = %s except:1234567890abcdef\n", (char*)url2["sign"]);
	printf("host:%s port:%d\n", (char*)url2.Host(), url2.Port());
	return 0;
}

int main()
{
	int ret = http_test();
	printf("main:ret = %d\n", ret);
	return ret;
}
