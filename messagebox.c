#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/cred.h> //---- user informations
#include <linux/uidgid.h>
#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>     /* kmalloc() */
#include <linux/fs.h>       /* everything... */
#include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>    /* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/file.h>  //File operations

#include <asm/switch_to.h>      /* cli(), *_flags */
#include <asm/uaccess.h>    /* copy_*_user */

#include "messagebox.h"

#define MSGBOX_MAJOR 0
#define MSGBOX_READ_MODE 0
//#define MSGBOX_SIZE 140 //like twitter

int msgbox_major = MSGBOX_MAJOR;
int msgbox_minor = 0;
int msgbox_read_mode = MSGBOX_READ_MODE;
//int msgbox_size = MSGBOX_SIZE;

module_param(msgbox_major, int, S_IRUGO);
module_param(msgbox_minor, int, S_IRUGO);
//module_param(msgbox_size, int, S_IRUGO);
module_param(msgbox_read_mode, int, S_IRUGO);


MODULE_AUTHOR("erdem emekligil, ibrahim seckin, samil can");
MODULE_LICENSE("Dual BSD/GPL");

struct message {
	int from;
	int to;
	int isRead;
	size_t message_size;
	char *message;
	struct message * next;
};

struct messagebox_dev {
	struct message *messages;
	struct cdev cdev;
	struct message *end;
	int read_mode;
	struct semaphore sem;
};

struct messagebox_dev glb_msgdev;
struct message *glb_msgread;

int messagebox_trim()
{
	struct message * tmp;
	printk(KERN_NOTICE "trim starts \n");

	while (glb_msgdev.messages != NULL) {//tmp.next != glb_mesgev.messages
		tmp = glb_msgdev.messages;
		glb_msgdev.messages = glb_msgdev.messages->next;    
		kfree(tmp->message);
		kfree(tmp);

	}

	printk(KERN_NOTICE "trim ends \n");
	return 0;
}

int messagebox_open(struct inode *inode, struct file *filp)
{
	printk(KERN_NOTICE "open starts \n");
	
	filp->private_data = &glb_msgdev;
	printk(KERN_NOTICE "open sonu \n");
	return 0;
}


int messagebox_release(struct inode *inode, struct file *filp)
{
	return 0;
}

int char_2_int(char* uid, int size){
	int i,j,k,tmp_exp;
	int returnNumber = 0;
	char *tmp;
	j=0;
	for(i=size-1; i >= 0; --i){
		tmp = uid+i;
		tmp_exp = 1;

		for(k=0;k<j;k++){//c de ust ifadesi yok!
			tmp_exp = tmp_exp * 10;
		}

		returnNumber += tmp_exp * ( *tmp - 48);

		j = j+1;
	}
	
	return returnNumber;
}



char* int_2_char (int uid, int* size){
	
	int tmp,i;
	char *c;
	if(uid == 0){ //root check
		(*size) = 1;
		c = kmalloc(1*sizeof(char), GFP_KERNEL );
		c[0] = '0';
		return c;
	}

	tmp = uid;
	while(tmp > 0){ 
		tmp = tmp/10;
		(*size)++;
	}


	c = kmalloc((*size)*sizeof(char), GFP_KERNEL );
	tmp = uid;
	i = (*size);
	i--;
	while(uid > 0){ 
		tmp = uid%10;
		c[i] = (char)(((int)'0')+tmp);
		uid = uid/10;
		i--;
	}

	return c;
}

//this function gets username and returns its uid
char* read_for_uid(char* username, int* uid_size, int username_size){
	char* uid;
	char buf[0];
	struct file * filp;
	int i = 0;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* open a file */
	filp = filp_open("/etc/passwd",O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk("open /etc/passwd for getuid error\n");
		return;
	}
	else {
		printk("open /etc/passwd for getuid success\n");
	}

	filp->f_pos = 0;
	buf[0] = 10;

	int j = 0;
	while(i< 2000){
		if(buf[0] == 10){//checks if it is end line
			filp->f_op->read(filp, buf, 1, &filp->f_pos); //points first letter of username
			j = 0;
			while(buf[0] != ':'){
				if(username[j] == buf[0]){
					j++;
					filp->f_op->read(filp, buf, 1, &filp->f_pos); 
				}
				else{
					break;
				}
			}

			//here filp points ':'

			if(j == username_size){
				filp->f_pos += 2; //now it points uid

				filp->f_op->read(filp, buf, 1, &filp->f_pos); //buf[0] = '1', points 2nd of uid.
				j = 0;
				while(buf[0] != 58){ //checks until char is ':'
					filp->f_op->read(filp, buf, 1, &filp->f_pos);
					j++;
				}
				(*uid_size) = j; //j = 4, buf[0] = ':', pointer points user group
				uid = kmalloc ((*uid_size) * sizeof(char) , GFP_KERNEL);
				filp->f_pos -= j+1;

				j = 0;
				do{ //checks until char is ':'
					filp->f_op->read(filp, buf, 1, &filp->f_pos);
					if(buf[0] != 58){
						uid[j] = buf[0];
						j++;
					}
				}while(buf[0] != 58);

				printk(KERN_NOTICE "read_for_uid::uid_first: %s \n",uid);
				return uid;
			}
		}

		filp->f_op->read(filp, buf, 1, &filp->f_pos);
		i++;
	}

	return NULL;
}

