/*
 * You must populate this struct with information from the kernel's task_struct.
 */
struct pid_info {
        int pid;
        char comm[16];
        long state;
};

/*
 * This struct is used to communicate with the kernel in part II.
 */
struct sysfs_message {
	int pid;
	struct pid_info *address;
};
