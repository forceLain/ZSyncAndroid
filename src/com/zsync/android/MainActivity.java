package com.zsync.android;

import java.io.File;

import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;

public class MainActivity extends Activity {
	
	private static final String URL = "http://162.243.253.131/state1/bundle_top_stories.zip.zsync";
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		final Button button = new Button(this);
		button.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View v) {
				ZSync zsync = new ZSync();
				File file = new File(getExternalFilesDir(
			            Environment.DIRECTORY_DOWNLOADS), "zsync");
				file.mkdirs();
				String path = file.getAbsolutePath()+File.separator;
				button.setText(String.valueOf(zsync.sync(MainActivity.this, URL, path)));
			}
		});
		setContentView(button);
		button.setText("...");
	}

}
