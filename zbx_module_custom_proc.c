
#include "sysinc.h"
#include "module.h"

#include "zbxalgo.h"
#include "common.h"
#include "log.h"
#include "cfg.h"
#include "log.h"
#include "zbxjson.h"
#include "comms.h"

#define PATH_MAX 4096
#define NAME_MAX 255
typedef struct{
	char pname[256];
	pid_t pid;
}custom_proc_t;

/* the variable keeps timeout setting for item processing */
static int	item_timeout = 0;

const char	ZBX_MODULE_CUSTOM_PROC_CONFIG_FILE[] = "/etc/zabbix/zbx_module_custom_proc.conf";

char *CONFIG_PID_FILE_PATH = NULL;

int	zbx_module_PROC_DISCOVERY(AGENT_REQUEST *request, AGENT_RESULT *result);
int	zbx_module_PROC_CPU_UTIL(AGENT_REQUEST *request, AGENT_RESULT *result);
int	zbx_module_PROC_MEM(AGENT_REQUEST *request, AGENT_RESULT *result);
int	zbx_module_PROC_SUBS(AGENT_REQUEST *request, AGENT_RESULT *result);
int	zbx_module_PROC_THREADS(AGENT_REQUEST *request, AGENT_RESULT *result);
int	zbx_module_PROC_STARTTIME(AGENT_REQUEST *request, AGENT_RESULT *result);


static int zbx_module_custom_proc_load_config(int requirement);
int custom_get_procs_pid(zbx_vector_ptr_t *procs);

