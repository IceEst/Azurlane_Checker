package com.example.checker;

import android.app.Application;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.util.Base64;
import android.util.Log;
import java.io.File;
import java.security.MessageDigest;
import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.XC_MethodHook;
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

        // 【修改点】：不再直接执行，而是 Hook 游戏的 Application 类
        // 只有等到这里，App 的构造函数 <init> 才跑完，PmsHook 的伪装才生效
        XposedHelpers.findAndHookMethod(
            "com.manjuu.azurlane.App", // 目标类 (来自 classes2.dex)
            lpparam.classLoader,
            "onCreate",                // 目标方法
            new XC_MethodHook() {
                @Override
                protected void beforeHookedMethod(MethodHookParam param) throws Throwable {
                    Log.e("Checker", ">>> Application启动，开始执行六维检测流程 <<<");

                    // 获取 Context (直接从 Application 对象获取，比反射更稳定且符合游戏视角)
                    Application app = (Application) param.thisObject;
                    Context context = app.getApplicationContext();

                    // --- 维度 1: API 签名 ---
                    try {
                        // 使用当前 Context 获取签名，如果伪装成功，这里打印出来的应该是原版签名
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
        );
    }
}
