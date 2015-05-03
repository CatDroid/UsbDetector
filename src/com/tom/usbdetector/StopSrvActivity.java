package com.tom.usbdetector;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;

public class StopSrvActivity extends Activity {

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_stop_srv);
		Intent service = new Intent(this,ListenerService.class);
		this.startService(service);
	}

	@Override
	protected void onDestroy() {
		Intent service = new Intent(this,ListenerService.class);
		this.stopService(service);
		super.onDestroy();
	}
	
}
