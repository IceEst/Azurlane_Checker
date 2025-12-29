#include <jni.h>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <android/log.h>
#include <stdio.h>

#define LOG_TAG "Checker"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT void JNICALL
Java_com_example_checker_HookEntry_runNativeCheck(JNIEnv* env, jobject thiz, jstring apkPath) {
    const char* path = env->GetStringUTFChars(apkPath, nullptr);

    // --- 维度 3: Native Stat ---
    struct stat s;
    if (stat(path, &s) == 0) {
        LOGE("[STAT] Size: %lld, MTime: %ld", (long long)s.st_size, s.st_mtime);
    }

    // --- 维度 4 & 5: Native Open & FD/readlink ---
    int fd = open(path, O_RDONLY);
    if (fd != -1) {
        char header[5] = {0};
        read(fd, header, 4);
        LOGE("[OPEN] FD: %d, Header: %s", fd, header);

        char link_path[256] = {0};
        char proc_fd_path[64];
        sprintf(proc_fd_path, "/proc/self/fd/%d", fd);
        ssize_t len = readlink(proc_fd_path, link_path, sizeof(link_path) - 1);
        if (len != -1) {
            link_path[len] = '\0';
            LOGE("[READLINK] FD Path: %s", link_path);
        }
        close(fd);
    }

    // --- 维度 6: Maps 探测 ---
    FILE* maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[512];
        while (fgets(line, sizeof(line), maps)) {
            if (strstr(line, "base.apk") || strstr(line, "base_orig.apk")) {
                LOGE("[MAPS] %s", line);
            }
        }
        fclose(maps);
    }

    env->ReleaseStringUTFChars(apkPath, path);
}

