package com.zsync.android;

import java.io.File;

import android.content.Context;

public class ZSync{
	static {
	    System.loadLibrary("zsyncandroid");
	}
	private static final String TEMP_DIR = "ztemp";
	private static final String TAG = ZSync.class.getSimpleName();
	
	private Context ctx;
	
	public ZSync(Context ctx){
		this.ctx = ctx;
	}
	
	public boolean sync(final String url, final File workingDir){
		workingDir.mkdirs();
		if (!workingDir.isDirectory()){
			return false;
		}
		
		File tempDir = new File(workingDir.getAbsolutePath()+File.separator+TEMP_DIR);
		tempDir.mkdirs();
		
		if (!tempDir.isDirectory()){
			return false;
		}
		
		init(url, tempDir.getAbsolutePath()+File.separator);
		if (readControlFile() != 0){
			release();
			deleteRecursive(tempDir);
			return false;
		}
		String fileName = getOriginName();
		String fullName = workingDir.getAbsolutePath()+File.separator+fileName;
		String tempFileName = fullName+".part";
		setFileName(fullName);
		setTempFileName(tempFileName);
		if (isFileExists(fullName)){
			fetchFromLocal(fullName);
		}
		if (isFileExists(tempFileName)){
			fetchFromLocal(tempFileName);
		}
		restartZsyncTempFile();
		if (fetchFromUrl() != 0){
			release();
			deleteRecursive(tempDir);
			return false;
		}
		applyDiffs();
		long modifTime = getZsyncMtime();
		File originFile = renameFile(tempFileName, fullName);
		if (originFile != null && originFile.isFile() && modifTime != -1){
			originFile.setLastModified(modifTime);
		}
		release();
		deleteRecursive(tempDir);
		return true;
	}
	
	private void deleteRecursive(File fileOrDirectory) {
	    if (fileOrDirectory.isDirectory()){
	    	for (File child : fileOrDirectory.listFiles()){
	    		deleteRecursive(child);	    		
	    	}
	    }
	    fileOrDirectory.delete();
	}
	
	private boolean isFileExists(String filename) {
		File file = new File(filename);
		return file.isFile();
	}
	
	private File renameFile(String oldFileName, String newFileName) {
		File oldfile = new File(oldFileName);
		if (oldfile.isFile()){
			File newFile = new File(newFileName);
			oldfile.renameTo(newFile);
			return newFile;
		}
		return null;
	}

	private native void init(String url, String tempDir);
	private native int readControlFile();
	private native void restartZsyncTempFile();
	private native int fetchFromUrl();
	private native void applyDiffs();
	private native void readLocal();
	private native void release();
	private native String getOriginName();
	private native void setFileName(String fileName);
	private native void fetchFromLocal(String fileName);
	private native void setTempFileName(String tempFileName);
	private native long getZsyncMtime();
}