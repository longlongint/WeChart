/*
 * @Author: hzq 
 * @Date: 2018-08-29 15:56:14 
 * @Last Modified by: hzq
 * @Last Modified time: 2018-08-29 20:18:06
 */
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>

#include "my_io.h"
#include "login.h"
#include "format.h"
#include "rsa.h"
#include "my_epoll.h"
#include "my_socket.h"

/* 工作目录 */
const char const* g_work_path = "/home/wechart";

const char const *serv_public_key = "serv.rsa.public";
const char const *serv_private_key = "serv.rsa.private";
const char const *cli_public_key = "cli.rsa.public";
const char const *cli_private_key = "cli.rsa.private";

const char const *serv_userDataFile = "serv.user.data";
const char const *cli_userDataFile = "cli.user.data";
struct User *g_servUserdata = NULL;
struct User *g_cliUserdata = NULL;

/**
 * @brief 改变工作路径
 * 
 * @retval 
 *      成功返回0,失败返回-1
 */
int file_init(const char const *work_path,const char const* public_key,const char const *private_key,
              struct User **g_userdata,const char const* userDataFile)
{
    user_online.online_count = 0;   //初始化在线人数为0
    int err = mkdir(work_path,DIR_MODE);
    if(err == -1){
        if(errno == EEXIST){
            ;//已经存在就不用做什么
        }
        //如果路径不存在,就是说没有/home目录
        else if(errno == ENOENT){
            mkdir("/home/",DIR_MODE);
            mkdir(work_path,DIR_MODE);
        }else{
            perror("mkdir");
            return -1;
        }
    }
    print("created work path\n");

    err = chdir(work_path);
    if(err == -1){
        perror("chdir");
    }
    print("work path changed:\n\t\t");
    system("pwd");

    int fd1 = open(public_key,O_WRONLY | O_EXCL | O_CREAT | FILE_MODE);
    int fd2 = open(private_key,O_WRONLY | O_EXCL | O_CREAT | FILE_MODE);
    if(fd1 == -1 && fd2 == -1){
        if(errno == EEXIST){                //如果两个文件都已经存在
            print("file has created\n");    //说明秘钥文件已经生成
        }else{                              //如果碰到了其他问题
            perror("open");                 //打印提示信息
            return -1;                      //并退出
        }
    }else{                                  //如果缺少秘钥文件
        print("%s and %s is not created,now is trying to create it\n",public_key,private_key);
        err = create_key(public_key,private_key);     //则重新创建
        assert(err != -1);
    }

    print("key file created\n");
    
    err = read_userdata(userDataFile,g_userdata);
    print("read user data success\n");

    return err;
}


void Write(int fd, void *ptr, size_t nbytes){
	int r=write(fd, ptr, nbytes);
    if ( r!= nbytes){
		printf("write error: %d %d\n",r,(int)nbytes);
        exit(-1);
    }
}
ssize_t Read(int fd, void *ptr, size_t nbytes)
{
	ssize_t		n;

	if ( (n = read(fd, ptr, nbytes)) == -1){
		perror("read error");
    }
	return(n);
}
/** 
 * @brief  保存用户信息,保存到链表和文件
 * @note   暂时用文件维护，后面再用数据库好了
 * @param  *data: 格式如:   hzq\n!$@*%&\n142857\n\0
 * @param  *filename: 
 * @retval 
 */
int save_userData(const char *filename,struct User **g_userdata,char *data)
{ 
    assert(g_userdata != NULL);

    char *p1 = NULL,*p2 = NULL,*p3 = NULL;
    p1 = strstr((const char *)data,(const char *)"\n");
    p2 = strstr((const char *)p1+1,  (const char *)"\n");
    p3 = strstr((const char *)p2+1,  (const char *)"\n");
    if(p1==NULL || p2==NULL || p3 == NULL || p1-data >=32 || p2-p1 >= 32){
        printf("data format error\n");
        return -1;
    }
    *p1 = '\037';
    *p2 = '\037';
    *p3 = '\00';
    if(memcmp(p2+1,(*g_userdata)->m_identification,sizeof((*g_userdata)->m_identification)) != 0){
        printf("%s\n",p2+1);
        printf("%s\n",(*g_userdata)->m_identification);
        printf("identification error\n");
        return e_wongIdent;
    }
    *(p2+1) = '\0';
    char userdata[MAX_MESSAGE_SIZE];
    char ident_string[max_string_len];
    create_rand_num(11,ident_string);
    snprintf(userdata,MAX_MESSAGE_SIZE,"%s%s\n",data,ident_string);

    printf("analyse success\n");
    *p1 = '\0';
    *p2 = '\0';
    if(isUserExist(*g_userdata,data)){
        printf("user exist\n");
        return e_userExist;
    }

    int fd = open(filename,O_WRONLY|O_APPEND);
    assert(fd != -1);
    printf("try to write to user data file\n");
    Write(fd,userdata,strlen(userdata));
    close(fd);
 
    struct User *p4=*g_userdata;
    struct User *p =calloc(1,sizeof(struct User));
    memcpy(p->m_name,data,strlen(data)+1);
    memcpy(p->m_passwd,p1+1,strlen(p1+1)+1);
    memcpy(p->m_identification,ident_string,strlen(ident_string)+1);
    while(p4->next !=NULL)
        p4=p4->next;
    p4->next =p;

    print_userData(*g_userdata);

    return 0;
}
/**
 * @brief  添加朋友信息
 * @note  
 * @param  *filename: 
 * @param  **g_userdata: 
 * @param  *data: hzq\n32466214221\n\0
 * @retval 
 */
