package com.zsync.android;

import java.io.File;

import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;

public class MainActivity extends Activity implements OnClickListener {
	
	Button state1, state2, jniMigration;
	TextView textView;
	
	private static final String URL_STATE_1 = "http://162.243.253.131/state2/bundle_top_stories.zip.zsync";
	private static final String URL_STATE_2 = "http://162.243.253.131/state3/bundle_top_stories.zip.zsync";
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main_activity);
		state1 = (Button) findViewById(R.id.state1);
		state1.setOnClickListener(this);
		state2 = (Button) findViewById(R.id.state2);
		state2.setOnClickListener(this);
		textView = (TextView) findViewById(R.id.text_log);
	}

	@Override
	public void onClick(View v) {
		ZSync zsync = new ZSync(this);
		File file = new File(getExternalFilesDir(
	            Environment.DIRECTORY_DOWNLOADS), "zsync");
		
		switch (v.getId()) {
		case R.id.state1:
			zsync.sync(URL_STATE_1, file);
			break;
		case R.id.state2:
			zsync.sync(URL_STATE_2, file);
			break;
		}
	}
}
