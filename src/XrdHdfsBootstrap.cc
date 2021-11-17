
/**
 *  Bootstrap the Hadoop environment, then load the actual Hadoop module.
 */
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlugin.hh"
#include "XrdVersion.hh"

#define BUFSIZE 8192

XrdVERSIONINFO(XrdOssGetStorageSystem, "hdfs");
XrdSysError HdfsBootstrapEroute(0, "hdfs_bootstrap_");

// Forward declarations.
class XrdOss;
class XrdSysLogger;

static XrdOss* Bootstrap(XrdOss*, XrdSysLogger*, const char*, const char*);
static int DetermineEnvironment();

XrdOss* g_hdfs_oss = NULL;

extern "C"
{
    XrdOss* XrdOssGetStorageSystem(XrdOss* native_oss, XrdSysLogger* Logger, const char* config_fn, const char* parms)
    {
        if (g_hdfs_oss)
        {
            return g_hdfs_oss;
        }

        XrdOss* result = Bootstrap(native_oss, Logger, config_fn, parms);
        if (result)
        {
            g_hdfs_oss = result;
        }
        return result;
    }
}

static int loadJvm()
{
    // GROSS!
    const char* path = getenv("LD_LIBRARY_PATH");
    // beacon 1a: checking LD_LIBRARY_PATH
    if (!path)
    {
        HdfsBootstrapEroute.Emsg("loadJvm", "LD_LIBRARY_PATH not set");
        return 1;
    }
    size_t path_len = strlen(path);
    const char* next = strchr(path, ':');
    size_t len = next ? (next - path) : path_len;
    char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);
    while (1)
    {
        if (len + strlen("/libjvm.so") >= BUFSIZE)
        {
            HdfsBootstrapEroute.Emsg("loadJvm", "LD_LIBRARY_PATH too long");
            return 1;
        }
        if (len)
        {
            strncpy(buf, path, len);
            strncpy(buf + len, "/libjvm.so", BUFSIZE - len);
            if (dlopen(buf, RTLD_GLOBAL | RTLD_LAZY))
                return 0;
        }

        if (!next)
            break;
        path = next + 1;
        if (*path == '\0')
            break;
        next = strchr(path, ':');
        len = next ? (next - path) : strlen(path);
    }
    return 1;
}

// beacon 4: loading libhdfs.so
static int loadLibHdfs()
{
    // GROSS!
    const char* path = getenv("LD_LIBRARY_PATH");
    if (!path)
    {
        HdfsBootstrapEroute.Emsg("loadLibHdfs", "LD_LIBRARY_PATH not set");
        return 1;
    }
    size_t path_len = strlen(path);
    const char* next = strchr(path, ':');
    size_t len = next ? (next - path) : path_len;
    char buf[BUFSIZE];
    memset(buf, 0, BUFSIZE);
    while (1)
    {
        if (len + strlen("/libhdfs.so") >= BUFSIZE)
        {
            HdfsBootstrapEroute.Emsg("loadLibHdfs", "LD_LIBRARY_PATH too long");
            return 1;
        }
        if (len)
        {
            strncpy(buf, path, len);
            strncpy(buf + len, "/libhdfs.so", BUFSIZE - len);
            if (dlopen(buf, RTLD_GLOBAL | RTLD_LAZY))
                return 0;
        }

        if (!next)
            break;
        path = next + 1;
        if (*path == '\0')
            break;
        next = strchr(path, ':');
        len = next ? (next - path) : strlen(path);
    }
    return 1;
}

