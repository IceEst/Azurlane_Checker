package com.example.fakeio;

import de.robv.android.xposed.IXposedHookLoadPackage;
import de.robv.android.xposed.callbacks.XC_LoadPackage;
import de.robv.android.xposed.XposedBridge;

public class HookEntry implements IXposedHookLoadPackage {
    @Override
    public void handleLoadPackage(XC_LoadPackage.LoadParam lpparam) throws Throwable {
        // 只针对碧蓝航线
        if (!lpparam.packageName.equals("com.bilibili.azurlane")) return;

        try {
            // 加载核心 C++ 库
            System.loadLibrary("fakeio");
            XposedBridge.log("FakeIO: Native 库加载成功，IO 重定向已启动");
        } catch (Throwable e) {
            XposedBridge.log("FakeIO: 库加载失败: " + e.getMessage());
        }
    }
}
