# **zabbix进程级监控**
**利用zabbix支持模块加载的方式，实现满足我们需求的进程级监控模块**  
## **一、需求分析与概要设计**
目前需要添加的进程级监控数据有：   
* 进程（包括子进程）的CPU使用率
* 进程（包括子进程）的内存占用率
* 进程的子进程（递归）数量
* 进程（包括子进程）的总线程数
* 进程的启动时间

zabbix自带的进程监控是用进程名、用户名、cmdline来过滤统计进程数据的，但是Linux系统上唯一标识进程的是PID，zabbix自带的容易出现监控不准确问题。所以我们要实现精确的监控我们需要监控的进程，应该使用PID的方式。

整个监控是采用zabbix的自动发现来实现的，zabbix服务器先通过自动发现需要监控的进程及其PID，然后自动创建需要的监控项来实施监控。时序如下图所示：  
![](C:/Users/yl1576/Desktop/%E6%88%AA%E5%9B%BE/pid.png)

## **二、item key 设计**
根据需要监控的进程数据，按照 zabbix item key 的样式设计以下item key：

* **custom.proc.discovery**
    > 描述：
    自动发现需要监控的进程Pid

    > 返回值:  json数据

    ```json
    {
        "data":[
            {
                "{#PNAME}":"<pname>"
            },
            {
                "{#PNAME}":"<pname>"
            },
            .....
        ]
    }
    ```

* **custom.proc.cpu.util[<pname\>,<type\>,<mode\>]**
    > 描述：进程的CPU使用率

    > 返回值： 浮点数

    > 参数：  
    >   * PID - 进程ID
    >   * type - CPU使用率类型：total(default), user, system
    >   * mode - 数据收集模式： avg1(default), avg5, avg15

* **custom.proc.mem[<pname\>]**
    > 描述： 进程占用的内存，字节

    > 返回值： 整数

* **custom.proc.subs[<pname\>]**
    > 描述： 进程的子进程数量

    > 返回值： 整数

* **custom.proc.threads[<pname\>]**
    > 描述： 进程的线程数

    > 返回值： 整数

* **custom.proc.starttime[<pname\>]**
    > 描述： 进程启动时间

    > 返回值： 整数

## **三、item key 数据获取流程**
### **1. custom.proc.discovery**
此步骤是实现进程监控首先要做的一步。  
zabbix agent端的查询处理通过读取进程配置文件知道pid文件的存放目录，然后到存放目录读取需要监控的进程pid，构造对应json数据返回给zabbix server。时序如下图所示：
![discovery](C:/Users/yl1576/Desktop/%E6%88%AA%E5%9B%BE/discovery.png)  

### **2. custom.proc.cpu.util**
cpu的使用率和物理上的速率一样，是一段时间内的平均值，没有瞬时值的概念，是所以我们需要获取cpu的使用率一般是指：过去多少时间内的CPU使用率，比如一分钟、五分钟和十五分钟。这就需要我们持续收集一段时间的数据才能计算出cpu使用率。可以创建新的进程收集数据，通过IPC将数据传给查询数据的进程，然后处理。zabbix agent自带的cpu数据收集正是这么做的，IPC方式是共享内存。  

![cpu](C:/Users/yl1576/Desktop/%E6%88%AA%E5%9B%BE/cpu_active.png)  

但是，zabbix自带的collector写到共享内存里的数据是针对自带的item-key建立的查询，使用[procname, username, cmdline]标识的，不包含pid，无法满足我们的要求,所以我们需要创建自己的收集进程，收集数据。

* cpu 使用率数据获取的时序如下图所示：

![cpu](C:/Users/yl1576/Desktop/%E6%88%AA%E5%9B%BE/cpu.png)  


### **3. 其他**
除了cpu使用率外的其他数据都是可以直接从/proc/<PID>目录下的文件中直接获取的。
![other](C:/Users/yl1576/Desktop/%E6%88%AA%E5%9B%BE/other.png)


# **接口设计**

CUSTOM_COLLECTOR_DATA   //  extern *custom_collector
{
    zbx_dshm_t  procstat;
}

zbx_dshm_t
{
    int shmid;
    int proj_id;
    size_t size;
    zbx_shm_copy_func_t copy_func; // typedef void (*zbx_shm_copy_func_t)(void *dst,
                                    //size_t size_dst, const void *src);
    ZBX_MUTEX lock;
}

zbx_dshm_ref_t      // static custom_procstat_ref
{
    int shmid;
    void *addr;
}

custom_procstat_data_t
{
    zbx_uint64_t utime;
    zbx_uint64_t stime;
    zbx_timespec_t timestamp;
}
custom_procstat_header_t
{
    int query_count;
}
custom_procstat_query_t
{
    char pname[MAX_PNAME_LENGTH];
    int h_first;
    int h_count;
    int last_accessed;
    int runid;
    int error;
    int running;
    custom_procstat_data_t h_data[MAX_COLLECTOR_HISTORY];
}
custom_procstat_query_data_t
{
    pid_t pid;
    int error;
    zbx_uint64_t utime;
    zbx_uint64_t stime;
}

void init_custom_collector_data();
void custom_procstat_init(void);


int custom_procstat_get_util(pid_t pid, int period, int type,
                                double *value, char **errmsg);
目的： 获取指定pid进程cpu使用率

参数:       
            pid         -[IN] 进程id
            period      -[IN] 时间周期
            type        -[IN] cpu 使用率类型
            value       -[OUT] cpu使用率
            errmsg      -[OUT] 错误信息

返回值：SUCCEED
        FAIL

    staic void custom_procstat_reattach();
    static custom_procstat_query_t *procstat_get_query(void *base, pid_t pid)
    static void custom_procstat_add(pid_t pid);
        static void custom_procstat_copy_data(void *dst, size_t size_dst, const void *src);



static int custom_procstat_build_local_query_vector(zbx_vector_ptr_t *custom_queries ptr,int runid);

static int custom_get_process_pid()
static int custom_get_monitored_pids(pid)

static int custom_proc_get_process_ppid(pid_t pid, pid_t *ppid)