int get_userid(char *username, int username_size){
	int uid_size = 0;
	int uid;
	char* uid_str = read_for_uid(username, &uid_size, username_size);
	if(uid_size == 0)
		return -1;
	uid = char_2_int(uid_str, uid_size);
	return uid;
}


char* read_from_file(char* uid_str, int uid_size){
	struct file * filp;
	int i = 0;
	char buf[1];
	char* username;
	int j = 0;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* open a file */
	filp = filp_open("/etc/passwd",O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk("open /etc/passwd error\n");
		return;
	}
	else {
		printk("open /etc/passwd success\n");
	}

	filp->f_pos = 0;
	buf[0] = 'a';
	username = kmalloc(32 * sizeof(char), GFP_KERNEL); //max size : 32
	
	while(i< 2000){
		//ret = vfs_read(filp, x, 1, &filp->f_pos);

		if(buf[0] == ':'){
			filp->f_op->read(filp, buf, 1, &filp->f_pos); //points x
			if(buf[0] == 'x'){
				filp->f_op->read(filp, buf, 1, &filp->f_pos); //points :
				if(buf[0] == ':'){
					filp->f_op->read(filp, buf, 1, &filp->f_pos); //points first digit of uid
					j = 0;
					while(buf[0] != ':'){
						if(uid_str[j] == buf[0]){
							j++;
							filp->f_op->read(filp, buf, 1, &filp->f_pos); 
						}
						else{
							break;
						}
					}
					

					if(j == uid_size){
						//j = 4 points :
						filp->f_pos -= j+5; //points last letter of username
						j = 0;
						while(buf[0] != 10){
							if(filp->f_pos < 0){ //root check
								username[j] = 10;
								break;
							}
							filp->f_op->read(filp, buf, 1, &filp->f_pos);
							username[j] = buf[0];
							filp->f_pos -= 2;
							j++;
						}
						printk(KERN_NOTICE "read_from_file::username %c \n",username[0]);
						return username;
					}
				}
			}
		}

		filp->f_op->read(filp, buf, 1, &filp->f_pos);
		i++;
	} 
}

char* get_username(int uid, int* size){
	int uid_size = 0;
	char* uid_str = int_2_char(uid, &uid_size);
	char* reverse_uname = read_from_file(uid_str, uid_size);
	int i=0;
	char* username;
	while(reverse_uname[i] != 10){
		i++;
	}

	(*size) = i;
	username = kmalloc( (*size) * sizeof(char), GFP_KERNEL);

	i--;//points last one
	while(i >= 0){
		username[(*size)-i-1] = reverse_uname[i];
		i--;
	}
	kfree(reverse_uname);
	return username;
}

