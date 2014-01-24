package com.zsync.android;

import java.io.File;

import android.app.Activity;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends Activity implements OnClickListener {
	
	Button state1, state2, jniMigration;
	TextView textView;
	
	private static final String URL_STATE_1 = "http://162.243.253.131/state2/bundle_top_stories.zip.zsync";
	private static final String URL_STATE_2 = "http://162.243.253.131/state3/bundle_top_stories.zip.zsync";
	private static final String TAG = MainActivity.class.getName();
	
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
		
		String url;
		switch (v.getId()) {
		case R.id.state1:
			url = URL_STATE_1;
			break;
		case R.id.state2:
			url = URL_STATE_2;
			break;

		default:
			url = null;
			break;
		}
		
		if (url != null){
			File file = new File(getExternalFilesDir(
		            Environment.DIRECTORY_DOWNLOADS), "zsync");
			MyTask task = new MyTask(url, file);
			task.execute();
			textView.setText("---");
		}
		
	}
	
	private class MyTask extends AsyncTask<Void, Void, Long>{
		
		private String url;
		private File tempDir;

		public MyTask(String url, File tempDir){
			this.url = url;
			this.tempDir = tempDir;
		}

		@Override
		protected Long doInBackground(Void... params) {
			ZSync zsync = new ZSync(MainActivity.this);
			try {				
				zsync.syncOrThrow(url, tempDir);
			} catch (Exception e) {
				Log.e(TAG, e.getMessage());
			}
			
			return zsync.getBytesLoaded();
		}
		
		@Override
		protected void onPostExecute(Long result) {
			textView.setText("Loaded: "+result);
		}
		
	}
}
