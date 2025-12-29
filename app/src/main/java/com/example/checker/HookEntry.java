package com.example.checker;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.util.Base64;
import android.util.Log;
import java.io.File;
import java.security.MessageDigest;
import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XposedHelpers;
import de.robv.android.xposed.callbacks.XC_LoadPackage;

public class HookEntry implements IXposedHookLoadPackage {
    static {
        System.loadLibrary("checker");
    }

    public native void runNativeCheck(String apkPath);

    @Override
    public void handleLoadPackage(XC_LoadPackage.LoadPackageParam lpparam) throws Throwable {
        if (!lpparam.packageName.equals("com.bilibili.azurlane")) return;

        Log.e("Checker", ">>> 开始执行六维检测流程 <<<");

        // 获取 Context (反射方式)
        Object activityThread = XposedHelpers.callStaticMethod(XposedHelpers.findClass("android.app.ActivityThread", null), "currentActivityThread");
        Context context = (Context) XposedHelpers.callMethod(activityThread, "getSystemContext");

        // --- 维度 1: API 签名 ---
        try {
            PackageInfo pi = context.getPackageManager().getPackageInfo(lpparam.packageName, PackageManager.GET_SIGNATURES);
            MessageDigest md = MessageDigest.getInstance("SHA1");
            md.update(pi.signatures[0].toByteArray());
            Log.e("Checker", "[SIGNATURE] SHA1: " + Base64.encodeToString(md.digest(), Base64.DEFAULT).trim());
        } catch (Exception e) {
            Log.e("Checker", "[SIGNATURE] 失败: " + e.getMessage());
        }

        // --- 维度 2: Java 文件 ---
        String apkPath = lpparam.appInfo.sourceDir;
        Log.e("Checker", "[JAVA_FILE] Path: " + apkPath);
        Log.e("Checker", "[JAVA_FILE] Length: " + new File(apkPath).length());

        // 执行 Native 检测 (3, 4, 5, 6)
        runNativeCheck(apkPath);
    }
}
