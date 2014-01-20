package com.zsync.android;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;

import android.os.AsyncTask;

public class GetZsyncControlFileTask extends AsyncTask<String, Void, String> {
	
	public interface GetZsyncControlFileTaskCallback{
		void onSuccess();
		void onError(String error);
	}
	
	private GetZsyncControlFileTaskCallback callback;
	private String fileName;
	
	public GetZsyncControlFileTask(String fileName, GetZsyncControlFileTaskCallback callback){
		this.fileName = fileName;
		this.callback = callback;
	}

	@Override
	protected String doInBackground(String... params) {
		InputStream input = null;
        OutputStream output = null;
        HttpURLConnection connection = null;
        try {
            URL url = new URL(params[0]);
            connection = (HttpURLConnection) url.openConnection();
            connection.connect();

            // expect HTTP 200 OK, so we don't mistakenly save error report 
            // instead of the file
            if (connection.getResponseCode() != HttpURLConnection.HTTP_OK){
            	return "Server returned HTTP " + connection.getResponseCode() 
                        + " " + connection.getResponseMessage();
            }

            // download the file
            input = connection.getInputStream();
            output = new FileOutputStream(fileName);

            byte data[] = new byte[4096];
            int count;
            while ((count = input.read(data)) != -1) {
                output.write(data, 0, count);
            }
        } catch (Exception e) {
            return e.toString();
        } finally {
            try {
                if (output != null)
                    output.close();
                if (input != null)
                    input.close();
            } 
            catch (IOException ignored) { }

            if (connection != null){
            	connection.disconnect();	                	
            }
        }
        return null;
	}
	
	@Override
	protected void onPostExecute(String result) {
		if (callback == null){
			return;
		}
		if (result == null){
			callback.onSuccess();
		} else {
			callback.onError(result);
		}
	}

}
