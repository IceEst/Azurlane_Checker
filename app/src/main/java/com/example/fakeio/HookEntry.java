package com.example.fakeio;

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
        System.loadLibrary("fakeio");
    }

    public native void runNativeCheck(String apkPath);

    @Override
    public void handleLoadPackage(XC_LoadPackage.LoadPackageParam lpparam) throws Throwable {
        if (!lpparam.packageName.equals("com.bilibili.azurlane")) return;

        Log.e("Inspector", "========== 启动六维度安全自检 ==========");

        // 获取当前应用的 Context 实例
        Object activityThread = XposedHelpers.callStaticMethod(XposedHelpers.findClass("android.app.ActivityThread", null), "currentActivityThread");
        Context context = (Context) XposedHelpers.callMethod(activityThread, "getSystemContext");

        // 1. 检测 API 签名 (Signature Hash)
        try {
            PackageManager pm = context.getPackageManager();
            PackageInfo pi = pm.getPackageInfo(lpparam.packageName, PackageManager.GET_SIGNATURES);
            MessageDigest md = MessageDigest.getInstance("SHA1");
            md.update(pi.signatures[0].toByteArray());
            String signatureHash = Base64.encodeToString(md.digest(), Base64.DEFAULT).trim();
            Log.e("Inspector", "[1. API签名] SHA1: " + signatureHash);
        } catch (Exception e) {
            Log.e("Inspector", "[1. API签名] 检测失败: " + e.getMessage());
        }

        // 2. 检测 Java 层文件属性 (File.length)
        String apkPath = lpparam.appInfo.sourceDir;
        File f = new File(apkPath);
        Log.e("Inspector", "[2. Java文件] 路径: " + apkPath);
        Log.e("Inspector", "[2. Java文件] Size (length): " + f.length());

        // 执行 Native 层剩余四项检测
        runNativeCheck(apkPath);
    }
}
