package com.zsync.android;

import android.content.Context;

public class ZSync {
	
	static {
	    System.loadLibrary("zsyncandroid");
	}
	
	public int sync(Context ctx, String url, String dir){
		return dosync(url, dir);
	}
	
	private native int dosync(String url, String path);

}