ssize_t messagebox_read(struct file *filp, char __user *buf,size_t count, loff_t *f_pos)
{
	ssize_t return_size = 0;
	printk(KERN_NOTICE "read starts \n");
	if (down_interruptible(&glb_msgdev.sem))
	   return -ERESTARTSYS;


	if(glb_msgread != NULL){ //mesaj varsa
		printk(KERN_NOTICE "glb_msgread->to: %d   get_current_user()->uid: %d" ,glb_msgread->to ,get_current_user()->uid);
		while(return_size == 0){
			if(glb_msgread == NULL){ //kendine ait mesaji hic olmayan
				glb_msgread = glb_msgdev.messages;
				break;
			}
			if(glb_msgdev.read_mode == 1){//only see unread messages
				printk(KERN_NOTICE "read mode: %d " , glb_msgdev.read_mode);
				printk(KERN_NOTICE "isRead: %d " , glb_msgread->isRead);
				if(glb_msgread->isRead == 0){
					if(glb_msgread->to == get_current_user()->uid){
						int from_user_size;
						char* from_user = get_username(glb_msgread->from, &from_user_size);

						int total_size = from_user_size + glb_msgread->message_size + 7;

						char* total_message = kmalloc(total_size * sizeof(char), GFP_KERNEL);

						total_message[0] = 'F';
						total_message[1] = 'r';
						total_message[2] = 'o';
						total_message[3] = 'm';
						total_message[4] = ' ';
						int i = 5;

						while(i < from_user_size + 5){
							total_message[i] = from_user[i-5];
							i++;
						}

						//bob icin i = 8, total_message[7] = b 
						total_message[i] = ':';
						total_message[i+1] = ' ';
						i += 2;

						while(i < total_size){
							total_message[i] = glb_msgread->message[i-7-from_user_size];
							i++;
						}

						//"From " + get_username(glb_msgread->from) + ": " + glb_msgread->message

						return_size = total_size;
						if (copy_to_user(buf, total_message, total_size)) {
							return_size = -EFAULT;
							goto out;
						}
						glb_msgread->isRead = 1;//means that message is read
					}
				}
			}
			else{//mode = 0
				if(glb_msgread->to == get_current_user()->uid){
					int from_user_size;
					char* from_user = get_username(glb_msgread->from, &from_user_size);

					int total_size = from_user_size + glb_msgread->message_size + 7;

					char* total_message = kmalloc(total_size * sizeof(char), GFP_KERNEL);

					total_message[0] = 'F';
					total_message[1] = 'r';
					total_message[2] = 'o';
					total_message[3] = 'm';
					total_message[4] = ' ';
					int i = 5;

					while(i < from_user_size + 5){
						total_message[i] = from_user[i-5];
						i++;
					}

					//bob icin i = 8, total_message[7] = b 
					total_message[i] = ':';
					total_message[i+1] = ' ';
					i += 2;

					while(i < total_size){
						total_message[i] = glb_msgread->message[i-7-from_user_size];
						i++;
					}

					//"From " + get_username(glb_msgread->from) + ": " + glb_msgread->message

					return_size = total_size ;
					if (copy_to_user(buf, total_message, total_size)) {
						return_size = -EFAULT;
						goto out;
					}
					glb_msgread->isRead = 1;//means that message is read
				}
			}
			glb_msgread = glb_msgread->next; //mesaji olsun olmasin
		}

		printk(KERN_NOTICE "read returns %d \n", return_size);
	}
	else{
		glb_msgread = glb_msgdev.messages;
		printk(KERN_NOTICE "no more messages- read returns 0 \n");
	}

out:
	up(&glb_msgdev.sem);
	return return_size;
}

