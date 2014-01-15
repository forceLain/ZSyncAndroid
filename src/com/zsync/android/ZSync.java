package com.zsync.android;

public class ZSync {
	
	static {
	    System.loadLibrary("zsyncandroid");
	}
	
	public int sync(String url){
		return dosync(url);
	}
	
	private native int dosync(String url);

}
