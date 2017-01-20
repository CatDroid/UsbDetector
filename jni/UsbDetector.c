/*
 * UsbDetector.c
 *
 *  Created on: 2015-3-16
 *      Author: tom
 */

#undef __cplusplus

#include <stdio.h>
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <poll.h>
#include <assert.h>

// 没有导出来，不能用 jniRegisterNativeMethods NELEM 等
// #include "JNIHelp.h"

#include "prebuilt/CreateSecondLib.h"


#define INFO 1
#define DEBUG 1
#if DEBUG
#define ALOGD(x...)  __android_log_print(ANDROID_LOG_DEBUG,"UsbDetectorJNI",x)
#else
#define ALOGD(x...)  do {} while (0)
#endif
#define ALOGE(x...) __android_log_print(ANDROID_LOG_ERROR,"UsbDetectorJNI",x)
// 打印是： D/UsbDetectorJNI( 1901): Java_com_tom_usbdetector_ListenerService_getJNIInfo
// 要加上 LOCAL_LDLIBS    := -llog

#if defined(__cplusplus)
#warning "define c++"
#endif

const static int TIMEOUT = 10000;
static pthread_t sUsbThread;
static jboolean sExit = JNI_FALSE;
static int sPipeFd[2]; // 0 read 1 write
static jobject sJniCallbacksObj;
static JNIEnv* sUsbDetectorEnv;
static JavaVM* sJavaVM;
static jmethodID sMethod_sendUSBplug;

