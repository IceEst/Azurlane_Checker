#include <jni.h>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>
#include <errno.h>

extern "C" {
    #include "dobby.h"
}

#define LOG_TAG "FakeIO_Debug"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 确保该路径下的文件存在，且权限为 644 (chmod 644)
const char* REDIRECT_TARGET = "/data/user/0/com.bilibili.azurlane/files/base_orig.apk";
const char* APK_PATH_PREFIX = "/data/app/";
const char* BASE_APK_NAME = "base.apk";

int (*orig_openat)(int, const char*, int, mode_t) = nullptr;
int (*orig_newfstatat)(int, const char*, struct stat*, int) = nullptr;

/**
 * 判断是否需要重定向
 * 移除 strstr(pathname, "!") 的限制，确保所有对 base.apk 的访问都指向原版
 */
bool should_redirect(const char* pathname) {
    if (pathname == nullptr) return false;
    
    // 检查路径是否包含 /data/app/ 且包含 base.apk
    if (strstr(pathname, APK_PATH_PREFIX) != nullptr && 
        strstr(pathname, BASE_APK_NAME) != nullptr) {
        return true;
    }
    return false;
}

int my_openat(int dirfd, const char* pathname, int flags, mode_t mode) {
    if (should_redirect(pathname)) {
        int res = orig_openat(dirfd, REDIRECT_TARGET, flags, mode);
        if (res != -1) {
            LOGI("成功重定向 openat: %s -> %s", pathname, REDIRECT_TARGET);
        } else {
            LOGE("重定向 openat 失败! 目标: %s, 错误: %s", REDIRECT_TARGET, strerror(errno));
        }
        return res;
    }
    return orig_openat(dirfd, pathname, flags, mode);
}

int my_newfstatat(int dirfd, const char* pathname, struct stat* buf, int flags) {
    if (should_redirect(pathname)) {
        int res = orig_newfstatat(dirfd, REDIRECT_TARGET, buf, flags);
        if (res != -1) {
            LOGI("成功重定向 fstatat: %s, 报告大小: %lld", pathname, (long long)buf->st_size);
        } else {
            LOGE("重定向 fstatat 失败! 目标: %s, 错误: %s", REDIRECT_TARGET, strerror(errno));
        }
        return res;
    }
    return orig_newfstatat(dirfd, pathname, buf, flags);
}

__attribute__((constructor))
void init() {
    // 针对 Android 系统的符号查找
    void* openat_ptr = dlsym(RTLD_DEFAULT, "openat");
    
    // 尝试查找不同的 stat 符号名
    void* stat_ptr = dlsym(RTLD_DEFAULT, "newfstatat");
    if (!stat_ptr) stat_ptr = dlsym(RTLD_DEFAULT, "fstatat64");
    if (!stat_ptr) stat_ptr = dlsym(RTLD_DEFAULT, "fstatat");

    if (openat_ptr) {
        DobbyHook(openat_ptr, (dobby_dummy_func_t)my_openat, (dobby_dummy_func_t*)&orig_openat);
    } else {
        LOGE("无法找到 openat 符号");
    }

    if (stat_ptr) {
        DobbyHook(stat_ptr, (dobby_dummy_func_t)my_newfstatat, (dobby_dummy_func_t*)&orig_newfstatat);
    } else {
        LOGE("无法找到 fstatat 符号");
    }

    LOGI("FakeIO: 增强版重定向部署完成 (针对 10MB 差异优化)");
}
