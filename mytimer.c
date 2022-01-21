
/* Necessary includes for device drivers */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> 				/* printk() */
#include <linux/slab.h> 				/* kmalloc() */
#include <linux/fs.h> 					/* everything... */
#include <linux/errno.h> 				/* error codes */
#include <linux/types.h> 				/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> 				/* O_ACCMODE */
#include <asm/system.h> 				/* cli(), *_flags */
#include <asm/uaccess.h> 				/* copy_from/to_user */
#include <linux/timer.h>   				/* Needed for timer */
#include <linux/string.h> 				/*For string functions*/


MODULE_LICENSE("Dual BSD/GPL");

/* Declaration of memory.c functions */
static int mytimer_open(struct inode *inode, struct file *filp);
static int mytimer_release(struct inode *inode, struct file *filp);
static ssize_t mytimer_read(struct file *filp,
		char *buf, size_t count, loff_t *f_pos);
static ssize_t mytimer_write(struct file *filp,
		const char *buf, size_t count, loff_t *f_pos);
static void mytimer_exit(void);
static int mytimer_init(void);
void task_timer(unsigned long arg);
void print_timer(int);

/* Structure that declares the usual file */
/* access functions */
struct file_operations mytimer_fops = {
	read: mytimer_read,
	write: mytimer_write,
	open: mytimer_open,
	release: mytimer_release
};

/* Declaration of the init and exit functions */
module_init(mytimer_init);
module_exit(mytimer_exit);

static unsigned capacity = 256;
static unsigned bite = 256;
module_param(capacity, uint, S_IRUGO);
module_param(bite, uint, S_IRUGO);

/* Global variables of the driver */

static int mytimer_major = 61; 				/* Major number */
spinlock_t lockI = SPIN_LOCK_UNLOCKED; 			/* the lock for the counter */
static char *mytimer_buffer;				/* Buffer to store data */
static int mytimer_len;					/* length of the current message */
struct timer_list Timer[11]; 				/*setting up the timer structure*/
static char message[11][128]; 				/*String arrray holding the message of each running timer*/
static unsigned long timer_number=0; 			/*count of timers, currently runninng*/
static unsigned int expires[11]; 			/*will store the expiry time of all the current running kernel timers */
static char display_message [11][256];			/*will store the timers to be displayed when -l command is used */
static char update_message[256];			/*Will store a message when the timer is updated*/
static char *update_ptr=update_message;			/*pointer to update_message*/
static int flag=0;					/*flag set when -l command is called*/
static int N;						/*N holds the value of current timers to be printed*/

static int mytimer_init(void)
{
	int result;

	/* Registering device */
	result = register_chrdev(mytimer_major, "mytimer", &mytimer_fops);
	if (result < 0)
	{
		printk(KERN_ALERT
			"mytimer: cannot obtain major number %d\n", mytimer_major);
		return result;
	}

	/* Allocating mytimer for the buffer */
	mytimer_buffer = kmalloc(capacity, GFP_KERNEL); 
	if (!mytimer_buffer)
	{ 
		printk(KERN_ALERT "Insufficient kernel memory\n"); 
		result = -ENOMEM;
		goto fail; 
	} 
	memset(mytimer_buffer, 0, capacity);
	mytimer_len = 0;

	printk(KERN_ALERT "Inserting mytimer module\n"); 
	return 0;

fail: 
	mytimer_exit(); 
	return result;
}

static void mytimer_exit(void)
{
	/* Freeing the major number */
	unregister_chrdev(mytimer_major, "mytimer");

	/* Freeing buffer memory */
	if (mytimer_buffer)
	{
		kfree(mytimer_buffer);
	}

	printk(KERN_ALERT "Removing mytimer module\n");

	//killing the current timer
 	if (!del_timer (&Timer[timer_number])) {
	printk (KERN_INFO "Couldn't remove timer!!\n");
        }
    	else {
	printk (KERN_INFO "timer removed \n");
        }

}

static int mytimer_open(struct inode *inode, struct file *filp)
{
	
	return 0;
}

static int mytimer_release(struct inode *inode, struct file *filp)
{
	
	return 0;
}

static ssize_t mytimer_read(struct file *filp, char *buf,size_t count, loff_t *f_pos)
{ 
	int i;
	char temp_str[256]="";
	/* end of buffer reached */
	if (*f_pos >= mytimer_len)
	return 0;
         
	/* Transfering data to user space */ 
	
	 //If -l had been called     
	 if(flag == 1 )
	 {
	 strcpy(temp_str,"\0");
	 
	 //if N is less than the current running timers
	 if(N!=0 && N<=timer_number)
	 {
	 for(i=1; i <=N ; i++)
	 {
	 strcat(temp_str,display_message[i]);
	 strcat(temp_str,",");
	 }}
	 
	 //if N=0 is given then printing all the current timers or if Current timers is less than N
	 if(N==0 || N>timer_number)
	 {
	 for(i=1; i <=timer_number ; i++)
	 {
	 strcat(temp_str,display_message[i]);
	 strcat(temp_str,",");
	 }}  
	  
	//if currently running timers are 0 then printing the current timers
	if(timer_number != 0)
	{
         if (copy_to_user(buf, temp_str, 512))
	 return -EFAULT;
	}
	}
	
	
	if(flag == 0)
	{
	 if (copy_to_user(buf, update_message, 512))
	 return -EFAULT;
	}
	flag=0;
	return count; 
}