int add_friend(const char *filename,struct User **g_userdata,char *data)
{
    assert(g_userdata != NULL);

    print("line:%d,start of %s\n",__LINE__,__FUNCTION__);
    puts(data);
    char *p1 = NULL,*p2 = NULL;
    p1 = strstr((const char *)data,(const char *)"\n");
    print("line:%d\n",__LINE__);
    assert(p1!=NULL);
    p2 = strstr((const char *)p1+1,(const char *)"\n");
    print("line:%d\n",__LINE__);
    if(p1==NULL || p2==NULL || p1-data >=32 || p2-p1 >= 32){
        printf("data format error\n");
        return -1;
    }
    *p1 = '\0';
    *p2 = '\0';
    print("data = %s\n",data);
    print("p1+1 = %s\n",p1+1);
    print("line:%d\n",__LINE__);
    if(isUserExist(*g_userdata,data)){
        printf("friend \"%s\" is already in you list.\n",data);
        return e_userExist;
    }
    print("line:%d\n",__LINE__);
    if(isIdentificationExist(*g_userdata,p1+1)){
        printf("you friend has changed his name? please delete first\n");
        return e_wongIdent;
    }
    print("line:%d\n",__LINE__);
    struct User *p4=*g_userdata;
    struct User *p =calloc(1,sizeof(struct User));
    print("line:%d\n",__LINE__);
    memcpy(p->m_name,data,strlen(data)+1);
    print("line:%d\n",__LINE__);
    memcpy(p->m_identification,p1+1,strlen(p1+1)+1);
    print("line:%d\n",__LINE__);
    while(p4->next !=NULL)
        p4=p4->next;
    print("line:%d\n",__LINE__);
    p4->next =p;
    print("line:%d\n",__LINE__);


    print_userData(*g_userdata);
    save_userDatabylist(filename,*g_userdata);

    print("end of the %s\n",__FUNCTION__);
    return 0;
}
/**
 * @brief  保存链表p记录的用户到文件filename
 * @note   
 * @param  *filename: 
 * @param  *p: 
 * @retval None
 */
void save_userDatabylist(const char const *filename,struct User *p)
{
    char buf[MAX_MESSAGE_SIZE];
    bzero(buf,MAX_MESSAGE_SIZE);
    int fd = open(filename,O_WRONLY |O_TRUNC);
    if(fd == -1){
        perror("save_userDatabylist");
        exit(1);
    }
    while(p!=NULL){
        snprintf(buf,MAX_MESSAGE_SIZE,"%s\037%s\037%s\n",p->m_name,p->m_passwd,p->m_identification);
        Write(fd,buf,strlen(buf));
        p=p->next;
    }
}

/** 
 * @brief  读用户的注册文件，并用保存保存到结构体 struct user
 * @note   
 * @param  *filename: 文件名
 * @retval 成功返回0,失败返回负数
 */
