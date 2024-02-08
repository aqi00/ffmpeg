package com.example.video.util;

import android.annotation.SuppressLint;
import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.FileUtils;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import android.provider.OpenableColumns;
import android.text.TextUtils;

import androidx.annotation.RequiresApi;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class GetFilePathFromUri {
    public static String dirPathName = "appFiles"; // 目录名称

    // 判断手机的外部存储是否有，如果没有就用内部存储
    public static String getFileDirPath(Context ctx, String dir) {
        String directoryPath = "";
        if (Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState())) { // 判断外部存储是否可用
            directoryPath = ctx.getExternalFilesDir(dir).getAbsolutePath();
        } else { // 没外部存储就使用内部存储
            directoryPath = ctx.getFilesDir() + File.separator + dir;
        }
        File file = new File(directoryPath);
        if (!file.exists()) { // 判断文件目录是否存在
            file.mkdirs();
        }
        return directoryPath;
    }

    // 根据Uri获取文件绝对路径，兼容Android 10
    public static String getFileAbsolutePath(Context ctx, Uri imageUri) {
        if (ctx == null || imageUri == null) {
            return null;
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q && DocumentsContract.isDocumentUri(ctx, imageUri)) {
            if (isExternalStorageDocument(imageUri)) { // 为外部存储
                String docId = DocumentsContract.getDocumentId(imageUri);
                String[] split = docId.split(":");
                String type = split[0];
                if ("primary".equalsIgnoreCase(type)) {
                    return Environment.getExternalStorageDirectory() + "/" + split[1];
                }
            } else if (isDownloadsDocument(imageUri)) { // 为下载目录
                String id = DocumentsContract.getDocumentId(imageUri);
                if (!TextUtils.isEmpty(id)) {
                    if (id.startsWith("raw:")) {//已经返回真实路径
                        return id.replaceFirst("raw:", "");
                    }
                }
                Uri contentUri = ContentUris.withAppendedId(Uri.parse("content://downloads/public_downloads"), Long.parseLong(id));
                return getDataColumn(ctx, contentUri, null, null);
            } else if (isMediaDocument(imageUri)) { // 为媒体目录
                String docId = DocumentsContract.getDocumentId(imageUri);
                String[] split = docId.split(":");
                String type = split[0];
                Uri contentUri = null;
                if ("image".equals(type)) {
                    contentUri = MediaStore.Images.Media.EXTERNAL_CONTENT_URI;
                } else if ("video".equals(type)) {
                    contentUri = MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
                } else if ("audio".equals(type)) {
                    contentUri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
                }
                String selection = MediaStore.Images.Media._ID + "=?";
                String[] selectionArgs = new String[]{split[1]};
                return getDataColumn(ctx, contentUri, selection, selectionArgs);
            }
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            return uriToFileApiQ(ctx, imageUri);
        } else if ("content".equalsIgnoreCase(imageUri.getScheme())) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                return getFilePathFromUri(ctx, imageUri);
            } else {
                return getDataColumn(ctx, imageUri, null, null);
            }
        } else if ("file".equalsIgnoreCase(imageUri.getScheme())) {
            return imageUri.getPath();
        }
        return null;
    }

    private static String getRealFilePath(Context ctx, final Uri uri) {
        if (null == uri) {
            return null;
        }
        final String scheme = uri.getScheme();
        String data = null;
        if (scheme == null) {
            data = uri.getPath();
        } else if (ContentResolver.SCHEME_FILE.equals(scheme)) {
            data = uri.getPath();
        } else if (ContentResolver.SCHEME_CONTENT.equals(scheme)) {
            String[] projection = {MediaStore.Images.ImageColumns.DATA};
            Cursor cursor = ctx.getContentResolver().query(uri, projection, null, null, null);
            if (null != cursor) {
                if (cursor.moveToFirst()) {
                    int index = cursor.getColumnIndex(MediaStore.Images.ImageColumns.DATA);
                    if (index > -1) {
                        data = cursor.getString(index);
                    }
                }
                cursor.close();
            }
        }
        return data;
    }

    private static boolean isExternalStorageDocument(Uri uri) {
        return "com.android.externalstorage.documents".equals(uri.getAuthority());
    }

    private static boolean isDownloadsDocument(Uri uri) {
        return "com.android.providers.downloads.documents".equals(uri.getAuthority());
    }

    private static String getDataColumn(Context ctx, Uri uri, String selection, String[] selectionArgs) {
        String column = MediaStore.Images.Media.DATA;
        String[] projection = {column};
        try (Cursor cursor = ctx.getContentResolver().query(uri, projection, selection, selectionArgs, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int index = cursor.getColumnIndexOrThrow(column);
                return cursor.getString(index);
            }
        } catch (Exception e){
            e.printStackTrace();
        }
        return null;
    }

    private static boolean isMediaDocument(Uri uri) {
        return "com.android.providers.media.documents".equals(uri.getAuthority());
    }

    // Android 10 以上适配
    @RequiresApi(api = Build.VERSION_CODES.Q)
    private static String uriToFileApiQ(Context ctx, Uri uri) {
        File file = null;
        if (uri.getScheme().equals(ContentResolver.SCHEME_FILE)) {
            file = new File(uri.getPath());
        } else if (uri.getScheme().equals(ContentResolver.SCHEME_CONTENT)) {
            ContentResolver resolver = ctx.getContentResolver();
            Cursor cursor = resolver.query(uri, null, null, null, null);
            if (cursor.moveToFirst()) {
                @SuppressLint("Range")
                String displayName = cursor.getString(cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME));
                String dirPath = getFileDirPath(ctx, dirPathName);
                File cache = new File(dirPath, displayName);
                try (InputStream is = resolver.openInputStream(uri);
                     FileOutputStream fos = new FileOutputStream(cache);) {
                    FileUtils.copy(is, fos);
                    file = cache;
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
        return file.getAbsolutePath();
    }

    private static String getFilePathFromUri(Context ctx, Uri uri) {
        String realFilePath = getRealFilePath(ctx, uri);
        if (!TextUtils.isEmpty(realFilePath)) {
            return realFilePath;
        }
        String filesDir = getFileDirPath(ctx,dirPathName);
        String fileName = getFileName(uri);
        if (!TextUtils.isEmpty(fileName)) {
            File copyFile1 = new File(filesDir + File.separator + fileName);
            copyFile(ctx, uri, copyFile1);
            return copyFile1.getAbsolutePath();
        }
        return null;
    }

    private static String getFileName(Uri uri) {
        if (uri == null) {
            return null;
        }
        String fileName = null;
        String path = uri.getPath();
        int cut = path.lastIndexOf('/');
        if (cut != -1) {
            fileName = path.substring(cut + 1);
        }
        return fileName;
    }

    private static void copyFile(Context ctx, Uri srcUri, File dstFile) {
        try (InputStream is = ctx.getContentResolver().openInputStream(srcUri);
             OutputStream os = new FileOutputStream(dstFile);) {
            if (is == null) {
                return;
            }
            copyStream(is, os);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static int copyStream(InputStream is, OutputStream os) {
        final int BUFFER_SIZE = 1024 * 2;
        byte[] buffer = new byte[BUFFER_SIZE];
        int count = 0, n = 0;
        try (BufferedInputStream in = new BufferedInputStream(is, BUFFER_SIZE);
             BufferedOutputStream out = new BufferedOutputStream(os, BUFFER_SIZE);) {
            while ((n = in.read(buffer, 0, BUFFER_SIZE)) != -1) {
                out.write(buffer, 0, n);
                count += n;
            }
            out.flush();
        } catch (Exception e) {
            e.printStackTrace();
        }
        return count;
    }

}