static XrdOss* Bootstrap(XrdOss* native_oss, XrdSysLogger* Logger, const char* config_fn, const char* parms)
{
    if (DetermineEnvironment())
    {
        return 0;
    }

    // Load actual module; code taken from XrdOssApi
    XrdSysPlugin* myLib;
    XrdOss* (*ep)(XrdOss*, XrdSysLogger*, const char*, const char*);

    // Load the JVM from the environment we just computed.
    // The dynamic linker only pays attention to the LD_LIBRARY_PATH the process was started with.
    // beacon 1: check if the JVM lib is loaded
    HdfsBootstrapEroute.logger(Logger);
    HdfsBootstrapEroute.Say("Loading Hadoop module.");

    bool loadedJvm = loadJvm() == 0;
    if (!loadedJvm)
    {
        HdfsBootstrapEroute.Emsg("Bootstrap", "Failed to load JVM from LD_LIBRARY_PATH");
        return 0;
    }

    bool loadedLibHdfs = loadLibHdfs() == 0;
    if (!loadedLibHdfs)
    {
        HdfsBootstrapEroute.Emsg("Bootstrap", "Failed to load libhdfs.so from LD_LIBRARY_PATH");
        return 0;
    }

    auto libXrdHdfsReal = dlopen("libXrdHdfsReal-" XRDPLUGIN_SOVERSION ".so", RTLD_NOW);
    if (!libXrdHdfsReal)
    {
        const char* eTxt = dlerror();
        HdfsBootstrapEroute.Emsg("Bootstrap", "Failed to dlopen libXrdHdfsReal-" XRDPLUGIN_SOVERSION ".so", eTxt);
        return 0;
    }

    myLib = new XrdSysPlugin(&HdfsBootstrapEroute, "libXrdHdfsReal-" XRDPLUGIN_SOVERSION ".so");
    // beacon 2: check if HDFS plugin can be loaded
    if (myLib == nullptr)
    {
        HdfsBootstrapEroute.Emsg("Bootstrap", "Failed to load xrootd-hdfs plugin.");
        return 0;
    }

    // Now get the entry point of the object creator
    //
    ep = (XrdOss * (*)(XrdOss*, XrdSysLogger*, const char*, const char*))(myLib->getPlugin("XrdOssGetStorageSystem"));
    // beacon 3: check if the plugin can be loaded
    if (!ep)
    {
        HdfsBootstrapEroute.Emsg("Bootstrap", "Failed to load XrdOssGetStorageSystem from the xrootd-hdfs plugin.");
        return 0;
    }

    // Pass on the initialization to that module.
    //
    return ep(native_oss, Logger, config_fn, parms);
}

static int CheckEnvVar(const char* var, const char* input)
{
    const char* equal_sign = strchr(input, '=');
    if (equal_sign && (strncmp(input, var, equal_sign - input) == 0))
    {
        setenv(var, equal_sign + 1, 1);
        return 1;
    }
    return 0;
}

// TODO: paths should be configurable
static const char* command_string = "source /opt/hadoop/etc/hadoop/hadoop-env.sh && /usr/libexec/xrootd-hdfs/xrootd_hdfs_envcheck";

static int DetermineEnvironment()
{

    FILE* fp = popen(command_string, "r");
    char environ_buffer[BUFSIZE];
    memset(environ_buffer, 0, BUFSIZE);

    if (fp == NULL)
    {
        printf("Unable to bootstrap environment (%s): %s\n", command_string, strerror(errno));
        return 1;
    }

    int needs_classpath = 1;
    int needs_libhdfs_opts = 1;
    int needs_ld_library_path = 1;
    char* cur_pos = environ_buffer;
    while (fgets(environ_buffer, BUFSIZE - 1, fp) != NULL)
    {
        while ((*cur_pos != '\0'))
        {
            if (needs_classpath && CheckEnvVar("CLASSPATH", cur_pos))
            {
                needs_classpath = 0;
            }
            else if (needs_libhdfs_opts && CheckEnvVar("LIBHDFS_OPTS", cur_pos))
            {
                needs_libhdfs_opts = 0;
            }
            else if (needs_ld_library_path && CheckEnvVar("LD_LIBRARY_PATH", cur_pos))
            {
                needs_ld_library_path = 0;
            }
            cur_pos += strlen(cur_pos) + 1;
        }
    }

    int status = pclose(fp);
    if (status == -1)
    {
        printf("Unable to determine reason for child exit (%s): %s\n", command_string, strerror(errno));
        return 1;
    }
    else if (WIFEXITED(status))
    {
        status = WEXITSTATUS(status);
        if (status)
        {
            printf("Non-zero exit code from child (%s): %d\n", command_string, status);
            return 1;
        }
    }
    else
    {
        printf("Unexpected exit from %s: %d\n", command_string, status);
        return 1;
    }
    return 0;
}
