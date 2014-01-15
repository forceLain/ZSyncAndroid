package com.zsync.android;

import android.app.Activity;
import android.os.Bundle;
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
				button.setText(String.valueOf(zsync.sync(URL)));
			}
		});
		setContentView(button);
		button.setText("...");
	}

}