static jboolean checkCallbackThread(JavaVM* vm) {

	JNIEnv* env  = NULL;
    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
    	return JNI_ERR;
    }

    assert(env != NULL);
    ALOGD("checkCallbackThread CurrentJniEnv = %p, sUsbDetectorEnv = %p\n", env , sUsbDetectorEnv );

    // 当前的线程就是 原来attached的线程 通过JNIEnv指向的地址判断
    if (sUsbDetectorEnv != env || sUsbDetectorEnv == NULL) {
        ALOGE("Callback env check fail: env: %p, callback: %p", env, sUsbDetectorEnv);
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

static void setupUsbDetectorEnv(jboolean attach ) {


	assert(sJavaVM != NULL); // assert 在默认情况下 是没有作用的，需要配置debug

	/*
	 * assert 断言，如果不是这样的话，就会出现错误
	 * F/libc    ( 3972): jni/UsbDetector.c:64: setupUsbDetectorEnv: assertion "sJavaVM == NULL" failed
	 * F/libc    ( 3972): Fatal signal 6 (SIGABRT) at 0x00000f84 (code=-6), thread 3989 (tom.usbdetector)
	 */

    JavaVM* vm =  sJavaVM;

    if (attach  == JNI_TRUE) {
        JavaVMAttachArgs args;
        char name[] = "UsbDetectorThread";
        args.version = JNI_VERSION_1_6;
        args.name = name;
        args.group = NULL;
        ALOGD("111");
        (*vm)->AttachCurrentThread(vm,&sUsbDetectorEnv, &args); // 这样DDMS中就可以看到有一个线程UsbDetectorThread
        ALOGD("Callback thread attached: %p", sUsbDetectorEnv);
    } else {
    	if (!checkCallbackThread(vm)) {
    		ALOGE("Callback: '%s' is not called on the correct thread", __FUNCTION__);
    		return;
        }
        (*vm)->DetachCurrentThread(vm); // detachCurrentThread 需要在原来attached的线程中调用
    }
    ALOGD("setupUsbDetectorEnv %s Done" , (attach?"Attach":"Detach"));
}


static jboolean postUSBplug(JNIEnv* env, int direction )
{
	//jmethodID mid;
	// 非主线程 jnienv FindClass 导致  java.lang.ClassNotFoundException
	//jclass cls = (*env)->FindClass(env,"com/tom/usbdetector/ListenerService"); //后面是包名+类名
	//if(cls == NULL) ALOGE("class is null");
	//mid = (*env)->GetMethodID(env,cls,"sendUSBplug","(I)Z");// TestMethod java中的方法名
	//if(mid == NULL) ALOGE("method error");
	// 不同的返回参数类型，调用不同的方法 CallBooleanMethod 返回的是boolean CallObjectMethod 返回的是L/类

	// object必须使globalref
	// JNI ERROR (app bug): accessed stale local reference
	jboolean result = (*env)->CallBooleanMethod(env, sJniCallbacksObj, sMethod_sendUSBplug, (jint)direction); //object 注意下是jni传过来的jobject
	return result;
}


static void* usb_thread(void* argv)
{
	struct sockaddr_nl snl;
	const int buffersize = 16*1024;
	int retval = 0;
	int hotplug_sock = 0;
	int stopFd = *( (int*) argv );// pipe read endpoit

	ALOGD("usb_thread start ");
	ALOGD("PID = 0x%x, TID = 0x%x\n", getpid() , pthread_self());

	setupUsbDetectorEnv(JNI_TRUE);

	ALOGD("Create Socket\n");
	memset(&snl, 0x0, sizeof(snl));
	snl.nl_family = AF_NETLINK;
	snl.nl_pid = pthread_self() << 16 | getpid();
	snl.nl_groups = 0xFFFFFFFF;
	hotplug_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
	if (hotplug_sock == -1) {
		ALOGE("####socket is failed in %s error:%d %s\n", __FUNCTION__, errno, strerror(errno));
		retval = -1;
		pthread_exit(NULL);
	}

	ALOGD("Set Socket Opt\n");
	setsockopt(hotplug_sock, SOL_SOCKET, SO_RCVBUFFORCE, &buffersize, sizeof(buffersize));

	ALOGD("Bind Socket\n");
	retval = bind(hotplug_sock, (struct sockaddr *)&snl, sizeof(struct sockaddr_nl));
	if (retval < 0) {
		ALOGE("####bind is failed in %s error:%d %s\n", __FUNCTION__, errno, strerror(errno));
		close(hotplug_sock);
		pthread_exit(NULL);
	}

	//  必须把新建的线程attached到JVM虚拟机，否这回调Java方法会遇到：
	//  JNI ERROR: non-VM thread making JNI call


	ALOGD("Start Poll Loop\n");
	while(1)
	{
		char buf[4096*2] = {0};
		int ready;

        struct pollfd fds[2];
        fds[0].fd = hotplug_sock;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fds[1].fd = stopFd;
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        ready = poll(&fds, sizeof(fds)/sizeof(struct pollfd), TIMEOUT);

        if(sExit){

        	ALOGD("errno = %d EINTR = ?%d " , errno, EINTR);
        	if(ready > 0 && fds[1].events == POLLIN){
        		int readsize = read(stopFd, buf  ,sizeof(buf) );
        		ALOGD("exit buf = %s\n" , buf );
        	}

        	ALOGD("usb detector loop exit!");
        	break;

        }

        if(ready > 0 && fds[0].events == POLLIN)
        {
    		int count = recv(hotplug_sock, &buf, sizeof(buf),0);
    		if(count > 0)
    		{

#if  INFO == 1
				const char *checkitnow = buf;
				ALOGD("\n\n####received : \n");
				while( checkitnow ){
					ALOGD("%s ", checkitnow );
					checkitnow += strlen(checkitnow) + 1;
					if( checkitnow - buf >= count ){
						break;
					}
				}
				 // 控制台日志
				//printf("console log\n");
				fprintf(stdout , "console log\n");
				ALOGD("####received End\n\n\n");
#endif

    		    char* s  = buf ;
    		    s += strlen(s) + 1; // 去掉第一行，第一行都是 remove@xxx add@xxx change@xxx等形式的信息 无需进入比较
    		    unsigned char isAdd = 2 ;
    		    while( s ) {

    		    	if (!strncmp(s, "ACTION=", strlen("ACTION="))){
    		    		int len = strlen("ACTION=") ;
    		    		if( !strncmp(s + len , "add" , strlen("add") )  ){
    		    			isAdd = 1;
    		    		}else if( !strncmp(s + len , "remove" ,strlen("remove")) ){
    		    			isAdd = 0;
    		    		}
    		    	}
    		    	if (!strncmp(s, "SUBSYSTEM=", strlen("SUBSYSTEM="))){
						unsigned char flag = strncmp(s + strlen("SUBSYSTEM="), "hid",strlen("hid"));
						ALOGD("current_state = %d\n" , flag);
						if( flag == 0 ){ // =="hid"
							if( isAdd == 1){
								//postUSBplug( sUsbDetectorEnv , com_tom_usbdetector_ListenerService_ListenerHandler_MSG_USB_PLUGIN);
							}else if(isAdd == 0){
								//postUSBplug( sUsbDetectorEnv , com_tom_usbdetector_ListenerService_ListenerHandler_MSG_USB_PLUGOUT);
							}

						}
						break;
    		    	}
    		    	s += strlen(s) + 1;
    		    	if( s - buf >= count) break;
    		    }


			}
		}else{
			ALOGD("poll timeout");
		}
	}
	ALOGD("End Poll Loop\n");

	setupUsbDetectorEnv(JNI_FALSE);


	int* result = (int*)malloc(sizeof(int));
	memset(result,0,sizeof(*result));
	*result = 0;
	pthread_exit(result);  // 不能是在线程栈局部变量 可以是堆malloc或者是数据段static局部变量


	return NULL ;
}

JNIEXPORT jstring JNICALL Java_com_tom_usbdetector_ListenerService_getJNIInfo( JNIEnv* env, jobject thiz )
{
	// env ：
	// 你会发现env参数使用的参数完全不一样，这是因为前者是C++源文件，
	// 后者是C源文件，结构JNIEnv在两种环境中定义方式不一样，如果按照C的方式去写肯定没法编译通过

    return (*env)->NewStringUTF(env,"Usb Detector Native works");
}

JNIEXPORT void JNICALL Java_com_tom_usbdetector_ListenerService_setJNIString
  (JNIEnv * env , jobject obj, jstring jstr)
{
		jboolean isCopy = JNI_TRUE;

		// GetStringUTFChars 和 GetStringChars 返回的是 maybe a copy of java string
		// 根据java.lang.String 的不可变语义(?? 不知道原因)
		// 一般会把isCopy设为NULL,不关心Java VM对返回的指针是否直接指向 java.lang.String 的内容
		char * strUtf = (*env)->GetStringUTFChars(env,jstr,&isCopy);

		if( NULL == strUtf ){//不要忘记检测，否则分配内存失败会抛出异常
			return ;/* OutOfMemoryError already thrown */
			// 在 JNI 中, 发生异常,不会改变代码执行轨迹,所以,当返回 NULL,要及时返回,或马上处理异常
		}

		// GetStringUTFLength 等到的是字节的长度，不含'\0'
		// GetStringChars 等到的是字符的长度
		jsize lengthUtf = (*env)->GetStringUTFLength(env,jstr);
		ALOGD("strUtf = %s , length = %d , %s\n" , strUtf, lengthUtf,
				isCopy?"C String is a copy of the Java String;"
						: "C String points to actual string;");
		// strUtf = 你好 中国 , length = 13 , C String is a copy of the Java String; 副本
		// UTF-8 字符串以’\0’结尾，而 Unicode 字符串不是

// 如果一个jstring指向一个 UTF-8编码的字符串
// 为了得到这个字符串的字节长度，可以调用标准 C 函数 strlen,当然也可以用GetStringUTFLength

		char * strUnicode = (*env)->GetStringChars(env,jstr,&isCopy);
		jsize lengthUni = (*env)->GetStringLength(env,jstr);
		ALOGD("strUnicode = %s , length = %d , %s\n" , strUnicode,lengthUni,
				isCopy?"C String is a copy of the Java String;"
						: "C String points to actual string;");
		// strUnicode = `O}Y  , length = 5 , C String points to actual string; 堆中对象

		strUtf[lengthUtf - 2] = '2'; // 这样只修改Native层副本的字符串，但是原来Java层的没有修改
		strUnicode[ lengthUni - 1] = '3'; // 这样真的会修改原来Java的字符串

		ALOGD("change Java String ? str = %s \n" ,strUtf);

		(*env)->ReleaseStringUTFChars(env, jstr ,strUtf);//JNIEnv* JavaString, char*
		(*env)->ReleaseStringChars(env,jstr, strUnicode);
		/*
		 * 记住在调用 GetStringChars 之后,要调用 ReleaseStringChars 做释放,
		 * 不管在调用 GetStringChars 时为 isCopy 赋值 JNI_TRUE 还是 JNI_FALSE,因不同 JavaVM 实现的原因,
		 * ReleaseStringChars 可能释放内存,也可能释放一个内存占用标记(isCopy 参数的作用,
		 * 从 GetStringChars 返回一个指针,该指针直接指向 String 的内容,
		 * 为了避免该指针指向的内 容被 GC,要对该内存做锁定标记)
		 */


		return ;

}


JNIEXPORT void JNICALL Java_com_tom_usbdetector_ListenerService_setJNIIntArray
  (JNIEnv * env , jobject obj , jintArray javaArray)
{
	jboolean isCopy = JNI_TRUE ;
	jint* point2JavaArrayElements = (*env)->GetIntArrayElements(env,javaArray,&isCopy);
	ALOGD("isCopy = %d , point2JavaArrayElements = %d ", isCopy , *point2JavaArrayElements );
	*(point2JavaArrayElements+1)  = 12 ;// isCopy = 0
	(*env)->ReleaseIntArrayElements(env, javaArray, point2JavaArrayElements , 0 );//JNI_COMMIT JNI_ABORT

	return ;
}

// jni 层 数组不是用int[]来透明映射，而是通过jintArray来间接不透明映射(代表需要通过相应方法来对数据进行处理)
JNIEXPORT jcharArray JNICALL Java_com_tom_usbdetector_ListenerService_oneplusArray( JNIEnv* env , jclass clazz, jintArray input)
{
	const jint length = (*env)->GetArrayLength(env , input);//获得Java数组长度
	ALOGD("oneplusArray len = %d",length);
	if(length <= 0) goto ErrSize;

	jint* copyArray = (jint*)malloc(length);
	if(copyArray == NULL) goto ErrMalloc;

	(*env)->GetIntArrayRegion(env, input, 0, length, copyArray );// 把Java的数组拷贝到C数组中

	jcharArray charArray = (*env)->NewCharArray(env,length);// 在Native/JNI层新建Java数组
	if(charArray == NULL) goto ErrNewArray;
	jchar* pCharArray = (*env)->GetCharArrayElements(env,charArray,NULL);// 获取Java数组直接操作指针(需要释放)
	if(pCharArray == NULL) goto ErrGetArray;

	int i = 0;
	for( i = 0 ; i <= length ; i++ , pCharArray++){
		(*pCharArray) =  copyArray[i] + 'a';
	}

	(*env)->ReleaseCharArrayElements(env,charArray,pCharArray,0);//最后一个 0 将内容复制回来，并释放数组

	for(i = 0; i <= length ; i++ ){
		copyArray[i] += 1;
	}
	(*env)->SetIntArrayRegion(env, input ,0, length ,copyArray ); // 从C数组向Java数组提交修改(使用C数组给Java数组赋值)

	if( copyArray != NULL )
		free(copyArray);
	else
		ALOGD("free ? ?");

	return charArray;

ErrGetArray:
ErrNewArray:
	if( copyArray != NULL ) free(copyArray);
ErrMalloc:
ErrSize:
	return NULL;
}

JNIEXPORT jboolean JNICALL Java_com_tom_usbdetector_ListenerService_startUsbEventThread( JNIEnv* env , jobject thiz)
{
	ALOGD("Thread 1 env = %p " , env );

	if(pipe(sPipeFd) < 0) // 创建pipe用于退出
	{
		ALOGE("pipe error!\n");

		jclass ex_clazz = (*env)->FindClass(env,"java/lang/NullPointerException");
		if( NULL != ex_clazz ){
			(*env)->ThrowNew(env, ex_clazz,"Pipe Create Error");
		}
		ALOGD( "Native Generate Exception But Keep going");
		// 如果在JNIEnv抛出exception之后，还继续使用JNIEnv调用函数(比如env->FindClass env->NewStringUTF)，就会出现异常：
		// JNI WARNING: JNI function FindClass called with exception pending
		// Pending exception is:
		// java.lang.NullPointerException: Pipe Create Error
		return JNI_FALSE;

	}else{
		ALOGD("pipe read = %d, write = %d\n" , sPipeFd[0] , sPipeFd[1] );
	}


    jclass cls = (*env)->FindClass(env,"com/tom/usbdetector/ListenerService"); //后面是包名+类名
    if(cls == NULL) ALOGE("class is null");
    jmethodID mid = (*env)->GetMethodID(env,cls,"sendUSBplug","(I)Z");// TestMethod java中的方法名
    if(mid == NULL) ALOGE("method error");
    // findClass 要global_ref getmethodID 就不要
    //sMethod_sendUSBplug = (*env)->NewGlobalRef(env , mid );
    sMethod_sendUSBplug = mid;


    /*
     * 全局引用在一个本机方法的多次不同调用之间使用。
     * 他们只能通过使用NewGlobalRef函数来创建。
     * 全局引用可以在几个线程之间使用。全局引用提供了诸多好处。
     *
     * 但是有一个小问题：Java无法控制全局引用的生命周期。
     * 用户必须自行判断全局引用何时不再需要，同时使用DeleteGlobalRef来释放他
     *
     */
	sJniCallbacksObj = (*env)->NewGlobalRef(env,thiz);


	int ret = pthread_create(&sUsbThread, NULL, usb_thread, &sPipeFd[0]);
	return (ret==0)?JNI_TRUE:JNI_FALSE;
}

// 注意，如果 Java_com_tom_usbdetector_ListenerService_stopUsbEventThread Java上面定义他是static的话，
// 		那么传下来的jobject是Class对象/实例，而不是这个类的实例，CallXXXMethod就会出现异常(相当与用Class.Method的方法调用了实例方法)
JNIEXPORT jboolean JNICALL Java_com_tom_usbdetector_ListenerService_stopUsbEventThread( JNIEnv* env , jobject thiz)
{

	if(sPipeFd[1] <= 0){
		return JNI_FALSE;
	}

	sExit = JNI_TRUE;
	const char* wakeup = "User Want USB Thread to STOP" ;
	errno = 0;
	int ret = write( sPipeFd[1], wakeup, strlen(wakeup)+1 );
	ALOGD("Java->JNI STOP Write ret = %d, errno = %d\n",ret,errno);

	void *thread_result;
	pthread_join(sUsbThread,&thread_result);
	ALOGD("Usb Thread Stop Over result = 0x%x",*(int*)thread_result);


	(*env)->DeleteGlobalRef(env,sJniCallbacksObj); // 删除全局引用
	sJniCallbacksObj = NULL;
	//(*env)->DeleteGlobalRef(env,sMethod_sendUSBplug);
	//sMethod_sendUSBplug = NULL;

	checkCallbackThread(sJavaVM); // 主线程的JNIEnv，跟新线程的JNIEnv，是不一样的

	jboolean result = JNI_FALSE;
	result = ( *(int*)thread_result == 0 )? JNI_TRUE:JNI_FALSE ;
	free(thread_result);
	ALOGD("JNI return result = %d",result); // JNI_TRUE = 1 ; JNI_FALSE = 0
	return result; // 对应java的false关键字
}


JNIEXPORT jint Java_com_tom_usbdetector_ListenerService_JavaThread( JNIEnv* env , jobject thiz ,jstring str)
{
	ALOGD("Thread 2 env = %p " , env );
	const char* thread2str =  (*env)->GetStringUTFChars(env, str, NULL);
	if( NULL == thread2str ){
		jclass exceptionClazz = (*env)->FindClass(env,"java/lang/RuntimeException");
		(*env)->ThrowNew(env, exceptionClazz, "Unable GetStringUTFChars");
		return -1;
	}
	ALOGD("thread2str = %s\n" , thread2str);
	 (*env)->ReleaseStringUTFChars(env,str ,thread2str);
	return 0;
}

JNIEXPORT void android_dynamic_register_native()
{
	ALOGD("android_dynamic_register_native DONE !");
}

static JNINativeMethod gMethods[] = {
    {
        "android_dynamic_register_native",
        "()V",
        (void *)android_dynamic_register_native
    },
};

// 在NDK环境下，由于没有AndroidRuntime(不能调用AndroidRuntime::getJNIEnv AndroidRuntime::getJavaVM) 所以要用JNI_OnLoad
// 在SDK环境下 没有这个限制
jint JNI_OnLoad(JavaVM *jvm, void *reserved)
{
	JNIEnv* env = NULL;
    int status;

    ALOGD("UsbDetector: loading JNI Start \n");

    // Check JNI version
    if ((*jvm)->GetEnv(jvm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
    	return JNI_ERR;
    }
    assert(env != NULL);

    sJavaVM = jvm;


    ALOGD("UsbDetector: loading JNI Done TOMCAT = %d\n", TOMCAT);

    jclass clazz;
    static const char* const kClassName =  "com/tom/usbdetector/ListenerService";
    clazz = (*env)->FindClass(env,kClassName);
    assert( clazz!= NULL );

    (*env)->RegisterNatives(env, clazz , gMethods , sizeof(gMethods) / sizeof(gMethods[0]) );

    (*env)->DeleteLocalRef(env,clazz);
// 看Eclipse的Console
#ifdef NDEBUG
#warning "define Debug"
#else
#warning "NOT define Debug"
#endif
// 调用第三方提供的动态库
    int ret = callPrebuiltLibFun(12);

    ret = callPrebuiltLibFunStr("hello");


// setprop libc.debug.malloc 10
    // 1 执行内存泄漏探测
    // 5 填补分配的内存检查超支出
    // 10 填补内存 和 定点检查超支

//    char* buffer = (char*)malloc(1024);
//    int j = 0;
//    for( ; j <= 1024 ; j++ )
//    {
//    	buffer[j] = 'a';
//    }
//    free(buffer);


 // adb logcat | ndk-stack -sym  obj/local/armeabi-v7a/
 // *((char*)0 ) = 0;


    // __android_log_assert("0!=0","UsbDetectorJNI","0 === 0 ");
    // 打印assert之后，会停止运行
    return JNI_VERSION_1_6;
}


// 如果没有JNI_OnLoad函数，那么加载动态库 就会显示
// No JNI_OnLoad found in /data/app-lib/com.tom.usbdetector-1/libUsbDetector.so

//sigset_t sig;
//sigemptyset(&sig); //清空信号集
//sigaddset(&sig,SIGUSR1); //将某个信号加入到信号集中
//pthread_sigmask(SIG_BLOCK,&sig,NULL);//设置该线程的信号屏蔽字为SIGUSR1

//struct sigaction action;
//memset(&action, 0, sizeof(action));
//sigemptyset(&action.sa_mask);
//action.sa_flags = 0;
//action.sa_handler = stop_signal;
//int rc = sigaction(SIGUSR1, &action, NULL);
//if(rc)
//{
//	ALOGE("sigaction error/n");
//	// To Do ??
//}


//void stop_signal(int signo)
//{
//	ALOGD("signo = %d" , signo);
//    if(signo != SIGUSR1)
//    {
//        ALOGD("unexpect signal %d/n", signo);
//        return ;
//    }
//
//	int* result = (int*)malloc(sizeof(int));
//	memset(result,0,sizeof(*result));
//	*result = 0;
//	pthread_exit(result);
//
//}

//pthread_cancel(sUsbThread); ANDROID NDK do NOT support pthread_cancel

//	接收不到信号
//	int kill_ret = pthread_kill(sUsbThread, SIGUSR1);// 给线程的信号
//	if(kill_ret == 0)
//		ALOGD("kill signal send out OK"); // 只是代表信号发送成功
//	else if (kill_ret == ESRCH) // 线程不存在
//		ALOGD("指定的线程不存在或者是已经终止\n");
//	else if(kill_ret == EINVAL) // 信号不合法
//		ALOGD("调用传递一个无用的信号\n");

