package com.example.jianying.util;

import android.content.Context;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public class FileUtil {
    public static boolean isExists(String filePath) {
        File file = new File(filePath);
        if (file.exists() && file.length()>0) {
            return true;
        } else {
            return false;
        }
    }
    // 把res/raw中的文件复制到指定目录
    public static void copyFileFromRaw(Context ctx, int id, String fileName, String storagePath) {
        File dir = new File(storagePath);
        if (!dir.exists()) { // 如果目录不存在，就创建新的目录
            dir.mkdirs();
        }
        String storageFile = storagePath + File.separator + fileName;
        File file = new File(storageFile);
        if (file.exists()) {
            return;
        }
        try (InputStream is = ctx.getResources().openRawResource(id);
             FileOutputStream fos = new FileOutputStream(file)) {
            byte[] buffer = new byte[is.available()];
            int lenght = 0;
            while ((lenght = is.read(buffer)) != -1) {
                fos.write(buffer, 0, lenght);
            }
            fos.flush();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
