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
	private long bytesLoaded;
	
	public ZSync(Context ctx){
		this.ctx = ctx;
	}
	
	public long getBytesLoaded(){
		return bytesLoaded;
	}
	
	public void syncOrThrow(final String url, final File workingDir){
		bytesLoaded = 0;
		
		workingDir.mkdirs();
		if (!workingDir.isDirectory()){
			throw new IllegalArgumentException(workingDir.getAbsolutePath()+" is not a directory");
		}
		
		File tempDir = new File(workingDir.getAbsolutePath()+File.separator+TEMP_DIR);
		tempDir.mkdirs();
		
		if (!tempDir.isDirectory()){
			throw new RuntimeException("Cannot create temp directory "+tempDir.getAbsolutePath());
		}
		
		init(url, tempDir.getAbsolutePath()+File.separator);
		if (readControlFile() != 0){
			release();
			deleteRecursive(tempDir);
			throw new RuntimeException("Cannot read zsync control file");
		}
		String fileName = getOriginName();
		String fullName = workingDir.getAbsolutePath()+File.separator+fileName;
		String tempFileName = fullName+".part";
		setFileName(fullName);
		setTempFileName(tempFileName);
		if (isFileExists(fullName)){
			fetchFromLocal(fullName); //if we have an old version
		}
		if (fetchFromUrl() != 0){
			release();
			deleteRecursive(tempDir);
			throw new RuntimeException("Cannot fetch remaingn blocks from url "+url);
		}
		if (applyDiffs() != 0){
			release();
			deleteRecursive(tempDir);
			throw new RuntimeException("Error while applying diffs to local version");
		}
		long modifTime = getZsyncMtime();
		File originFile = renameFile(tempFileName, fullName);
		if (originFile != null && originFile.isFile() && modifTime != -1){
			originFile.setLastModified(modifTime);
		}
		bytesLoaded = release();
		deleteRecursive(tempDir);
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
	private native int fetchFromUrl();
	private native int applyDiffs();
	private native void readLocal();
	private native long release();
	private native String getOriginName();
	private native void setFileName(String fileName);
	private native void fetchFromLocal(String fileName);
	private native void setTempFileName(String tempFileName);
	private native long getZsyncMtime();
}