static ZBX_METRIC keys[] =
/*      KEY                     FLAG		FUNCTION        	TEST PARAMETERS */
{
    {"custom.proc.discovery", 0,    zbx_module_PROC_DISCOVERY, NULL},
    {"custom.proc.cpu.util", CF_HAVEPARAMS, zbx_module_PROC_CPU_UTIL, "systemd"},
    {"custom.proc.mem", CF_HAVEPARAMS, zbx_module_PROC_MEM, "systemd"},
    {"custom.proc.subs", CF_HAVEPARAMS, zbx_module_PROC_SUBS, "systemd"},
    {"custom.proc.threads", CF_HAVEPARAMS, zbx_module_PROC_THREADS, "systemd"},
    {"custom.proc.starttime", CF_HAVEPARAMS, zbx_module_PROC_STARTTIME, "systemd"},
	{NULL}
};

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_api_version                                           *
 *                                                                            *
 * Purpose: returns version number of the module interface                    *
 *                                                                            *
 * Return value: ZBX_MODULE_API_VERSION_ONE - the only version supported by   *
 *               Zabbix currently                                             *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_api_version()
{
	return ZBX_MODULE_API_VERSION_ONE;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_timeout                                          *
 *                                                                            *
 * Purpose: set timeout value for processing of items                         *
 *                                                                            *
 * Parameters: timeout - timeout in seconds, 0 - no timeout set               *
 *                                                                            *
 ******************************************************************************/
void	zbx_module_item_timeout(int timeout)
{
	item_timeout = timeout;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_list                                             *
 *                                                                            *
 * Purpose: returns list of item keys supported by the module                 *
 *                                                                            *
 * Return value: list of item keys                                            *
 *                                                                            *
 ******************************************************************************/
ZBX_METRIC	*zbx_module_item_list()
{
	return keys;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_init                                                  *
 *                                                                            *
 * Purpose: the function is called on agent startup                           *
 *          It should be used to call any initialization routines             *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - module initialization failed               *
 *                                                                            *
 * Comment: the module won't be loaded in case of ZBX_MODULE_FAIL             *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_init()
{
	int ret = ZBX_MODULE_FAIL;

	zabbix_log(LOG_LEVEL_INFORMATION, "module [zbx_module_proc], func [zbx_module_init], using configuration file: [%s]", ZBX_MODULE_CUSTOM_PROC_CONFIG_FILE);

	if (SYSINFO_RET_OK != zbx_module_custom_proc_load_config(ZBX_CFG_FILE_REQUIRED))
		return ret;

	ret = ZBX_MODULE_OK;
	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_uninit                                                *
 *                                                                            *
 * Purpose: the function is called on agent shutdown                          *
 *          It should be used to cleanup used resources if there are any      *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - function failed                            *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_uninit()
{
	return ZBX_MODULE_OK;
}


int	zbx_module_PROC_DISCOVERY(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	const char __function_name[]="zbx_module_PROC_DISCOVERY";
    zbx_vector_ptr_t procs;
	struct zbx_json	j;
	custom_proc_t *proc;
	char pid_str[256];
	int i;

	zbx_vector_ptr_create(&procs);

	zabbix_log(LOG_LEVEL_INFORMATION, "running in %s()", __function_name);
	zabbix_log(LOG_LEVEL_INFORMATION, "config_path: %s", CONFIG_PID_FILE_PATH);
	

	if (SUCCEED != custom_get_procs_pid(&procs))
	{
		zabbix_log(LOG_LEVEL_TRACE, "custom_get_procs_pid() error in %s()", __function_name);
		goto clean;
	}

	zabbix_log(LOG_LEVEL_INFORMATION, "line 156, after succeed custom_get_procs_pid");


	zbx_json_init(&j, ZBX_JSON_STAT_BUF_LEN);

	zbx_json_addarray(&j, ZBX_PROTO_TAG_DATA);

	for (i = 0; i < procs.values_num; i++)
	{
		proc = (custom_proc_t *)procs.values[i];
		zbx_snprintf (pid_str, sizeof(pid_str), "%d",proc->pid);
		//itoa(proc->pid, pid_str, 10);
		zbx_json_addobject(&j, NULL);
		zbx_json_addstring(&j, "{#PNAME}", proc->pname, ZBX_JSON_TYPE_STRING);
		zbx_json_addstring(&j, "{#PID}", pid_str, ZBX_JSON_TYPE_STRING);		
		zbx_json_close(&j);
	}
	zbx_json_close(&j);

	zabbix_log(LOG_LEVEL_INFORMATION, "running in %s() haved json", __function_name);
	zabbix_log(LOG_LEVEL_INFORMATION, "json \n %s", j.buffer);
	
	SET_STR_RESULT(result, strdup(j.buffer));

	
clean:
	zbx_json_free(&j);
	zbx_vector_ptr_destroy(&procs);

	return SYSINFO_RET_OK;
}

int	zbx_module_PROC_CPU_UTIL(AGENT_REQUEST *request, AGENT_RESULT *result)
{
    ;
}

int	zbx_module_PROC_MEM(AGENT_REQUEST *request, AGENT_RESULT *result)
{
    ;
}

int	zbx_module_PROC_SUBS(AGENT_REQUEST *request, AGENT_RESULT *result)
{
    ;
}

int	zbx_module_PROC_THREADS(AGENT_REQUEST *request, AGENT_RESULT *result)
{
    ;
}

int	zbx_module_PROC_STARTTIME(AGENT_REQUEST *request, AGENT_RESULT *result)
{
    ;
}

static int zbx_module_custom_proc_load_config(int requirement)
{
	int     ret = SYSINFO_RET_FAIL;

	//char	*config = NULL;

	struct cfg_line cfg[] =
	{
		/* PARAMETER,               VAR,                                      TYPE,
			MANDATORY,  MIN,        MAX */
		{"PidFilePath",		&CONFIG_PID_FILE_PATH,		TYPE_STRING,
			PARM_OPT, 	0,			0},
		{NULL}
	};

	if( SUCCEED != parse_cfg_file(ZBX_MODULE_CUSTOM_PROC_CONFIG_FILE, cfg, requirement, ZBX_CFG_STRICT))
		return ret;

	ret = SYSINFO_RET_OK;
	return ret;
}

int custom_get_procs_pid(zbx_vector_ptr_t *procs)
{
	const char 	*__function_name = "custom_get_procs_pid";

	DIR			*dir;
	int 		i;
	int			ret = FAIL, pid, n, fd;	
	char 		filename[PATH_MAX];
	char		tmp[MAX_STRING_LEN];
	struct dirent		*entries;
	custom_proc_t *proc;
	
	zabbix_log(LOG_LEVEL_TRACE, "In %s()", __function_name);

	zabbix_log(LOG_LEVEL_INFORMATION, "running in %s()", __function_name);

	if (NULL == (dir = opendir(CONFIG_PID_FILE_PATH)))
	{
		zabbix_log(LOG_LEVEL_TRACE, "Open dir %s error in %s()",CONFIG_PID_FILE_PATH, __function_name);
		goto out;
	}
	
	proc = malloc(sizeof(custom_proc_t));

	while (NULL != (entries = readdir(dir)))
	{

		if (!strcmp (entries->d_name, "."))
			continue;
		if (!strcmp (entries->d_name, ".."))
			continue;
		
		zbx_snprintf(filename, sizeof(filename), "%s/%s", CONFIG_PID_FILE_PATH, entries->d_name);

		zabbix_log(LOG_LEVEL_INFORMATION, "running in %s() , filename %s", __function_name, filename);

		if (-1 == (fd = open(filename, O_RDONLY)))
		{
			zabbix_log(LOG_LEVEL_TRACE, "Open file %s error in %s()",filename, __function_name);
			zabbix_log(LOG_LEVEL_INFORMATION, "Open file %s error in %s()",filename, __function_name);
			
			continue;
		}
		
		if (-1 == (n = read(fd, tmp, sizeof(tmp)-1)))
		{
			zabbix_log(LOG_LEVEL_TRACE, "Cann't read file %s error in %s()",filename, __function_name);
			zabbix_log(LOG_LEVEL_INFORMATION, "Cann't read file %s error in %s()",filename, __function_name);
			
			close(fd);
			continue;
		}

		i = 0;
		while(tmp[i] != '\n')
			i++;
		tmp[i] = '\0';

		zabbix_log(LOG_LEVEL_INFORMATION, "file content: %s %d tmp[0]:%d, tmp[1]:%d",tmp,n,tmp[0],tmp[1]);

		if(FAIL == is_uint32(tmp, &proc->pid))
		{
			zabbix_log(LOG_LEVEL_INFORMATION, "is_uint32(tmp, &(proc->pid))");			
			close(fd);
			continue;
		}

		zabbix_log(LOG_LEVEL_INFORMATION, "proc->pid=%d", proc->pid);
	
		zbx_strlcpy(proc->pname, entries->d_name, NAME_MAX + 1);

		zabbix_log(LOG_LEVEL_INFORMATION, "proc->pname=%s, proc->pid=%d",proc->pname,proc->pid);

		zbx_vector_ptr_append(procs, proc);

		close(fd);
	}
	closedir(dir);

	ret = SUCCEED;
out:
//	zabbix_log(LOG_LEVEL_TRACE, "End of %s(): %s, processes:%d", __function_name, zbx_result_string(ret),
//			procs->values_num);
	return ret;
}

