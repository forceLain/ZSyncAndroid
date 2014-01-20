package com.zsync.android;

import android.content.Context;

import com.zsync.android.GetZsyncControlFileTask.GetZsyncControlFileTaskCallback;

public class ZSync {
	
	static {
	    System.loadLibrary("zsyncandroid");
	}
	
	private static final String ZSYNC_TEMP_FILENAME = "zsynccontrolfile.tmp";
	private static final String TAG = ZSync.class.getSimpleName();
	private String path;
	private String url;
	private String zFileName;
	
	public void startSync(Context ctx, final String url, final String dir){
		this.path = dir;
		this.url = url;
		zFileName = path+ZSYNC_TEMP_FILENAME;
		
		GetZsyncControlFileTask task = new GetZsyncControlFileTask(zFileName, callback);
		task.execute(url);
	}
	
	private GetZsyncControlFileTaskCallback callback = new GetZsyncControlFileTaskCallback() {
		
		@Override
		public void onSuccess() {
			init(url, path, zFileName);
			makeTempFile();
			readLocal();
			renameTempFile();
			fetchRemainingBlocks();
			completeZsync();
			completeFile();
			release();
		}
		
		@Override
		public void onError(String error) {
			
		}
	};
	
	private native void init(String url, String path, String zfile);
	private native void makeTempFile();
	private native void renameTempFile();
	private native void fetchRemainingBlocks();
	private native void completeZsync();
	private native void completeFile();
	private native void readLocal();
	private native void release();
}
