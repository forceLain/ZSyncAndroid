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

public class MainActivity extends Activity implements OnClickListener {
	
	Button go;
	TextView textView;
	
	private static final String URL_1 = "http://162.243.253.131/prod/business.json-0.zip.zsync";
	private static final String URL_2 = "http://162.243.253.131/prod/lifestyle.json-0.zip.zsync";
	private static final String BAD_URL = "http://162.243.253.131/prod/preview.json-0.zip.zsync";
	private static final String TAG = MainActivity.class.getName();
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main_activity);
		go = (Button) findViewById(R.id.go);
		go.setOnClickListener(this);
		textView = (TextView) findViewById(R.id.text_log);
	}

	@Override
	public void onClick(View v) {
		
		File file = new File(getExternalFilesDir(
	            Environment.DIRECTORY_DOWNLOADS), "zsync");
		
		String[] list = {
				"http://162.243.253.131/prod/business.json-0.zip.zsync",
				"http://162.243.253.131/prod/lifestyle.json-0.zip.zsync",
				"http://162.243.253.131/prod/local.json-0.zip.zsync",
				"http://162.243.253.131/prod/multimedia.json-0.zip.zsync",
				"http://162.243.253.131/prod/opinions.json-0.zip.zsync",
				"http://162.243.253.131/prod/politics.json-0.zip.zsync",
				"http://162.243.253.131/prod/sports.json-0.zip.zsync",
				"http://162.243.253.131/prod/top-stories.json-0.zip.zsync",
				"http://162.243.253.131/prod/world.json-0.zip.zsync",
				"http://162.243.253.131/prod/preview.json-0.zip.zsync",
		};
		
		for (String string : list) {
			MyTask task = new MyTask(string, file);
			task.execute();
		}
		textView.setText("---");
		
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
			textView.setText(textView.getText()+"\nLoaded: "+result);
		}
		
	}
}
