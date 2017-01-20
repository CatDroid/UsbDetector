package com.tom.usbdetector;

import java.util.Arrays;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.widget.Toast;

// adb logcat -s BootupReceiver ListenerService
public class ListenerService extends Service {
	
	private final static String TAG = "ListenerService";
	private ListenerThread mHandlerThread = null;
	private ListenerHandler mListenerHandler = null;
	
	static
	{
		System.loadLibrary("UsbDetector");
		int input[] = new int[]{2,2,2,2,2,2};
		char output[] = null;
		Log.d(TAG , "before input = " + Arrays.toString(input));
		output = oneplusArray(input);
		Log.d(TAG , "after input = " + Arrays.toString(input));
		Log.d(TAG , "output = " + Arrays.toString(output));
	}
	public native String getJNIInfo();
	public native void setJNIString(String str);
	public native void setJNIIntArray(int[] array);
	public native boolean startUsbEventThread();
	public native boolean stopUsbEventThread();
	static public native char[] oneplusArray(int[] array);
	public native void android_dynamic_register_native();
	
	// 在Java中创建线程调用JNI
	private native int JavaThread(String str);
	
	public ListenerService() {
		
	}

	@Override
	public IBinder onBind(Intent intent) {
		throw new UnsupportedOperationException("Not yet implemented");
	}

	@Override
	public void onCreate() {

		// loadLibrary path = /vendor/lib:/system/lib
		String libpath = System.getProperty("java.library.path");
		Log.d(TAG , "loadLibrary path = " + libpath);
		
		System.out.print("System.out Test! 2\n");
		
		
		android_dynamic_register_native();
		
		
		mHandlerThread = new ListenerThread("ServerService");// 如果用Thread 没有不能设置looper,在DDMS中线程可以看到
		mHandlerThread.start();// 必须先start才能 getLooper
		Log.d(TAG, "[onCreate] Handler Thread = " + mHandlerThread.getLooper());
		mListenerHandler = new ListenerHandler(mHandlerThread.getLooper(), this.getApplicationContext() );
		super.onCreate();
		Toast.makeText(this.getApplicationContext() , "get = " + getJNIInfo() , Toast.LENGTH_SHORT).show();
		Log.d(TAG, "get = " + getJNIInfo());
		try{
			Log.d(TAG, "startUsbEventThread = " + startUsbEventThread() );
		}catch(Exception ex){
			Log.e(TAG , "catch exception ");
			ex.printStackTrace();
		}
		
		String string = new String("你好 中国");
		Log.d(TAG ,"before JNI string = " + string);
		setJNIString(string);
		Log.d(TAG , "after JNI string = " + string);
		
		
		int[] intArray = new int[]{1,2,3,4,5};
		Log.d(TAG , "before Native , intArray = " + Arrays.toString(intArray));
		setJNIIntArray(intArray);
		Log.d(TAG , "after Native , intArray = " + Arrays.toString(intArray));
		
		
		new Thread(){

			@Override
			public void run() {
				
				int left = 10 ;
				while( --left != 0 ){
					Log.d(TAG , "Java Thread is Running left = " + left);
					JavaThread("test"+left);
					Log.d(TAG , "Java Thread is Running next = " + left);
				}
				super.run();
			}
				
		}.start();
	}
	
	

	@Override
	public void onDestroy() {
		Log.d(TAG, "stopUsbEventThread  Start ");
		Log.d(TAG, "stopUsbEventThread = " + stopUsbEventThread() );
		super.onDestroy();
		Log.d(TAG, "stopUsbEventThread  Done ");
	}

	@Override
	public int onStartCommand(Intent intent, int flags, int startId) {
		// TODO Auto-generated method stub
		return super.onStartCommand(intent, flags, startId);
	}
	
	// 如果Java类的函数是static(类方法)，那么在JNI的函数参数是
	// void classInitNative(JNIEnv* env, jclass clazz)
	
	// 如果Java类的函数是非static(实例方法)，那么在JNI的函数参数是
	// bool initNative(JNIEnv* env, jobject obj)
	
	public boolean sendUSBplug(int direction)
	{
		Log.d(TAG,"sendUSBplug");
		Message msg = new Message();
		if( direction == ListenerHandler.MSG_USB_PLUGIN ){ // 1 plug in  0 plug out
			msg.arg1 = ListenerHandler.MSG_USB_PLUGIN;
		}else if( direction == ListenerHandler.MSG_USB_PLUGOUT){
			msg.arg1 = ListenerHandler.MSG_USB_PLUGOUT;
		}else{
			return false;
		}
		mListenerHandler.sendMessage(msg);
		return true;
	}
	

	class ListenerThread extends HandlerThread
	{
		public ListenerThread(String name) {
			super(name);	
		}
	}
	
	class ListenerHandler extends Handler
	{

		private final static int MSG_USB_PLUGIN = 1;
		private final static int MSG_USB_PLUGOUT = 0; 
		private Context mContext = null;
		
		
		public ListenerHandler(Looper looper , Context context ) {
			super(looper);
			mContext = context;
		}

		@Override
		public void handleMessage(Message msg) {
			
			switch( msg.arg1 ){
			
				case MSG_USB_PLUGIN :
					// TODO start activity ?
					Log.d(TAG,"ListenerHandler MSG MSG_USB_PLUGIN ");
					Toast.makeText(mContext, "MSG_USB_PLUGIN", Toast.LENGTH_SHORT).show();
					break;
					
				case MSG_USB_PLUGOUT:
					// TODO start activity ?
					Log.d(TAG,"ListenerHandler MSG MSG_USB_PLUGOUT ");
					Toast.makeText(mContext, "MSG_USB_PLUGOUT", Toast.LENGTH_SHORT).show();
					break;
					
				default:
					Log.e(TAG , "Listener Handler Receive Error Msg");
					throw new UnsupportedOperationException("Not yet implemented");
					
			}
			super.handleMessage(msg);
		}
		
	}
	
}
