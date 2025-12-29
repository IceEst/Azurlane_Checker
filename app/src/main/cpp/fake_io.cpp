#include <jni.h>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <android/log.h>
#include <dlfcn.h>
#include <string.h>

#define LOG_TAG "FakeIO"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

const char* ORIG_BASE_NAME = "base.apk";
const char* REDIRECT_TARGET = "/data/data/com.bilibili.azurlane/base_orig.apk";

int (*orig_openat)(int, const char*, int, mode_t) = nullptr;
int (*orig_newfstatat)(int, const char*, struct stat*, int) = nullptr;

// 智能路径处理函数
std::string get_new_path(const char* pathname) {
    std::string path_str(pathname);
    size_t pos = path_str.find(ORIG_BASE_NAME);
    if (pos != std::string::npos) {
        // 保留 base.apk 之后的所有字符（包括 !/ 及其后面的路径）
        std::string suffix = path_str.substr(pos + strlen(ORIG_BASE_NAME));
        std::string final_path = std::string(REDIRECT_TARGET) + suffix;
        LOGI("智能重定向成功: %s -> %s", pathname, final_path.c_str());
        return final_path;
    }
    return path_str;
}

int my_openat(int dirfd, const char* pathname, int flags, mode_t mode) {
    if (pathname && strstr(pathname, ORIG_BASE_NAME)) {
        std::string n_path = get_new_path(pathname);
        return orig_openat(dirfd, n_path.c_str(), flags, mode);
    }
    return orig_openat(dirfd, pathname, flags, mode);
}

int my_newfstatat(int dirfd, const char* pathname, struct stat* buf, int flags) {
    if (pathname && strstr(pathname, ORIG_BASE_NAME)) {
        std::string n_path = get_new_path(pathname);
        return orig_newfstatat(dirfd, n_path.c_str(), buf, flags);
    }
    return orig_newfstatat(dirfd, pathname, buf, flags);
}

__attribute__((constructor))
void init() {
    // 使用 dlsym 确保在 x86_64 模拟器环境下的稳定性
    void* openat_ptr = dlsym(RTLD_DEFAULT, "openat");
    void* stat_ptr = dlsym(RTLD_DEFAULT, "newfstatat");
    if (!stat_ptr) stat_ptr = dlsym(RTLD_DEFAULT, "fstatat64");

    if (openat_ptr) {
        DobbyHook(openat_ptr, (void*)my_openat, (void**)&orig_openat);
    }
    if (stat_ptr) {
        DobbyHook(stat_ptr, (void*)my_newfstatat, (void**)&orig_newfstatat);
    }
    LOGI("FakeIO: 智能 Hook 已部署，等待游戏启动...");
}
