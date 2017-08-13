zbx_module_custom_proc: zbx_module_custom_proc.c
	gcc -fPIC -shared -o custom_proc.so zbx_module_custom_proc.c -I../../../include