ssize_t messagebox_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{

	ssize_t retval = -ENOMEM;
	struct message * temp;
	unsigned int uid;
	char* start_of_username;
	char* parse;
	int username_size = 0;
	char* to_username;
	char *temporary_message;

	printk(KERN_NOTICE "write starts \n");

	if (down_interruptible(&glb_msgdev.sem))
	    return -ERESTARTSYS;

	temp = kmalloc(sizeof(struct message), GFP_KERNEL);
	if(!temp){
		retval = -ENOMEM;
		goto out;
	}

	//Message init
	uid = get_current_user()->uid;
	temp->from = uid;

	temp->message_size = count;
	temp->next = NULL;
	temp->message = kmalloc(count * sizeof(char), GFP_KERNEL);
	if(!temp->message){
		retval = -ENOMEM;
		goto out;
	}

	if (copy_from_user(temp->message, buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	printk(KERN_NOTICE "To check \n");
	start_of_username = temp->message;

	if(*start_of_username == 'T'){
		start_of_username++;
		if(*start_of_username == 'o'){
			start_of_username += 2; //username basini gosteriyor
		}
		else{
			retval = -EINVAL; //format hatasi
			goto out;
		}

	}
	else{
		retval = -EINVAL; //format hatasi
		goto out;
	}
	
	//gelen to username'in size'i hesaplaniyor, bu daha sonradan alan almak icin kullanilacak.
	parse = start_of_username;
	
	while(*parse != ':'){
		if(username_size >= 32){
			retval = -EINVAL; //format hatasi
			goto out;
		}
		parse++;
		username_size++;
	}

	
	to_username = kmalloc(username_size * sizeof(char), GFP_KERNEL);
	if(!to_username){
		retval = -ENOMEM;
		goto out;
	}

	//username stringe atiliyor.
	parse = start_of_username; //parse username basini gosteriyor
	int i = 0;
	while(i < username_size){
		to_username[i] = *parse;
		parse++;
		i++;
	}

	//user var mı yok mu burada kontrol edilecek
	temp->to = get_userid(to_username, username_size);
	if(temp->to == -1){
		printk(KERN_NOTICE "User not found \n");
		retval = -EINVAL; //format hatasi
		goto out;
	}

	parse += 2; //mesaj basi ": "
	temp->message_size = count - username_size - 5; //messagewith \n character at the end.
	temp->isRead = 0; // is initialized to unread
	
	//temp->message a duzgun icerik saglama
	temporary_message = kmalloc(temp->message_size * sizeof(char), GFP_KERNEL);
	if(!temporary_message){
		retval = -ENOMEM;
		goto out;
	}
	i=0;
	while(i < temp->message_size){
		temporary_message[i] = parse[i];
		i++;
	}
	kfree(temp->message);
	
	temp->message = temporary_message;
	


	//Veri yapisi islemi
	if(glb_msgdev.end != NULL){ //checks if it is first message or not
		glb_msgdev.end->next = temp;
		glb_msgdev.end = glb_msgdev.end->next;
	}
	else{//First message
		glb_msgdev.end = temp;
		glb_msgdev.messages = temp;
		glb_msgread = temp; //if its first message, read pointer points first message. else its null.
	}

	retval = count;

	printk(KERN_NOTICE "write success message: %s, size: %d \n", temp->message, temp->message_size);
out:
	up(&glb_msgdev.sem);
	printk(KERN_NOTICE "Write out \n");
	return retval;
}

int messagebox_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg){
	
	int uid = get_current_user()->uid;

	if(uid){
		return -EACCES;
	}

	switch(cmd){
	case 0:
		glb_msgdev.read_mode = 0;
		printk(KERN_NOTICE "IOCTL 0 \n");
		break;
	case 1:
		glb_msgdev.read_mode = 1;
		printk(KERN_NOTICE "IOCTL 1 \n");
		break;
	default:
		printk(KERN_NOTICE "IOCTL DEFAULT \n");
		break;
	}
	return 0;
}

struct file_operations msgbox_fops = {
	.owner =    THIS_MODULE,
	.read =     messagebox_read,
	.write =    messagebox_write,
	.unlocked_ioctl =  messagebox_ioctl,
	.open =     messagebox_open,
	.release =  messagebox_release,
};


void messagebox_cleanup_module(void)
{
	dev_t devno = MKDEV(msgbox_major, msgbox_minor);
	printk(KERN_NOTICE "cleanup starts");


	messagebox_trim();

	cdev_del(&glb_msgdev.cdev);

	unregister_chrdev_region(devno, 1);
	printk(KERN_NOTICE "cleanup completed \n");
}

int messagebox_init_module(void)
{
	printk(KERN_NOTICE "init starts \n");
	int result;
	int err;
	dev_t devno = 0;

	if (msgbox_major) {
		devno = MKDEV(msgbox_major, msgbox_minor);
		result = register_chrdev_region(devno, 1, "messagebox");
	} else {
		result = alloc_chrdev_region(&devno, msgbox_minor, 1, "messagebox");
		msgbox_major = MAJOR(devno);
	}
	if (result < 0) {
		printk(KERN_WARNING "messagebox: can't get major %d\n", msgbox_major);
		return result;
	}


	/* Initialize device. */
	glb_msgdev.messages=NULL;
	glb_msgdev.end=NULL;
	glb_msgdev.read_mode = 0;
	//devno = MKDEV(msgbox_major, msgbox_minor);
	cdev_init(&glb_msgdev.cdev, &msgbox_fops);
	glb_msgdev.cdev.owner = THIS_MODULE;
	glb_msgdev.cdev.ops = &msgbox_fops;
	sema_init(&glb_msgdev.sem,1);
	err = cdev_add(&glb_msgdev.cdev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding messagebox", err);

	printk(KERN_NOTICE "init completed \n");
	return 0; /* succeed */

}

module_init(messagebox_init_module);
module_exit(messagebox_cleanup_module);