int read_userdata(const char *filename,struct User **p_data)
{
    print("now try to open : %s\n",filename);
    /* 1.检查配置文件 */
    int fd_userData = open(filename,O_WRONLY | O_EXCL | O_CREAT ,FILE_MODE);
    if(fd_userData != -1){
        print("creat file \"%s\" success\n",filename);
        char buf[MAX_MESSAGE_SIZE];
        /* 用户名+密码+识别号 */
        snprintf(buf,MAX_MESSAGE_SIZE,"root\037142857Hzq076923\037142857\n");
        Write(fd_userData,buf,strlen(buf));
        close(fd_userData);
    }else{
        perror("creat user data ");
    }
    fd_userData = open(filename,O_RDONLY);
    if(fd_userData == -1){
        perror("open user data for read");
        exit(0);
    }
    
    printf("open \"%s\" success,fd = %d\n",filename,fd_userData);
    int i = 0;
    int index = 0;  //记录当前读到哪个字段
    char ch = 0;
    struct User *p = NULL,*p1=NULL;
    free_list(p_data);//先释放这个指针
    while(1){
        p = (struct User *)calloc(1,sizeof(struct User));
        //bzero(p,sizeof(struct User));
        assert(p != NULL);

        while(Read(fd_userData,&ch,1) > 0){

            /* 1.长度超过了限制 */
            if(i >= max_string_len){
                printf("file may have been damaged\n");
                free(p);
                p = NULL;
                i = 0;
                index = 0;
                break;
            }

            /* 2.遇到换行符的时候判断时候接受完成 */
            if(ch == '\n'){
                if(index != 2){
                    printf("file may have been damaged\n");
                    free(p);
                    p = NULL;
                }
                p->m_identification[i] = 0;//添加结束符
                i = 0;
                index = 0;
                break;
            }
            /* 3.碰到分隔符 */
            else if(ch == '\037'){
                ((char *)p)[index*max_string_len + i] = '\0';    //添加结束符
                index++;
                i = 0;
            }
            /* 4.正常字符 */
            else{
                ((char *)p)[index*max_string_len + i++] = ch;
            }
        }
        /* 1.读到文件尾或者遇到错误 */
        if(((char *)p)[0] == 0){
            break;
        }
        /* 正常读完一个用户的数据 */
        else{
            if(*p_data == NULL){
                *p_data = p;
                p1 = p;
            }else{
                p1->next = p;
                p1 = p1->next;
            }
        }
    }
    printf("read over\n");
    print_userData(*p_data);
    close(fd_userData);
    return 0;
}
void free_list(struct User **p)
{
    struct User *p1=*p;
    while(p1){
        p1=p1->next;
        free(*p);
        *p=p1;
    }
    *p=NULL;
}

void print_userData(struct User *g_userdata)
{
    struct User *p = g_userdata;
    printf("------------------------user information-----------------------------\n");
    while(p != NULL){
        printf("user name:   %s\n",p->m_name);
        printf("user passwd: %s\n",p->m_passwd);
        printf("user ident..:%s\n",p->m_identification);
        printf("user sess..: %s\n",p->m_session);
        printf("user ip:     %s\n",p->m_ip);
        printf("user port:   %d\n",p->m_port);
        printf("user sockfd  %d\n",p->m_sockfd);
        printf("------------------------------------------\n");
        p=p->next;
    }
    printf("------------------------------------------end of the information-----\n");
}
/** 
 * @brief  生成一个随机数字串,不会和已有的QQ号重复
 * @note   string的长度必须大于num,因为要加上结束符
 * @param  num: 
 * @param  *string: 
 * @retval 成功返回0,失败返回-1
 */
int create_rand_num(int num,char *string)
{
    int i = 0,count = 0;
    assert(num > 0);
    while(count < 20){
        int err = create_rand_string(num,string);
        assert(err != -1);
        
        for(i=0;i<num;i++){
            string[i] = (abs(string[i]) % 9) + '0';
        }
        string[num] = '\0';
        if(!isIdentificationExist(g_servUserdata,string)){
            break;
        }   
        count++;
        if(count > 20){
            printf("create rand num failed\n");
            return -1;
        }
    }
    return 0;

}
int create_rand_string(int num,char *string)
{
    assert(num > 0);
    assert(string != NULL);
    int fd_rand = open("/dev/urandom",O_RDONLY);
    if(fd_rand == -1){
        perror("open /dev/urandom");
        return -1;
    }
    int i = 0;
    while(i<num){
        i += Read(fd_rand,string+i,num-i);
    }
    close(fd_rand);
    return 0;
}
int isIdentificationExist(struct User *g_userdata,char *string)
{
    struct User *p = g_userdata;
    while(p != NULL){
        if(strlen(string) != strlen(p->m_identification)){
            p=p->next;
            continue;
        }
        if(memcmp(string,p->m_identification,strlen(string)) == 0){
            return 1;
        }
        p=p->next;
    }
    return 0;
}
int isUserExist(struct User *g_userdata,char *string)
{
    struct User *p = g_userdata;
    while(p != NULL){
        if(strlen(string) != strlen(p->m_name)){
            p=p->next;
            continue;
        }
        if(memcmp(string,p->m_name,strlen(string)) == 0){
            return 1;
        }
        p=p->next;
    }
    return 0;
}
static char sprint_buf[1024];
int print(char *fmt,...)
{
    int n = 0;
#if PRINT
    va_list args;
    va_start(args,fmt);
    n = vsprintf(sprint_buf,fmt,args);
    va_end(args);
    write(fileno(stdout),sprint_buf,n);

#endif
    return n;
}