static ssize_t mytimer_write(struct file *filp, const char *buf,
							size_t count, loff_t *f_pos)
{
	
	int temp,n,i,k=0;
	char tbuf[256], *tbptr = tbuf;
	timer_number ++;
	flag=0;
	/* end of buffer reached */
	if (*f_pos >= capacity)
	{
		printk(KERN_INFO
			"write called: process id %d, command %s, count %d, buffer full\n",
			current->pid, current->comm, count);
		return -ENOSPC;
	}

	/* do not eat more than a bite */
	if (count > bite) count = bite;

	/* do not go over the end */
	if (count > capacity - *f_pos)
		count = capacity - *f_pos;

	if (copy_from_user(mytimer_buffer + *f_pos, buf, count))
	{
		return -EFAULT;
	}

	for (temp = *f_pos; temp < count + *f_pos; temp++)					  
		tbptr += sprintf(tbptr, "%c", mytimer_buffer[temp]);
	
	
	//Extracting the message from the user input
	for(i=*f_pos + 2;i<(count + *f_pos)-1; i++) 
	{
	message[timer_number][k] = tbuf[i];
	k++;
	}
	k=0;
	
	//checking if -l command has been called
	if(strcmp(message[timer_number], "-l") == 0)
	{
	timer_number --;
	flag=1;
	sscanf(tbuf, "%d", &N);	//the N will store the number of timers to print
	print_timer(N);
	return count;
	}
	
	//Extracting the expiry of the timer from the user input
	sscanf(tbuf,"%d",&n);	
	
	//checking if the timer already exists. If yes, updating with the new Timer
	if(timer_number != 1)
	{
	for(i=0;i<timer_number;i++)
	{
	  if(strcmp(message[i],message[timer_number])== 0)
	  {
		if(mod_timer(&Timer[i], (jiffies + (HZ*n))))
		{
		sprintf(update_ptr, "The timer%s was updated",message[i]);
		expires[i]=jiffies + (HZ*n);
		timer_number--;
		return count;
		}
           
	}}}
	//will terminate when 11th timer is created	
	if(timer_number==10)
	{
	printk(KERN_INFO "will not add any more timers\n");
	}
	if(timer_number == 11)
	{
	printk(KERN_INFO "Cannot add this timer\n");
	return count;
	}
	
	//Initialising kernel timer
    	unsigned long expiryTime = jiffies + (HZ*n); /* HZ gives number of ticks per second */
    	init_timer (&Timer[timer_number]);
    	Timer[timer_number].function = task_timer;
    	Timer[timer_number].expires = expiryTime;
    	expires[timer_number]=expiryTime;
    	Timer[timer_number].data = timer_number;
    	add_timer (&Timer[timer_number]);
    	
       	*f_pos += count;
	mytimer_len = *f_pos;
	return count;
}

//function called when a kernel timer expires
void task_timer(unsigned long arg) 
{   
	spin_lock (&lockI);
	printk (KERN_INFO "%s\n",message[arg]);
	expires[timer_number]=0;
	timer_number--;
	spin_unlock (&lockI);
}

//function to create display messages for -l command
void print_timer(int n)
{
	unsigned long currentTime = jiffies;
	int i,j;
	unsigned long temp;
	char temp_message[10][128], temp_str[128];
	unsigned long time_remaining[10];

	//making another copy of the messages to be displayed with the active kernels
	for(i=1;i<=timer_number;i++)
	{
	strcpy(display_message[i],"\0");
	strcpy(temp_message[i],"\0");
	strcpy(temp_message[i],message[i]);
	} 
	
	//calculating the remaining time for each active timer
	for(i=1;i<=timer_number;i++)
	{
	time_remaining[i] = (expires[i]-currentTime)/HZ;
	} 
	
	//Sorting the array
	for(i=1; i<=timer_number;i++){
	for(j=1; j<=(timer_number-1); j++){
	if(time_remaining[j+1] < time_remaining[j]){
		temp =time_remaining[j];
		time_remaining[j]=time_remaining[j+1];
		time_remaining[j+1]=temp;
		
		//performing sort on the message array simultaneously
		strcpy(temp_str,temp_message[j]);
		strcpy(temp_message[j],temp_message[j+1]);
		strcpy(temp_message[j+1],temp_str);
	 }}}
	
	 //creating a string array which holds the final messages to be printed
	 for(i = 1; i<=n; i++)
	 {
	 strcat(display_message[i],temp_message[i]);
	 strcat(display_message[i]," ");
	 sprintf(temp_str, " %lu", time_remaining[i]);
	 strcat(display_message[i],temp_str);
	 }
	 
	 if(n == 0)
	 {
	  for(i = 1; i<=timer_number; i++)
	 {
	 strcat(display_message[i],temp_message[i]);
	 strcat(display_message[i]," ");
	 sprintf(temp_str, " %lu", time_remaining[i]);
	 strcat(display_message[i],temp_str);
	 }}
	
	return;
}
