package com.tom.usbdetector;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class BootupReceiver extends BroadcastReceiver {
	
	private final String TAG = "BootupReceiver" ;
	
	public BootupReceiver() {
	}

	@Override
	public void onReceive(Context context, Intent intent) {
		Log.d(TAG , "onReceive intent = " + intent);
		//Intent service = new Intent(context,ListenerService.class);
		//context.startService(service);
		
	}
}
