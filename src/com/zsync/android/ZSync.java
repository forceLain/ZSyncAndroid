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
	private long pointer;
	
	public ZSync(Context ctx){
		this.ctx = ctx;
	}
	
	public long getBytesLoaded(){
		return bytesLoaded;
	}
	
	public void syncOrThrow(final String url, final File workingDir){
		bytesLoaded = 0;
		workingDir.mkdirs();
		File tempDir = new File(workingDir.getAbsolutePath()+File.separator+TEMP_DIR);
		tempDir.mkdirs();
		
		pointer = init(url, tempDir.getAbsolutePath()+File.separator);
		if (readControlFile(pointer) != 0){
			release(pointer);
			throw new RuntimeException("Cannot read zsync control file");
		}
		String fileName = getOriginName(pointer);
		String fullName = workingDir.getAbsolutePath()+File.separator+fileName;
		String tempFileName = fullName+".part";
		setFileName(fullName, pointer);
		setTempFileName(tempFileName, pointer);
		if (isFileExists(fullName)){
			fetchFromLocal(fullName, pointer); //if we have an old version
		}
		if (fetchFromUrl(pointer) != 0){
			release(pointer);
			throw new RuntimeException("Cannot fetch remaingn blocks from url "+url);
		}
		if (applyDiffs(pointer) != 0){
			release(pointer);
			throw new RuntimeException("Error while applying diffs to local version");
		}
		long modifTime = getZsyncMtime(pointer);
		File originFile = renameFile(tempFileName, fullName);
		if (originFile != null && originFile.isFile() && modifTime != -1){
			originFile.setLastModified(modifTime);
		}
		bytesLoaded = release(pointer);
		pointer = 0;
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

	private native long init(String url, String tempDir);
	private native int readControlFile(long pointer);
	private native int fetchFromUrl(long pointer);
	private native int applyDiffs(long pointer);
	private native long release(long pointer);
	private native String getOriginName(long pointer);
	private native void setFileName(String fileName, long pointer);
	private native void fetchFromLocal(String fileName, long pointer);
	private native void setTempFileName(String tempFileName, long pointer);
	private native long getZsyncMtime(long pointer);
}